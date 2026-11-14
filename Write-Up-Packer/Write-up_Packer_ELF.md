# Write-up : Packer ELF

Ceci est un projet d'étude visant a explorer la structure des executables et la manière dont Linux les execute.

L'idée est de créer un simple packer inspiré de [UPX](https://github.com/upx/upx) pour mettre en pratique les notions que j'ai pu apprendre grace aux cours de la plateforme OpenSecurityTraining (cours Life of Binaires).

## Qu'est ce qu'un packer ?

Un packer est un outil qui transforme un fichier exécutable en chiffrant et/ou compressant son contenu, afin de réduire sa taille et/ou de compliquer son analyse, tout en préservant son comportement originel grâce à un mécanisme de dépaquetage.

## Strategie de packing

Beaucoup de stratégies existent, certaines très simples et d'autres plus complexes. Dans le cadre de ce projet, afin de réellement toucher a toutes les notions importantes, nous nous intereserons a la methodologie employee par le packer open source [UPX](https://github.com/upx/upx).

Cette strategie consiste sommairement en les etapes suivante :

- Le packer ouvre le fichier executable qu'il doit packer et le chiffre/compresse.
- Il cree un nouveau fichier et insere cette data chiffrée/compressée à l'intérieur.
- Il insere également du code executable (qu'on appellera stub) qui a pour but de trouver décompresser/déchiffrer et d'executer 

Nous avons donc en sortie un nouvel executable avec comme seul code analysable le stub de déchiffrement et chargement. Le code originel est completement opaque. Lors de l'execution le stub sera executé en premier, et, une fois déchifré, l'executable originel sera executé de maniere identique a son état originel (tout ceci uniquement dans la mémoire, sans créer de fichier temporaire, donc pas d'execve classique)

# Sommaire

Liste de choses.

# Rappels Théoriques

## Mémoire virtuelle

Avant de commencer, il nous faut expliquer rapidement comment fonctionne l'adressage virtuel sous Linux.

La RAM physique de notre ordinateur est principalement gérée par le kernel. Celui-ci s'occupe notamment d'attribuer de la mémoire aux différents processus et d'optimiser son utilisation en arrière-plan (en regroupant les données pour défragmenter par exemple) sans que nous nous en rendions compte.

Mais cela peut paraître inquiétant lorsque nous développons un programme qui utilise cette RAM. Un développeur ne veut surtout pas que ses pointeurs se retrouvent invalidés simplement parce que le kernel a eu besoin de déplacer ses données ailleurs en mémoire physique, n'est-ce pas ?

Pour pallier ce problème (entre autres), les systèmes modernes utilisent un mécanisme appelé mémoire virtuelle.

### La mémoire virtuelle

En vulgarisant on peut dire qu'une adresses virtuelle est simplement un "nombre/identifiant" servant a retrouver la bonne adresse physique. Sur les architectures `x86-64`, elles sont représentées sur 64 bits et sont comprises généralement dans une plage allant de `0x0000000000010000` à `0x00007fffffffefff` (ces limites dépendent de la configuration du système).

Lorsque Linux exécute un processus, il lui fournit un ensemble de structures appelées "page tables" (nous ne rentrerons pas dans les détails techniques ici). Ces structures ont pour but de faire le lien entre les adresses virtuelles et les adresses physiques au sein d'un processus. A l'intérieur de ces tables, chaque adresse virtuelle se voit en général attribuée un adresse physique dans la RAM. Si le kernel a besoin de réorganiser la RAM, il n'aura qu'a mettre a jour l'adresse physique associée à l'adresse virtuelle. Ainsi, lorsqu'on "déréférence" une adresse virtuelle, le processeur va chercher les données présentes à l'adresse physique associée à cette adresse virtuelle (ou déclancher un `PAGE FAULT` dans le cas ou aucne adresse physique n'est spécifiée).

Lorsqu'on parle "d'adresses" en programmation, on parle pratiquement toujours d'adresses virtuelles.

Chaque processus possède ses propres page tables indépendantes, qui représentent son "espace d'adressage virtuel". Cela signifie que deux processus peuvent parfaitement utiliser la même adresse virtuelle, par exemple `0x400000`, sans pour autant accéder aux mêmes données.

> **NOTE :** Les threads d'un même processus partagent tous le même espace d'adressage virtuel.

### Les pages mémoire

Pour comprendre totalement ce mécanisme, il faut introduire le concept de page mémoire. Nous avons dit que `"une adresse virtuelle" -> "une adresse physique"`. En réalité, il s'agit plutôt de `"une page virtuelle" -> "une page physique"`.

Une page est simplemnt un "bloc de mémoire de taille fixe". Sur `x86-64`, ce bloc est en général de `0x1000` bytes (4096). Le kernel travaille avec des pages de RAM (page frames) plutot qu'avec des adresses isolées. Donc lorsque le kernel doit allouer de la mémoire pour un processus, il lui attribue un nombre depages en fonction du besoin, et non pas exactement le nombre de bytes réellement nécessaires.

**Exemple :** Besoin de `0x2431` --> on arrondit à 3 pages de `0x1000`.

> **Attention :** Ici, nous parlons de l'allocation de mémoire nécessaire à l'établissement du contexte d'exécution d'un processus, et non de l'allocation de mémoire de travail par le programme pendant son exécution (via `malloc()`, par exemple). Dans ce second cas, la mémoire demandée peut déjà être disponible dans une région précédemment réservée (le kernel n'alloue pas de nouvelles pages a chaque `malloc()`, la heap agit comme une sorte réserve de mémoire). Si ce n'est pas le cas, de nouvelles pages virtuelles peuvent être demandées au kernel.

Donc pour préciser ce qui à été dit plus haut : les pages tables permettent de relier les pages virtuelles aux pages physiques. Pour simplifier, nous pouvons imaginer ces tables représentent un grand dictionnaire contenant des paires « key/value » :

- **Key :** l'adresse de début d'une page virtuelle.
- **Value :** l'adresse physique du page frame correspondant.

Prenons un exemple avec deux processus différents :

**Processus A :**

```text
0x400000 → 0x12A000
0x401000 → 0x31000
```

**Processus B :**

```text
0x400000 → 0x58000
0x401000 → 0x92000
```

Nous remarquons que les deux processus utilisent exactement les mêmes adresses virtuelles, mais que celles-ci correspondent à des emplacements physiques différents. Imaginons maintenant que le processus A tente de lire les données situées à l'adresse virtuelle `0x400123`. Le processeur identifie la page virtuelle contenant cette adresse (`0x400000`), retrouve son page frame correspondant (`0x12A000`), puis y ajoute le décalage de `0x123` octets. Notre lecture s'effectuera donc à l'adresse physique `0x12A123`. Cette traduction est effectuée automatiquement par un composant matériel du processeur appelé MMU (Memory Management Unit).

Cette implémentation permet également d'autres avantages. Par exemple, deux processus peuvent posséder chaqu'un une page virtuelle qui correspond a la meme page physique. Cela permet notamment de partager le code des bibliothèques partagées entre plusieurs processus, sans avoir besoin de le dupliquer en RAM. Ceci autorise également deux pages virtuelles consécutives à correspondre à deux page frames physiques très éloignés. Un programme peut donc manipuler une plage d'adresses virtuelles continue, même si les données correspondantes sont dispersées dans la RAM physique. Exemple :

**Processus :**

```text
0x400000 → 0x12A000
0x401000 → 0x31000
```

Si notre programme utilise un ptr = `0x400900`, il peu très bien l'incrémenter jusqu'à dépasser `0x401000`, même si le page frame physique de ces deux pages n'est pas contigu. Lorsque le processeur déréférencera ce pointeur, il trouvera automatiquement la bonne adresse physique.

> **NOTE :** Les page tables ne sont pas de simples dictionnaires « key/value ». Ce sont des structures hiérarchiques beaucoup plus complexes, contenant également des informations sur les permissions d'accès aux pages (plus d'info ici [documentation du noyau Linux](https://docs.kernel.org/mm/page_tables.html)). De plus, une page virtuelle ne possède pas nécessairement un page frame associé à tout instant : par exemple, pour optimiser, certaines pages peuvent ne pas encore avoir été chargées en RAM et ne le seront que lorsqu'on en a explicitement besoin. Enfin, les exemples précédents utilisent des pages de 0x1000, mais x86-64 prend également en charge des pages plus grandes. Cependant, pour comprendre les opérations de mapping effectuées dans le cadre de ce projet, le modèle simplifié présenté ici est suffisant.

## Format ELF (Executable and Linking Format)

Ici nous traiterons uniquement le cas ELF `x86-64`.

Un fichier ELF n'est pas directement du code machine destiné au processur. C'est avant tout un format de fichier qui permet de stocker du code machine, des données, ainsi que des informations décrivant comment ces éléments doivent être interprétés.

Un ELF peut représenter plusieurs choses. Les plus communes sont :

- **Executable :** Un programme destiné à être exécuté par le système.
- **Shared object (.so) :** Une bibliothèque dont le code peut être chargé et utilisé par d'autres programmes. Souvent les .so ne sont pas executables independamment.
- **Relocatable object (.o) :** Un fichier contenant du code compilé, mais qui doit être linké pour être intégré à un programme.

### Sections et Segments

Lors de la compilation d'un programme, le compilateur et le linker organisent son contenu en différentes parties appelées sections. Chaque section contient de la data spécifique.

Quelques exemples classiques de sections générées automatiquement par presque tous les compilateurs/linkers :

- "`.text`" contient généralement les instructions machine du programme.
- "`.rodata`" contient généralement des données en lecture seule.
- "`.data`" contient généralement des données initialisées qui peuvent être modifiées pendant l'exécution.

> **NOTE :** Il n'y a aucune règle de nommage officielle pour les sections.

Cette organisation par sections est très utile lors du linking de différents fichiers objets en un executable final. Le linker peut par exmple retrouver l'ensemble du code machine des tous les objets et l'unir ensemble dans le `.text` final de l'executable, ou encore exclure de l'ELF final des sections entieres qui ne servaient que pour la construction. De la même maniere, cette organisation peremet a différents outils Linux de parser rapidement et simplement les differents contenus d'un ELF (readelf, objdump...).

Cependant, les sections ne servent que pour la construction et l'analyse d'un ELF. Le kernel n'a aucunement besoin de connaitre cette subdivision en sections. Tout ce qui lui importe c'est : quelles parties du fichier faut il charger en mémoire, et comment interpreter ces bouts de fichier chargés.

Une autre manière d'organiser la data est donc nécessaire : les segments.

Un segment est essentiellement une zone contigue de données. Ce terme est utilisé pour designer une zone du fichier ELF, le plus souvent en englobant une ou plusieurs sections. Lors du chargement, certains de ces segments sont traités par le système pour charger en mémoire les données nécessaires à l'execution. On utilise le terme segment pour designer aussi bien une zone de données d'un ELF sur disque, que la representation de cette zone en mémoire, lorsqu'elle est chargée.

### Organisation de la structure ELF

Nous pouvons distinguer 4 éléments principaux :

- **HEADER**

  ```
  typedef struct
  {
    unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
    Elf64_Half	e_type;			    /* Object file type */
    Elf64_Half	e_machine;		    /* Architecture */
    Elf64_Word	e_version;		    /* Object file version */
    Elf64_Addr	e_entry;		    /* Entry point virtual address */
    Elf64_Off	    e_phoff;		    /* Program header table file offset */
    Elf64_Off	    e_shoff;		    /* Section header table file offset */
    Elf64_Word	e_flags;		    /* Processor-specific flags */
    Elf64_Half	e_ehsize;		    /* ELF header size in bytes */
    Elf64_Half	e_phentsize;		/* Program header table entry size */
    Elf64_Half	e_phnum;		    /* Program header table entry count */
    Elf64_Half	e_shentsize;		/* Section header table entry size */
    Elf64_Half	e_shnum;		    /* Section header table entry count */
    Elf64_Half	e_shstrndx;		    /* Section header string table index */
  } Elf64_Ehdr;
  ```

  Il s'agit d'une structure de taille fixe située au tout début du fichier. Il contient les caractéristiques générales de l'ELF, comme son architecture, son type, l'adresse de la première instruction qui sera exécutée ou encore la position de la Section Header Table et de la Program Header Table. C'est en quelque sorte le point de départ permettant d'interpréter le reste du fichier.

- **PROGRAM HEADER TABLE**

  ```
  typedef struct
  {
    Elf64_Word	p_type;			/* Segment type */
    Elf64_Word	p_flags;		/* Segment flags */
    Elf64_Off	    p_offset;		/* Segment file offset */
    Elf64_Addr	p_vaddr;		/* Segment virtual address */
    Elf64_Addr	p_paddr;		/* Segment physical address */
    Elf64_Xword	p_filesz;		/* Segment size in file */
    Elf64_Xword	p_memsz;		/* Segment size in memory */
    Elf64_Xword	p_align;		/* Segment alignment */
  } Elf64_Phdr;
  ```

  Il s'agit d'un tableau de structures appellées Program Header. Un Program Header contient des informations destinées a être lues par le chargeur du système : l'adresse de chargement de certains segments, leurs permissions, leur zone de chargement... Chaque Program Header peut désigner un segment (donc une zone contigue de l'ELF), un segment chargeable (donc un segment qui doit être chargé en mémoire), ou de la métadonée de chargement qui ne représente aucune zone de l'ELF, simplement des informations sur comment gérer le chargement et l'execution. Bien que ce ne soit pas une règle officielle, la Program Header Table se situe souvent avant la Data, juste après le Header. Il existe de nombreux types (`p_type`) de Program Headers, mais dans le cadre de ce write-up nous nous interesserons seulement aux suivants :

  - **`PT_LOAD` :** Le plus important. Cet header decrit un segment de l'ELF qui sera chargé en mémoire. A noter qu'il est très fréquent que ce type de segment couvre plusieurs autres segments plus petis. Le premier segment `PT_LOAD` d'un ELF comprend souvent au moins le segment `PT_PHDR` et `PT_INTERP`.
  - **`PT_PHDR` :** Decrit un segment qui contient l'entièreté des entrées de la Program Header Table.
  - **`PT_INTERP` :** Decrit un segment qui contient le nom absolu d'un exectuable ELF qui jouera le role d'interpreteur (cette notion sera expliquée plus tard).
  - **`PT_DYNAMIC` :** Decrit un segment qui contient la section `.dynamic`. Cette section fournit des informations nécessaires à la liaison dynamique. Elle permet notamment de savoir quelles bibliothèques partagées (.so) doivent être chargées et contient les informations pour retrouver les adresse mémoire des fonctions qu'elles exportent. Nous nous interesserons un peu plus en détails a cette fonction plus tard.
  - **`PT_GNU_STACK` :** Ce Program Header ne decrit aucun segment, il renseigne seulement un flag qui indique si la stack qu'un ELF va avoir lors de son execution doit être executable ou non (si ce n'est pas clair, ça le sera plus tard).

- **DATA**

  C'est le contenu du fichier : instructions machine, constantes, variables initialisées, informations de symboles, etc. La majorité est rangée par sections, qui peuvent a leur tour être regroupées par segments.

- **SECTION HEADER TABLE**

  ```
  typedef struct
  {
    Elf64_Word	sh_name;		/* Section name (string tbl index) */
    Elf64_Word	sh_type;		/* Section type */
    Elf64_Xword	sh_flags;		/* Section flags */
    Elf64_Addr	sh_addr;		/* Section virtual addr at execution */
    Elf64_Off	    sh_offset;		/* Section file offset */
    Elf64_Xword	sh_size;		/* Section size in bytes */
    Elf64_Word	sh_link;		/* Link to another section */
    Elf64_Word	sh_info;		/* Additional section information */
    Elf64_Xword	sh_addralign;	/* Section alignment */
    Elf64_Xword	sh_entsize;		/* Entry size if section holds table */
  } Elf64_Shdr;
  ```

  Il s'agit d'un tableau de structures appellées Section Header. Un Section Header décrit une section: sa position, sa taille, son type et différentes propriétés. Cette table n'est pas utilisée par le chargeur du système, et peut donc être absente sans poser de problèmes d'execution. La premiere entrée de cette table est toujours nulle. Le plus souvent, la Section Header Table est placée après la Data, à la fin du fichier, mais ce n'est pas une obligation.

![](images/4a6e2f92e2c294d8f02dc65b170f7b13a1a282be.jpg)

> **NOTE :** On remarque que le Header et la Program Header Table font partie d'un segment sans pour autant faire partie d'une section.

Ce qu'il faut retenir ici est que les Sections Headers et Program Headers se partagent globalement la meme donnée. Ce sont simplement des structures qui subdivisent l'ELF de maniere differente. Cela permet deux lectures differentes, une qui sert a executer l'ELF et l'autre qui sert a le construire.

Nous allons maintenant entrer un peu plus dans les details d'un Prgoram Header.

### Program Header

Lorsqu'on utilise `readelf -lW` pour afficher la Program Header Table, on obtient quelque chose comme ça :

```
readelf -lW ./elf

Elf file type is EXEC (Executable file)
Entry point 0x401130
There are 13 program headers, starting at offset 64

Program Headers:
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  PHDR           0x000040 0x0000000000400040 0x0000000000400040 0x0002d8 0x0002d8 R   0x8
  INTERP         0x000318 0x0000000000400318 0x0000000000400318 0x00001c 0x00001c R   0x1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x000000 0x0000000000400000 0x0000000000400000 0x000910 0x000910 R   0x1000
  LOAD           0x001000 0x0000000000401000 0x0000000000401000 0x0034f9 0x0034f9 R E 0x1000
  LOAD           0x005000 0x0000000000405000 0x0000000000405000 0x000e5c 0x000e5c R   0x1000
  LOAD           0x006df8 0x0000000000406df8 0x0000000000406df8 0x000298 0x0002b8 RW  0x1000
  DYNAMIC        0x006e08 0x0000000000406e08 0x0000000000406e08 0x0001d0 0x0001d0 RW  0x8
  NOTE           0x000338 0x0000000000400338 0x0000000000400338 0x000020 0x000020 R   0x8
  NOTE           0x000358 0x0000000000400358 0x0000000000400358 0x000044 0x000044 R   0x4
  GNU_PROPERTY   0x000338 0x0000000000400338 0x0000000000400338 0x000020 0x000020 R   0x8
  GNU_EH_FRAME   0x0056e8 0x00000000004056e8 0x00000000004056e8 0x000184 0x000184 R   0x4
  GNU_STACK      0x000000 0x0000000000000000 0x0000000000000000 0x000000 0x000000 RW  0x10
  GNU_RELRO      0x006df8 0x0000000000406df8 0x0000000000406df8 0x000208 0x000208 R   0x1

 Section to Segment mapping:
  Segment Sections...
   00     
   01     .interp 
   02     .interp .note.gnu.property .note.gnu.build-id .note.ABI-tag .gnu.hash .dynsym .dynstr .gnu.version .gnu.version_r .rela.dyn .rela.plt 
   03     .init .plt .text .fini 
   04     .rodata .eh_frame_hdr .eh_frame 
   05     .init_array .fini_array .dynamic .got .got.plt .data .bss 
   06     .dynamic 
   07     .note.gnu.property 
   08     .note.gnu.build-id .note.ABI-tag 
   09     .note.gnu.property 
   10     .eh_frame_hdr 
   11     
   12     .init_array .fini_array .dynamic .got 
```

Exliquons brièvement chaqu'un des champs:

- **`p_type` :** Indique le type de du Program Header, comme `PT_LOAD`, `PT_PHDR`, `PT_INTERP`... Les differents types sont consultables ici [linux/elf.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/elf.h). D'autres types existent pour d'autres architecture, mais dans le cadre de ce projet nous utilierons ce header comme référence.

- **`p_offset` :** Indique à quel offset du fichier se trouve le debut de la zone de données couverte par le segment.

- **`p_vaddr` :** Indique l'adresse mémoire (virtuelle) à laquelle la data couverte par ce segment se trouve en mémoire. Pour les exécutables de type PIE, cette adresse est relative à une adresse de base déterminée lors du chargement (pas d'inquiétudes, on va clarifier après).

