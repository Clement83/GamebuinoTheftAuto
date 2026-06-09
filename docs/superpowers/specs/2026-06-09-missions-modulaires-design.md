# Système de missions modulaire + mission « Mauvaise affaire »

Date : 2026-06-09
Cible : GTA demake Gamebuino META (sketch `gta/`, écran 80×64, ~25 fps).

## Objectif

Remplacer la mission mono-objectif actuelle (codée en dur dans `gta.ino`) par un
**système de missions modulaire** :

- Des **téléphones fixes** dans la ville, à coordonnées stables, chacun déclenche
  **toujours la même** mission scénarisée.
- Des missions composées d'**objectifs enchaînés** de quelques types génériques.
- Une **narration** : petits messages qui défilent en bas de l'écran pour
  raconter ce qui se passe.

Deux missions livrées :

- **Mission A — « Règlement de comptes »** : l'actuelle (tuer une cible errante),
  enrichie d'un peu de lore (« va buter Joe »), portée sur le nouveau système.
- **Mission B — « Mauvaise affaire »** : la nouvelle mission multi-étapes
  demandée (récupérer une voiture, prendre Marco, rouler au chantier, Marco se
  fait abattre, éliminer le tueur).

## Principe d'architecture

On suit le pattern existant du projet : **logique pure dans un header**
(testable en host sans Gamebuino) + **glue device** dans `gta.ino`.

- `mission.h` (pur, déjà existant — on l'étend) : modèle de données des missions,
  moteur d'avancement des objectifs, IA de fuite (déjà là) et de poursuite
  (nouvelle), ligne de vue, vecteur flèche HUD.
- `gta.ino` (glue) : place les entités selon l'objectif courant (marqueurs,
  voiture de mission, cibles), dessine le bandeau de narration et le marqueur,
  réutilise la flèche HUD existante.

### YAGNI / hors périmètre

