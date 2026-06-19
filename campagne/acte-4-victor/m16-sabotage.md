# M16 — Sabotage *(intègre le broyeur de La Casse)*

*ACTE IV — Victor (M16–M18 + épilogue)*

> Script fidèle à l'implémentation (`OBJS_M16`, `game_state.h`). Réutilise le
> **broyeur de La Casse** comme objectif (`OBJ_CRUSH`).

- **Entrée** — `MISSIONS[30]`, `STORY_SEQ[15]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 15`).
- **Prime** — **550 $**.
- **Échec sélectif** — chaque **voiture de luxe est un véhicule requis**
  (`failOnCarLoss`, `MISSIONS[30]`) : si elle **explose avant le broyeur** (prise
  de feu, puis bail → épave qui pète sur la route), → **MISSION RATÉE** (*« La
  caisse est détruite ! »*). Le **broyage volontaire** ne déclenche rien : il pose
  `crushDone` et efface `carIsMission` (cf. `missionCarLossFail`, `mission.h`).
  Exploser **au volant** te tue (échec par la séquence MORT).

## Objectifs (séquence moteur)

### 1. `OBJ_ENTER_CAR` → Chinatown
- **Narration** — *« Tony : on va lui faire mal au portefeuille. Vole sa voiture
  de luxe, à Chinatown. »*
- **Script** — `mCar` posée aux coords de Chinatown.
- **Action joueur** — aller à pied jusqu'à la caisse et **monter dedans**.
- **Complétion** — `inMissionCar`.

### 2. `OBJ_CRUSH` → La Casse *(`requireCar`, rayon 14)*
- **Narration** — *« Amène-la à la Casse. Descends près de la grue et fais-la
  BROYER. »*
- **Action joueur** — conduire la voiture de luxe jusqu'à La Casse, s'arrêter
  près de la grue, **descendre** ; la séquence de broyage de La Casse
  (`SEQ_CRUSH`) écrase l'épave.
- **Complétion** — `missionCrushDone` (la voiture de mission vient d'être broyée).
- **Atteint** — *« Une de moins. Tony : ça pique, hein Victor ? »*

### 3. `OBJ_ENTER_CAR` → Le Casino
- **Narration** — *« Encore une, garée devant le Casino. »*
- **Script** — `mCar` posée aux coords du Casino.
- **Action joueur** — monter dans la seconde voiture de luxe.
- **Complétion** — `inMissionCar`.

### 4. `OBJ_CRUSH` → La Casse *(`requireCar`, rayon 14)*
- **Narration** — *« Rebelote : au broyeur, et reste près de la grue jusqu'au
  bout. »*
- **Action joueur** — reconduire à La Casse et faire broyer la seconde caisse.
- **Complétion** — `missionCrushDone`.
- **Atteint** — *« Tony : ça va le rendre fou. »*

## Clôture
→ `finishMission()` : **+550 $**, **MISSION ACCOMPLIE**, `campaignStep → 16`.
