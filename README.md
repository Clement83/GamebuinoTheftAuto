# GTA Demake — Gamebuino META

Un demake top-down façon GTA1/2 pour la **Gamebuino META** (microcontrôleur
SAMD21 / Cortex-M0+, écran 80×64, RAM et flash très limitées). Ville générée,
trafic IA (voitures + piétons), conduite arcade avec drift, combat à pied,
armes, et un système de **missions** déclenchées par des **cabines
téléphoniques**.

## Structure du projet

```
gta/                  Sketch Arduino + headers C++ "purs" (le firmware)
  gta.ino             Boucle de jeu, rendu, entrées, orchestration
  engine.h            Caméra + collision (portage de tools/engine.py)
  ai.h                IA d'errance sur grille (portage de tools/ai.py)
  car.h               Physique voiture arcade (drift)
  combat.h            Coup de poing / dégâts
  weapons.h           Définition des armes + munitions
  mission.h           Couche mission (objectifs, LOS, fuite, flèche HUD)
  assets.h / *_data.cpp   Données générées (tileset, sprites, carte) — NE PAS éditer
tools/                Générateurs Python (PC) -> écrivent les *_data.cpp
  build_assets.py     tileset.csv + PNG -> gta/assets.{h,cpp}
  build_city.py       city/city.txt (DSL) -> gta/citymap.{h,cpp}
  build_player.py     sprite piéton procédural -> gta/player.{h,cpp}
  build_car.py        frames de rotation voiture -> gta/car.{h,cpp}
  build_weapons.py    icônes d'armes 6x6 -> gta/weapons_gfx.{h,cpp}
  engine.py / ai.py   "source de vérité" du comportement, en parité avec les .h
  host_test/          Tests natifs g++ (pas de dépendance Gamebuino)
assets/               PNG sources, tileset.csv, catalogues
city/                 city.txt (DSL de la ville) + aperçus PNG générés
GTA_textures_LC/      Rip de textures GTA (577 BMP numérotés, non nommés)
previews/             Rendus PNG de debug (le client n'affiche pas inline)
```

Les fichiers `gta/*_data.cpp` et certains `.h` sont **générés** par les scripts
`tools/build_*.py` — on édite la source (PNG, `tileset.csv`, `city/city.txt`),
pas le `.cpp` de sortie.

## Build & flash

Le sketch vit dans un dossier `gta` (< 4 caractères), donc la lib réclame le
flag `FOLDER_NAME`. Depuis la racine du dépôt :

```sh
# Compiler
arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native \
  --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta

# Compiler + téléverser (port à adapter, cf. `arduino-cli board list`)
arduino-cli compile -u -p /dev/ttyACM0 \
  --fqbn gamebuino:samd:gamebuino_meta_native \
  --build-property 'compiler.cpp.extra_flags=-DFOLDER_NAME="GTADEMAKE"' gta
```

## Tests (natifs, sur PC)

Les `.h` de logique sont compilables en natif (aucune dépendance Gamebuino) :

```sh
# Tests autonomes (mission, combat, ai, weapons)
g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/t && /tmp/t

# engine et ai_city référencent la carte -> lier les données
g++ -std=c++11 -I gta tools/host_test/test_engine_host.cpp \
  gta/citymap_data.cpp gta/assets_data.cpp -o /tmp/t && /tmp/t
```

Tests Python des générateurs : `.venv/bin/pytest`.

## Cabines téléphoniques

Deux familles de cabines, dessinées en pixel-art 8×8 procédural
(`drawPhoneBooth` dans `gta.ino`), posées sur le **trottoir** le plus proche
(`findSidewalkSpot`, tuile `TILE_PAVEMENT`) pour rester accessibles à pied :

- **Cabines bleues — missions secondaires** (`PHONES`). Une par mission,
  réparties en grille. Au repos, la plus proche **sonne** (toit clignotant +
  ondes) ; décrocher (A) lance la mission associée. Rejouables.

- **Cabines rouges — trame principale** (`STORY_PHONES`). 3-4 dispersées sur la
  carte. **Muettes pour l'instant** : ce sont des points d'ancrage posés
  d'avance pour la future mission scénarisée. Quand l'histoire sera branchée,
  l'une d'elles sonnera et fera avancer la trame principale. On les distingue
  des cabines de missions par leur couleur rouge, et elles restent visibles en
  permanence (repères) même pendant une mission.

Pour brancher la trame : donner un état "qui sonne" à une `STORY_PHONES`,
réutiliser la logique de décrochage des cabines bleues, et déclencher la
mission scénarisée correspondante.