- Pas d'état d'échec de mission (le jeu n'a pas de mort joueur ni de PV).
- Sortir de la voiture en cours de route n'annule pas la mission.
- Pas d'éditeur de mission ni de format sérialisé : les missions sont des
  tableaux C++ `const` en flash.

## Modèle de données (mission.h, pur)

Trois types d'objectifs génériques couvrent les deux missions :

```c
enum ObjType : uint8_t {
  OBJ_GOTO     = 0,  // atteindre un point monde dans un rayon
  OBJ_ENTER_CAR = 1, // être au volant de la voiture de mission
  OBJ_KILL     = 2,  // la cible de mission est morte
};

struct Objective {
  uint8_t type;        // ObjType
  int16_t x, y;        // point monde (centre px) : GOTO -> destination ;
                       //   ENTER_CAR/KILL -> où spawner la voiture/cible
  uint8_t radius;      // GOTO : rayon de validation (px)
  bool    requireCar;  // GOTO : exige d'être au volant ("conduis jusqu'à")
  uint8_t event;       // événement déclenché à la complétion (EV_*), ou EV_NONE
  const char *text;    // narration affichée quand l'objectif devient actif
};

struct MissionDef {
  const char *title;
  const Objective *objectives;
  uint8_t count;
};
```

Événements scriptés (transitions) :

```c
enum MissionEvent : uint8_t {
  EV_NONE = 0,
  EV_MARCO_JOIN  = 1,  // Marco monte dans la voiture (passager + message)
  EV_MARCO_DIE   = 2,  // Marco meurt, le tueur (chasseur) apparaît
};
```

Runtime (état courant d'une mission en cours) :

```c
struct MissionRun {
  uint8_t def;     // index de la mission active (ou MISSION_NONE)
  uint8_t step;    // index de l'objectif courant
  bool    active;
};
```

### Moteur pur, testable

- `bool missionObjectiveDone(const Objective &o, const MissionState &s)` :
  vrai si l'objectif courant est rempli compte tenu de l'état de jeu fourni
  (`MissionState` = pos joueur, `driving`, pos voiture, cible vivante/morte,
  voiture de mission occupée).
- `uint8_t missionAdvance(MissionRun &run, const MissionDef &def)` : passe à
  l'objectif suivant, renvoie l'événement de la transition (EV_*), termine la
  mission si dernier objectif.

`MissionState` est une petite struct de valeurs (pas de dépendance Gamebuino)
remplie par `gta.ino` chaque frame, ce qui rend `missionObjectiveDone`
entièrement testable en host.

### IA de poursuite (nouvelle)

Miroir de la fuite existante (`missionFleeExit/Step`) : au lieu de **maximiser**
la distance au joueur, on la **minimise**.

```c
uint8_t missionChaseExit(grid, w, h, tx, ty, curDir, playerCx, playerCy, rng);
void    missionChaseStep(grid, w, h, x, y, dir, tgtx, tgty, speed,
                         playerCx, playerCy, rng);
```

Utilisée par le tueur de Marco à l'étape finale de « Mauvaise affaire » : il
fonce vers le joueur sur les tuiles franchissables.

## Contenu des missions

### Mission A — « Règlement de comptes » (téléphone n°1)

1. `OBJ_KILL` (cible "Joe", verte, erre + fuit en ligne de vue — comportement
   actuel inchangé). Narration d'intro : *« Joe m'a balancé. Retrouve-le et
   fais-le taire. »* Événement de fin : *« Beau boulot. Joe parlera plus. »*

Comportement identique à la mission actuelle : à la mort de la cible, la mission
se termine et le téléphone se réarme.

### Mission B — « Mauvaise affaire » (téléphone n°2)

1. `OBJ_GOTO` parking (rayon) puis `OBJ_ENTER_CAR` (voiture de mission garée au
   marqueur). *« J'ai laissé une caisse au parking. Va la chercher. »*
2. `OBJ_GOTO` `requireCar` marqueur 2. *« Passe prendre Marco au coin de la
   rue. »* Complétion → `EV_MARCO_JOIN` : Marco monte (flag passager + message
   *« Marco : Roule, au chantier ! »*).
3. `OBJ_GOTO` `requireCar` marqueur 3 (chantier). *« Conduis-moi au chantier. »*
   Complétion → `EV_MARCO_DIE` : le tueur apparaît, Marco meurt (*« Un type
   surgit… Marco s'effondre ! »*).
4. `OBJ_KILL` le tueur (rouge, **fonce sur le joueur** via `missionChaseStep`).
   *« Tu vas pas laisser passer ça. Bute-le. »* Fin : *« Justice est faite. »*

À la fin, la mission se termine et le téléphone n°2 se réarme.

## Narration (glue, gta.ino)

- File de messages courte (FIFO de quelques entrées) ; chaque message a une
  durée (frames).
- Affichage : **bandeau en bas de l'écran** via la police de la lib
  (`gb.display.print`, pas d'asset). Auto-timé (disparaît seul). Si le texte
  dépasse la largeur, il **défile horizontalement**.
- Sources de messages : narration d'objectif (à l'activation) + messages
  d'événement (`EV_*`).

## Marqueurs & HUD (glue)

- Marqueur de destination : pilier clignotant coloré dessiné au sol à la
  position de l'objectif courant (réutilise le style procédural du téléphone).
  Affiché pour les objectifs `OBJ_GOTO`/`OBJ_ENTER_CAR`.
- Flèche HUD existante (`missionArrowVec` + `drawMissionArrow`) : réutilisée pour
  pointer la position de l'objectif courant (destination ou cible).

## Placement (glue)

- Coordonnées des deux téléphones et des marqueurs : **fixes**, choisies sur des
  tuiles valides de la map générée (`citymap_data.cpp`, statique → stable). À
  choisir lors de l'implémentation en vérifiant la nature des tuiles
  (trottoir pour téléphone/marqueur à pied, route/parking pour la voiture de
  mission et les marqueurs `requireCar`).
- Téléphones inactifs (déjà accomplis ou mission en cours) : un seul téléphone
  sonne/est décrochable à la fois ? → un seul `missionState` global actif :
  décrocher un téléphone démarre sa mission ; les autres restent silencieux tant
  qu'une mission est en cours.

## Tests host (tools/host_test/test_mission_host.cpp, étendu)

Logique pure testée :

- `missionObjectiveDone` pour chaque type (GOTO dans/hors rayon, GOTO
  `requireCar` selon `driving`, ENTER_CAR, KILL).
- `missionAdvance` : enchaînement des objectifs, événement renvoyé à chaque
  transition, fin de mission au dernier objectif.
- `missionChaseExit` : choisit bien la sortie qui rapproche du joueur (miroir des
  tests de fuite existants).
- Conservation des tests existants (ligne de vue, fuite, flèche HUD).

## Risques / points d'attention

- **RAM/flash** : structures `const` en flash, peu de RAM ajoutée (un
  `MissionRun`, un flag passager, la file de narration). À surveiller mais
  marginal.
- **Coords fixes vs map générée** : si la map est régénérée, les coords doivent
  être revérifiées. Documenter les coords choisies dans `gta.ino`.
- **Parité** : la mission A doit reproduire le comportement actuel de Joe
  (fuite/ligne de vue) une fois portée sur le moteur.