- **`p_paddr` :** Indique l'adresse physique du segment sur les systèmes qui utilisent cette information. Sous Linux, pour les exécutables classiques en espace utilisateur, ce champ est ignoré. Il est souvent initialisé à la même valeur que `p_vaddr`.

- **`p_filesz` :** Indique la taille de la zone de données couverte par le segment à l'intérieur du fichier.

- **`p_memsz` :** Indique la taille de la zone de données couverte par le segment dans la mémoire. 

  > **NOTE :** Lorsque `p_memsz` est supérieur à `p_filesz` pour un segment `PT_LOAD`, l'espace supplémentaire en mémoire est initialisé à zéro. C'est notamment le cas des données non initialisées (`.bss`). Cette technique évite de stocker inutilement dans le fichier des octets dont on sait qu'ils devront être initialisés à zéro. Memsz ne peut pas être inférieur a filesz.

- **`p_flags` :** Pour les segments `PT_LOAD`, indique les permissions mémoire demandées lors du chargement (read/write/execute). Ces permissions s'appliquent aux pages mémoire couvrant le segment et peuvent ensuite être modifiées, notamment avec mprotect(). Durant le runtime, une operation a une adresse a l'intérieur d'une page qui ne possède pas les permissions correspondantes se verra refusée par le kernel (qui enverra un segfault au processus). Pour d'autres types de Program Headers, ces flags peuvent avoir une signification différente (comme `PT_GNU_STACK`), ou être simplmement ignorés (comme `PT_PHDR`, `PT_INTERP`, `PT_DYNAMIC`...).

