Nous disposons désormais d'un *stub* autonome, capable d'être exécuté indépendamment de son adresse de chargement.

Son objectif est de retrouver l'exécutable original, de le restaurer en mémoire et de préparer son environnement d'exécution, afin de pouvoir lui transférer le contrôle comme l'aurait fait le kernel.

Pour ce faire, notre *stub* va devoir reproduire la pluspart des opérations que nous avons expliquées dans la partie "Rappels Théoriques". Cependant, il existe une différence importante avec le chargement classique d'un exécutable : **notre stub s'exécute à l'intérieur d'un processus qui a déjà été initialisé par Linux.**

En effet, lorsque nous lançons l'ELF packé, le kernel prépare son environnement d'exécution : il charge les segments du *stub*, initialise la *stack*, prépare l'*Auxiliary Vector*, configure certains registres, etc.

Nous allons donc essayer de réutiliser au maximum cet environnement, plutôt que de tout reconstruire nous-mêmes.

Le fonctionnement du *stub* peut être résumé ainsi :

1. Récupérer l'état initial de la *stack* et retrouver les informations concernant notre ELF packé.

2. Localiser le *payload*, puis déchiffrer et décompresser l'exécutable original.

3. Charger ses segments `PT_LOAD` aux adresses appropriées.

4. Retrouver et charger son éventuel *interpreter*.

5. Adapter la *stack* et l'*Auxiliary Vector* pour qu'ils correspondent à l'exécutable original.

