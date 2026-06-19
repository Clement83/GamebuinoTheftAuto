# M4 — Le dernier trajet *(pivot tragique)*

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

> Script fidèle à l'implémentation (`OBJS_DEAL` = `MISSION_DEAL`, `game_state.h` ;
> glue + cinématiques `mod_mission.h`).

- **Entrée** — `MISSIONS[1]` (`MISSION_DEAL`, réusiné depuis l'ancienne cabine
  « Mauvaise affaire »), `STORY_SEQ[3]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 3`).
  Décrocher → `startMission(1)`.
- **Prime** — **250 $**.
- **Échec** — **oui** : après la mort de Marco, si le **tueur** (KILL en mode
  poursuite) sort des limites du monde (`±8 px`) → `failMission("Le tueur s'est
  échappé.")`, bandeau **MISSION RATÉE**, `campaignStep` inchangé (on rejoue).

## Setup au décrochage
Coords POI résolues. L'objectif 1 est `OBJ_ENTER_CAR` : la caisse de mission
sera posée à son activation (pas en avance). Bandeau *« Le dernier trajet »*.

## Objectifs (séquence moteur)

### 1. `OBJ_ENTER_CAR` → Le Garage
- **Narration** — *« Marco : un dernier rendez-vous, ce soir. Prends la caisse au
  Garage. »*
- **Script** — `mCar` (caisse orange) posée aux coords du Garage (`mCarActive`).
- **Action joueur** — aller à pied au Garage et **monter dans la caisse de
  mission** (elle devient la voiture du joueur, `carIsMission`).
- **Complétion** — `inMissionCar`.

### 2. `OBJ_GOTO` → Le Garage *(`requireCar`, rayon 14)* — `EV_MARCO_JOIN`
- **Narration** — *« Passe prendre Marco devant le Garage. »*
- **Script** — `allyColor = MARCO` (count 0). Marco (cyan) émerge près du Garage.
- **Action joueur** — s'arrêter devant le Garage **en voiture**.
- **Complétion** — voiture dans les 14 px du Garage.
- **Transition** `EV_MARCO_JOIN` **en conduisant** → `marcoAboard = true` (Marco
  monte directement, pas de filature à pied).
- **Atteint** — *« Marco monte. Marco : direction le Chantier. »*

### 3. `OBJ_GOTO` → Le Chantier *(`requireCar`, rayon 16)* — `EV_MARCO_DIE`
- **Narration** — *« Conduis Marco au Chantier. Il est nerveux ce soir. »*
- **Action joueur** — conduire Marco jusqu'au Chantier.
- **Complétion** — voiture dans les 16 px du Chantier → déclenche la cinématique.
- **Atteint** — *« Le Chantier. Marco descend, méfiant... une silhouette
  l'attend dans l'ombre. »*

## Cinématique de mort (`CUT_MARCO_DEATH`, joueur figé)
À la complétion de l'objectif 3, `EV_MARCO_DIE` lance `startMarcoDeathCut()`
(l'objectif KILL suivant est **déjà actif** ; la scène l'arme elle-même) :

1. La caisse s'arrête net. Le tueur est posé au point de RDV (coords du KILL).
   Marco descend et **marche vers lui** (`npcWalkToward`).
2. Face-à-face : *« Marco : qu'est-ce que tu fous là, toi ? »*
3. *« L'autre : désolé, Marco. Rien de personnel. »*
4. **Coup de feu** : Marco s'effondre comme un PNJ (splat), *« Marco
   s'effondre. »*, le corps reste un moment.
5. Reprise de la main : `killerChase = true`, `target.chase = true`,
   *« Le tueur file ! Rattrape-le ! »*

### 4. `OBJ_KILL` → le tueur *(count 3, poursuite armée)*
- **Narration (déjà affichée par l'objectif)** — *« Le tueur fonce sur toi. Pas
  question de le laisser filer ! »*
- **Script** — pas de `enterObjective` (la cinématique a tout posé). `targetHp =
  3` (encaisse plusieurs coups). En mode `chase`, le tueur **poursuit le joueur
  et frappe** au corps-à-corps (`missionUpdate`).
- **Action joueur** — le **rattraper et l'abattre** avant qu'il ne sorte du
  monde (poing, arme, ou voiture lancée).
- **Complétion** — tueur mort.
- **Atteint** — *« Justice est faite. ...pour l'instant. »*

## Clôture
→ `finishMission()` : **+250 $**, **MISSION ACCOMPLIE**, `campaignStep → 4`
(ouverture de l'Acte II : le téléphone rouge re-sonne pour M5).