- **`p_align` :** Indique la contrainte d'alignement du segment dans le fichier et en mémoire. Nous reviendrons sur son fonctionnement lors des détails sur l'implémentation du loader.

Quelques exemples pour s'assurer que tout soit clair :

- `ProgramHeader[0]` est de type `PHDR`. On sait donc qu'il decrit un segment qui va contenir toutes les entrées de la Program Header Table. Le segment commence au `0x40ème` bytes du fichier, et occupe `0x2d8` bytes. Dans l'image finale chargée en mémoire, ce segment sera à l'adresse virtuelle `0x400040`, et aura la même taille que dans le fichier (`0x2d8`). Ici, bien que le flag `R` soit set, celui-ci n'a aucun impact sur les permissions mémoire, car ce Program Header ne provoque pas lui-même de mapping. Ses données sont néanmoins présentes en mémoire puisqu'elles sont englobées dans un segment `PT_LOAD`.

- `ProgramHeader[2]` est de type `PT_LOAD`. On sait que ce segment va être chargé en mémoire. Il comprend l'intervalle `[0x00-0x910)` du fichier ELF. On voit donc qu'il englobe notamment le Header de l'ELF et les segments `PHDR` et `INTERP`. En mémoire il se trouvera à l'addrese `0x400000`, et aura la même taille que dans le fichier. Ici le flag `R` est appliqué lors du mapping comme permission de base du segment (ces permissions peuvent ensuite être modifiées avec `mprotect`).

