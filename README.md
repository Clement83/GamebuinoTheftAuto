# GTA Demake — Gamebuino META

Un demake top-down façon GTA1/2 pour la **Gamebuino META** (microcontrôleur
SAMD21 / Cortex-M0+, écran 80×64, RAM et flash très limitées). Ville générée
de façon procédurale (organique, par seed), trafic IA (voitures + piétons,
sens uniques, anti-chevauchement), conduite arcade avec drift, voitures
destructibles, combat à pied, armes, niveau de recherche policier (étoiles),
musique chiptune, POI interactifs (casino, hôpital, garage, broyeur de La
Casse…), et une **campagne scénarisée de 18 missions + épilogue** doublée de
**missions secondaires rejouables**, toutes deux déclenchées par des
**cabines téléphoniques**.

# Pour jouer

## Installer (sur la console)

Pas besoin de compiler : le dépôt fournit le jeu déjà packagé dans le dossier
**`GTADEMAKE/`** (le `.bin`, l'icône et l'écran-titre).

1. Branche la carte microSD de la Gamebuino META sur ton PC.
2. **Copie-colle le dossier `GTADEMAKE` à la racine de la carte SD**, à côté des
   autres jeux (chaque jeu Gamebuino est un dossier à la racine).
3. Remets la carte dans la console : GTA Demake apparaît dans le menu des jeux.

> La racine de la SD ressemble alors à `GTADEMAKE/`, `Pns/`, `Tetris/`, … — un
> dossier par jeu. Ne renomme pas `GTADEMAKE` (le firmware attend ce nom).

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

## Campagne & cabines téléphoniques

Deux familles de cabines, dessinées en pixel-art 8×8 procédural
(`drawPhoneBooth` dans `gta.ino`), posées sur le **trottoir** le plus proche
(`findSidewalkSpot`, tuile `TILE_PAVEMENT`) pour rester accessibles à pied :

- **Cabines bleues — missions secondaires** (`PHONES`). Une par mission,
  réparties en grille. Au repos, la plus proche **sonne** (toit clignotant +
  ondes) ; décrocher (A) lance la mission associée. Rejouables.

- **Cabine rouge — trame principale**. **Une seule**, ancrée à la **Planque**
  (pas plusieurs dispersées sur la carte). Elle sonne au lancement et après
  chaque mission de trame réussie ; décrocher (A) lance la mission suivante de
  `STORY_SEQ[]` (`campaignStep`, sauvegardé). Le dernier décrochage déclenche
  l'épilogue.

La trame est entièrement implémentée : **18 missions + épilogue**, réparties
en 4 actes (Marco → fausse piste chez les Loups → la vérité avec Sarah →
Victor). Voir [campagne/README.md](campagne/README.md) pour le scénario
complet (boucle de jeu, types d'objectifs, lieux, personnages) et les fiches
`campagne/acte-*/m*.md`, chacune un script fidèle au code (`mod_mission.h`).

# Pour développer

La suite de ce README couvre le **développement** : structure du code, build,
génération des données, tests.

## Structure du projet

```
gta/                  Sketch Arduino + headers C++ "purs" (le firmware)
  gta.ino             Point d'entrée : includes, setup(), loop(), inclusion des modules
  game_state.h        Types, données et état global (une seule unité de compilation)
  game_decls.h        Prototypes de toutes les fonctions libres (appels croisés)
  mod_*.h             Corps de fonctions par domaine, #include depuis gta.ino :
                      world / render / ai / combat / mission / economy /
                      player / hud / sequence / save. Tout reste 'static' dans
                      une seule TU — ce ne sont PAS des unités compilées à part.
  engine.h            Caméra + collision (portage de tools/engine.py)
  ai.h                IA d'errance sur grille (portage de tools/ai.py)
  car.h               Physique voiture arcade (drift)
  combat.h            Coup de poing / dégâts
  weapons.h           Définition des armes + munitions
  mission.h           Couche mission (objectifs, LOS, fuite, flèche HUD)
  wanted.h            Machine d'état du niveau de recherche policier (étoiles)
  slot.h              Logique pure de la machine à sous du Casino
  save.h              Profils de sauvegarde (struct + pack/apply purs)
  smoke.h / music.h   Données générées (fumée voiture, pistes chiptune)
  assets.h / *_data.cpp   Données générées (tileset, sprites, carte) — NE PAS éditer
tools/                Générateurs Python (PC) -> écrivent les *_data.cpp
  build_assets.py     tileset.csv + PNG -> gta/assets.{h,cpp}
  build_city.py       city/city.txt -> gta/citymap.{h,cpp} (+ aperçu PNG)
  citygen.py          générateur procédural de ville (value noise déterministe
                      par seed : zones, routes, ponts, blocs, trottoirs, spawn)
  citydsl.py          compilateur pur du DSL de city.txt (appelle citygen)
  pois.py             placement déterministe des POI sur la grille générée
  build_player.py     sprite piéton procédural -> gta/player.{h,cpp}
  build_car.py        frames de rotation voiture + fumée -> gta/car.{h,cpp}, smoke.h
  build_weapons.py    icônes d'armes 6x6 -> gta/weapons_gfx.{h,cpp}
  build_music.py      pistes chiptune (2 voix) -> gta/music.{h,cpp}
  build_junk_tiles.py tuiles pixel-art de La Casse -> assets/named/tiles/
  catalog.py          inventaire des BMP du rip -> assets/catalog.csv + planches-contact
  curate.py           mappe les ids GTA choisis -> assets/named/** + tileset.csv
  gbimg.py            helpers de conversion d'images (pipeline Gamebuino)
  viewer.py           viewer pygame interactif de la ville compilée (debug)
  preview_slot.py     aperçu de la machine à sous (écran 80x64)
  engine.py / ai.py   "source de vérité" du comportement, en parité avec les .h
  host_test/          Tests natifs g++ (pas de dépendance Gamebuino)
  tests/              Tests pytest des générateurs (citygen, citydsl, pois, ai, engine…)
assets/               PNG sources, tileset.csv, catalogues, sprites/tuiles nommés
city/                 city.txt (issu de citygen, par seed) + aperçu PNG généré
campagne/             Scripts narratifs des 18 missions + épilogue (doc, lit le code)
docs/superpowers/     Specs et plans de conception (un par feature/jalon)
POI.md                Feuille de route des POI (état d'avancement, économie)
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
