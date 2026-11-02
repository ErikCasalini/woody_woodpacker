# LZSS — cheat sheet

Compression maison utilisée par Woody. Code : `src/common/lzss.{c,h}`.

## L'idée en une phrase

On remplace les bouts de données **déjà vus** par une **référence arrière**
(« recule de `distance`, recopie `longueur` octets ») ; le reste part en
**littéraux** (octets bruts). Un **bit de flag** par token dit lequel des deux c'est.

- Compresser = chercher la plus longue répétition (coûteux).
- Décompresser = lire et recopier (trivial, sans libc).

## Les constantes

| Nom | Valeur | Quoi |
|-----|--------|------|
| `LZSS_WINDOW` | 4096 | distance max d'une référence (1 → 4096) |
| `LZSS_MIN_MATCH` | 3 | longueur mini d'une référence |
| `LZSS_MAX_MATCH` | 18 | longueur maxi (`3 + 15`) |

`MIN_MATCH = 3` car une référence pèse 2 octets : couvrir moins de 3 octets ne
gagnerait rien.

## Le format

Flux = suite de **groupes**. Chaque groupe = **1 octet de flags** + **jusqu'à 8
tokens**. On lit les bits du flag du **bit 0 au bit 7** :

```
bit = 0  ->  littéral    : 1 octet recopié tel quel
bit = 1  ->  référence   : 2 octets (distance, longueur)
```

**Référence sur 2 octets :**

```
octet0 = (distance-1) & 0xFF
octet1 = ((distance-1) >> 8) << 4   |   (longueur - 3)
         \___ 4 bits hauts de dist ___/   \__ 4 bits long __/

décodage :  distance = (((octet1 >> 4) << 8) | octet0) + 1     // 1..4096
            longueur = (octet1 & 0x0F) + 3                      // 3..18
```

On range `distance-1` pour tenir dans 12 bits (0..4095 ⇒ 1..4096).

## Compresser (`lzss_compress`)

```
tant qu'il reste de l'entrée :
    poser un octet de flags (= 0) pour le groupe
    répéter 8 fois (ou jusqu'à la fin) :
        chercher la plus longue répétition dans les 4096 octets précédents
        si longueur >= 3 :  bit = 1, écrire les 2 octets de référence
        sinon            :  bit = 0, écrire l'octet littéral
```

## Décompresser (`lzss_decompress`)

```
tant qu'il reste de l'entrée :
    lire l'octet de flags
    pour chaque bit 0..7 :
        bit = 0 : copier 1 octet entrée -> sortie
        bit = 1 : lire la référence, recopier `longueur` octets
                  depuis sortie[position - distance]
```

**Piège à connaître — la copie se chevauche.** On recopie **octet par octet vers
l'avant**, pas avec un `memcpy`. Quand `distance < longueur`, source et destination
se chevauchent et ça régénère la répétition :

```
sortie "AB" + référence (distance=2, longueur=6)  ->  "ABABAB"
   A B                    (on part de pos-2)
   A B A                  lire A, écrire A
   A B A B                lire B, écrire B
   A B A B A B            ... et ainsi de suite
```

## Le seul truc contre-intuitif

Sur des données **incompressibles** (ex : aléatoire), aucune répétition → tout en
littéraux → on paie **1 octet de flags tous les 8 octets**, soit **+12,5 %**. LZSS
peut donc faire *grossir* une entrée déjà incompressible. C'est normal pour du LZ ;
c'est pour ça que le packer garde une porte de sortie « stocker brut ».
