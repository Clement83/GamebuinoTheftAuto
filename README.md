# GTA Demake — Gamebuino META

Un demake top-down façon GTA1/2 pour la **Gamebuino META** (microcontrôleur
SAMD21 / Cortex-M0+, écran 80×64, RAM et flash très limitées). Ville générée,
trafic IA (voitures + piétons), conduite arcade avec drift, combat à pied,
armes, et un système de **missions** déclenchées par des **cabines
téléphoniques**.

## Jouer (sur la console)

Pas besoin de compiler : le dépôt fournit le jeu déjà packagé dans le dossier
**`GTADEMAKE/`** (le `.bin`, l'icône et l'écran-titre).

1. Branche la carte microSD de la Gamebuino META sur ton PC.
2. **Copie-colle le dossier `GTADEMAKE` à la racine de la carte SD**, à côté des
   autres jeux (chaque jeu Gamebuino est un dossier à la racine).
3. Remets la carte dans la console : GTA Demake apparaît dans le menu des jeux.

> La racine de la SD ressemble alors à `GTADEMAKE/`, `Pns/`, `Tetris/`, … — un
> dossier par jeu. Ne renomme pas `GTADEMAKE` (le firmware attend ce nom).

La suite de ce README s'adresse au **développement** (build, génération des
données, tests).

## 🎮 Contrôles

```
        ▲
     ◄ ✚ ►        (A)  (B)
        ▼      [ SELECT ]
```

| Bouton | À pied | Au volant |
|:------:|:-------|:----------|
| **✚ Croix** | Se déplacer | Braquer (◄ ►) |
| **A** | 👊 Attaquer (poing / arme) | Accélérer |
| **B** | 🤸 Saut (par-dessus les voitures) | Freiner / marche arrière |
| **SELECT** | 🚪 Action : entrer/interagir | 🚪 Sortir de la voiture |

> **SELECT = le bouton « action »** : c'est le même pour **entrer** et **sortir**
> d'une voiture, **entrer** dans un bâtiment (armurerie, casino, Planque, Bar,
> Commerces, Bureaux), **décrocher** une cabine qui sonne, ou **repeindre** au
> Pay'n'Spray (rentrer en voiture et s'arrêter dans le garage). À pied, sans rien
> à portée, il **change d'arme**.

## Sauvegarde (3 profils)

Au démarrage, un **écran de sélection** propose **3 profils** indépendants
(`PROFIL 1/2/3`). Chaque ligne montre un aperçu (`$argent` + `Mission n/total`)
ou `- VIDE -`. **HAUT/BAS** choisit, **A** lance (charge le profil ou démarre
une partie neuve sur un slot vide), **B** efface le slot (deux appuis pour
confirmer).

On **sauvegarde en dormant à la Planque** (le « dodo » déjà existant) : l'état
persistant — **argent, avancement de la campagne, armes & munitions** — est
écrit dans le profil actif (bandeau « Partie sauvegardee »). Le reste (position,
trafic, niveau de recherche) est transitoire et régénéré à chaque lancement.

Côté code : `gta/save.h` (struct `SaveProfile` + helpers purs, testés par
`tools/host_test/test_save_host.cpp`) et la glue `gb.save` dans `gta.ino`
(`profileRead` / `profileWriteCurrent` / `profileLoadCurrent`, menu
`updateProfileMenu` / `drawProfileMenu`).

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
  save.h              Profils de sauvegarde (struct + pack/apply purs)
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
# Tests autonomes (mission, combat, ai, weapons, save)
g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/t && /tmp/t
g++ -std=c++11 -I gta tools/host_test/test_save_host.cpp -o /tmp/t && /tmp/t

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
