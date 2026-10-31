# Woody : chiffrer le programme embarqué

Ce texte décrit le chiffrement tel qu'il est implémenté dans le code actuel.
Il prolonge [APPRENTISSAGES.md](APPRENTISSAGES.md), en particulier les sections
« Fabriquer le nouvel ELF » et « Retrouver le programme embarqué ».

## 1. Le problème

Sans chiffrement, le programme original (le *payload*) est embarqué **en clair**
dans le fichier `woody`. N'importe qui peut l'extraire, lire ses chaînes avec
`strings`, inspecter ses symboles, etc. Le but du chiffrement est de rendre ce
payload **illisible au repos** sur le disque.

Le déchiffrement se fait au **runtime, en mémoire**, obligatoirement **avant** le
mapping des segments : quand le stub prend la main, il remet le payload en clair
dans son image mémoire, puis le charge normalement.

## 2. Ce que le chiffrement n'est pas

C'est de l'**obfuscation, pas de la sécurité**. La clé est écrite **dans le
binaire lui-même** (le stub doit pouvoir se déchiffrer tout seul, sans
intervention). Un analyste déterminé peut donc la récupérer, en lisant le
header, ou en laissant tourner le programme et en dumpant la mémoire. Le
chiffrement gêne l'analyse statique (un `strings woody` ne révèle plus rien du
payload) ; il ne la rend pas impossible. C'est le modèle de tout *packer* réel.

## 3. L'algorithme : RC4

- **Chiffrement par flot** (*stream cipher*) : il génère un ruban pseudo-aléatoire
  (*keystream*) aussi long que nécessaire et le combine au message par XOR, octet
  par octet. La taille du payload est donc **quelconque**.
- **Clé de 64 bits** : elle tient telle quelle dans le champ `key`
  (`Elf64_Xword`) du header, sans modifier la structure (XTEA aurait exigé
  128 bits, donc un `uint8_t[16]`).
- **Symétrique** : comme le XOR est sa propre inverse (`M ^ R ^ R == M`),
  **chiffrer et déchiffrer sont la même opération**. Le packer et le stub
  appellent exactement la même fonction `rc4_crypt`.
- **Sans libc** : l'algorithme tient dans un tableau `s[256]` sur la pile, ce qui
  permet de le réutiliser dans le stub *freestanding*.

Le sujet interdit seulement le ROT/XOR simple ; RC4 passe. Sa faiblesse
cryptographique réelle n'a aucune importance ici (voir §2).

### Fonctionnement (rappel)

`rc4_crypt` (dans `src/common/rc4.c`) procède en deux phases :

1. **KSA** (*Key Scheduling Algorithm*) : initialise `s = [0, 1, …, 255]` puis le
   brasse en fonction de la clé. C'est la **seule** phase où la clé intervient.
   Résultat : une permutation des 256 octets, dont l'ordre dépend entièrement de
   la clé.
2. **PRGA** (*Pseudo-Random Generation Algorithm*) : à chaque octet à traiter,
   fait évoluer `s` par un échange, en tire un octet de ruban, et l'applique par
   XOR : `data[n] ^= s[(s[i] + s[j]) & 0xff]`.

Comme tout est **déterministe**, la même clé produit le même `s` initial, donc le
même ruban. Le stub peut ainsi régénérer exactement le ruban du packer et défaire
le XOR.

## 4. Le header du payload

Défini dans `src/common/payload.h` :

```c
typedef struct payload_header_s
{
    unsigned char   magic[8];          /* "WOODY\0\0\0" */
    Elf64_Xword     compressed_size;   /* = decompressed_size tant qu'il n'y a pas de compression */
    Elf64_Xword     decompressed_size; /* taille du payload en octets */
    Elf64_Off       offset;            /* = sizeof(payload_header_t) : début du payload après le header */
    Elf64_Xword     key;               /* clé RC4, 8 octets */
} payload_header_t;
```