- `ProgramHeader[5]` a la particularité d'avoir un memsz supérieur a son filesz, on sait donc que ce segment en mémoire sera plus grand que le segment dans le fichier, et que l'espace supplémentaire sera initialisé a `0` (bss).

- `ProgramHeader[11]` est de type `GNU_STACK`. Ce type de Program Header ne decrit pas un segment, mais sert a indiquer au kernel quelles la mémoire représentant la Stack du processus (qui est egalement un espace memoire attribué par le kernel au demarrage d'un processus, nous y reviendrons) doit être executable ou non. Ici rien n'est donc chargé, le champ `p_flags` est utilisé pour renseigner cette information via le champ `p_flags` les autres champs sont ignorés.

Nous avons desormais toutes les briques necessaires pour comprendre comment le système charge et execute un ELF.

## Chargement d'un ELF

Lorsqu'un processus appelle `execve()` pour exécuter un fichier ELF, le kernel doit effectuer plusieurs opérations d'initialisation. Nous détaillerons les principales étapes dans cette section, mais l'ordre dans lequel le kernel les effectue n'est pas forcément identique à l'ordre dans lequel nous les aborderons. 

### Mapping des Segments

Pour charger les segments `PT_LOAD`, le kernel doit créer des mappings mémoire aux adresses virtuelles appropriées, en respectant les contraintes imposées par la taille des pages.

Pour réaliser ces opérations, Linux utilise les mécanismes de mapping mémoire, également accessibles aux programmes utilisateurs grâce à l'appel système `mmap()`.

`mmap()` permet notamment de créer deux types de mappings : les mappings anonymes et les mappings de fichier.

- **Mapping anonyme :**  Permet de réserver une région de mémoire virtuelle qui n'est associée à aucun fichier. Son contenu initial est constitué de zéros. La taille de la région effectivement mappée est arrondie à un multiple de la taille des pages.

- **Mapping par fichier :** Permet d'associer une région de mémoire virtuelle à une portion de fichier, définie par un offset de départ et une longueur.

  Cependant, `mmap()` peut mapper uniquement un nombre entier de pages mémoire. Pour garder une  correspondance correcte entre `[page mémoire] <--> [portion de fichier]`, il impose également que l'offset du fichier soit aligné sur une frontière de page.

  Pour charger un segment `PT_LOAD`, le kernel doit donc aligner l'offset de début de sa portion de fichier sur la frontière de page précédente, puis étendre le mapping jusqu'à la frontière de page suivant sa fin.

  > **NOTE :** `mmap()` accepte une longueur de mapping qui n'est pas un multiple de `page_size`, mais le kernel arrondira implicitement cette longueur au prochain multiple.

Prenons maintenant un exemple concret pour comprendre comment le kernel charge ces segments en mémoire. Nous considérerons une taille de page de `0x1000` bytes.

```text
Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
LOAD           0x000000 0x0000000000400000 0x0000000000400000 0x001324 0x001324 R E 0x1000
LOAD           0x001422 0x0000000000402422 0x0000000000402422 0x000e10 0x000e10 R   0x1000
LOAD           0x002341 0x0000000000500341 0x0000000000500341 0x001a4f 0x0039bf RW  0x1000
LOAD           0x003d90 0x0000000000999d90 0x0000000000999d90 0x0003c8 0x0003c8 RW  0x1000
```

Observons le deuxième `PT_LOAD`. Son contenu commence à l'offset `0x1422` du fichier et doit être chargé à l'adresse virtuelle `0x402422`. Cependant, comme les mappings s'effectuent par pages entières, le kernel doit aligner le début du mapping sur `0x1000` dans le fichier et `0x402000` en mémoire.

Avec un `p_filesz` de `0xe10`, les données de ce segment se terminent à l'adresse `0x403232`. Le mapping doit donc couvrir deux pages entières, soit l'intervalle `[0x402000, 0x404000)`.

**Cela signifie que des octets du fichier situés avant et après le segment seront également présents en mémoire**, simplement parce qu'ils appartiennent aux pages nécessaires au mapping.

Il existe également deux cas particuliers :

- **Lorsque** `p_memsz > p_filesz` **:** le kernel initialise à zéro l'espace supplémentaire appartenant au segment. Il commence par remplacer par des zéros les éventuels octets supplémentaires du fichier présents après le segment, dans la dernière page mappée. Si nécessaire, il complète ensuite cette région avec des pages anonymes initialisées à zéro (segment 3).

- **Lorsque le mapping atteint EOF :** les octets situés après la fin du fichier, dans la dernière page du mapping, sont automatiquement remplis de zéros (segment 4).

Enfin, les permissions définies par `p_flags` s'appliquent aux pages mappées, y compris aux octets du fichier chargés en supplément.

Le schéma suivant illustre ces différents cas, en distinguant les données appartenant aux segments, les octets supplémentaires du fichier chargés à cause de l'alignement des pages et les régions de mémoire initialisées à zéro.

![](images/fc461c7aab9b2ba4ff9bc930e3621c86a5958d7c.jpg)

Dans cet exemple, le mapping requière 10 pages mémoire de `0x1000` bytes.

Avant de de continuer avec les étapes suivantes de l'execution, il convient d'introduire les deux types d'ELF que nous traiterons dans ce write-up.

### PIE et PDE

**Position Dependent Executable (PDE ou Non-PIE) :** Depuis le debut, nos exemples se réfèrent à ce type d'executable. Un PDE est conçu pour être chargé à des adresses virtuelles précises. Chaque `PT_LOAD` spécifie l'adresse à laquelle il doit être chargé, et si l'adresse du premier `PT_LOAD` n'est pas disponible (déja occupée) l'execution échoue. Les ELF PDE sont de type `ET_EXEC`,  indiqué dans le champ `e_type` de l'ELF Header.

**Position Independent Executable (PIE) :** Ce type d'executable n'a aucune dépendance d'adresses spécifiques. Ce qui signifie qu'il peut être chargé et executé à n'importe quelles adresses virtuelles valides.

Contrairement à un PDE classique, les adresses virtuelles renseignées dans un PIE, notamment les `e_entry` et les `p_vaddr` des segments, ne correspondent pas directement à leurs adresses réelles en mémoire, mais a une adresses relatives à une base de chargement.

Lors du chargement d'un exécutable `ET_DYN`, le kernel doit déterminer un *load bias* compatible avec les contraintes de l'ELF. Ce *load bias* est simplement un décalage qui sera ajouté à chaque `p_vaddr` pour obtenir l'adresse réelle de chargement du segment.

Si l'ASLR est activé, ce *load bias* est obtenu aléatoirement (toujours en respectant les contraintes de l'ELF), ce qui rend les adresses réelles de chargement différentes entre deux executions.

