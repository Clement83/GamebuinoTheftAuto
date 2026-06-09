# GTA Demake sur Gamebuino META — Design / Spec

> Statut : **proposé** (en attente de revue utilisateur)
> Date : 2026-06-08
> Approche retenue : **A — Compilateur PC → tableaux en flash (+ preview Python)**

## 1. Contexte & objectif

Demake de GTA 1 (Liberty City) sur **Gamebuino META**. On dispose des graphismes
originaux extraits : `GTA_textures_LC/LC/tiles/` (384 BMP, quasi tous 64×64×8bit) et
`GTA_textures_LC/LC/sprites/` (193 BMP, tailles variables : véhicules, piétons, armes,
icônes, frames d'explosion).

**Objectif MVP :** un **piéton** qui se déplace à la croix directionnelle dans une
**grande ville** scrollante, rendue depuis un **fichier de configuration éditable**,
avec collision contre les obstacles et caméra qui suit. Une **preview PC (GIF)** permet
de tout voir sans matériel ; la cible finale tourne sur une **vraie META**.

## 2. Contraintes matérielles & toolchain (vérifiés)

| Élément | Valeur |
|---|---|
| Écran | 80×64 px, mode **RGB565** (couleur 16 bits) ; pixel-doublé sur le TFT 160×128 |
| CPU | SAMD21 Cortex-M0+ @ 48 MHz |
| RAM | 32 KB (framebuffer 80×64×2 = ~10.2 KB géré par la lib) |
| Flash | 256 KB — **les `const` y vivent déjà** sur ARM (pas de `pgm_read_*`) |
| Stockage | lecteur SD (non utilisé en approche A) |

Toolchain confirmée sur la machine de dev :
- `arduino-cli 1.4.1`
- Cores : `arduino:samd 1.8.14`, `gamebuino:samd 1.2.2`
- Lib : `Gamebuino META 1.3.3`
- Device détecté : `/dev/ttyACM0` → FQBN **`gamebuino:samd:gamebuino_meta_native`**
- L'agent peut **compiler + téléverser + lire le port série** ; il ne peut PAS voir
  l'écran ni presser les boutons (→ mode démo auto-walk + confirmation visuelle humaine).

## 3. Décisions arrêtées

| # | Décision | Choix |
|---|---|---|
| D1 | Cible | Gamebuino META (couleur 80×64) |
| D2 | Taille de tuile | **8×8 px** → 10×8 = 80 tuiles à l'écran |
| D3 | Mode écran | RGB565 80×64 → **pas d'unification de palette nécessaire** |
| D4 | Stockage ville | Approche **A** : compilée sur PC → tableau d'index en **flash** |
| D5 | Identification assets | sous-ensemble curé finement nommé **+** catalogue auto des 577 |
| D6 | Perso | **piéton**, 4 directions (N/E/S/O), 2 frames de marche |
| D7 | Taille ville par défaut | **192×192** tuiles (1536×1536 px ≈ 24×24 écrans) ; max 256×256 |
| D8 | Build | Arduino-style via `arduino-cli`, headers générés dans le dossier sketch |
| D9 | Dépendances PC | Python 3 + **Pillow (PIL)** uniquement (GIF inclus) ; pytest pour les tests |

## 4. Architecture & arborescence

Pipeline déterministe **PC → flash**. Source de vérité = `city.txt` + sous-ensemble
d'assets nommés. Deux consommateurs partagent la même logique : la **preview Python**
et la **sketch Gamebuino** (parité garantie par une math de moteur identique).

```
gta/                                  (racine = dossier actuel)
├─ GTA_textures_LC/LC/{tiles,sprites}/   sources BMP (jamais modifiées)
├─ assets/
│  ├─ catalog.csv                     577 images : id,folder,w,h,categorie_devinee,nom
│  ├─ contact/                        planches-contact par catégorie (navigation)
│  ├─ named/                          SOUS-ENSEMBLE CURÉ (PNG renommés lisibles)
│  │  ├─ tiles/   grass.png road_h.png road_v.png road_cross.png pavement.png
│  │  │           water.png building_a.png building_b.png ...
│  │  └─ sprites/ player_s0.png player_s1.png player_n0.png ... car_red.png
│  └─ tiles8/                         tuiles 8×8 PNG (consommées par preview.py)
├─ city/
│  ├─ city.txt                        ← LA config éditable
│  └─ citymap.png                     rendu debug de la ville entière (généré)
├─ tools/
│  ├─ catalog.py                      BMP → catalog.csv + planches-contact
│  ├─ build_assets.py                 named/ → gta/assets.h+.cpp + assets/tiles8/
│  ├─ build_city.py                   city.txt → gta/citymap.h+.cpp + city/citymap.png
│  ├─ preview.py                      → preview.gif (auto-walk)
│  ├─ engine.py                       math de moteur PYTHON (caméra/collision) partagée
│  └─ tests/                          pytest
├─ gta/                               DOSSIER SKETCH (compilé par arduino-cli)
│  ├─ gta.ino                         setup/loop, input, rendu, mode démo
│  ├─ engine.h                        math de moteur C++ pure (host-testable)
│  ├─ assets.h / assets_data.cpp      tileset + flags + frames perso (généré)
│  └─ citymap.h / citymap_data.cpp    cityMap[] + dimensions + spawn (généré)
├─ preview.gif                        sortie visible du MVP
└─ docs/superpowers/specs/            ce document
```

## 5. Composants

Chaque outil a une responsabilité unique, des entrées/sorties explicites, et est
testable indépendamment.

### 5.1 `tools/catalog.py` — catalogue & planches *(livrable « renommage », partie auto)*
- **Entrée :** les 577 BMP.
- **Traitement :** pour chaque image, lit dimensions/mode via PIL ; devine une catégorie
  par heuristiques (64×64 → `tile` ; ratio très large → `banner` ; série `S992-1029` →
  `fx_explosion` ; etc.). Génère des planches-contact 6×6 (PNG, labellisées par id).
- **Sortie :** `assets/catalog.csv` (`id,folder,w,h,categorie_devinee,nom`) où `nom`
  est vide tant que non curé ; `assets/contact/*.png`.

### 5.2 Curation (agent + vision) *(livrable « renommage », partie fine)*
- L'agent parcourt les planches et **nomme finement le sous-ensemble** nécessaire
  (routes, croisements, trottoir, herbe, eau, ≥2 façades/toits, 1 piéton multi-frames,
  1 voiture décor), corrige les catégories ambiguës dans `catalog.csv`.
- Produit `assets/named/**` : vrais PNG renommés et lisibles (ex. `road_cross.png`).
  Convention de nom : `snake_case`, suffixes directionnels `_n/_e/_s/_w`,
  variantes `_a/_b/...`, frames `0/1`.

### 5.3 `tools/build_assets.py` — conversion → tileset & perso
- **Entrée :** `assets/named/tiles/*.png`, `assets/named/sprites/player_*.png`, +
  un mapping nom→flags (quelles tuiles sont **solides**).
- **Traitement :** réduit 64×64 → **8×8** par rééchantillonnage *box/area* (préserve un
  pixel-art propre, évite l'aliasing du nearest sur /8) ; convertit en **RGB565** ;
  empile les tuiles comme **frames d'une seule image Gamebuino** (idiomatique + compact).
- **Sortie :**
  - `gta/assets.h` (déclarations) + `gta/assets_data.cpp` (données `const`) :
    - `enum TileId : uint8_t { TILE_GRASS=0, TILE_ROAD_H, ... NUM_TILES };`
    - `const uint16_t tileset[NUM_TILES][8*8];` : pixels **RGB565** bruts (flash).
    - `const uint8_t tileFlags[NUM_TILES];` (`bit0 = TILE_SOLID`).
    - `const uint16_t playerSprite[4][PLAYER_FRAMES][8*8];` + `PLAYER_TRANSPARENT`.
    - `enum Dir : uint8_t { DIR_NORTH=0, DIR_EAST, DIR_SOUTH, DIR_WEST };`
  - `assets/tiles8/*.png` (mêmes tuiles 8×8 en PNG, pour `preview.py`).
- **Choix de rendu :** tableaux RGB565 bruts blittés via `gb.display.drawPixel` côté
  sketch → **aucune dépendance au format binaire `Image`** de la lib (un risque en moins).

### 5.4 `city.txt` (DSL) + `tools/build_city.py` — compilateur de ville
DSL ligne-à-ligne, déterministe (PRNG à graine fixe), commandes appliquées dans
l'ordre (les suivantes écrasent les précédentes) :

```
; commentaire (; ou #). 'tile' = nom de tuile défini dans assets.h.
size <W> <H>                    ; obligatoire, en premier ; ≤ 256×256
seed <N>                        ; défaut 1
fill <tile>                     ; remplit toute la map
rect <tile> <x0> <y0> <x1> <y1> ; rectangle plein inclusif (clampé aux bornes)
hline <tile> <x0> <x1> <y>      ; ligne horizontale
vline <tile> <y0> <y1> <x>      ; ligne verticale
roadgrid spacing <S> width <W> [margin <M>]  ; rues + croisements + trottoirs auto
river vertical <x> width <W>    ; rivière (water) verticale
river horizontal <y> width <W>  ; rivière horizontale
blocks <tile> density <D> [on <baseTile>]    ; remplit cases baseTile(=grass) hors-route, densité D∈[0,1]
alias <c> <tile> [<c> <tile> …] ; alias mono-caractère pour stamp ; '.' = ne touche pas
stamp <x> <y>                   ; zone ASCII ancrée coin haut-gauche en (x,y)
  ……lignes ASCII……
end
player <x> <y> <north|south|east|west>   ; spawn (doit être sur une tuile non-solide)
```

- **Validation :** mot-clé/tuile/alias/dir inconnus → erreur **avec n° de ligne** ;
  `size` hors limites → erreur ; coords hors map → clamp (rect/lignes) ou erreur (stamp/player) ;
  spawn sur tuile solide → erreur. Déterminisme vérifié par test (même `city.txt` → même sortie).
- **Sortie :**
  - `gta/citymap.h` + `gta/citymap_data.cpp` :
    `#define CITY_W/CITY_H`, `PLAYER_START_X/Y` (en tuiles), `PLAYER_START_DIR`,
    `const uint8_t cityMap[CITY_H*CITY_W];` (row-major : `cityMap[ty*CITY_W+tx]`).
  - `city/citymap.png` : la ville entière rendue (debug visuel d'un coup d'œil).

### 5.5 `tools/engine.py` + `tools/viewer.py` — moteur & viewer interactif PC
- `engine.py` : fonctions pures **source de vérité** du comportement (clamp caméra,
  `is_solid`, `try_move` collision axe par axe → glisse le long des murs). 100 % Python.
  La parité avec `engine.h` (C++) est vérifiée au **M4** (quand le sketch existe et qu'on
  peut compiler `engine.h` en natif).
- `viewer.py` : **viewer de map interactif pygame** (remplace l'ancien `preview.gif`).
  Charge `tiles8/` + `cityMap` + frames perso. **Vue d'ensemble qui scrolle, zoom réglable**
  (tuiles rendues ×N, défaut ×4 ; `+`/`-` pour zoomer/dézoomer jusqu'à voir toute la ville),
  centrée + clampée sur le perso. Un **rectangle overlay** matérialise la zone 80×64 que la
  Gamebuino afficherait (calculée via `clamp_camera`, donc clampée aux bords comme sur device).
  **Flèches directionnelles** → déplacement 4 dir via `try_move` (collisions réelles),
  orientation + alternance des 2 frames de marche ; `Échap` pour quitter. Rester simple :
  c'est un outil de validation visuelle de la map, pas un jeu.
- **Tests (headless, pytest) :** invariants de `engine.py` — perso dans les bornes,
  caméra clampée, pas de traversée de tuile solide, glissement le long des murs.
  `viewer.py` (boucle pygame) n'est pas testé unitairement ; il consomme `engine.py` testé.
- **Dépendance :** ajoute `pygame` (outil de dev PC uniquement, n'impacte pas le portage).

### 5.6 `gta/engine.h` + `gta/gta.ino` — sketch Gamebuino
**`engine.h` (math pure, host-testable, identique à `engine.py`) :**
- `uint16_t clampCamera(int playerPx, int worldPx, int screenPx)`
- `bool isSolidAt(int tileX, int tileY)` (lit `cityMap` + `tileFlags`)
- `void tryMove(int& x, int& y, int dx, int dy)` (collision séparée par axe → glisse le long des murs)

**`gta.ino` :**
- `setup()` : `gb.begin();` init perso depuis `PLAYER_START_*` ; `SerialUSB.begin()`.
- `loop()` : `while(!gb.update());`
  1. **Input** : croix → `(dx,dy)` (4 dir). En **mode démo** (compile-flag `DEMO_AUTOWALK`),
     `(dx,dy)` vient d'un chemin scripté identique à `preview.py`.
  2. `tryMove` (collision) ; mise à jour orientation + frame de marche.
  3. **Caméra** : `cam = clampCamera(playerPx + size/2 - ecran/2, worldPx, ecranPx)`.
  4. **Rendu tuiles** : `col0 = cam.x>>3, offX = cam.x&7` ; boucle **11×9** tuiles
     (10×8 visibles + 1 de marge par axe pour le scroll sub-tuile) ; chaque pixel via
     `gb.display.drawPixel(px, py, (Color)tileset[id][i])` (~6300 px/frame, OK à 25 fps).
  5. **Perso** : `drawPixel` des pixels de `playerSprite[dir][anim]` ≠ `PLAYER_TRANSPARENT`.
  6. **Debug série** (périodique) : pos perso (tuile), cam, FPS (`gb.getCpuLoad()`), heap libre.

## 6. Budgets

| | RAM (32 KB) | Flash (256 KB) |
|---|---|---|
| Framebuffer 80×64×2 | ~10.2 KB | — |
| tileset (32 tuiles, Image RGB565) | — | ~4.5 KB |
| cityMap 192×192×1 | — | ~36 KB |
| frames perso (8) | — | <2 KB |
| état jeu + pile | <1 KB | — |
| lib Gamebuino + code | (qq KB) | (dizaines de KB) |

→ RAM dominée par le framebuffer, large sous 32 KB. Marge flash confortable
(256×256 = 64 KB de map reste possible).

## 7. Jalons & critères de vérification

- **M1 — Assets & renommage.** Livrables : `catalog.csv`, `assets/contact/`,
  `assets/named/`, `gta/assets.{h,cpp}`, `assets/tiles8/`.
  *Vérif :* ouvrir les PNG renommés (œil) ; pytest : `NUM_TILES>0`, chaque tuile 8×8,
  flags cohérents, frames perso = 8.
- **M2 — Config & compilateur.** Livrables : `city.txt`, `tools/build_city.py`,
  `gta/citymap.{h,cpp}`, `city/citymap.png`.
  *Vérif :* pytest (roadgrid, rect, stamp, alias, bornes, **déterminisme**, erreurs
  ligne) ; œil sur `citymap.png`.
- **M3 — Viewer interactif PC.** Livrable : `tools/engine.py` + `tools/viewer.py` (pygame).
  *Vérif :* lancer `python3 -m tools.viewer`, conduire le perso aux flèches → rues cohérentes,
  scroll fluide, collisions correctes, cadre caméra clampé aux bords ; pytest sur les invariants
  de `engine.py`. (Parité `engine.py` ↔ `engine.h` reportée au M4.)
- **M4 — Sketch Gamebuino.** Livrables : `gta/gta.ino`, `gta/engine.h`.
  *Vérif :* `arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native gta/` (0 erreur,
  tailles RAM/flash sous les limites) ; `arduino-cli upload -p /dev/ttyACM0 …` ; **mode démo
  auto-walk** + lecture série (map chargée, perso parcourt le chemin, caméra clampe, ≥20 FPS,
  pas de crash) ; **confirmation visuelle par l'utilisateur** (croix + rendu) sur la vraie META.

M1→M3 sont 100 % PC : un **viewer interactif où l'on conduit le perso dans la ville**
arrive avant tout passage hardware.

## 8. Stratégie de test
- **Outils PC** : pytest (compilateur ville surtout : cas nominaux, erreurs, déterminisme).
- **Moteur** : tests de parité Python↔C++ sur clamp caméra / collision (mêmes entrées →
  mêmes sorties) ; `engine.h` compilé en natif PC pour ces tests.
- **Visuel** : `preview.gif` (M3) puis vraie META (M4).
- TDD pour les modules logiques (compilateur, moteur).

## 9. Hypothèses & risques
- **H1** Rendu via `drawPixel` (API garantie) → on évite toute dépendance au format
  binaire `Image` ; si la perf l'exige, optimisation ultérieure par accès direct au buffer.
- **H2** Perf : ~6300 `drawPixel`/frame (tuiles) + perso, visés à ≥20 FPS ; si insuffisant
  → blit par lignes / accès direct au buffer, ou réduire la fenêtre. Mesuré via série au M4.
- **H3** Le port série natif ré-énumère après reset/upload ; prévoir une petite
  attente/re-détection avant lecture série.
- **H4** Lisibilité d'un sprite à 8×8 : un piéton GTA1 réduit reste minuscule ; si peu
  lisible, perso à 10–12 px (indépendant de la tuile) — ajustable sans changer l'archi.

## 10. Hors-scope (YAGNI pour le MVP)
Voitures conduisibles, PNJ/trafic/IA, audio/musique, missions/HUD/score, jour-nuit,
sauvegarde, collisions fines sub-pixel, streaming SD (approche C). Tous envisageables
**après** le MVP sans casser l'architecture.