La **taille de la clé n'est stockée nulle part** : elle est fixée par le type du
champ (`sizeof(payload_header->key)` = 8). Packer et stub utilisent la même
expression `sizeof(...)`, donc ils s'accordent forcément sans métadonnée
supplémentaire.

## 5. Côté packer : chiffrer

Fichiers : `src/packer/main.c`, `src/packer/output.c`.

1. **Générer la clé**: `generate_payload_key()` (output.c) lit 8 octets depuis
   `/dev/urandom` (via `open`/`read`, **jamais** `rand()`) et les place dans
   `payload_header->key`. Une clé aléatoire à chaque packing : deux `woody`
   produits du même binaire ont des clés et donc des payloads chiffrés
   différents.
2. **Afficher la clé**: `main.c` imprime `key_value: 0x<hex>`, chaque octet sur
   2 chiffres (`%02x`), soit 16 caractères hexa pour 8 octets. Sert surtout de
   preuve, en démonstration, qu'une vraie clé aléatoire a été générée.
3. **Chiffrer le payload**: dans `fill_output_elf()` (output.c), avant l'écriture.
   Le fichier cible est mappé en lecture seule (`PROT_READ`) ; on travaille donc
   sur une **copie privée** : `malloc` + `memcpy`, puis `rc4_crypt` sur la copie,
   puis `write`, puis `free`. Seuls les octets du payload sont chiffrés ; le
   header `WOODY` (dont la clé) reste en clair.

## 6. Côté stub : déchiffrer

Fichier : `src/stub/payload.c`, fonction `decrypt_payload()`. C'est le miroir
exact du packer :

1. Calculer le début du payload :
   `(unsigned char*)payload_header + payload_header->offset`.
2. **Déchiffrer en place** avec `rc4_crypt`, en passant `&payload_header->key`,
   `sizeof(payload_header->key)` et `decompressed_size`. Le déchiffrement est fait
   directement sur les octets mappés (pas sur une copie) : c'est cette image
   mémoire que le mapping des segments utilisera ensuite.
3. **Vérifier l'intégrité** : contrôler que les 4 premiers octets du payload
   déchiffré sont bien le magic ELF (`ELFMAG0..3` = `0x7f 'E' 'L' 'F'`).

Le stub est *freestanding* : pas de `memcmp` de la libc, la comparaison des 4
octets est écrite à la main.

## 7. Robustesse : corruption → pas de crash

La vérification du magic est le **garde-fou** exigé par le sujet. Si le header ou
le payload a été modifié, le déchiffrement produit du bruit, le magic ne
correspond pas, et `decrypt_payload` renvoie `NULL`. L'appelant
(`execute_packed_elf`) affiche alors le message `corrupted` et s'arrête
proprement (code de sortie 1), **sans** tenter de charger n'importe quoi et donc
sans segfault.

## 8. Vérifier soi-même

```sh
# Packer un binaire et observer la clé affichée
./woody_woodpacker /bin/ls        # -> key_value: 0x....

# Le payload n'est plus en clair : il ne commence pas par 7f 45 4c 46
#   (le magic ELF) juste après le header "WOODY"

# Le binaire packé se comporte comme l'original
./woody                           # (après la bannière) sortie identique à /bin/ls

# Corruption -> message "corrupted", pas de crash
#   (voir tests/test_packer.py : test_stub_reports_corrupted_payload_without_crashing)

# La suite complète
make test
```

## 9. Lien avec la compression (à venir)

Les champs `compressed_size` / `decompressed_size` existent en prévision de la
compression (bonus). Le pipeline deviendra alors, côté packer,
*compresser → chiffrer → écrire*, et côté stub *déchiffrer → décompresser →
vérifier le magic → mapper*. Tant que la compression n'est pas implémentée, les
deux tailles sont égales et `decrypt_payload` traite `decompressed_size` octets.
Voir [TODO_compression.md](../TODO_compression.md).
```