Les ELF PIE sont de type `ET_DYN` au même titre que les bibliothèques partagées. Beaucoup de compilateurs modernes choisissent ce type d'ELF par défault lors des compilations.

**Exemple :**

```
PDE: p_vaddr = 0x400000
PIE: p_vaddr = 0x1120  --> au chargement : adresse réelle = 0x1120 + 0x760000 = 0x761120
     p_vaddr = 0x1120  --> au chargement : adresse réelle = 0x1120 + 0x230000 = 0x231120
```

### Environnement d'execution

#### Stack

En plus des segments `PT_LOAD`, le kernel doit préparer une région mémoire destinée à la stack du programme.

Pour cela, il choisit une plage d'adresses dans l'espace virtuel du processus, généralement située vers les adresses hautes.

Bien que la stack partage le même espace d'adressage virtuel que les mappings `mmap()` classiques, le kernel utilise une stratégie différente pour déterminer son emplacement.

Les mappings `mmap()` classiques sont généralement placés dans une région située plus bas dans l'espace virtuel, afin de conserver suffisamment de place pour permettre à la stack de s'étendre.

Si l'ASLR est activé, l'emplacement de la stack  est randomisé.

Contrairement aux segments `PT_LOAD`, cette région ne correspond à aucune portion du fichier ELF. Il s'agit d'une région de mémoire anonyme, dont les pages physiques sont allouées selon les besoins.

