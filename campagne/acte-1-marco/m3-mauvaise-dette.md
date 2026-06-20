# M3 — Mauvaise dette

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

> Script fidèle à l'implémentation (`OBJS_M3`, `game_state.h` ; glue
> `mod_mission.h`).

- **Entrée** — `MISSIONS[18]`, `STORY_SEQ[2]`, **6 objectifs**. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 2`).
  Décrocher → `startMission(18)`.
- **Prime** — **180 $**.
- **Marco compagnon physique** — comme M1/M2, Marco est désormais une **vraie
  entité** : on **va le chercher** au Garage, il **te suit à pied** sur le coup,
  et on le **ramène** au Garage à la fin. Il est **invulnérable** ici (pas de
  `failOnAllyDeath`) : les assaillants ne ciblent que le joueur.
- **Échec** — **aucun** (pas de mort d'allié fatale ; le payeur fuit mais la
  mission ne peut pas échouer).

## Setup au décrochage
Coords POI résolues, tout à pied. Bandeau *« Mauvaise dette »*.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Garage *(rayon 12)*
- **Narration** — *« Un mauvais payeur fait le mort. Marco veut un exemple. Va le
  chercher au Garage. »*
- **Action joueur** — aller **chercher Marco** au Garage.

### 2. `OBJ_TALK` → Marco *(rayon 8)* — `EV_MARCO_JOIN`
- **Narration** — *« Marco : deux secondes petit, j'arrive ! »*
- **Script** — Marco te rejoint et **te suit à pied** (`marcoFollow`).
- **Atteint** — *« Marco : ce gars-la rit de nous depuis trop longtemps.
  Suis-moi. »*

### 3. `OBJ_GOTO` → Chinatown *(rayon 16)*
- **Narration** — *« Le payeur se planque a Chinatown, entoure de ses gros bras.
  Approche. »*
- **Action joueur** — rejoindre le marqueur à Chinatown.

### 4. `OBJ_BEAT` → ses hommes de main *(2 × `EK_THUG`, `SP_PRESENT`, taunt)*
- **Face-à-face** — flag `taunt = 1` : à l'approche, `startTauntCut` joue les
  répliques `ambushTauntLines` keyées *« Mauvaise dette »* —
  *« Marco : tu sais pourquoi on est la. Paie tes dettes. »* puis
  *« Le payeur : ...Bossez-le, les gars ! »*, puis la bagarre démarre.
- **Narration** — *« Ecarte ses hommes de main. »*
- **Script** — 2 gros bras posés et **visibles d'emblée** (`SP_PRESENT`). Marco
  présent mais **invulnérable** (ennemis ciblent le joueur).
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« La voie est libre. Reste le payeur. »*

### 5. `OBJ_KILL` → le payeur *(count 1, fugitif)*
- **Narration** — *« Il detale ! Rattrape-le. »*
- **Script** — `count == 1` ; la cible **détale** en ligne de vue
  (`missionFleeStep`) sans contre-attaquer.
- **Action joueur** — la **rattraper et l'achever** (poing, arme, ou écrasement
  voiture).
- **Complétion** — cible morte.
- **Atteint** — *« Dette reglee. Marco : il s'en souviendra... s'il s'en souvient
  encore. »*

### 6. `OBJ_GOTO` → Le Garage *(rayon 14)* — `EV_MARCO_LEAVE`
- **Narration** — *« C'est fait. Ramene Marco au Garage. »*
- **Action joueur** — **ramener Marco** au Garage ; cinématique de départ → fin.

## Clôture
→ `finishMission()` : **+180 $**, **MISSION ACCOMPLIE**, `campaignStep → 3`
(prochaine mission = M4, `STORY_SEQ[3] == 1`).