6. Transférer l'exécution au point d'entrée approprié (*interpreter* ou `e_entry` de l'ELF original).

#### Récupérer le contrôle dès le point d'entrée

Comme expliqué précedemment, notre *stub* a été compilé sans le code de démarage habituel fourni par le *runtime C*.

Il nous faut donc créer notre propre point d'entrée.

Nous pourrions parfaitement définir directement notre fonction C `execute_packed_elf()` comme point d'entrée de l'exécutable, et le kernel commencerait à exécuter les instructions machine générées par le compilateur pour cette fonction.

Cependant, nous avons besoin de récupérer la valeur exacte de `RSP` au moment où le kernel nous transfère le contrôle, car elle pointe vers le début de la *stack* initiale, contenant `argc`, `argv`, `envp` et `auxv`.

Or, les instructions au début d'une fonction générées à la compilation servent souvent a modifier `RSP` pour l'aligner conformément aux *calling conventions*. `RSP` serait donc modifié avant même que nous puissions en récupérer la valeur.

Nous utilisons donc un petit point d'entrée en assembleur afin de récupérer immédiatement `RSP`, puis d'appeler notre fonction C en lui transmettant cette adresse comme argument, tout en respectant sa convention d'appel :

```
.section .text.stub_entry, "ax", @progbits

.global stub_entry
.type stub_entry, @function
stub_entry:
    mov rdi, rsp
    call execute_packed_elf
    ud2
```

La première instruction copie `RSP` dans `RDI`.

Pourquoi `RDI` ? Parce que, selon la convention d'appel utilisée sur Linux `x86-64`, le premier argument d'une fonction C est transmis dans ce registre.

Ainsi, lorsque nous appelons notre fonction :

```c
_Noreturn void execute_packed_elf(uintptr_t *stack)
```

Le paramètre `stack` contient directement l'adresse du sommet initial de la pile.

Nous pouvons ensuite écrire le reste de notre logique en C, sans nous soucier des modifications que le compilateur pourrait apporter à `RSP`.

L'instruction `ud2` sert de sécurité : elle provoque une exception si notre fonction devait retourner alors que ce comportement n'est pas prévu.

> **NOTE :** L'instruction `call` modifie elle-même `RSP`, puisqu'elle place une adresse de retour sur la pile. Cela ne pose aucun problème : nous avons déjà sauvegardé sa valeur initiale dans `RDI`, avant cet appel.

#### Effectuer des appels système sans libc

N'ayant a disposition aucune librairie, la seule manière d'effectuer des *syscalls* comme `mmap()` ou`open()` est de passer par l'instruction assembler `syscall`. 

Par soucis de longueur de ce write-up, nous n'expliquerons pas cette implémentation dans les détails. Il faut simplement retenir qu'une fonction assembler par *syscall* a été crée, et sera appelée avec les mêmes arguments qu'aurait demandé la fonction de la `libc` originelle.

> **NOTE :** Contrairement aux fonctions de la `libc`, nos wrappers ne renseignent pas `errno`. Sur cette architecture, un appel système échoué renvoie directement une valeur négative correspondant à `-errno` dans `RAX`. Il faut donc vérifier correctement ces valeurs de retour.

Liste des *syscalls* custom :

```c
ssize_t			sys_write(int fd, const void *buf, size_t count);
_Noreturn void	sys_exit(int status);
void			*sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int				sys_munmap(void *addr, size_t length);
int				sys_mprotect(void *addr, size_t size, int prot);
int				sys_open(const char *path, int flags, ...);
int				sys_close(int fd);
int				sys_fstat(int fd, struct stat *buf);
```

Exemple d'implémentation du *syscall* `write` :

```
.text

.global sys_write
.type sys_write, @function
sys_write:
	mov eax, 0x1
	syscall
	ret

.size sys_write, .-sys_write
```

Cette technique nous permet de conserver un code C relativement lisible.

Plus d'info sur l'instruction `syscall` : [syscall and the Linux x86-64 ABI - x86-64 assembly](https://asm-editor.specy.app/learn/courses/x86/talking-to-the-outside-world/syscall-and-the-linux-abi).

#### Retrouver notre propre ELF en mémoire

Après avoir récupéré `RSP`,  notre *stub* doit retrouver les données qu'il va utiliser pour commencer son travail.

Nous savons que l'ELF packé contient trois segments `PT_LOAD` :

- Un segment contenant l'ELF Header et la Program Header Table.

- Un segment contenant le code executable et les constantes du *stub*.

- Un segment contenant le *payload* (l'ELF original).

L'execution ayant déja commencé, ces segments ont été chargés en mémoire par le kernel. Il nous suffit donc de retrouver leurs adresses mémoire.

Pour cela, nous pouvons exploiter une information présente dans l'*Auxiliary Vector* en y accedant depuis l'adresse de la *stack*.

##### Retrouver l'Auxiliary Vector

Comme nous l'avons vu précédemment, la *stack* initiale contient successivement :

```c
---- Adresses hautes ----
          AT_NULL
          ...
          auxv[1]
          auxv[0]
          NULL
          ...
          envp[1]
          envp[0]
          NULL
          ...
          argv[1]
          argv[0]
          argc
---- Adresses basses ----
```

Puisque nous connaissons l'adresse de `argc`, nous pouvons parcourir cette structure pour retrouver le début de `auxv`.

Notre fonction `find_auxv_start()` avance simplement a travers la stack jusqu'a passer le `NULL` de `envp`.

Nous pouvons ensuite parcourir les entrées de l'*Auxiliary Vector* jusqu'à trouver celle qui nous intéresse.

La première information recherchée est `AT_PHDR`.

Cette entrée contient l'adresse mémoire de la *Program Header Table* de l'exécutable chargé par le kernel.

**Nous pouvons donc retrouver notre Program Header Table sans avoir besoin de connaître l'adresse à laquelle l'ELF packé a été chargé.**

Cette propriété est particulièrement intéressante pour les exécutables `ET_DYN`, dont l'adresse de chargement peut varier à chaque exécution.

##### Retrouver l'ELF Header et le load bias

Une fois notre *Program Header Table* retrouvée, nous voulons obtenir l'adresse de l'*ELF Header* qui contient les informations nécessaires pour naviguer dans le contenu de notre ELF chargé.

C'est ici que notre choix de format de packing devient utile.

Nous avons volontairement construit notre ELF packé de manière à ce que :

- L'*ELF Header* soit au début du premier `PT_LOAD`.

- La *Program Header Table* soit placée <u>immédiatement</u> après lui.

- Ces deux éléments soient chargés dans le même segment.

Nous savons donc que :

```c
stub->header = (Elf64_Ehdr*)(
    (unsigned char*)stub->phdr - sizeof(Elf64_Ehdr)
);
```

Cette opération repose sur une propriété de **notre propre format**, et ne serait pas nécessairement valable pour un ELF quelconque.

Il nous reste maintenant à déterminer le *load bias*, qui nous permettera de trouver les adresses réelles des informations, car nous le rappelons, dans le cas d'un `ET_DYN`, les adresse virtuelles renseignées dans les *headers* sont des adresses relatives à une adresse comune de chargement.

Pour notre ELF packé, deux cas sont donc possibles :

- S'il est de type `ET_EXEC`: 

  Les adresses indiquées dans les Program Headers sont déjà les adresses réelles de chargement.

  Nous utilisons donc : 

  ```c
  stub->load_bias = 0;
  ```

- S'il est de type `ET_DYN`:

  Nous avons imposé que son premier `PT_LOAD` possède un `p_vaddr` de `0`.

  Puisque l'ELF Header se trouve au début de ce segment, son adresse mémoire correspond directement au *load bias* :

  ```c
  stub->load_bias = (uintptr_t)stub->header;
  ```

Nous possédons désormais toutes les informations nécessaires pour retrouver n'importe quel segment de notre ELF packé.

Leur adresse mémoire sera simplement :

```c
segment_addr = stub->load_bias + segment->p_vaddr;
```

##### Localiser le payload

Nous devons ensuite retrouver le segment contenant l'exécutable original.

Notre packer garantit que le *payload* se trouve dans le dernier `PT_LOAD` de la *Program Header Table*.

Nous parcourons donc celle-ci depuis sa dernière entrée jusqu'à rencontrer un segment `PT_LOAD`.

Son adresse est obtenue grâce au calcul précédent.

À cet emplacement se trouve notre structure `payload_header_t`, contenant notamment les tailles des données, leur position et le *flag* indiquant si le *payload* est compressé ou non.

Nous pouvons ainsi retrouver le début du *payload* chiffré, sans avoir à rechercher une signature à travers toute la mémoire.

#### Restaurer le fichier ELF original

Une fois le *payload* localisé, nous pouvons commencer sa restauration.

La clé de déchiffrement est transmise à notre ELF packé comme premier argument après le nom du programme, c'est-à-dire `argv[1]`.

Puisque notre pointeur `stack` référence directement `argc`, cet argument se trouve à :

```c
stack[2]
```

Nous convertissons cette chaîne hexadécimale en une clé de 32 octets, puis nous l'utilisons pour déchiffrer le *payload* avec ChaCha20.

Le déchiffrement est effectué directement dans le segment contenant le *payload*. C'est la raison pour laquelle nous avons donné la permission d'écriture à ce segment.

Nous devons ensuite reconstruire le fichier ELF original.

Pour cela, nous créons un mapping anonyme en lecture/écriture, suffisamment grand pour accueillir le fichier décompressé.

Si le *payload* avait été compressé, nous utilisons LZSS pour écrire les données décompressées dans cette région. Sinon, nous y copions simplement les données déchiffrées.

Nous obtenons ainsi **une copie complète du fichier ELF original en mémoire**.

Il faut bien distinguer deux choses :

- **La représentation du fichier :** notre région temporaire contient les octets de l'ELF tels qu'ils se trouvaient sur le disque, avec son ELF Header, ses Program Headers et ses données.

- **La représentation du programme chargé :** c'est l'ensemble des segments `PT_LOAD` placés à leurs adresses d'exécution, avec leurs permissions et leur éventuel espace BSS.

Pour le moment, nous possédons uniquement la première représentation.

Le rôle des étapes suivantes sera de construire la seconde à partir de la première.

> **NOTE :** Cette distinction apparaît dans notre structure `elf_info_t`, qui contient notamment `file_header` et `file_phdr` pour accéder à la représentation du fichier, ainsi que `mem_phdr` et `load_bias` pour retrouver les informations de l'image chargée.

#### Choisir les adresses de chargement

Nous arrivons maintenant à l'une des parties les plus importantes de notre implémentation.

Nous devons reproduire les mappings des segments `PT_LOAD` de l'ELF original.

Cependant, contrairement au kernel lors d'un `execve()`, **nous travaillons dans un espace d'adressage qui contient déjà plusieurs mappings**.

Il faut notamment prendre en compte :

- Les segments de notre ELF packé, contenant le *stub* et son *payload*.

- La *stack* initiale, que nous voulons conserver.

- Les éventuelles autres régions déjà créées par le kernel.

- Nos mappings temporaires utilisés pendant le dépaquetage.

Nous devons donc choisir les adresses de chargement en évitant d'écraser ces régions.

La manière de procéder dépend du type de l'exécutable original.

##### Chargement d'un ET_EXEC

Pour un `ET_EXEC`, nous n'avons pas le choix des adresses.

Ses segments doivent être chargés aux adresses absolues indiquées dans leurs Program Headers.

Notre *load bias* sera donc égal à `0`.

Cela explique pourquoi, lors de la construction de notre ELF packé, nous avons pris soin de placer ses segments en dehors de la plage mémoire requise par l'exécutable original.

Sans cette précaution, il serait possible que le chargement d'un segment original écrase directement le code de notre *stub* pendant son exécution.

Il reste cependant un problème : même si les segments de notre ELF packé ne se trouvent pas dans cette plage, d'autres mappings pourraient l'occuper.

Nous devons donc vérifier que l'espace nécessaire est disponible.

Pour cela, notre fonction `map_ET_EXEC()` commence par déterminer l'intervalle mémoire couvrant l'ensemble des segments `PT_LOAD` de l'exécutable original.

Elle tente ensuite de réserver cet intervalle avec un mapping anonyme, en utilisant le flag `MAP_FIXED_NOREPLACE`.

Ce flag demande au kernel de créer un mapping à l'adresse exacte que nous indiquons (toujours arrondie à la frontière de page), **mais de refuser l'opération si une région est déjà occupée**.

Si la réservation échoue, cela signifie qu'entre la page de l'adrese rensignée et la dernière page de l'intervalle, il y aurait un mapping qui serait écrasé. Nous ne pouvons donc pas poursuivre sans risquer de corrompre une région existante.

Si elle réussit, nous savons que l'intervalle était disponible au <u>moment de la vérification</u>.

Dans notre implémentation actuelle, nous libérons ensuite cette réservation avant de créer les mappings individuels des segments. Ce qui implique qu'il faut être extrèmement vigilant a ne pas effectuer d'actions qui causeraint un potentiel mapping entre l'étape de vérification de place et l'étape de mapping effective.

##### Chargement d'un ET_DYN

Pour un `ET_DYN`, la situation est différente.

Nous sommes libres de choisir un *load bias*, mais nous devons respecter les contraintes imposées par les Program Headers.

La première difficulté est de trouver une plage mémoire suffisamment grande.

Nous rappelons que le *load bias* devra être le même pour tous les `PT_LOAD` pour conserver les distances relatives entre ces derniers.

Pour cela, nous commençons par déterminer quelle sera la taille de l'intervalle de chargement de l'ensemble des `PT_LOAD` (incluant également les espaces vides potentiels). On cherche donc :

- L'adresse du début du premier segment, arrondie à la frontière de page précédente.

- La plus grande adresse de fin parmi tous les segments, arrondie à la frontière de page suivante.

Nous pourrions alors simplement utiliser `mmap(NULL, taille, ...)` pour laisser Linux choisir une adresse disponible (comme pour `ET_EXEC`) et utiliser l'adresse obtenue comme *load bias*.

Cependant, un problème supplémentaire intervient : **les contraintes de `p_align`.**

###### Respecter les contraintes d'alignement

Nous avons déjà vu qu'un segment `PT_LOAD` doit respecter certaines contraintes entre ses positions dans le fichier et en mémoire.

Pour rappel, lorsqu'un `p_align` est supérieur à `1`, le format ELF impose :

```c
p_vaddr % p_align == p_offset % p_align
```

Cependant, dans le cas d'un `ET_DYN`, les adresses réellement utilisées ne sont pas directement les `p_vaddr`.

Nous devons leur ajouter notre *load bias*.

La contrainte devient donc :

```c
(load_bias + p_vaddr) % p_align == p_offset % p_align
```

Or, puisque l'ELF original est supposé respecter la première égalité, nous pouvons conserver cette propriété en choisissant un *load bias* qui est lui-même un multiple de `p_align`.

Il faut également respecter l'alignement imposé par la taille des pages mémoire.

Notre solution consiste donc à rechercher le plus grand alignement parmi :

```c
alignment = max(page_size, tous_les_p_align_des_PT_LOAD);
```

Puis à choisir un *load bias* multiple de cette valeur.

Pourquoi le plus grand alignement suffit-il ?

Parce que les valeurs de `p_align` supérieures à `1` sont des puissances de deux. Un nombre aligné sur `0x200000` sera donc également aligné sur `0x1000`, `0x2000`, etc.

Mais une difficulté subsiste.

**`mmap()` garantit un alignement sur les pages mémoire ordinaires, pas nécessairement sur un alignement plus grand comme `0x200000`.**

Nous ne pouvons donc pas utiliser directement n'importe quelle adresse retournée par `mmap()`.

Pour résoudre ce problème, nous réservons un intervalle légèrement plus grand que nécessaire.

La taille demandée est :

```
Taille de la plage PT_LOAD + alignment
```

Cet espace supplémentaire nous permet de décaler le début de notre future image vers la prochaine adresse compatible avec l'alignement recherché, tout en conservant suffisamment de place pour l'ensemble des segments.

Il faut cependant faire attention à un détail : **le premier `PT_LOAD` ne commence pas nécessairement à un `p_vaddr` de `0`.**

Nous ne devons donc pas simplement arrondir l'adresse retournée par `mmap()`.

Nous devons tenir compte de l'adresse virtuelle de début de notre intervalle :

```c
load_bias = round_to_page_up(
    (uintptr_t)raw_addr - range.start,
    alignment
);
```

Ainsi, lorsque nous ajouterons `range.start` à notre *load bias*, le début de la région à charger se trouvera bien dans l'intervalle que nous avons identifié comme disponible.

Cette opération garantit également que notre *load bias* satisfait les contraintes d'alignement.

Enfin, comme pour `ET_EXEC`, notre implémentation libère cette réservation avant de procéder aux mappings individuels.

> **NOTE :** Le choix de la valeur exacte du *load bias* n'est pas uniquement une question d'alignement. Il faut également que toutes les adresses obtenues soient valides dans l'espace utilisateur et qu'aucune opération arithmétique ne provoque de dépassement. Notre packer effectue plusieurs vérifications sur les tailles et adresses de l'ELF original afin de limiter ces problèmes.

#### Reproduire les mappings des segments

Nous connaissons désormais les adresses auxquelles les segments doivent être chargés.

Il reste à créer les mappings et à y placer les données.

Une solution évidente serait d'utiliser `mmap()` directement sur le fichier original, comme le fait généralement le kernel.

Cependant, **notre fichier original n'existe plus sous la forme d'un fichier ouvert sur le disque** : nous l'avons reconstruit dans une région mémoire anonyme.

Or, un mapping de fichier nécessite un *file descriptor* associé à un objet que le kernel peut mapper.

Nous pourrions envisager de créer un fichier temporaire ou un objet mémoire de type `memfd`, mais cela introduirait des opérations supplémentaires dont nous n'avons pas besoin.

Nous avons déjà tous les octets du fichier accessibles en mémoire.

Nous allons donc adopter une autre stratégie :

1. Créer des mappings anonymes aux adresses de chargement des segments.

2. Y copier les données nécessaires depuis notre représentation temporaire de l'ELF.

3. Initialiser les éventuels espaces BSS.

4. Appliquer les permissions finales.

Cette méthode est relativement simple, mais elle demande une précaution importante.

**Nous ne devons pas simplement copier `p_filesz` octets à partir de `p_vaddr`.**

Pourquoi ?

Parce que, comme nous l'avons expliqué dans les rappels théoriques, le kernel travaille avec des pages entières.

##### Copier les pages, pas seulement les données du segment

Reprenons l'exemple d'un segment avec les propriétés suivantes :

```c
p_offset = 0x1422
p_vaddr  = 0x402422
p_filesz = 0xE10
page_size = 0x1000
```

Le segment commence à l'adresse `0x402422`.

Mais son mapping commence nécessairement au début de la page qui contient cette adresse, soit `0x402000`.

Dans un mapping directement associé au fichier, la page commencerait par les données présentes à l'offset `0x1000` du fichier.

Les octets situés entre `0x1000` et `0x1422` se retrouveraient donc également en mémoire, avant le début réel du segment.

De même, les octets situés après la fin des données du segment pourraient aussi être présents jusqu'à la fin de sa dernière page, dans la limite de l'EOF.

Si nous copiions uniquement les `p_filesz` octets à partir de `p_vaddr`, ces données supplémentaires seraient absentes.

Pour reproduire le comportement d'un mapping de fichier, nous devons donc calculer trois informations.

**1. L'adresse de début du mapping :**

```c
page_start = round_to_page_down(
    load_bias + segment->p_vaddr,
    page_size
);
```

**2. L'offset du fichier correspondant à cette adresse :**

```c
file_off = segment->p_offset
    - ((load_bias + segment->p_vaddr) - page_start);
```

**3. La taille des données à copier :**

Nous calculons d'abord la fin des données du segment :

```c
data_end_addr = load_bias
    + segment->p_vaddr
    + segment->p_filesz;
```

Puis nous arrondissons cette adresse à la frontière de page suivante :

```c
copy_size = round_to_page_up(
    data_end_addr,
    page_size
) - page_start;
```

Il reste toutefois à s'assurer que la copie ne dépasse pas la fin du fichier original.

Nous limitons donc `copy_size` au nombre d'octets réellement disponibles à partir de `file_off`.

Nous pouvons finalement effectuer notre copie :

```c
memcpy(
    (void*)page_start,
    (unsigned char*)elf->file_header + file_off,
    copy_size
);
```

Nous obtenons ainsi le contenu que nous attendrions d'un mapping associé au fichier, y compris les octets supplémentaires éventuellement présents dans les pages couvrant le segment.

> **NOTE :** Cette approche suppose que les contraintes de correspondance entre `p_offset` et `p_vaddr` sont valides. Notre packer effectue des vérifications sur ces champs avant d'accepter l'ELF original.

##### Initialiser le BSS

Nous devons maintenant gérer le cas où `p_memsz` est supérieur à `p_filesz`.

Dans ce cas, nous savons que le segment doit contenir une région supplémentaire initialisée à zéro.

Une grande partie du travail est déjà effectuée : puisque nous avons créé des mappings anonymes, les pages qui ne contiennent pas de données copiées sont déjà initialisées à zéro.

Il reste cependant un cas particulier.

Si le segment se termine au milieu d'une page, nous avons potentiellement copié des octets provenant du fichier au-delà de la fin de ses données.

Or, si cette région appartient au BSS, elle doit contenir des zéros.

Nous devons donc explicitement effacer les octets situés entre la fin des données du segment et la fin de la page correspondante :

```c
if (segment->p_memsz > segment->p_filesz)
    memset(
        (void*)data_end_addr,
        0,
        round_to_page_up(data_end_addr, page_size)
            - data_end_addr
    );
```

Les éventuelles pages BSS supplémentaires n'ont pas besoin d'être remplies manuellement, puisqu'elles proviennent déjà de mappings anonymes.

C'est précisément ce qui nous permet de reproduire le comportement expliqué dans le schéma de chargement des segments présenté précédemment.

##### Les segments qui partagent des pages

Une autre particularité doit être prise en compte.

Deux segments `PT_LOAD` peuvent avoir des données appartenant à une même page mémoire.

C'est notamment possible lorsque la fin d'un segment et le début du suivant ne sont pas alignés sur des frontières de pages distinctes.

Notre implémentation crée les mappings des segments dans l'ordre de la Program Header Table.

Pour le premier segment, elle utilise `MAP_FIXED_NOREPLACE`, afin d'éviter d'écraser immédiatement une région déjà occupée.

Pour les segments suivants, elle utilise `MAP_FIXED`.

La différence est importante : `MAP_FIXED` autorise le remplacement d'un mapping existant à l'adresse demandée.

Cela nous permet notamment de reproduire les situations où plusieurs segments utilisent certaines pages communes.

Cependant, ce flag doit être utilisé avec précaution, car il peut également remplacer des mappings qui n'appartiennent pas à l'exécutable que nous sommes en train de charger.

Notre stratégie actuelle ne garantit pas complètement l'absence de ce type de collision.

Nous reviendrons sur cette limite plus bas.

##### Appliquer les permissions finales

Pendant le chargement, nous créons nos mappings avec les permissions `PROT_READ | PROT_WRITE`.

Cela nous permet de copier les données, même lorsque le segment final doit être uniquement lisible ou exécutable.

Mais nous ne voulons évidemment pas conserver ces permissions.

Une fois tous les segments chargés, nous parcourons à nouveau la Program Header Table pour appliquer les permissions définies par leurs `p_flags`.

Nous devons simplement convertir les flags ELF en flags utilisés par `mprotect()` :

```c
prot = 0;

if (segment->p_flags & PF_R)
    prot |= PROT_READ;

if (segment->p_flags & PF_W)
    prot |= PROT_WRITE;

if (segment->p_flags & PF_X)
    prot |= PROT_EXEC;
```

Nous appelons ensuite `mprotect()` sur l'ensemble des pages couvertes par le segment.

Puisque les permissions sont appliquées par pages entières, plusieurs segments qui partagent une page ne peuvent pas posséder des permissions indépendantes pour les octets situés dans cette page.

Dans notre implémentation, nous appliquons les permissions dans l'ordre de la Program Header Table. Pour une page partagée, les permissions du dernier segment traité seront donc celles conservées à la fin.

Nous avons maintenant reproduit les principales opérations de chargement des segments `PT_LOAD`.

#### Charger l'éventuel interpréteur

Notre exécutable original est désormais présent en mémoire, à ses adresses de chargement.

Cependant, comme nous l'avons expliqué précédemment, cela ne signifie pas nécessairement qu'il est prêt à être exécuté.

Il peut encore nécessiter l'intervention d'un *interpreter*.

Nous parcourons donc la Program Header Table originale pour rechercher un segment de type `PT_INTERP`.

S'il n'existe pas, nous pourrons directement transférer le contrôle au point d'entrée de l'exécutable.

Sinon, nous devons charger l'interpréteur avant de poursuivre.

##### Retrouver et charger le fichier de l'interpréteur

Le segment `PT_INTERP` contient le chemin absolu vers le fichier à charger.

Nous le récupérons depuis notre représentation temporaire de l'ELF original, puis nous utilisons nos appels système pour ouvrir ce fichier.

Contrairement à l'exécutable original, l'interpréteur existe toujours sur le système sous la forme d'un véritable fichier.

Nous pouvons donc le mapper temporairement en lecture avec `mmap()`.

Cette opération nous permet de retrouver son ELF Header et sa Program Header Table.

À partir de là, nous pouvons réutiliser une grande partie du code de chargement que nous venons d'implémenter.

Nous chargeons ses segments `PT_LOAD` dans une nouvelle région mémoire, puis nous appliquons leurs permissions finales.

Une fois l'opération terminée, nous pouvons fermer le *file descriptor* et libérer le mapping temporaire utilisé pour lire le fichier de l'interpréteur.

Les segments que nous avons chargés restent, eux, présents en mémoire.

##### Attention aux interpréteurs ET_EXEC

Un point important mérite d'être mentionné.

Nous avons expliqué que les interpréteurs sont généralement conçus pour pouvoir démarrer de manière autonome.

Cependant, **cela ne signifie pas nécessairement qu'ils sont tous de type `ET_DYN`.**

Linux peut également prendre en charge des interpréteurs `ET_EXEC`.

Dans ce cas, leur chargement pose un problème supplémentaire : comme pour n'importe quel `ET_EXEC`, leurs segments doivent être placés à des adresses fixes.

Or, notre packer a uniquement pris en compte les adresses de l'exécutable original lorsqu'il a choisi l'emplacement de son *stub*.

Il n'a pas nécessairement anticipé les adresses requises par un éventuel interpréteur `ET_EXEC`.

Un interpréteur de ce type pourrait donc demander un mapping qui entre en collision avec une région contenant notre *stub*.

Puisque nous sommes déjà en train d'exécuter ce code, un remplacement de ses pages pourrait provoquer une corruption immédiate de notre processus.

Dans notre implémentation actuelle, nous avons choisi de refuser les interpréteurs `ET_EXEC` plutôt que d'introduire une gestion supplémentaire de ces collisions.

Les interpréteurs `ET_DYN`, comme le *dynamic linker* habituel de la glibc, sont quant à eux pris en charge.

##### Déterminer le véritable point d'entrée

Nous pouvons maintenant déterminer à quelle adresse nous devrons transférer le contrôle.

Deux cas sont possibles.

**Sans interpréteur :**

```c
entry = (void*)(
    elf->file_header->e_entry + elf->load_bias
);
```

**Avec interpréteur :**

```c
entry = (void*)(
    interp.file_header->e_entry + interp.load_bias
);
```

Dans le second cas, nous ne transférerons donc pas directement le contrôle à l'exécutable original.

Nous commencerons par exécuter son interpréteur, exactement comme le ferait le kernel.

Ce dernier pourra alors effectuer les opérations de préparation nécessaires, notamment le chargement des bibliothèques partagées et les *relocations*, avant de transférer lui-même le contrôle au véritable programme.

#### Réutiliser la stack initiale

Nous arrivons maintenant à une autre difficulté importante.

Nous avons chargé les segments de l'exécutable original et son éventuel interpréteur.

Cependant, **la stack et l'Auxiliary Vector ont été préparés par le kernel pour notre ELF packé, et non pour l'exécutable original.**

Nous devons donc adapter cet environnement.

Une première solution aurait été de construire entièrement une nouvelle *stack* contenant les arguments, les variables d'environnement, les entrées de l'*Auxiliary Vector* et toutes les données qu'elles référencent.

Mais cette approche serait particulièrement complexe.

En effet, nous devrions notamment reproduire l'organisation de la pile, respecter les contraintes d'alignement et nous assurer que tous les pointeurs contenus dans cette structure désignent les bonnes adresses.

Or, le kernel a déjà réalisé une grande partie de ce travail pour nous.

**Notre solution consiste donc à conserver la stack originale construite par Linux, puis à modifier uniquement les informations nécessaires.**

C'est précisément pour cette raison que nous avions besoin de récupérer la valeur initiale de `RSP` dès la première instruction de notre *stub*.

##### Corriger l'Auxiliary Vector

Nous commençons par modifier les entrées qui décrivent le chargement de l'exécutable.

Dans notre implémentation, quatre champs sont modifiés :

`**AT_PHDR**` **:** doit contenir l'adresse mémoire de la Program Header Table de l'exécutable original.

Cette adresse ne doit pas être confondue avec celle de notre représentation temporaire du fichier.

Nous devons retrouver l'emplacement de la table dans les segments réellement chargés.

Il faut également noter que la présence d'un segment `PT_PHDR` n'est pas obligatoire.

Notre fonction `find_phdr_addr()` recherche donc un segment `PT_LOAD` contenant l'offset du fichier auquel se trouve la Program Header Table.

Lorsqu'elle le trouve, elle peut calculer son adresse mémoire grâce à :

```c
mem_phdr = load_bias
    + segment->p_vaddr
    + (e_phoff - segment->p_offset);
```

`**AT_PHNUM**` **:** doit contenir le nombre d'entrées de la Program Header Table originale.

`**AT_ENTRY**` **:** doit contenir l'adresse réelle du point d'entrée de l'exécutable original, c'est-à-dire `e_entry + load_bias`.

`**AT_BASE**` **:** doit contenir le *load bias* de l'interpréteur que nous avons chargé, ou `0` si aucun interpréteur n'est présent.

Ces informations permettront notamment au *dynamic linker* de retrouver correctement les structures de l'exécutable principal.

Les autres champs de l'*Auxiliary Vector* sont conservés.

Par exemple, `AT_PAGESZ` représente toujours la taille réelle des pages mémoire du système. Il n'y a donc aucune raison de modifier cette valeur.

De même, `AT_PHENT` conserve sa valeur, puisque nous travaillons toujours avec des structures `Elf64_Phdr` de taille identique.

> **NOTE :** Notre implémentation ne remplace pas toutes les informations associées à l'ELF packé. Par exemple, `AT_EXECFN` continue de référencer le chemin utilisé pour exécuter le fichier packé. Nous ne reconstruisons donc pas un environnement strictement identique à celui qu'aurait produit un `execve()` direct sur l'exécutable original.

##### Supprimer la clé des arguments

Il nous reste une autre modification importante.

Notre ELF packé attend la clé de déchiffrement comme premier argument après le nom du programme.

Par exemple :

```
./woody 0x1234...ABCD hello world
```

Cependant, l'exécutable original ne doit pas recevoir cette clé comme argument.

Nous voulons qu'il voie uniquement :

```c
argv[0] = "./woody"
argv[1] = "hello"
argv[2] = "world"
```

Nous devons donc supprimer l'entrée correspondant à la clé.

Une solution naïve consisterait à modifier uniquement `argc` et à remplacer le pointeur vers la clé.

Mais cela ne suffirait pas.

En effet, comme nous l'avons vu, la *stack* contient plusieurs tableaux placés les uns à la suite des autres :

```c
argc
argv[]
NULL
envp[]
NULL
auxv[]
```

Si nous supprimons simplement un pointeur au milieu de `argv`, sans déplacer les éléments suivants, nous risquons de perturber cette organisation.

Nous devons donc décaler **l'ensemble des données situées après le pointeur vers la clé** d'un emplacement de 8 octets vers les adresses inférieures.

Dans notre implémentation, nous utilisons `memmove()` pour cela.

Pourquoi `memmove()` plutôt que `memcpy()` ?

Parce que la région source et la région de destination se chevauchent.

`memmove()` est précisément conçu pour effectuer correctement ce type de déplacement.

Notre fonction commence donc par retrouver la fin de l'*Auxiliary Vector*, identifiée par `AT_NULL`.

Elle calcule ensuite la taille de la région à déplacer, depuis le pointeur qui suit la clé jusqu'à la fin de cette entrée `AT_NULL`.

Nous effectuons alors :

```c
ft_memmove(stack + 2, stack + 3, size);
```

Cette opération décale les pointeurs des arguments restants, les pointeurs des variables d'environnement et les entrées de l'*Auxiliary Vector*.

Nous décrémentons enfin `argc` :

```c
*(uint64_t*)stack = argc - 1;
```

La *stack* possède désormais une organisation cohérente, sans que l'argument contenant la clé apparaisse dans `argv`.

Il faut toutefois noter que nous supprimons ici **le pointeur vers la clé dans la liste des arguments**, et non nécessairement les octets de la chaîne contenant cette clé, qui peuvent toujours être présents ailleurs dans la *stack*.

Autre avantage de notre approche : puisque nous n'avons jamais modifié l'adresse du sommet initial de la *stack*, nous conservons son alignement d'origine.

> **NOTE :** Nous avons choisi de conserver `argv[0]` tel qu'il a été fourni lors du lancement du fichier packé. Cette valeur ne correspond donc pas nécessairement au nom de l'exécutable original.

#### Libérer les ressources temporaires

Nous avons maintenant terminé toutes les opérations nécessaires à notre chargement.

Avant de transférer le contrôle, nous pouvons supprimer plusieurs régions dont nous n'avons plus besoin :

- Le mapping temporaire contenant la représentation complète du fichier ELF original.

- Le mapping temporaire utilisé pour lire le fichier de l'éventuel interpréteur.

- Le segment contenant le *payload* chiffré, puis déchiffré, de notre ELF packé.

Nous conservons évidemment les segments que nous venons de charger, ainsi que la *stack* initiale.

Cette étape n'est pas indispensable au fonctionnement immédiat du programme, mais elle évite de conserver inutilement des copies de données volumineuses en mémoire.

Nous utilisons `munmap()` sur les plages correspondantes, en prenant soin d'arrondir leurs limites aux frontières des pages.

#### Transférer le contrôle à l'exécutable

Nous sommes enfin prêts à lancer l'exécutable original.

À ce stade, nous disposons :

- De ses segments `PT_LOAD`, placés à leurs adresses de chargement.

- De son éventuel interpréteur, également chargé en mémoire.

- D'une *stack* initiale dont les arguments ont été corrigés.

- D'un *Auxiliary Vector* contenant les principales informations nécessaires au démarrage de l'exécutable original.

- De l'adresse exacte de la première instruction à exécuter.

Il ne nous reste plus qu'à transférer le contrôle.

Cependant, un simple appel de fonction C ne conviendrait pas.

En effet, nous ne voulons pas **appeler une fonction** puis retourner dans notre *stub*.

Nous voulons reproduire la situation dans laquelle le kernel termine ses préparations et commence l'exécution du programme.

Cela implique notamment de restaurer `RSP` à sa valeur initiale.

Nous utilisons donc une dernière petite fonction assembleur :

```
.global jump_to_with_stack
.type jump_to_with_stack, @function

jump_to_with_stack:
    mov rsp, rsi
    xor rdx, rdx
    jmp rdi
```

Cette fonction reçoit deux arguments :

```c
jump_to_with_stack(entry, stack);
```

Selon la convention d'appel C, `entry` sera placé dans `RDI` et `stack` dans `RSI`.

La première instruction :

```
mov rsp, rsi
```

Restaure le pointeur de pile initial.

Les appels de fonctions et les variables locales utilisés pendant l'exécution du *stub* n'ont donc plus d'importance.

Ensuite :

```
xor rdx, rdx
```

Met `RDX` à zéro.

Sur l'ABI `x86-64`, ce registre possède une signification particulière lors du démarrage du code utilisateur : il peut notamment servir à transmettre une fonction de finalisation fournie par le *dynamic linker*.

Lorsque nous transférons directement le contrôle à un programme sans interpréteur, cette fonction n'existe pas. Nous initialisons donc ce registre à zéro.

Enfin :

```
jmp rdi
```

Transfère directement le contrôle à l'adresse de notre point d'entrée.

Nous utilisons `jmp` et non `call`, car nous ne voulons pas placer une nouvelle adresse de retour sur la pile.

Le programme démarre donc avec `RSP` pointant vers `argc`, exactement comme attendu lors d'un démarrage ELF classique.

Si un interpréteur était présent, c'est lui qui commence à s'exécuter. Il effectue alors ses propres opérations de préparation, avant de transférer le contrôle à l'exécutable original.

Dans le cas contraire, nous commençons directement l'exécution au `e_entry` original.

Notre *stub* a terminé son travail.

#### Limites de notre implémentation

Notre objectif était de reproduire les principales opérations nécessaires pour permettre à l'exécutable original de s'exécuter à partir de sa représentation chiffrée et compressée.

Cependant, notre méthode ne reproduit pas parfaitement toutes les opérations effectuées par Linux lors d'un véritable `execve()`.

Plusieurs différences méritent d'être mentionnées.

**La gestion des collisions mémoire.**

Nous utilisons des réservations temporaires pour déterminer les adresses de chargement, mais notre implémentation les libère avant de créer les mappings définitifs.

De plus, les segments suivant le premier sont chargés avec `MAP_FIXED`, qui peut remplacer des mappings existants.

La stratégie la plus robuste consisterait à conserver la réservation de l'intervalle pendant toute l'opération, puis à remplacer uniquement les pages que nous avons nous-mêmes réservées.

Il faudrait également traiter avec attention les éventuels espaces vides entre les segments.

Notre approche actuelle fonctionne pour les cas que nous ciblons, mais elle ne garantit pas l'absence de toutes les collisions possibles.

**Les informations conservées par le kernel.**

Même si nous avons adapté les structures visibles dans la *stack*, le kernel conserve également ses propres informations sur le processus.

Par exemple, l'emplacement initial de la *heap* et les informations utilisées pour certaines opérations comme `brk()` ont été déterminés lors du chargement de notre ELF packé.

Le simple fait de mapper les segments originaux ne suffit pas à faire recalculer toutes ces informations comme si le kernel avait directement chargé l'exécutable original.

**Les informations d'identification de l'exécutable.**

Nous ne modifions pas toutes les informations permettant d'identifier le programme exécuté.

Par exemple, `AT_EXECFN` et certains éléments exposés par `/proc` peuvent continuer à faire référence à l'ELF packé.

**Les types d'interpréteurs pris en charge.**

Nous refusons actuellement les interpréteurs `ET_EXEC`, même si Linux peut en prendre certains en charge.

Enfin, notre *stub* repose sur plusieurs hypothèses concernant le format des ELF qu'il reçoit. Une partie de ces contraintes est vérifiée par le packer avant la construction du nouvel exécutable, mais notre chargeur ne prétend pas reproduire l'intégralité des vérifications et des comportements du kernel.

Ces limites ne remettent pas en cause l'objectif pédagogique du projet : comprendre comment un exécutable peut être reconstruit et démarré directement depuis la mémoire.

Elles montrent cependant qu'il existe une différence importante entre **reproduire suffisamment de mécanismes pour permettre à un programme de fonctionner** et **reproduire intégralement le comportement du chargeur ELF de Linux**.