Le kernel alloue ainsi les premières pages nécessaires pour y placer les informations de démarrage du programme. La stack pourra ensuite s'étendre vers les adresses inférieures, dans les limites autorisées, à mesure que le programme utilisera davantage de mémoire.

Voici un exemple typique de l'initialisation des données de la stack par le kernel :

```

------------------------------------------------------------- 0x7fff6c845000
     0x7fff6c844ff8: 0x0000000000000000
            _  4fec: './stackdump\0'                      <------+
      env  /   4fe2: 'ENVVAR2=2\0'                               |    <----+
           \_  4fd8: 'ENVVAR1=1\0'                               |   <---+ |
           /   4fd4: 'two\0'                                     |       | |     <----+
     args |    4fd0: 'one\0'                                     |       | |    <---+ |
           \_  4fcb: 'zero\0'                                    |       | |   <--+ | |
               3020: random gap padded to 16B boundary           |       | |      | | |
    - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|       | |      | | |
               3019: 'x86_64\0'                        <-+       |       | |      | | |
     auxv      3009: random data: ed99b6...2adcc7        | <-+   |       | |      | | |
     data      3000: zero padding to align stack         |   |   |       | |      | | |
    . . . . . . . . . . . . . . . . . . . . . . . . . . .|. .|. .|       | |      | | |
               2ff0: AT_NULL(0)=0                        |   |   |       | |      | | |
               2fe0: AT_PLATFORM(15)=0x7fff6c843019    --+   |   |       | |      | | |
               2fd0: AT_EXECFN(31)=0x7fff6c844fec      ------|---+       | |      | | |
               2fc0: AT_RANDOM(25)=0x7fff6c843009      ------+           | |      | | |
      ELF      2fb0: AT_SECURE(23)=0                                     | |      | | |
    auxiliary  2fa0: AT_EGID(14)=1000                                    | |      | | |
     vector:   2f90: AT_GID(13)=1000                                     | |      | | |
    (id,val)   2f80: AT_EUID(12)=1000                                    | |      | | |
      pairs    2f70: AT_UID(11)=1000                                     | |      | | |
               2f60: AT_ENTRY(9)=0x4010c0                                | |      | | |
               2f50: AT_FLAGS(8)=0                                       | |      | | |
               2f40: AT_BASE(7)=0x7ff6c1122000                           | |      | | |
               2f30: AT_PHNUM(5)=9                                       | |      | | |
               2f20: AT_PHENT(4)=56                                      | |      | | |
               2f10: AT_PHDR(3)=0x400040                                 | |      | | |
               2f00: AT_CLKTCK(17)=100                                   | |      | | |
               2ef0: AT_PAGESZ(6)=4096                                   | |      | | |
               2ee0: AT_HWCAP(16)=0xbfebfbff                             | |      | | |
               2ed0: AT_SYSINFO_EHDR(33)=0x7fff6c86b000                  | |      | | |
    . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .        | |      | | |
               2ec8: environ[2]=(nil)                                    | |      | | |
               2ec0: environ[1]=0x7fff6c844fe2         ------------------|-+      | | |
               2eb8: environ[0]=0x7fff6c844fd8         ------------------+        | | |
               2eb0: argv[3]=(nil)                                                | | |
               2ea8: argv[2]=0x7fff6c844fd4            ---------------------------|-|-+
               2ea0: argv[1]=0x7fff6c844fd0            ---------------------------|-+
               2e98: argv[0]=0x7fff6c844fcb            ---------------------------+
     0x7fff6c842e90: argc=3
```

Exemple illustratif, le contenu est susceptible d'évoluer avec le temps.

