# M6 — Message aux Loups

*ACTE II — La vengeance manipulée (M5–M11) · fausse piste, les Loups*

> Script fidèle à l'implémentation (`OBJS_M6`, `game_state.h`).

- **Entrée** — `MISSIONS[20]`, `STORY_SEQ[5]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 5`).
- **Prime** — **250 $**.
- **Échec** — aucun.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Chinatown *(rayon 16)*
- **Narration** — *« Inconnu : envoie-leur un message. Va dans le quartier des
  Loups. »*
- **Action joueur** — rejoindre le marqueur à Chinatown (quartier des Loups).
- **Atteint** — *« Les voilà, accoudés au mur. Ils t'ont vu. »*

### 2. `OBJ_BEAT` → trois Loups *(3 × `EK_THUG`, `SP_PRESENT`)*
- **Narration** — *« Mets trois Loups au tapis. Qu'ils comprennent. »*
- **Script** — 3 gros bras posés et **visibles d'emblée**, qui foncent dès qu'on
  approche.
- **Action joueur** — mettre les **3** au sol.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Ça devrait attirer leur attention. Inconnu : bien joué. »*

## Clôture
→ `finishMission()` : **+250 $**, **MISSION ACCOMPLIE**, `campaignStep → 6`.
