# Woody : Chiffrement (ChaCha20)

## 1. L'algorithme : ChaCha20

- **Chiffrement par flot** (*stream cipher*) : il génère un ruban
  pseudo-aléatoire (*keystream*) aussi long que nécessaire et le combine au
  message par XOR, octet par octet. La taille du payload est donc **quelconque**.
- **Clé de 256 bits** (`uint8_t[32]`), **nonce de 96 bits** (12 octets) et
  **compteur de bloc** de 32 bits. La clé ne tient plus dans un `Elf64_Xword` :
  le champ `key` du header devient un `uint8_t[32]`.
- **Symétrique** : comme le XOR est sa propre inverse (`M ^ K ^ K == M`),
  **chiffrer et déchiffrer sont la même opération**. Le packer et le stub
  appellent exactement la même fonction `chacha20_crypt`.
- **Sans libc** : l'algorithme tient dans une grille de 16 mots de 32 bits sur la
  pile, avec seulement des additions, des XOR et des rotations, ce qui permet de
  le réutiliser dans le stub *freestanding*.

### Nonce et compteur

ChaCha20 prend deux entrées en plus de la clé. Le **nonce** (*number used once*)
permet d'obtenir un keystream différent avec la même clé ; le **compteur**
numérote les blocs de 64 octets du keystream.

La règle d'or est de ne **jamais réutiliser le couple (clé, nonce)** : deux
messages chiffrés avec le même keystream se trahissent mutuellement par un simple
XOR des deux chiffrés. Woody respecte cette règle de la façon la plus simple :

- le **nonce est fixé à zéro** (12 octets nuls, en dur) et le **compteur part de
  zéro** à chaque fichier ;
- mais une **clé aléatoire est tirée à chaque packing**, donc le couple
  (clé, nonce) n'est jamais réutilisé.

Rien de plus n'est stocké dans le header. Une évolution possible plus tard serait
un nonce aléatoire rangé dans le header, à côté de la clé.

### Fonctionnement

`chacha20_crypt` (dans `src/common/chacha20.c`) construit une **grille de 16 mots
de 32 bits** (16 × 4 = 64 octets, la taille d'un bloc) :

```
[ constante ][   clé    ][ compteur ][  nonce  ]
  cases 0-3    cases 4-11   case 12    cases 13-15
```

- Les cases 0 à 3 contiennent la constante fixe imposée par la norme
  (RFC 8439) : `"expand 32-byte k"`. Elle garantit qu'une partie de la grille
  reste fixe et désordonnée quelle que soit la clé (sans elle, une grille toute à
  zéro resterait nulle après mélange et laisserait le payload en clair).
- Pour produire un bloc de 64 octets de keystream, la grille est mélangée sur
  **20 tours** de *quarter-rounds* (alternance tours de colonnes / tours de
  diagonales), puis on **rajoute la grille de départ** au résultat
  (*feed-forward*). Ce dernier ajout est essentiel : il rend le mélange non
  réversible, sinon on pourrait remonter de la grille finale jusqu'à la clé.
- Pour un payload de plus de 64 octets, le **compteur est incrémenté** à chaque
  bloc (`init[12]++`), ce qui donne un nouveau bloc de keystream.

Comme tout est **déterministe**, les mêmes (clé, nonce, compteur) produisent
toujours le même keystream. Le stub peut ainsi régénérer exactement le ruban du
packer et défaire le XOR.