**Source :** [How programs get run: ELF binaries [LWN.net]](https://lwn.net/Articles/631631)

#### Auxiliary Vector

Comme nous avons l'habitude de le voir, en haut de la stack (adresse la plus basse) nous avons `argc`, suivi du tableau des `argvs`, et du tableau des `envps`. 

Nous voyons cependant qu'après`envp` il y a une liste de structures de type `Elf64_auxv_t`, défini ainsi dans l'header [elf/elf.h](https://github.com/bminor/glibc/blob/master/elf/elf.h)  :

```c
typedef struct
{
  uint64_t a_type;		/* Entry type */
  union
    {
      uint64_t a_val;	/* Integer value */
    } a_un;
} Elf64_auxv_t;
```

L'ensemble de ces structures constitue ce qu'on appelle l'*Auxiliary Vector* (`auxv`).

Chaque entrée contient deux champs :

- `a_type` : un identifiant qui indique la nature de l'information.

- `a_un.a_val` : la valeur associée à cet identifiant.

Lors de la préparation de l'environnement d'exécution, le kernel construit cet ensemble d'informations, en conserve une copie dans sa propre mémoire et en place également une copie dans la stack du programme.

L'objectif de l'*Auxiliary Vector* est de permettre au kernel de transmettre au programme certaines informations sur son environnement d'exécution, sans que celui-ci ait besoin d'effectuer des appels système pour les obtenir.

Ces informations concernent aussi bien les caractéristiques du système que les détails du chargement de l'exécutable lui-même. Par exemple, `AT_PAGESZ` indique la taille des pages mémoire du système, tandis que `AT_PHDR` contient l'adresse mémoire de la Program Header Table de l'exécutable.

Nous reviendrons sur certains des champs de l'*Auxiliary Vector* lorsque nous entrerons dans les détails de l'implémentation du packer, mais nous n'approfondirons pas d'avantage le sujet (pour plus d'infos : [getauxval() and the auxiliary vector [LWN.net]](https://lwn.net/Articles/519085/)).

#### Heap

Le kernel détermine également l'adresse de départ de la *heap*, une région mémoire destinée aux allocations dynamiques.

Pour cela, il se base généralement sur la fin du segment `PT_LOAD` situé à l'adresse mémoire la plus haute (BSS inclu).

Cette adresse peut ensuite être décalée aléatoirement si l'ASLR est activé.

Contrairement à la *stack*, la *heap* s'étend vers les adresses supérieures, notamment lorsque le programme demande davantage de mémoire avec l'appel système `brk()`.

Il faut cependant noter que toutes les allocations dynamiques n'utilisent pas nécessairement cette région. Par exemple, `malloc()` peut également utiliser des mappings anonymes `mmap()`.

#### Registres

Pour finir, le kernel prépare l'état initial des registres du processeur pour permettre le démarrage du programme.

Sur une architecture x86-64, il initialise notamment :

- `RIP` (*Instruction Pointer*) : avec l'adresse de la première instruction à exécuter.

- `RSP` (*Stack Pointer*) : avec l'adresse du sommet initial de la stack.

Il est important de comprendre que chaque thread possède son propre état de registres. Lors d'un changement de contexte, le kernel peut sauvegarder cet état afin de le restaurer lorsque le thread reprend son exécution.

Ainsi, plusieurs programmes peuvent utiliser les mêmes registres physiques du CPU sans perdre leurs valeurs respectives.

## L'interpréteur ELF

À présent, notre programme est chargé et son environnement est pret. Nous pourrions simplement renseigner l'addresse de l'`entry_point` dans `RIP` et commencer l'execution.

Cependant, **charger un programme en mémoire ne signifie pas nécessairement qu'il est prêt à être exécuté.**

Prenons un exemple simple : un programme C utilisant la fonction `printf()`.

Cette fonction est généralement fournie par une bibliothèque externe, la `libc`, dont le code n'est pas directement intégré à notre exécutable.

Pour que notre programme puisse utiliser cette fonction, il faut donc que cette bibliothèque soit également chargée en mémoire, et que le programme puisse retrouver l'adresse de la fonction `printf()`.

De manière plus générale, certaines références à des symboles présentes dans un exécutable ne peuvent être résolues définitivement qu'au moment de son chargement. C'est notamment le cas lorsque des bibliothèques sont chargées à des adresses variables.

Il devient alors nécessaire d'effectuer certains ajustements d'adresses, appelés *relocations*, afin que le programme puisse fonctionner correctement.

Plutôt que de confier l'ensemble de ces opérations au kernel, Linux utilise un programme spécialisé : *l'interpréteur ELF*, qui vient effectuer tout un tas d'operations de "préparation" pour que l'ELF puisse s'executer dans les conditions prévues.

> **NOTE :** L'*Interpreter* est aussi souvent appelé *Dynamic Linker*, dû au fait que l'opération qu'il effectue le plus souvent est la résolution des liens dynamiques au runtime.

### Le segment PT_INTERP

Lorsqu'un exécutable nécessite un interpréteur, celui-ci est indiqué dans un segment de type `PT_INTERP`.

Ce segment décrit une zone de l'executable qui contient le chemin absolu vers un autre programme.

Sur un système Linux `x86-64` utilisant la glibc, ce chemin est généralement :

`/lib64/ld-linux-x86-64.so.2`

> **NOTE :** Le format part donc du principe que l'interpreter soit présent dans le système au chemin attendu.

Après avoir chargé les segments de l'ELF principal et préparé son environnement, le kernel charge également les segments l'*interpreter* de la même maniere que nous l'avons vu plus haut, puis initialise `RIP` à l'adresse de l'`entry_point` de ce dernier au lieu de celui du programme principal.

L'interpréteur prend alors le relais.

Il se charge notamment de charger les bibliothèques nécessaires, de résoudre les références aux symboles et d'effectuer les relocations requises.

> **NOTE  :** L'interpreteur utilise notamment `auxv` dans la *stack* pour retrouver les informations concernant l'ELF principal.

Une fois les préparations nécessaires terminées, il transfère finalement le contrôle au point d'entrée de notre programme.

Ainsi, dans le cas d'un exécutable utilisant un interpréteur, **le premier code exécuté en espace utilisateur n'est pas celui de notre programme, mais celui de son interpréteur.**

À l'inverse, lorsqu'aucun interpréteur n'est spécifié, le kernel transfère directement le contrôle au point d'entrée de l'exécutable.

Nous n'entrerons pas dans les détails des mechanismes de *relocation*, *dynamic linking* ainsi que les autres opérations effectuées par l'*interpreter*. Il est cependant important de comprendre leur existence, car ils vont influencer directement les choix d'implémentation de notre packer.

Un dernier point intéressant à noter est que l'*interpreter* ne possède pas lui-même d'*interpreter* (pas de segment `PT_INTERP`).

Il est donc conçu pour pouvoir démarrer de manière autonome, sans dépendre d'un autre *dynamic linker*. Une fois chargé en mémoire par le kernel, il effectue lui-même les opérations nécessaires à sa propre initialisation, notamment ses *relocations*.

# Implémentation























































# Choses a garder peut etre pour plus tard

Pour choisir cette *load bias*, le kernel se base sur plusieurs parmètres. La première contrainte qui intervient dans ce choix est le champ `p_align` de chaque segment `PT_LOAD`. 

`p_align` représente la congruence entre le decalage en mémoire et dans le fichier d'un segmentpar rapport a un debut de page.
Le kernel doit choisir un *load bias* pour lequel *load bias* + p_vaddr du premier segment soient un multiple du plus grand entre page_size et le plus grand des p_align de tous les segments.

Le champ `p_align` représente une contrainte d'alignement que le loader doit prendre en compte lors du chargement du segment en mémoire.

Nous pouvons noter les propriétés suivantes :

- Si `p_align == 1` ou `0`, le segment n'a aucune contrainte d'alignement supplémentaire imposée par ce champ.

- Si `p_align > 1`, il doit être une puissance de 2.

- Si `p_align > 1`, `p_offset % p_align` doit être égal à `p_vaddr % p_align`. Autrement dit, le segment doit commencer avec le même décalage par rapport à un multiple de `p_align`, aussi bien dans le fichier qu'en mémoire.

Pour mieux comprendre l'utilité de cette valeur, il peut être intéressant de s'attarder sur la manière dont elle est choisie par les linkers.

Les différentes sections d'un ELF contiennent des données ou des instructions qui peuvent avoir des contraintes d'alignement. Une fois chargées en mémoire, ces sections doivent commencer à des adresses respectant leurs contraintes respectives.

Ces contraintes sont renseignées dans le champ `sh_addralign` de chaque section.

Lors de la création de l'ELF, le linker organise donc les sections en mémoire de manière à respecter leurs contraintes d'alignement. Il doit également s'assurer que le chargement des segments ne viendra pas compromettre cet alignement.

Pour déterminer `p_align`, les linkers prennent généralement en compte les contraintes d'alignement des sections contenues dans le segment, mais aussi certains paramètres propres à l'architecture cible, comme la taille maximale de page configurée pour le linker.

**Exemple :** Un segment qui aurait un prérequis d'alignement de `0x10` pourrait se voir set son `p_align` a `0x1000`, puisque dans tous les l'alignement avec `page_size` est obligatoire et toutes les puissances de deux inférieures a une certaine valeur sont automatiquemtn alignées avec

Il existe également des utilisations plus spécifiques de `p_align`, notamment pour permettre des alignements compatibles avec les *Huge Pages*, mais nous ne nous y attarderons pas ici.

Comme vu précedemment, un 

Vu que le kernel charge toujours un nombre entier de pages, il pourrait simplement choisir une *base adress* alignée avec `page_size`, comme `0x300000` ou `0x3001000`. Comme `page_size` est une puissance de deux tout autant que les prérequis d'alignement des sections, les sections se retrouveraient alignées.

**Exemple :** `sh_addralign` = `0x10` --> aligné avec toutes les puissances de 2 supérieures ou égales à `0x10`.

Le problème apparait lorsque le prérequis d'alignement d'une section est <u>supérieur</u> à `page_size`.

Dans ce cas, l'exemple précédent ne fonctionne plus :

**Exemple :** `sh_addralign` = `0x2000` n'est pas forcément aligné avec une adresse alignée avec un `page_size` = `0x1000` (comme `0x3001000`).

Donc le kernel va choisir une adresse qui est alignée avec `page_size` OU avec le prérequis d'alignement le plus grand parmis tous les segments `PT_LOAD`, étant supérieur à `page_size`.

Le champ `p_align` d'un Program Header est renseigné par les linkers. Cependant il n'y a pas une logique unique, et chaqu'un emploie sa propre stratégie. Certains renseignent la `sh_addralign` la plus élevée parmis toutes les sections englobées par le segment qu'il décrit, d'autres renseignent la `page_size` max supportée par l'executable... 

Dans tous les cas le kernel (et notre packer également) va choisir une adresse de base qui est compatible avec la formule suivante : `base_addr % ([highest p_align] > page_size ? [highest p_align] : page_size) == 0`.

Résumé du calcul du bias :

```c
/* 1. Emplacement de référence */
load_bias = ELF_ET_DYN_BASE;

/* 2. Randomisation (ASLR) */
if (current->flags & PF_RANDOMIZE)
    load_bias += arch_mmap_rnd();

/* 3. Contraintes d'alignement */
alignment = maximum_alignment(...);

if (alignment)
    load_bias &= ~(alignment - 1);

/* 4. Calcul du load_bias pour les segments */
load_bias = ELF_PAGESTART(load_bias - vaddr);
```

Parler du aslr dans l'implementation

LINITATIONS :
peut pas ouvrir son propre fichier car /proc/self/exe pointe vers ./woody

FIX :

il faut etablir des ranges pour proteger heap et stack et les mapping mmap
