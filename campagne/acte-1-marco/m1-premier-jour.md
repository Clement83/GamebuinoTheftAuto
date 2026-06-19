# M1 — Premier jour

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

> Script fidèle à l'implémentation (`OBJS_M1`, `game_state.h` ; glue
> `mod_mission.h`). Décrit chaque objectif moteur, la narration exacte, les
> actions du joueur et le script déclenché.

- **Entrée** — `MISSIONS[16]`, `STORY_SEQ[0]` (1re mission de trame). `isStory`.
- **Déclencheur** — téléphone **rouge** à la **Planque**. Au lancement du jeu,
  `campaignStep == 0` : la cabine rouge sonne. Décrocher (A à portée) →
  `startMission(16)`.
- **Prime** — **120 $** (créditée à `finishMission`, puis `campaignStep++`).
- **Échec** — aucun (pas de chrono, pas d'allié tuable ici).

## Setup au décrochage (`startMission`)

- Les coords POI des 4 objectifs sont résolues (`buildMissionRuntime`).
- Objectif 2 est `OBJ_TALK`/`EV_MARCO_JOIN` **sans `requireCar`** → la **caisse
  de mission** (orange) est garée **dès maintenant** sur une case de route près
  du Garage (`findRoadSpotNear`). Elle attend le joueur, elle ne « pop » pas.
- Bandeau : *« Premier jour »* puis narration de l'objectif 1.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Garage *(à pied, rayon 12)*
- **Narration** — *« Premier jour. Marco, le bras droit du patron, t'attend au
  Garage. Vas-y à pied. »*
- **Action joueur** — marcher jusqu'au marqueur clignotant au Garage. Flèche HUD
  vers la destination. `requireCar` faux (à pied, mais la voiture n'est pas
  interdite).
- **Complétion** — joueur dans un rayon de **12 px** du Garage.

### 2. `OBJ_TALK` → Marco *(rayon 8)* — `EV_MARCO_JOIN`
- **Narration (activation)** — *« Marco : deux secondes petit, j'arrive ! »*
- **Script** — `allyColor = MARCO` (cyan, car `count != 1`). `marcoWaiting`.
  Marco est posé sur la case marchable la plus proche mais **reste dans
  l'immeuble** pendant `MARCO_EMERGE_DELAY` frames (le temps de la réplique),
  **puis sort** et marche jusqu'au marqueur (`marcoUpdate`).
- **Action joueur** — rester sur place / s'approcher du marqueur et regarder
  Marco sortir.
- **Complétion** — Marco a fini d'émerger **et** atteint son poste (à ≤ 2 px),
  **et** le joueur est dans les **8 px**. Tant que Marco marche encore,
  l'objectif **ne valide pas** (on doit le voir arriver).
- **Narration (atteint)** — *« Marco : la caisse est garée à côté. Embarque, on
  a un colis à livrer. »*
- **Transition** `EV_MARCO_JOIN` (à pied) → `marcoFollow = true` : Marco nous
  **emboîte le pas** à distance de confort (`marcoUpdate`).
- **Compagnon physique** — une fois allié, Marco monte quand on monte
  (`marcoAboard`) **et descend quand on descend** (MENU) : il est reposé à pied à
  côté de la caisse et reprend la filature (`gta.ino`, sortie MENU). Il remonte
  au prochain embarquement. Comportement générique des missions à allié (M1, M4,
  M13).

### 3. `OBJ_GOTO` → Les Quais *(`requireCar`, rayon 16)*
- **Narration** — *« En route pour les Quais. Roule peinard, attire pas les
  flics. »*
- **Action joueur** — monter dans la **caisse de mission** garée à côté
  (devient la voiture du joueur, `carIsMission`). À l'embarquement Marco monte
  passager (`marcoAboard`), narration *« Marco : roule, je monte derrière. »*
  Conduire jusqu'aux Quais.
- **Complétion** — voiture dans les **16 px** des Quais **en conduisant**.
- **Narration (atteint)** — *« Colis livré. Marco : nickel. Maintenant ramène-moi
  chez moi, petit. »*

### 4. `OBJ_GOTO` → Le Garage *(`requireCar`, rayon 14)* — `EV_MARCO_LEAVE`
- **Narration** — *« Ramène Marco au Garage. »*
- **Action joueur** — reconduire Marco au Garage.
- **Complétion** — voiture dans les **14 px** du Garage en conduisant →
  déclenche l'événement de fin.

## Cinématique de clôture (`CUT_MARCO_LEAVE`, joueur figé)

1. La caisse s'arrête net. Marco descend à côté (`findFootSpot`).
2. Marco marche vers sa porte (coords du POI Garage).
3. Arrivé : *« Marco : bon boulot pour un premier jour. Repose-toi, petit. »*
4. Il disparaît dans le bâtiment → `finishMission()` : **+120 $**, bandeau
   **MISSION ACCOMPLIE**, `campaignStep` passe à 1 → le téléphone rouge se
   réarme pour M2.
