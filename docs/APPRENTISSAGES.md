# Woody : comprendre le chargement d'un exécutable en construisant un packer

Woody est un projet d'études inspiré d'[UPX](https://upx.github.io/). L'idée de
départ est de fabriquer un exécutable qui embarque un autre programme et le code
nécessaire à sa préparation en mémoire, puis lui passe la main.

L'objectif est d'apprendre ce qui se passe entre « je lance un programme » et
« son code commence à s'exécuter » : format ELF, mémoire virtuelle, chargement,
liaison dynamique, pile initiale, conventions d'appel et génération de code.
Le temps disponible est limité. Le projet comporte donc des simplifications,
des cas non pris en compte et potentiellement des erreurs, y compris dans des
parties qui fonctionnent pour des exécutables ordinaires. Il ne prétend ni
remplacer le chargeur Linux, ni atteindre la compatibilité et la maturité d'UPX.

Ce texte décrit les mécanismes observables dans le code actuel. Les raisons
explicitement données dans les commentaires ou les notes sont présentées comme
telles ; ailleurs, les conséquences d'un choix sont expliquées sans inventer
une intention à son auteur. Les anciennes pistes de `todo.txt` ne constituent
pas une spécification à jour.

Il suffit de connaître les bases du C : fonctions, structures, tableaux et
pointeurs. Les notions système sont introduites avant leur utilisation.
Les exemples d'adresses sont illustratifs, sauf indication contraire. Le texte
s'appuie sur le C, l'assembleur, le Makefile et le script de liaison du projet.

## Parcours de lecture

1. [Le problème et le vocabulaire](#1-le-problème-et-le-vocabulaire)
2. [Ce qui se passe avant main](#2-ce-qui-se-passe-avant-main)
3. [Lire un ELF : sections, segments et en-têtes](#3-lire-un-elf--sections-segments-et-en-têtes)
4. [Passer du fichier à la mémoire](#4-passer-du-fichier-à-la-mémoire)
5. [Adresses fixes, PIE et load bias](#5-adresses-fixes-pie-et-load-bias)
6. [Construire un stub autonome](#6-construire-un-stub-autonome)
7. [Fabriquer le nouvel ELF](#7-fabriquer-le-nouvel-elf)
8. [Valider les entrées et protéger les fichiers](#8-valider-les-entrées-et-protéger-les-fichiers)
9. [Retrouver le programme embarqué](#9-retrouver-le-programme-embarqué)
10. [Charger ses segments](#10-charger-ses-segments)
11. [Confier la liaison dynamique à l'interpréteur](#11-confier-la-liaison-dynamique-à-linterpréteur)
12. [Préserver la pile et corriger auxv](#12-préserver-la-pile-et-corriger-auxv)
13. [Libérer les images et transférer le contrôle](#13-libérer-les-images-et-transférer-le-contrôle)
14. [Organiser le code et la construction](#14-organiser-le-code-et-la-construction)
15. [Les limites actuelles et leurs raisons](#15-les-limites-actuelles-et-leurs-raisons)
16. [Observer le fonctionnement soi-même](#16-observer-le-fonctionnement-soi-même)
17. [Ce que le projet permet de comprendre](#17-ce-que-le-projet-permet-de-comprendre)

## 1. Le problème et le vocabulaire

Une archive compressée contient des données qu'un autre logiciel doit extraire.
Un exécutable empaqueté contient aussi le code qui permettra de préparer le
programme original à son propre démarrage. UPX applique ce principe à la
compression d'exécutables ; Woody en explore les mécanismes de chargement.

Dans l'état actuel de Woody, le programme original est **copié intégralement,
sans compression ni chiffrement**. `decrypt_payload()` porte déjà le nom d'une
étape future, mais ne fait que retourner un pointeur vers les octets embarqués.
Les champs de taille et de clé préparent une évolution du format ; ils ne
constituent pas une fonctionnalité déjà implémentée.

Le vocabulaire du dépôt distingue cinq objets :

| Terme | Ce qu'il désigne |
| --- | --- |
| `packer` | Le programme qui lit l'exécutable à empaqueter et écrit `woody`. |
| `target` | Le fichier ELF original donné au packer. |
| `output` | Le nouvel ELF produit, nommé `woody`. |
| `payload` | Le fichier original stocké comme données dans `woody`. |
| `stub` | Le code embarqué qui retrouve le payload, le charge et le lance. |

Il y a deux moments distincts :

```mermaid
flowchart LR
    T[ELF original] --> P[packer]
    S[Stub compilé et embarqué dans packer] --> P
    P --> W[Fichier woody]
    W --> K[Chargement par Linux]
    K --> R[Exécution du stub]
    R --> M[Chargement du programme original]
    M --> E[Passage à son interpréteur ou à son entrée]
```

Le packer est un programme de construction. Le stub est un programme de
démarrage. Ils n'ont ni les mêmes dépendances ni les mêmes contraintes.

## 2. Ce qui se passe avant main

Un fichier sur disque n'est pas encore un programme prêt à fonctionner. Il
décrit du code, des données et la façon de les disposer dans un processus.
Le processeur a besoin d'instructions présentes à des adresses accessibles,
avec des permissions adaptées et un état initial cohérent.

Sous Linux, `execve()` remplace le programme d'un processus par un nouvel
exécutable. Cela ne signifie pas nécessairement créer un nouveau processus.
Le noyau prépare notamment les mappings et la pile initiale. Pour un ELF
dynamique, il charge aussi l'interpréteur indiqué par `PT_INTERP` et lui confie
le démarrage. Voir [execve(2)](https://man7.org/linux/man-pages/man2/execve.2.html).

Le point d'entrée ELF, `e_entry`, n'est généralement pas `main`. Dans un
programme C classique, du code de démarrage et la bibliothèque C interviennent
avant `main`, puis prennent en charge sa terminaison.

```text
Programme dynamique ordinaire :
  noyau -> interpréteur ELF -> entrée du programme -> démarrage C -> main

Programme sans interpréteur :
  noyau -> entrée du programme -> son propre démarrage

Avec Woody :
  noyau -> entrée du stub -> chargement manuel de l'original
                          -> interpréteur éventuel -> entrée de l'original
```

Une première difficulté se déduit de cette chaîne : sauter directement à
`main` court-circuiterait tout ce qui devait être préparé avant lui. Même un
saut vers `e_entry` peut être prématuré si la liaison dynamique reste à faire.

Le stub ne relance pas l'original avec un nouvel `execve`. Il charge ses
segments dans le processus où il s'exécute déjà. Il doit donc préserver son
propre code pendant cette opération et préparer lui-même une partie de l'état
habituellement fourni par le noyau.

## 3. Lire un ELF : sections, segments et en-têtes

### 3.1. Un fichier structuré

ELF signifie *Executable and Linking Format*. Ce format sert notamment aux
exécutables, aux bibliothèques partagées et aux fichiers objets. Woody cible
seulement ELF64, little-endian, pour x86-64.

Little-endian signifie que l'octet de poids faible est stocké en premier. Par
exemple, la valeur sur 32 bits `0x12345678` s'écrit `78 56 34 12` en mémoire.
La classe et l'ordre des octets doivent être vérifiés avant d'interpréter les
champs avec les structures natives. Le [manuel elf(5)](https://man7.org/linux/man-pages/man5/elf.5.html)
décrit ces structures.

Le fichier commence par un `Elf64_Ehdr`. Parmi ses champs :

| Champ | Question à laquelle il répond |
| --- | --- |
| `e_ident` | Est-ce un ELF ? Quelle classe et quel ordre des octets ? |
| `e_type` | Quel type d'objet : notamment `ET_EXEC` ou `ET_DYN` ? |
| `e_machine` | Pour quelle architecture ? |
| `e_entry` | À quelle adresse virtuelle commence le programme ? |
| `e_phoff`, `e_phnum`, `e_phentsize` | Où sont les program headers, combien y en a-t-il et quelle est leur taille ? |
| `e_shoff`, `e_shnum`, `e_shstrndx` | Où sont les sections et où trouver leurs noms ? |

Un offset compte des octets depuis le début d'un fichier. Une adresse désigne
une position dans l'espace mémoire du processus. Confondre les deux produit
un pointeur qui peut sembler plausible tout en visant le mauvais endroit.

### 3.2. Deux lectures complémentaires : sections et segments

Les **sections** organisent le contenu pour les outils de compilation et de
liaison : `.text` pour du code, `.rodata` pour des constantes, `.data` pour des
données initialisées, `.bss` pour des données à initialiser à zéro, etc.

Les **program headers** décrivent ce qui est utile au démarrage. Les entrées
`PT_LOAD` indiquent les zones à charger ; d'autres entrées fournissent des
informations complémentaires. Tout program header ne correspond donc pas à une
allocation mémoire indépendante. Un segment chargeable peut contenir plusieurs
sections.

```text
Vue des outils :     .text   .rodata   .data   .bss   symboles   informations debug
                        \      |        \      /
Vue du chargement :   segments PT_LOAD, plus métadonnées de démarrage
```

Cette distinction explique deux choix de Woody : le packer cherche la **section**
`.stub` dans son propre fichier, mais le stub charge les **segments** de
l'original. Le fichier de sortie n'a pas de table des sections : sa table des
program headers suffit à décrire son chargement.

### 3.3. Les champs d'un PT_LOAD

| Champ | Sens dans le chargement |
| --- | --- |
| `p_offset` | Début des données du segment dans le fichier. |
| `p_vaddr` | Adresse virtuelle du segment, avant ajout éventuel du load bias. |
| `p_filesz` | Nombre d'octets provenant du fichier. |
| `p_memsz` | Nombre d'octets occupés en mémoire. |
| `p_flags` | Droits ELF : lecture `PF_R`, écriture `PF_W`, exécution `PF_X`. |
| `p_align` | Contrainte d'alignement du segment. |

`p_paddr` concerne l'adressage physique dans les environnements où il a un
sens. Woody le renseigne, mais ce n'est pas l'adresse physique de la RAM
utilisée par son processus Linux.

Si `p_memsz > p_filesz`, la fin du segment doit être initialisée à zéro. Les
`PT_LOAD` sont ordonnés par adresse virtuelle croissante. Ces règles et les
contraintes d'alignement sont décrites dans la
[gABI, Program Loading](https://gabi.xinuos.com/elf/07-pheader.html).

## 4. Passer du fichier à la mémoire

### 4.1. Une adresse virtuelle n'est pas une position sur le disque

Un processus travaille dans un espace d'adresses virtuelles. Le système établit
les correspondances nécessaires vers la mémoire physique et contrôle les
accès. Réserver une grande plage virtuelle ne signifie pas nécessairement
consommer immédiatement autant de RAM.

Pour un octet situé à l'offset `f` dans un segment, la conversion est :

```text
adresse à l'exécution = load_bias + p_vaddr + (f - p_offset)
```

Cette formule s'applique aux octets couverts par la partie fichier du segment.
Exemple avec `p_offset = 0x2000`, `p_vaddr = 0x402000` et `load_bias = 0` :
l'octet à l'offset `0x2123` arrive à l'adresse `0x402123`.

Un fichier entier mappé pour être lu et ses segments chargés pour être exécutés
sont deux représentations différentes. Dans le premier cas, on accède à
`file_header + offset`. Dans le second, on utilise les adresses des segments.
Le type `elf_info_t` garde justement des pointeurs distincts : `file_phdr`
pour l'image fichier et `mem_phdr` pour la table dans l'image chargée.

### 4.2. Les pages imposent une granularité

Les opérations de mémoire virtuelle travaillent par pages. Les exemples qui
suivent prennent une page de `4096` octets, soit `0x1000`. Le packer lit la
taille avec `sysconf(_SC_PAGESIZE)` ; le stub la récupère dans `AT_PAGESZ`.
Cela n'efface pas toutes les hypothèses de plateforme du projet : le layout
annonce aussi un `p_align` constant de `0x1000`.

Les helpers de `src/common/page.h` calculent :

```text
down(x, P) = x - x % P
up(x, P)   = x si x % P == 0, sinon x + P - x % P

down(0x2345, 0x1000) = 0x2000
up  (0x2345, 0x1000) = 0x3000
```

Pour couvrir un segment `[début, début + taille[`, il faut les pages :

```text
[down(début, P), up(début + taille, P)[
```

Le crochet ouvert signifie que la borne de fin est exclue. Cette convention
permet d'écrire la taille comme `fin - début`.

Une conséquence essentielle : deux segments distincts peuvent toucher la même
page. On ne peut pas raisonner seulement en intervalles d'octets des segments
quand on applique des mappings ou des permissions.

### 4.3. L'alignement est une relation entre fichier et mémoire

Un segment peut commencer au milieu d'une page. Ce qui compte pour un
chargement par pages est de conserver le même déplacement dans la page :

```text
p_vaddr % page_size == p_offset % page_size
```

Quand `p_align > 1`, ELF impose aussi une puissance de deux et :

```text
p_vaddr % p_align == p_offset % p_align
```

Cela explique pourquoi aligner seulement `p_vaddr` sur `0x1000` ne suffit pas
si `p_offset` vaut, par exemple, `0x120`.

Le loader actuel exploite cette relation lorsqu'il remonte du début d'un
segment au début de sa page pour retrouver les octets correspondants dans le
fichier. La validation vérifie la congruence modulo `p_align` quand il dépasse
1 ; elle ne vérifie pas séparément la congruence modulo la taille de page
pour tous les cas. C'est une hypothèse imparfaitement contrôlée, pas une
garantie générale de validité. Voir la section sur les limites.

## 5. Adresses fixes, PIE et load bias

### 5.1. ET_EXEC : respecter des adresses imposées

Pour les exécutables `ET_EXEC` pris en charge, le loader utilise les adresses
`p_vaddr` telles quelles. Le `load_bias` vaut zéro.

Si l'original attend du code à `0x401000`, déplacer simplement ce code à une
autre adresse ne suffit pas : le programme peut contenir des références qui
supposent son placement d'origine. Woody doit laisser ces adresses disponibles.

Le packer choisit donc un emplacement pour **son propre ELF de sortie** avant
ou après l'intervalle occupé par l'original. Il tient compte de l'ensemble de
cet intervalle, y compris les trous entre segments.

```text
Exemple de placement ET_EXEC, adresses croissantes :

[en-têtes woody][stub][payload] ... [segments de l'original à leurs adresses]
          ou
[segments de l'original à leurs adresses] ... [en-têtes woody][stub][payload]
```

Il faut protéger l'ensemble de l'enveloppe : écraser le payload pendant qu'on
le copie serait aussi problématique qu'écraser le code du stub.

### 5.2. ET_DYN et PIE : conserver les distances, déplacer l'ensemble

`ET_DYN` désigne un type ELF employé notamment pour les bibliothèques partagées
et les exécutables PIE (*Position Independent Executable*). Ce type seul ne
prouve pas qu'un fichier soit un programme autonome que Woody saura lancer.

Pour ces fichiers, le loader choisit un déplacement commun :

```text
adresse à l'exécution = load_bias + adresse virtuelle décrite par l'ELF
```

Si le biais vaut `0x70000000`, une entrée à `0x1230` se retrouve à
`0x70001230`. Toutes les distances relatives entre segments sont conservées.
L'ASLR est la randomisation du placement mémoire ; un code capable de changer
de base permet au système de varier ce placement. PIE et ASLR ne désignent
donc pas la même chose.

Woody conserve le type de l'original dans sa sortie : `ET_EXEC` reste
`ET_EXEC`, `ET_DYN` reste `ET_DYN`. Pour sa propre sortie `ET_DYN`, le packer
fait commencer le premier segment à l'adresse virtuelle zéro. Le stub peut
alors déduire son biais de l'adresse réelle de son en-tête.

### 5.3. Pourquoi le plus grand p_align compte

Déplacer un ELF ne doit pas casser ses contraintes d'alignement. Puisque le
fichier respectait déjà `p_vaddr % A == p_offset % A`, il suffit que le biais
soit multiple de `A` pour préserver cette égalité.

`determine_global_alignement()` retient le maximum entre la taille de page et
les `p_align`. Pour les puissances de deux attendues sur cette plateforme,
être multiple du plus grand garantit d'être multiple des plus petits.

`map_ET_DYN()` demande une zone un peu plus grande que l'intervalle nécessaire
afin de pouvoir décaler son début et choisir un biais correctement aligné.
La nuance entre début de mapping et biais sera reprise au chapitre 10.

## 6. Construire un stub autonome

Avant les options du Makefile, il faut distinguer les étapes de fabrication.
Le compilateur traduit le C en code machine, avec une étape d'assemblage ;
l'assembleur traduit aussi les fichiers `.s`. Les fichiers objets `.o`
contiennent du code et des références qui peuvent encore rester à résoudre.
L'éditeur de liens, ici `ld` pour le stub, rassemble ces objets et résout les
références entre leurs symboles. Le chargeur intervient plus tard, quand le
programme doit être disposé dans un processus pour s'exécuter.

```text
sources C et assembleur -> objets .o -> liaison -> stub.elf
                                                    |
                                      extraction de .stub par objcopy
                                                    |
                                      ajout dans le fichier packer
```

### 6.1. Le problème du démarrage de celui qui doit démarrer les autres

Le packer utilise les services ordinaires de la libc : `printf`, `open`,
`mmap`, etc. Le stub, lui, doit fonctionner avant que l'environnement de
l'original soit prêt. Lui donner ses propres dépendances dynamiques rendrait
son extraction et son déplacement beaucoup plus complexes.

La solution du projet est un stub **freestanding** : son code assure son
entrée, ses opérations mémoire et ses appels au noyau. Inclure `<elf.h>` ou
`<sys/mman.h>` donne des types et des constantes ; cela ne signifie pas que
le binaire soit lié à une bibliothèque qui implémente ces fonctions.

Les options de construction se complètent :

| Option | Rôle dans le projet |
| --- | --- |
| `-ffreestanding` | Compiler pour un environnement sans démarrage C hébergé ordinaire. |
| `-fno-stack-protector` | Éviter les dépendances du mécanisme de protection de pile ordinaire. |
| `-nostdlib` | Exprimer l'absence de bibliothèques et de démarrage standard lors d'une liaison via le pilote ; ici les compilations C utilisent `-c` et la liaison finale passe directement par `ld`. |
| `-fPIE` | Générer du code destiné à supporter un déplacement de sa base. |
| `ld -static --no-dynamic-linker` | Lier le stub sans chargeur dynamique. |
| `ld -T linker_script` | Imposer le contenu et l'ordre de la section extraite. |

Freestanding ne signifie pas que le compilateur n'émettra jamais un appel de
fonction auxiliaire. Clang peut encore générer des appels à `memcpy`, `memmove`
ou `memset`, notamment pour manipuler des structures. C'est pourquoi `src/ft/`
fournit aussi ces noms exacts, en plus des variantes `ft_*`. Voir les
[explications de Clang](https://clang.llvm.org/docs/UsersManual.html#freestanding-builds).

### 6.2. Les appels système et les deux conventions d'appel

Un appel système demande un service au noyau. Une fonction de libc est du code
utilisateur qui peut effectuer un tel appel et transformer son résultat.
Le stub utilise directement l'instruction x86-64 `syscall`.

Les petites fonctions de `src/stub/syscalls/` font le pont entre le C et Linux :

| Position | Appel de fonction C, System V AMD64 | Appel système Linux x86-64 |
| --- | --- | --- |
| Numéro du service | Sans objet | `rax` |
| Argument 1 | `rdi` | `rdi` |
| Argument 2 | `rsi` | `rsi` |
| Argument 3 | `rdx` | `rdx` |
| Argument 4 | `rcx` | `r10` |
| Argument 5 | `r8` | `r8` |
| Argument 6 | `r9` | `r9` |
| Résultat | `rax` | `rax` |

Ainsi `sys_mmap.s` doit déplacer le quatrième argument :

```asm
mov eax, 9       # numéro Linux x86-64 de mmap
mov r10, rcx     # adaptation du quatrième argument
syscall
ret
```

`syscall` altère notamment `rcx` et `r11`. Les erreurs brutes sont des codes
négatifs ; les wrappers du stub ne les traduisent pas en `errno`. Le packer,
qui utilise la libc, vérifie au contraire `MAP_FAILED` et utilise `perror`.
Voir les [conventions des appels système](https://man7.org/linux/man-pages/man2/syscall.2.html).

Les vérifications `(intptr_t)result < 0` du stub supposent ici les plages
d'adresses utilisateur usuelles de la plateforme. Ce n'est pas une interface
portable vers toutes les architectures.

### 6.3. Du code copiable exige plus qu'une option du compilateur

Le stub est lié une première fois, puis sa section `.stub` est copiée dans un
autre ELF à une autre adresse. Un appel relatif reste valable si l'appelant et
la cible sont déplacés ensemble. Une adresse absolue inscrite dans le code ne
se corrige pas spontanément après cette copie.

Le script rassemble donc, dans cet ordre :

```text
.text.stub_entry -> .text et .text.* -> .rodata et .rodata.* -> une section .stub
```

L'entrée assembleur est la première chose copiée. Le packer peut définir
`e_entry` comme le début du segment stub sans chercher un symbole dans la
sortie. Code et constantes restent dans le même bloc.

Le script écarte toutes les autres sections. Cela impose de ne pas dépendre de
données globales modifiables en `.data` ou `.bss`, de tables externes ou d'un
support d'exécution qui aurait été supprimé. Les variables locales du stub
sont sur sa pile ; ses tables constantes sont en lecture seule.

La contrepartie est une discipline de construction stricte : `-fPIE` et une
liaison sans dépendances ne prouvent pas à eux seuls que tous les octets copiés
sont autonomes. Il faut aussi inspecter le résultat généré. Le
[manuel de GNU ld](https://sourceware.org/binutils/docs/ld/Scripts.html)
explique le contrôle du layout par script de liaison.

## 7. Fabriquer le nouvel ELF

### 7.1. Embarquer le stub dans le packer

Le Makefile construit d'abord `stub_bin/stub.elf`, extrait sa section `.stub`
avec `objcopy --dump-section`, puis ajoute ces octets comme section `.stub`
dans le fichier `packer` avec `objcopy --add-section`.

Cette section sert de réserve de code. Elle n'a pas besoin d'être exécutée ou
même chargée avec les segments du packer. À l'exécution, celui-ci ouvre
`/proc/self/exe`, mappe son propre fichier et retrouve `.stub` à l'aide de la
table des sections et de sa table de noms.

Le packer peut donc fonctionner sans aller chercher `stub.elf` dans le
répertoire courant. En revanche, supprimer sa section `.stub` ou sa table des
sections casse cette méthode d'extraction.

### 7.2. Le layout de sortie

`elf_layout_t` contient l'en-tête et quatre program headers :

| Entrée | Type | Permissions | Contenu ou rôle |
| --- | --- | --- | --- |
| 1 | `PT_LOAD` | R | En-tête ELF et table des program headers. |
| 2 | `PT_LOAD` | RX | Code du stub et constantes. |
| 3 | `PT_LOAD` | RW | En-tête du payload puis fichier original. |
| 4 | `PT_GNU_STACK` | Selon l'original, RW par défaut | Indication des permissions de pile ; pas un quatrième bloc à charger. |

Le code et les constantes partagent un segment RX : c'est simple à extraire,
mais les constantes se trouvent aussi dans des pages exécutables. Le segment
payload est RW, ce qui prépare une transformation de ses données sur place ;
dans la version actuelle, le payload est seulement lu pour être copié.

Le wrapper ne contient ni `PT_INTERP` ni `PT_DYNAMIC`. Son démarrage dépend du
stub autonome. Les informations dynamiques de l'original restent dans le
fichier original embarqué.

```text
Sur disque, dans l'ordre d'écriture :

offset 0
  +-------------------------------+
  | En-tête ELF : 64 octets       |
  +-------------------------------+
  | 4 program headers : 4 x 56    |
  +-------------------------------+ <- offset 0x120
  | Octets de .stub               |
  +-------------------------------+
  | En-tête payload : 40 octets   |
  +-------------------------------+
  | Fichier ELF original complet |
  +-------------------------------+
```

Il n'y a pas de padding ajouté entre ces blocs par `fill_output_elf()`. Le
packer espace leurs adresses **virtuelles**, tout en conservant les restes
nécessaires à l'alignement fichier/mémoire.

Par exemple, le stub commence dans le fichier à `0x120`. Avant l'ajout de la
base de sortie, son adresse virtuelle vaut `0x1120` pour des pages de `0x1000`.
Ces deux nombres ont le même reste `0x120`. Les octets des en-têtes peuvent
ainsi apparaître aussi dans le préfixe de la page du stub ; les pages virtuelles
des segments sont distinctes, même si les pages du fichier qu'elles couvrent
se recoupent.

Ce détail explique pourquoi un schéma montrant systématiquement du padding
sur disque entre segments serait trompeur pour cette implémentation.

### 7.3. Un petit protocole entre packer et stub

`src/common/payload.h` définit `payload_header_t`. Sur la cible x86-64 actuelle :

| Offset relatif à l'en-tête payload | Champ | Taille | Valeur actuelle |
| --- | --- | --- | --- |
| 0 | `magic` | 8 | `WOODY` puis trois octets nuls. |
| 8 | `compressed_size` | 8 | Taille du fichier original. |
| 16 | `decompressed_size` | 8 | Même taille. |
| 24 | `offset` | 8 | 40, début du fichier original depuis l'en-tête payload. |
| 32 | `key` | 8 | Zéro. |

La structure est initialisée à zéro avant que les champs soient renseignés.
Le magic écrit inclut le zéro final de la chaîne `"WOODY"` ; le stub compare
ces six octets. Les deux octets restants ne sont pas contrôlés.

Cet en-tête est un contrat binaire : les deux composants doivent s'accorder
sur la taille, l'ordre des champs et leur sens. Le nom `offset` ne désigne ici
ni un offset depuis le début de `woody`, ni une adresse absolue.

Un magic permet d'identifier un format. Il ne constitue pas un contrôle
d'intégrité, encore moins une authentification cryptographique.

## 8. Valider les entrées et protéger les fichiers

### 8.1. Lire une structure ne rend pas ses champs fiables

Un fichier externe peut annoncer une table au-delà de sa fin, une taille
impossible ou une adresse dont l'addition déborde. Le convertir en pointeur
vers `Elf64_Ehdr` ne prouve rien sur la validité de ces valeurs.

`check_elf_validity()` procède par couches : métadonnées du fichier, en-tête,
puis table des program headers et contenu des segments. Il vérifie notamment
le format attendu, les tailles d'en-têtes, la présence de `PT_LOAD`, leur
ordre, leurs bornes, certains alignements et la présence de l'entrée dans un
segment exécutable. Pour `PT_INTERP`, il contrôle une longueur minimale et le
zéro terminal dans les bornes du fichier.

Il refuse aussi les fichiers set-user-ID ou set-group-ID. Ces modes concernent
les privilèges et le démarrage sécurisé ; le projet ne cherche pas à reproduire
leur comportement. Cela ne signifie pas que tous les mécanismes de privilèges
du système soient gérés ou validés par Woody.

### 8.2. Les additions peuvent mentir à cause d'un débordement

Avec des entiers non signés de taille fixe, dépasser la valeur maximale fait
repartir le résultat au début de l'intervalle. Une très grande taille peut donc
produire une petite somme.

Pour vérifier qu'un contenu tient dans le fichier, le projet utilise le motif :

```c
if (offset > file_size || size > file_size - offset)
    /* refuser */;
```

L'ordre compte : la soustraction n'est interprétable qu'une fois `offset`
validé. De même, avant `adresse + taille`, on vérifie
`taille <= UINT64_MAX - adresse`. Il faut encore vérifier que l'arrondi à la
page supérieure ne déborde pas après cette addition.

Ces contrôles garantissent certaines préconditions aux helpers communs.
`get_mapping_range()` calcule un intervalle ; il ne remplace pas un validateur.
L'interpréteur système est cependant chargé sans passer par ces mêmes
vérifications, ce qui constitue une frontière de confiance du projet.

### 8.3. Protéger le fichier original, même avec deux noms différents

`create_output_file()` ouvre `woody` **sans** `O_TRUNC`, puis compare le couple
`st_dev` / `st_ino` avec celui de l'entrée. Deux chemins peuvent désigner le
même fichier, par exemple via un lien physique. Comparer leurs chaînes de
caractères serait insuffisant.

Le fichier de sortie n'est tronqué qu'après cette comparaison. La validation
de l'entrée, la recherche du stub et le calcul du layout ont également lieu
avant l'ouverture de sortie, pour éviter d'abîmer une sortie existante lors de
ces échecs préalables.

En revanche, une fois la sortie tronquée, une erreur d'écriture ne restaure
pas l'ancienne version : le code tente de supprimer la sortie incomplète.
Chaque `write()` doit écrire la taille demandée, sinon il est traité comme un
échec ; il n'y a pas de boucle pour les écritures partielles ou `EINTR`.
L'écriture atomique via fichier temporaire reste une amélioration possible.

Les chemins `cleanup` centralisent fermeture et libération. Sauvegarder
`errno` avant ces opérations évite qu'une erreur de nettoyage masque celle
qui expliquait l'échec initial.

## 9. Retrouver le programme embarqué

### 9.1. Entrer sans démarrage C ordinaire

Le noyau transfère le contrôle à `stub_entry`, placé au début de la section
copiée. À cet instant, `rsp`, le registre de sommet de pile, désigne la pile
initiale du processus. Le stub en conserve l'adresse avant d'appeler du C :

```asm
stub_entry:
    mov rdi, rsp
    call execute_packed_elf
    ud2
```

Dans la convention C utilisée, `rdi` porte le premier argument. La fonction
`execute_packed_elf(uintptr_t *stack)` reçoit donc un pointeur vers la pile
initiale. `call` ajoute son adresse de retour sur la pile et le C y installe
ses variables locales, mais le pointeur sauvegardé continue de viser l'état
initial. La pile se développe vers les adresses inférieures.

`ud2` est une instruction volontairement invalide : revenir ici serait une
erreur de logique. Le chemin normal termine le processus en cas d'échec ou
transfère définitivement l'exécution.

### 9.2. Le stub commence par demander où il se trouve

Le stub parcourt la pile pour trouver le vecteur auxiliaire, ou `auxv`. Ce
tableau contient des couples type/valeur fournis au démarrage. Il y lit :

- `AT_PAGESZ`, pour connaître la taille de page ;
- `AT_PHDR`, pour retrouver sa propre table de program headers chargée.

Le format construit par le packer place cette table immédiatement après
l'en-tête ELF. `init_stub_info()` remonte donc de `sizeof(Elf64_Ehdr)` depuis
`AT_PHDR` pour retrouver l'en-tête. C'est une convention **interne à Woody**,
pas une méthode applicable à n'importe quel ELF.

Pour le wrapper `ET_EXEC`, le biais vaut zéro. Pour le wrapper `ET_DYN`, le
premier segment a été construit à l'adresse virtuelle zéro : l'adresse réelle
de l'en-tête donne alors le biais. Cette simplification concerne le wrapper,
pas tous les ELF `ET_DYN` que le loader pourrait rencontrer.

### 9.3. Le dernier PT_LOAD contient le payload

`find_elf_payload_header()` parcourt les program headers en sens inverse
jusqu'au dernier `PT_LOAD`. Il saute donc le `PT_GNU_STACK`, situé après les
segments chargeables dans la sortie.

Son adresse effective est :

```text
payload_header = stub.load_bias + dernier_PT_LOAD.p_vaddr
```

Après comparaison du magic, `decrypt_payload()` retourne actuellement :

```text
adresse de l'en-tête payload + payload_header.offset
```

On obtient l'image **fichier** de l'original. Elle contient encore son propre
en-tête, ses tables et tous ses octets. Le travail de chargement commence
seulement à ce stade.

## 10. Charger ses segments

Le cœur du chargement est dans `src/stub/loader.c`. Une lecture utile consiste
à séparer quatre questions : où disposer l'ensemble, quelles pages créer,
quels octets y placer et quels droits appliquer.

### 10.1. Déterminer l'intervalle global

`get_mapping_range()` prend le début du premier `PT_LOAD`, arrondi vers le bas,
puis le maximum des fins `p_vaddr + p_memsz`, arrondies vers le haut. Il
s'appuie sur l'ordre des segments déjà attendu par le reste du programme.

```text
Adresse virtuelle :
  [ LOAD A ]........trou........[ LOAD B ][ LOAD C ]
  ^                                               ^
  range.start                                     range.end

Taille de la plage = range.end - range.start
```

Les trous ne sont pas du contenu à copier. Leur étendue compte toutefois pour
trouver un placement qui préserve les distances relatives entre les segments.

### 10.2. Tester le placement d'un ET_EXEC

`map_ET_EXEC()` demande d'abord toute cette plage à ses adresses imposées avec
`PROT_NONE` et `MAP_FIXED_NOREPLACE`. Cela vérifie qu'elle est disponible sans
écraser les mappings existants. Il retire ensuite cette réservation et charge
les segments individuellement, avec un biais nul.

Cette réservation est donc temporaire : elle ne reste pas en place pendant
les copies et les trous ne restent pas réservés. Cela économise le maintien
d'une grande zone entre segments, mais affaiblit la garantie contre des
collisions ultérieures. Le code ne fournit pas une réservation atomique de
l'ensemble jusqu'à la fin du chargement.

### 10.3. Choisir le placement d'un ET_DYN

Pour `ET_DYN`, `map_ET_DYN()` demande à `mmap(NULL, ...)` une zone de taille :

```text
range.end - range.start + alignment
```

Le noyau retourne `raw_addr`, alignée sur une page. Le code calcule :

```text
load_bias = up(raw_addr - range.start, alignment)
première adresse occupée = load_bias + range.start
```

Soustraire `range.start` évite de confondre biais et première adresse chargée.
Le premier segment de l'original ne commence pas nécessairement à l'adresse
virtuelle zéro. L'espace supplémentaire rend possible cet ajustement tout en
gardant l'intervalle choisi à l'intérieur de la réservation, dans les cas
d'adresses et d'arithmétique attendus.

Exemple : `raw_addr = 0x70001000`, `range.start = 0x2000`, alignement `0x10000`.
Le biais arrondi vaut `0x70000000` et la première page occupée `0x70002000`.
Arrondir simplement `raw_addr`, puis ajouter `range.start`, ne représente pas
la bonne manière de calculer le biais dans le cas général : ici, cela donnerait
`0x70012000` pour la première page et ferait dépasser la fin de la réservation
à l'image complète.

La réservation est ensuite libérée avant le chargement effectif, comme pour
`ET_EXEC`. Une adresse choisie par `mmap` n'est donc pas une propriété acquise
pour toujours : après `munmap`, la zone est à nouveau disponible.

### 10.4. Créer les pages et copier les données

`map_segment()` crée un mapping privé anonyme, initialement RW, pour les pages
du segment. « Anonyme » signifie ici qu'il n'est pas adossé à un fichier ; ses
octets sont initialement nuls. `MAP_PRIVATE` exprime que les modifications ne
doivent pas être partagées comme celles d'un mapping partagé.

L'original est déjà présent sous forme de données dans le payload. Le loader
copie donc ces octets vers les pages d'exécution, au lieu de remapper un
fichier original ouvert séparément. Cette organisation pourra aussi recevoir
une image obtenue après décompression. Elle coûte des copies et des mappings
supplémentaires, mais sépare clairement source et destination.

Le premier segment non vide utilise `MAP_FIXED_NOREPLACE` : s'il rencontre une
zone occupée, le mapping échoue. Les suivants utilisent `MAP_FIXED`, qui peut
remplacer des pages déjà mappées. Cette différence permet aux segments
successifs de partager des pages ; elle exige aussi de maîtriser précisément
ce qui peut déjà s'y trouver. Les effets de ces flags sont documentés dans
[mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html).

### 10.5. Pourquoi copier aussi les bords de page

`fill_segment()` ne se contente pas de copier exactement `p_filesz` octets à
partir de `p_offset`. Il cherche à reproduire le contenu des pages d'un
mapping de fichier, y compris autour des limites logiques du segment :

```text
page_start = down(load_bias + p_vaddr, page_size)
file_off   = p_offset - ((load_bias + p_vaddr) - page_start)
copy_size  = up(load_bias + p_vaddr + p_filesz, page_size) - page_start
```

La copie est limitée à la fin du fichier. Les octets restants conservent les
zéros du mapping anonyme.

Cela devient utile quand un segment ultérieur recrée une page déjà touchée
par un segment antérieur : ne recopier que les octets strictement intérieurs
au segment pourrait perdre du contenu du bord de page. Cette stratégie vise
les dispositions ELF habituelles ; elle ne prouve pas que tous les
chevauchements construits artificiellement seront correctement reproduits.

### 10.6. La BSS est une obligation d'initialisation

Considérons un segment avec `p_filesz = 1000` et `p_memsz = 9000`. Les
1000 premiers octets viennent du fichier, les 8000 suivants doivent valoir
zéro. Il serait inutile de stocker ces milliers de zéros sur disque.

```text
Fichier :  [ données : p_filesz ]
Mémoire :  [ données : p_filesz ][ zéros : p_memsz - p_filesz ]
```

Le mapping anonyme initialise déjà les pages à zéro. Mais la copie élargie
aux pages peut avoir importé des octets du fichier après la fin logique des
données. Si `p_memsz > p_filesz`, `fill_segment()` remet donc à zéro la fin de
la dernière page copiée. Les pages suivantes sont restées nulles. Un segment
entièrement BSS, avec `p_filesz == 0`, n'a rien à copier.

Oublier cette étape peut laisser s'exécuter le programme tout en faussant ses
variables globales : une erreur de chargement n'est pas toujours un crash.

### 10.7. Appliquer les permissions après toutes les copies

Les mappings sont d'abord RW pour permettre leur initialisation. Une fois
tous les segments copiés, `apply_permissions()` traduit les flags ELF vers
les flags de `mprotect`, puis applique les droits finaux.

Cette traduction doit être explicite : les constantes `PF_R`, `PF_W`, `PF_X`
n'ont pas les mêmes valeurs numériques que `PROT_READ`, `PROT_WRITE`,
`PROT_EXEC`. Copier simplement l'entier des flags donnerait de mauvais droits.

Attendre la fin évite de rendre une page non modifiable avant une copie
ultérieure qui la touche aussi. Quand plusieurs segments partagent une page,
la dernière opération `mprotect` dans l'ordre des program headers détermine
ses droits finaux. Les droits ne sont pas fusionnés par une union automatique.

Ce découpage évite de demander systématiquement RWX pendant la préparation.
Il n'interdit cependant pas un segment RWX si les permissions de l'original
le demandent.

## 11. Confier la liaison dynamique à l'interpréteur

### 11.1. Mapper le code ne résout pas ses dépendances

Un programme peut appeler une fonction comme `printf` dont l'implémentation
est dans une bibliothèque partagée. Il reste alors à charger les dépendances,
résoudre leurs symboles et adapter certaines références aux adresses réelles.
Une **relocation** est une correction décrite dans les métadonnées ELF pour
effectuer ce type d'ajustement.

La GOT contient des entrées utilisées pour atteindre des adresses résolues ;
la PLT fournit des séquences de passage pour certains appels externes. Leur
fonctionnement complet dépend de l'architecture et du mode de liaison. Ce
qui compte ici est qu'un simple déplacement de segments ne remplace pas la
liaison dynamique. La [gABI, Dynamic Linking](https://gabi.xinuos.com/elf/08-dynamic.html)
décrit cette coopération.

### 11.2. Réutiliser le chargeur du système

`PT_INTERP` contient le chemin de l'interpréteur ELF, souvent un chargeur comme
`ld-linux-x86-64.so.2`. Le terme « interpréteur » ne signifie pas ici qu'il
interprète le langage C ou chaque instruction machine : il prépare
l'exécution et la liaison du programme.

`find_interpreter()` copie ce chemin dans un buffer local de `PATH_MAX`
octets. Cela permet de le garder pendant la préparation. `load_interpreter()`
ouvre ce fichier, mappe son image pour la lire, puis utilise `map_ET_DYN()`
pour charger ses segments.

Le stub ne contient pas un second éditeur de liens dynamique : il charge
l'interpréteur demandé et lui donne un état initial qui décrit l'original.
Cela réduit fortement le périmètre à réimplémenter, mais dépend de la présence
et de la compatibilité du chargeur et des bibliothèques du système.

Le code refuse actuellement les interpréteurs qui ne sont pas `ET_DYN`. Le
commentaire explique la raison : un interpréteur `ET_EXEC` pourrait imposer
des adresses qui écrasent le stub. Résoudre ces conflits demanderait une
gestion mémoire plus élaborée.

### 11.3. Deux adresses d'entrée à distinguer

S'il y a un interpréteur, le saut immédiat vise :

```text
interp.load_bias + interp.file_header->e_entry
```

L'entrée de l'original reste, elle, donnée par :

```text
elf.load_bias + elf.file_header->e_entry
```

Elle sera transmise dans `AT_ENTRY`. Sans interpréteur, le stub saute
directement à cette entrée de l'original. Ce chemin suppose que le programme
sache assurer lui-même ce qui reste nécessaire à son démarrage. L'absence de
`PT_INTERP` ne suffit donc pas à garantir que n'importe quel ELF soit pris en
charge, notamment pour certains modes PIE statiques.

## 12. Préserver la pile et corriger auxv

### 12.1. La pile initiale est une interface binaire

La pile initiale contient les arguments, l'environnement et le vecteur
auxiliaire. Les mots représentés ici font huit octets sur la cible du projet :

```text
Adresses croissantes
  rsp initial -> argc
                 argv[0] ---------> chaîne du premier argument
                 argv[1] ---------> chaîne du deuxième argument
                 ...
                 NULL
                 envp[0] ---------> chaîne NOM=VALEUR
                 ...
                 NULL
                 type auxv, valeur
                 type auxv, valeur
                 ...
                 AT_NULL, 0
                 ... autres données et chaînes initiales ...

La pile des appels du stub se développe vers les adresses inférieures.
```

`find_auxv_start()` saute `argc`, parcourt `argv` jusqu'à son pointeur nul,
puis fait de même pour `envp`. Il interprète la suite comme des
`Elf64_auxv_t`. `find_auxv_field()` cherche un type jusqu'à `AT_NULL`.

Le stub conserve les arguments et l'environnement reçus lors du lancement de
`woody`. Il ne retire pas `argv[0]`, contrairement à une ancienne piste des
notes. Un lancement `./woody bonjour` transmet donc cet appel à l'original ;
il ne rétablit pas le nom du fichier donné précédemment au packer.

### 12.2. La pile décrit initialement le mauvais ELF

Au démarrage du stub, `auxv` décrit l'enveloppe `woody`, car c'est elle que le
noyau vient de charger. Après chargement de l'original, ces informations
doivent désigner celui-ci.

`fix_auxv()` remplace quatre valeurs :

| Champ | Valeur fournie avant le transfert |
| --- | --- |
| `AT_PHDR` | Adresse de la table des program headers de l'original dans son image chargée. |
| `AT_PHNUM` | Nombre de program headers de l'original. |
| `AT_ENTRY` | Entrée réelle de l'original, biais inclus. |
| `AT_BASE` | Biais de l'interpréteur chargé, zéro s'il n'y en a pas. |

Le sens des champs est documenté dans
[getauxval(3)](https://man7.org/linux/man-pages/man3/getauxval.3.html).
Le stub lit lui-même ce tableau et n'appelle pas la fonction libc `getauxval`.

Sans ces corrections, le chargeur dynamique pourrait chercher les métadonnées
du programme dans les trois segments de l'enveloppe et préparer le mauvais
objet. Charger les bons octets ne suffit pas : il faut que les pointeurs qui
les décrivent soient cohérents.

Les autres entrées restent telles que le noyau les a fournies. Par exemple,
`AT_PHENT` reste compatible parce que la taille des entrées est la même ;
`AT_EXECFN` continue de correspondre au lancement de l'enveloppe. Les données
comme `AT_RANDOM` et les informations système restent en place. Woody ne
reconstruit donc pas une pile nouvelle en totalité.

### 12.3. Retrouver AT_PHDR sans imposer PT_PHDR

Un ELF n'a pas obligatoirement une entrée `PT_PHDR`. `find_phdr_addr()` cherche
plutôt un `PT_LOAD` qui contient l'offset `e_phoff` et applique la conversion :

```text
adresse de la table = load_bias + p_vaddr + (e_phoff - p_offset)
```

Cette adresse doit pointer dans l'image chargée, car l'image fichier sera
libérée. C'est la raison d'être du champ `mem_phdr`.

Limite du code : il contrôle que le **début** de la table tombe dans la
partie fichier d'un segment, sans vérifier que toute la table y soit
accessible. S'il ne trouve pas de segment, il retourne le biais. Ce repli ne
crée pas magiquement une table valide ; certains ELF peuvent donc passer la
validation du packer et rester inutilisables pour ce démarrage.

## 13. Libérer les images et transférer le contrôle

### 13.1. Supprimer la source après avoir préparé la destination

À ce stade, le processus contient à la fois des images fichiers utilisées
comme sources et les segments chargés pour l'exécution. `munmap_files()`
retire l'image fichier de l'interpréteur ainsi que les pages du payload.

Pour le payload, le calcul tient compte de l'en-tête et de l'offset interne :

```text
début = down(adresse_payload_header, page_size)
fin   = up(adresse_payload_header + offset + compressed_size, page_size)
```

Les adresses à utiliser après ce nettoyage doivent avoir été calculées avant.
Un pointeur vers l'en-tête fichier ne doit plus être déréférencé. En revanche,
les segments chargés, `mem_phdr`, les chaînes d'arguments et la pile restent
disponibles. Fermer un descripteur et supprimer un mapping sont des opérations
distinctes : `munmap` est ce qui retire l'accès mémoire.

Les en-têtes de l'enveloppe et les pages de code du stub restent mappés. Le
stub ne se désalloue pas lui-même juste avant son saut final. Ce résidu fait
partie des différences avec un chargement direct de l'original.

### 13.2. Restaurer la pile avant de sauter

`jump_to_with_stack(entry, stack)` reçoit l'adresse cible dans `rdi` et la
pile initiale dans `rsi` :

```asm
mov rsp, rsi
xor rdx, rdx
jmp rdi
```

Restaurer `rsp` permet au prochain démarrage de retrouver `argc` à l'endroit
attendu, sans les cadres d'appels du stub.

`rdx` peut porter, dans le protocole de démarrage utilisé par la libc, un
pointeur de finalisation fourni par le chargeur dynamique. Le mettre à zéro
évite de transmettre un résidu comme s'il s'agissait d'une fonction valide,
notamment pour un programme sans interpréteur. Quand le stub saute au chargeur
dynamique, c'est ce dernier qui préparera ensuite l'entrée de l'original.

`jmp` effectue un transfert définitif. Un `call` ajouterait une adresse de
retour sur la pile restaurée et décalerait ce que l'entrée cible interprète
comme `argc`. `_Noreturn` exprime aussi au compilateur que le chemin normal ne
revient pas à l'appelant.

## 14. Organiser le code et la construction

Les modules suivent les étapes du problème :

| Fichiers | Responsabilité à lire dans le code |
| --- | --- |
| [`src/packer/main.c`](../src/packer/main.c) | Orchestrer construction et nettoyage. |
| [`file.c`](../src/packer/file.c), [`validate.c`](../src/packer/validate.c) | Lire l'entrée et vérifier les préconditions. |
| [`stub_section.c`](../src/packer/stub_section.c) | Retrouver les octets du stub dans le packer. |
| [`layout.c`](../src/packer/layout.c), [`output.c`](../src/packer/output.c) | Calculer les en-têtes puis écrire le fichier. |
| [`execute_packed_elf.c`](../src/stub/execute_packed_elf.c) | Orchestrer le démarrage, avec échecs terminaux. |
| [`payload.c`](../src/stub/payload.c), [`loader.c`](../src/stub/loader.c) | Retrouver l'image originale et charger ses segments. |
| [`interpreter.c`](../src/stub/interpreter.c), [`auxv.c`](../src/stub/auxv.c) | Charger l'interpréteur et préparer ses informations de démarrage. |
| [`entry.s`](../src/stub/entry.s), [`jump_to_with_stack.s`](../src/stub/jump_to_with_stack.s), [`syscalls/`](../src/stub/syscalls/) | Assurer les transitions qui demandent des conventions de registres précises. |
| [`src/common/`](../src/common/) | Partager le format payload et les calculs de pages et d'intervalles. |
| [`src/ft/`](../src/ft/) | Fournir les primitives mémoire et chaînes nécessaires. |

`stub_info_t` décrit l'enveloppe déjà chargée. `elf_info_t` décrit l'original
ou l'interpréteur : image fichier, taille, tables et biais. Ces types évitent
de faire passer un unique pointeur « base » pour plusieurs notions différentes.

Les fonctions internes sont généralement `static`. Les headers exposent les
interfaces entre modules. Les codes d'erreur sont propres au packer et au
stub, dont les moyens d'affichage diffèrent : libc d'un côté, `sys_write` de
l'autre. Le helper `fail()` affiche l'erreur du stub puis termine via
`sys_exit`, au lieu de laisser continuer un chargement incomplet.

Les sources communes sont compilées **deux fois**, dans `packer_bin/` et
`stub_bin/`, avec les options du composant concerné. Réutiliser un `.o` du
packer dans le stub pourrait importer du code produit avec des hypothèses
inadaptées. Les fichiers de dépendances `.d`, générés avec `-MMD`, permettent
de recompiler les objets concernés lorsqu'un header change.

Le répertoire `src/elf_parser_tests/` est un espace d'expérimentation séparé
du build principal. Il contient notamment des explorations des relocations et
de la pile. Il aide à retrouver des sujets étudiés, mais ses mécanismes ne
doivent pas être attribués automatiquement au stub actuel.

Les notes de `todo.txt` montrent également l'évolution de la conception.
L'idée d'un segment de réservation permanent pour l'original `ET_EXEC` a été
remplacée par le placement de l'enveloppe hors de sa plage, puis un chargement
par le stub. Les restrictions anciennes sur `p_align` ne décrivent pas non
plus exactement le calcul actuel d'un alignement global. Une note d'étude
conserve le raisonnement passé ; seul le code indique ce qui s'exécute.

## 15. Les limites actuelles et leurs raisons

Une limite n'est pas toujours une erreur. Elle peut être un choix de périmètre,
une simplification acceptable pour apprendre ou un défaut restant à corriger.
Le distinguer permet de parler honnêtement du projet sans lui demander les
garanties d'un outil de production.

Le temps limité d'un projet d'études explique qu'on privilégie des mécanismes
compréhensibles et un chemin de fonctionnement principal. Il ne transforme
pas une hypothèse en garantie : des entrées peuvent être acceptées sans être
correctement exécutées, et des erreurs peuvent subsister hors des points
identifiés ci-dessous. Cette liste décrit les limites visibles, sans prétendre
être exhaustive ni tenir lieu d'audit de sécurité.

### 15.1. Un périmètre volontairement restreint

| Limitation | Raison ou intérêt du choix actuel | Conséquence et travail pour aller plus loin |
| --- | --- | --- |
| Linux, ELF64, x86-64, little-endian. | Étudier un seul format et une seule convention de démarrage garde le problème abordable. Les appels système et l'assembleur sont spécifiques à cette cible. | D'autres plateformes demanderaient des validations, types, conventions et chemins de chargement dédiés. |
| Entrées `ET_EXEC` et `ET_DYN` seulement. | Ce sont les deux familles traitées par le loader. | Un fichier objet `ET_REL` n'est pas un programme prêt à démarrer ; il faudrait notamment un travail de liaison. Tous les usages possibles de `ET_DYN` ne sont pas pour autant supportés. |
| Interpréteur `ET_DYN` seulement. | Le commentaire de `load_interpreter()` mentionne explicitement le risque de collision d'un interpréteur à adresses fixes avec le stub. | Un interpréteur `ET_EXEC` est refusé. Le supporter demanderait une stratégie pour gérer les conflits, au-delà de la simple copie de segments. |
| Fichiers setuid/setgid refusés. | Le validateur exclut ces cas ; le projet ne reproduit pas leur sémantique de privilèges et de démarrage sécurisé. | Le packer ne doit pas être présenté comme conservant les propriétés de sécurité ou les métadonnées système de l'original. |
| Liaison dynamique déléguée à l'interpréteur du système. | Réutiliser ce composant permet de se concentrer sur le chargement et l'état initial. | Le chemin `PT_INTERP` doit exister et les dépendances doivent être disponibles. Empaqueter ne rend pas le programme autonome vis-à-vis du système. |

Le projet ne documente pas de prise en charge générale des PIE statiques, des
chargeurs inhabituels ou des exécutables exigeant des propriétés de démarrage
particulières. Sans interpréteur, il saute à l'entrée : il n'ajoute pas une
phase générique de relocations. Un démarrage qui effectue lui-même certaines
préparations peut fonctionner, mais ce n'est pas une garantie pour toute la
famille « statique ».

### 15.2. Les fonctionnalités préparées mais absentes

**Compression et chiffrement.** Les tailles stockées sont égales, la clé reste
nulle et `decrypt_payload()` ne transforme aucun octet. L'état actuel permet
d'étudier le chargement indépendamment de l'algorithme de transformation. Pour
ajouter ces fonctions, il faudrait décider du format, des buffers, des bornes,
des erreurs et du moment où les données deviennent disponibles au loader.

Cela implique aussi que le fichier produit contient l'original en clair et
ajoute l'enveloppe à sa taille : aucune réduction de taille n'est obtenue
actuellement. Inspiré d'UPX ne signifie pas équivalent à son implémentation.

**Intégrité du payload.** Le magic identifie l'en-tête, mais les champs de
taille, d'offset et le contenu ELF embarqué ne sont pas revalidés par le stub.
La version actuelle fait confiance au fichier construit par son packer.
Un `woody` modifié après construction peut provoquer des accès invalides ou
des calculs incorrects. Ajouter un checksum permettrait de détecter certaines
corruptions ; cela ne remplacerait ni la validation des bornes ni, si elle
était recherchée, une authentification cryptographique.

**Robustesse de l'écriture.** Le contrôle des alias protège l'entrée contre
une troncature immédiate, mais la sortie n'est pas produite de façon atomique.
Une amélioration serait d'écrire complètement un fichier temporaire, gérer les
écritures partielles et interruptions, puis remplacer la sortie après succès.
Les propriétés exactes des fichiers et permissions à préserver resteraient à
définir.

### 15.3. Des frontières de confiance et des validations incomplètes

**Le packer se fait confiance.** `find_packer_section()` précise qu'il doit
être utilisé sur le packer produit par le build. Il ne vérifie pas toutes les
bounds de la table des sections et des noms. Cela simplifie un chemin supposé
interne, mais rendre le packer arbitrairement modifiable invalide cette
hypothèse. Un parseur robuste devrait contrôler ces tables avant chaque accès.

**L'interpréteur est supposé valide.** Après ouverture du chemin `PT_INTERP`,
le stub lit ses en-têtes et appelle le loader sans lui appliquer le validateur
du packer. Même un fichier système peut manquer, être remplacé ou ne pas
correspondre à l'architecture attendue. La vérification `ET_DYN` seule ne suffit
pas à établir sa validité. Une validation partageable et compatible avec
l'environnement freestanding réduirait cet écart.

**La congruence de page n'est pas entièrement vérifiée.** Le loader recule
`p_offset` du déplacement du début de segment dans sa page. Si la relation
fichier/adresse est incompatible, ce calcul peut produire une mauvaise source,
voire déborder en non signé. Le validateur vérifie le modulo de `p_align` quand
il est supérieur à 1, mais un `p_align` petit, nul ou égal à 1 ne garantit pas
à lui seul la relation modulo la page. Il faudrait expliciter cette
précondition et vérifier les soustractions avant les copies.

**La table des program headers peut être mal représentée après chargement.**
Le validateur prouve que la table existe dans le fichier. Cela ne prouve pas
qu'elle sera entièrement accessible dans un segment chargé. `find_phdr_addr()`
ne contrôle que son début et peut retourner le biais par défaut. Fournir un
`AT_PHDR` valide exige soit des contraintes d'entrée plus strictes, soit une
copie durable de la table avec des règles compatibles avec le chargeur ciblé.

**Toutes les additions ne sont pas protégées de bout en bout.** Le code
contrôle plusieurs débordements importants, notamment lors de la validation
et du calcul de taille du payload. D'autres calculs runtime ajoutent un biais,
une adresse ou une taille sans preuve locale complète. Des valeurs extrêmes
ou un payload corrompu peuvent dépasser les cas anticipés. Les adresses doivent
aussi être acceptables pour le système, pas seulement tenir dans `uint64_t`.

**Les structures sont lues par des pointeurs C.** L'en-tête du fichier original
se trouve après des blocs de taille variable et n'est pas explicitement réaligné
sur disque. Les accès non alignés ordinaires sont généralement possibles sur
x86-64, mais cela ne constitue pas une preuve de correction des casts de
structures au regard des exigences d'alignement du C. Une version plus robuste
pourrait lire les champs avec des copies vers des objets correctement alignés,
ou définir et vérifier un alignement du payload.

Ces points montrent pourquoi « reconnu comme ELF acceptable » et « correctement
chargeable par ce loader » restent deux propriétés différentes.

### 15.4. Une stratégie mémoire compréhensible, mais pas générale

**Réservation temporaire.** Le loader demande une zone globale, retient son
placement, puis la libère avant de mapper ses segments. Cette stratégie est
explicitement décrite dans les notes de conception. Elle vérifie une
disponibilité à un instant donné et évite de garder mappés tous les trous ;
elle laisse cependant une fenêtre entre vérification et utilisation.
Le premier segment utilise `MAP_FIXED_NOREPLACE`, mais les suivants utilisent
`MAP_FIXED`, qui peut écraser un mapping apparu entre-temps. Garder une
réservation dont le loader maîtrise les pages, puis remplacer seulement ces
pages, serait une piste plus solide.

**Segments très éloignés.** Libérer les trous ensuite ne supprime pas le coût
de la demande initiale : `get_mapping_range()` couvre toujours toute l'étendue.
Un ELF très dispersé peut échouer à cause des limites d'espace virtuel ou des
ressources, même si la somme de ses segments est faible. Les notes mentionnent
ce problème comme corrigé ; le calcul actuel conserve cette demande globale
temporaire, il faut donc nuancer cette annotation historique.

**Bornes d'adresses simplifiées.** Le placement du wrapper `ET_EXEC` suppose
une adresse minimale de `0x10000` et une borne haute liée à un espace
utilisateur classique sur 47 bits. Les commentaires les annoncent comme
hypothèses. Elles ne décrivent pas toutes les configurations possibles de
Linux x86-64. Le layout utilise aussi un alignement annoncé de `0x1000`.
Généraliser demanderait de définir les environnements supportés et de contrôler
les contraintes effectives plutôt que de traiter ces constantes comme
universelles.

**Pages partagées entre segments.** Les mappings successifs peuvent remplacer
une même page. Le loader recopie ses bords et applique les permissions dans
l'ordre des program headers. Ce choix est explicite et intelligible ; il
n'établit pas une équivalence avec le noyau pour chaque chevauchement possible.
Il faudrait définir les chevauchements acceptés ou reconstruire leur résultat
avec un modèle de pages plus complet.

**Protection de la pile.** `PT_GNU_STACK` est copié dans l'enveloppe pour que
le noyau configure la pile dès le lancement. Cela évite au stub de découvrir
et modifier lui-même toute la zone de pile. La pile peut donc rester exécutable
si l'original le demande. Ce mécanisme ne prend pas en charge à lui seul toutes
les autres propriétés ELF liées à la sécurité ou au matériel.

### 15.5. Le stub et l'identité du processus restent particuliers

**Le code extrait n'est pas automatiquement certifié autonome.** Le script
garde seulement code et constantes. Une future modification peut introduire
une dépendance à `.data`, `.bss`, une adresse absolue ou du support compilateur.
La présence de `-fPIE` ne suffit pas à la détecter. Des vérifications des
sections, des symboles non définis, des relocations et du désassemblage
pourraient rendre le contrat de build plus explicite. L'absence de relocation
résiduelle dans un ELF lié ne prouve pas à elle seule l'absence d'adresse
absolue déjà résolue.

**Les permissions sont simplifiées.** Le stub réunit son code et ses constantes
en RX et conserve un payload RW jusqu'au nettoyage. C'est pratique pour un
bloc autonome et une future transformation sur place, mais pas un effort
d'optimisation maximal de la surface exécutable ou de la mémoire modifiable.

**L'environnement de l'original n'est pas reproduit à l'identique.** La pile
reçue est réutilisée et seuls quatre champs auxv sont changés. Le noyau a
exécuté `woody` : `/proc/self/exe` continue de désigner l'enveloppe, `argv[0]`
reste celui du lancement et le stub laisse une partie de ses mappings en
place. D'autres états gérés par le noyau, comme les informations relatives au
tas initial, ne sont pas reconstruits comme lors d'un `execve` de l'original.
Un programme qui inspecte sa propre image ou suppose un état très précis peut
donc observer des différences ou échouer.

**Certaines métadonnées ne sont pas traitées spécialement.** Le stub ne
reproduit pas explicitement tous les effets de notes ELF, de `PT_GNU_PROPERTY`
ou de toutes les extensions du noyau et du chargeur. Des mécanismes comme TLS,
RELRO ou certaines initialisations relèvent en partie de l'interpréteur et du
démarrage utilisé. Les laisser à ces composants ne démontre pas que toutes
leurs combinaisons sont compatibles avec le chargement Woody.

**Le nettoyage n'est pas transactionnel.** Sur un échec intermédiaire, certains
mappings de segments peuvent rester présents jusqu'à `sys_exit`. Le noyau les
récupère à la fin du processus : c'est un compromis simple pour un chargeur
qui termine immédiatement en cas d'erreur. Le même code réutilisé comme
bibliothèque devrait suivre et libérer précisément ses allocations pour
permettre de continuer après un échec.

Le bon niveau d'affirmation est donc : le projet implémente un chemin de
chargement étudié et explicable pour sa cible, avec les compromis décrits ici.
Il reste de la place pour des oublis, des erreurs et des incompatibilités
inconnues. Les reconnaître fait partie du travail d'apprentissage.

## 16. Observer le fonctionnement soi-même

Ces manipulations relient les concepts aux octets réellement produits. Elles
servent à explorer un exemple ; elles ne démontrent pas une compatibilité
générale. Elles supposent les outils GNU habituels, Clang et, pour le dernier
exemple, `strace`.

### 16.1. Construire et regarder le stub

Depuis la racine du dépôt :

```sh
make
readelf -SW stub_bin/stub.elf
readelf -sW stub_bin/stub.elf
readelf -rW stub_bin/stub.elf
objdump -d -j .stub stub_bin/stub.elf
readelf -SW packer
```

Questions à se poser : où commence `stub_entry` dans `.stub` ? Les fonctions
et constantes nécessaires sont-elles dans cette section ? Y a-t-il des
symboles non définis ou des relocations résiduelles ? La section `.stub` du
packer doit-elle avoir une adresse mémoire utile pour qu'il puisse la copier ?

L'ELF de construction `stub.elf` sert à fabriquer un bloc d'octets ; ce n'est
pas celui que le noyau est censé lancer directement. Inspecter ses headers
reste utile, mais son rôle diffère de celui de `woody`.

### 16.2. Créer un exemple isolé

Les commandes suivantes gardent l'exemple et la sortie dans un répertoire
temporaire, sans remplacer un éventuel `woody` dans le dépôt :

```sh
woody_project_dir="$PWD"
woody_demo_dir=$(mktemp -d /tmp/woody-guide.XXXXXX)
cd "$woody_demo_dir"
cat > hello.c <<'EOF'
#include <stdio.h>

static unsigned char buffer[8193];

int main(int argc, char **argv)
{
    printf("argc=%d, argv[0]=%s, BSS=%u\n", argc, argv[0],
        (unsigned int)buffer[8192]);
    buffer[4096] = 42;
    return 7;
}
EOF
clang -O0 -fPIE -pie hello.c -o hello
"$woody_project_dir/packer" ./hello
./woody bonjour
echo "$?"
```

Le stub affiche sa bannière avant le message du programme. `argv[0]` est
`./woody`, la BSS observée vaut zéro et le code de retour attendu vaut 7.
La taille et la disposition exactes dépendent de la chaîne de compilation.

Pour explorer `ET_EXEC`, recompiler avec
`clang -O0 -no-pie hello.c -o hello-exec`, empaqueter ce nouveau fichier puis
inspecter à nouveau les adresses. Le fichier de sortie s'appelle toujours
`woody` dans le répertoire courant.

### 16.3. Comparer fichier et mémoire annoncée

Dans le même répertoire temporaire :

```sh
readelf -hW hello
readelf -lW hello
readelf -hW woody
readelf -lW woody
readelf -SW woody
wc -c hello woody
strace -e trace=mmap,mprotect,munmap,open,close,write ./woody bonjour
```

À observer :

- Les quatre program headers du wrapper, dont trois `PT_LOAD`.
- L'absence de table des sections et de `PT_INTERP` dans `woody`.
- Les couples offset/adresse des segments et leur reste modulo `0x1000`.
- Les tailles fichier et mémoire de l'original, notamment pour la BSS.
- Les réservations `PROT_NONE`, leur suppression, puis les mappings RW et les
  protections finales.
- L'ouverture de l'interpréteur par le stub, puis les opérations effectuées par
  cet interpréteur après le saut.
- La taille de sortie supérieure à l'original en l'absence de compression.

`strace` affiche aussi des appels faits par le chargeur dynamique et le
programme original ; toutes les lignes ne proviennent donc pas du stub.

### 16.4. Afficher les calculs du packer

De retour dans le dépôt, la macro utilisée dans le code est `DEBUG` :

```sh
cd "$woody_project_dir"
make -B PACKER_CFLAGS='-Wall -Werror -Wextra -g -MMD -DDEBUG'
```

Le prochain empaquetage affiche les en-têtes calculés. On peut alors comparer
les valeurs prévues par `init_output_headers()` et celles lues dans la sortie
avec `readelf -lW`.

La reconstruction forcée sert ici à prendre en compte le changement de flags :
Make ne déduit pas automatiquement d'une variable de compilation modifiée que
tous les objets existants doivent être reconstruits.

## 17. Ce que le projet permet de comprendre

Le projet donne un fil conducteur à plusieurs notions souvent apprises
séparément. Les program headers relient un format de fichier aux mappings ;
les pages expliquent les contraintes d'alignement, les chevauchements et les
permissions ; le load bias relie ces adresses à un placement réel.

La séparation entre image fichier et image chargée explique pourquoi copier
les octets ne suffit pas et pourquoi certains pointeurs doivent survivre au
nettoyage. La BSS montre que tout l'état initial d'un programme n'est pas
stocké dans son fichier.

Le stub freestanding rend visibles les services habituellement fournis par le
démarrage C et la libc. L'assembleur n'y remplace pas tout le C : il intervient
aux frontières où les registres et la pile constituent le contrat.

Enfin, le chargement de l'interpréteur et la correction d'auxv montrent que
l'exécution repose sur une coopération entre noyau, format ELF, chargeur
dynamique et code de démarrage. Un programme correctement chargé est un
ensemble cohérent d'octets, d'adresses, de permissions et de métadonnées.

Pour vérifier sa compréhension, on doit pouvoir expliquer :

1. Pourquoi la sortie fonctionne sans table des sections alors que le packer
   utilise une section `.stub` pour la construire.
2. Pourquoi le payload n'est pas déjà prêt à être exécuté quand le noyau l'a
   chargé comme données.
3. Pourquoi le stub et l'original `ET_EXEC` doivent occuper des plages différentes.
4. Pourquoi le biais d'un `ET_DYN` n'est pas toujours l'adresse de sa première page.
5. Pourquoi une fin de page doit parfois être remise à zéro après une copie.
6. Pourquoi appliquer les permissions à chaque segment immédiatement peut gêner
   le chargement d'un autre segment.
7. Pourquoi l'entrée de l'interpréteur diffère de `AT_ENTRY`.
8. Pourquoi `AT_PHDR` doit viser une représentation qui restera accessible.
9. Pourquoi le dernier transfert restaure `rsp` puis utilise `jmp`.
10. Pourquoi un exemple qui s'exécute correctement ne démontre ni la validité
    de tous les ELF acceptés ni une équivalence complète avec `execve`.

Ce sont ces mécanismes et la capacité à discuter leurs compromis qui
constituent l'apprentissage du projet. Les zones encore imparfaites indiquent
les prochaines questions à étudier, dans la limite du temps et du périmètre
que l'on choisit d'y consacrer.
