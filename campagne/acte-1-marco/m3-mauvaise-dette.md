# M3 — Mauvaise dette

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

> Script fidèle à l'implémentation (`OBJS_M3`, `game_state.h` ; glue
> `mod_mission.h`).

- **Entrée** — `MISSIONS[18]`, `STORY_SEQ[2]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 2`).
  Décrocher → `startMission(18)`.
- **Prime** — **180 $**.
- **Échec** — aucun (le payeur fuit mais ne peut pas « s'échapper » : seul un
  KILL en mode *poursuite armée* peut faire échouer, ce qui n'est pas le cas ici).
- **Note** — introduit la **poursuite à pied** d'une cible qui fuit en ligne de
  vue, avant la poursuite en voiture de M4.

## Setup au décrochage
Coords POI résolues, tout à pied. Bandeau *« Mauvaise dette »*.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Chinatown *(rayon 16)*
- **Narration** — *« Un mauvais payeur se planque à Chinatown. Marco veut un
  exemple. »*
- **Action joueur** — rejoindre le marqueur à Chinatown.
- **Atteint** — *« Il a payé des gros bras pour te recevoir. »*

### 2. `OBJ_BEAT` → ses hommes de main *(2 × `EK_THUG`, `SP_PRESENT`)*
- **Narration** — *« Écarte ses hommes de main. »*
- **Script** — 2 gros bras posés et **visibles d'emblée** (`SP_PRESENT`), qui
  foncent dès qu'on approche (`ENEMY_AGGRO_RANGE`).
- **Action joueur** — les mettre au tapis tous les deux.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« La voie est libre. Reste le payeur. »*

### 3. `OBJ_KILL` → le payeur *(count 1, fugitif)*
- **Narration** — *« Il détale ! Rattrape-le. »*
- **Script** — `count == 1`, `killerChase` faux → la cible est posée près du POI
  via `spawnTargetWanderNear` : elle **erre puis fuit** en ligne de vue
  (`missionFleeStep`), elle ne contre-attaque pas. `targetHp = 1`.
- **Action joueur** — la **rattraper et l'achever** (poing, arme, ou écrasement
  voiture).
- **Complétion** — cible morte (`target.active == false`) et aucun ennemi debout.
- **Atteint** — *« Dette réglée. Marco : il s'en souviendra... s'il s'en souvient
  encore. »*

## Clôture
→ `finishMission()` : **+180 $**, **MISSION ACCOMPLIE**, `campaignStep → 3`
(prochaine mission = M4, `STORY_SEQ[3] == 1`).
