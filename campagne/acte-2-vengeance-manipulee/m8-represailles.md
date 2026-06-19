# M8 — Représailles

*ACTE II — La vengeance manipulée (M5–M11) · fausse piste, les Loups*

> Script fidèle à l'implémentation (`OBJS_M8`, `game_state.h`).

- **Entrée** — `MISSIONS[22]`, `STORY_SEQ[7]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 7`).
- **Prime** — **350 $**.
- **Échec** — aucun. **Tony n'est pas une entité jouable** : il n'y a pas
  d'allié à protéger ni de condition d'échec « si Tony meurt ». La mission est une
  **double vague d'ennemis** au Garage, purement narrée comme une défense.
- **Conseil de jeu** — première vraie bataille rangée : passer par **AMU Nation**
  avant pour s'armer.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Garage *(rayon 14)*
- **Narration** — *« Tony : ils ont retrouvé mon Garage ! Ramène-toi, et arme-toi
  en chemin. »*
- **Action joueur** — rejoindre le marqueur au Garage.
- **Atteint** — *« Les Loups débarquent. Tiens bon. »*

### 2. `OBJ_BEAT` → 1re vague *(3 × `EK_THUG`, `SP_PRESENT`)*
- **Narration** — *« Première vague : repousse-les ! »*
- **Script** — 3 gros bras visibles d'emblée, qui foncent.
- **Action joueur** — les mettre au tapis.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Ça se calme... non, ils reviennent ! »*

### 3. `OBJ_BEAT` → 2e vague *(2 × `EK_GUNNER`, `SP_AMBUSH`)*
- **Narration** — *« Deuxième vague, et ils sont armés. Couvre-toi ! »*
- **Script** — 2 **tireurs** posés mais **passifs** jusqu'à l'approche
  (`SP_AMBUSH`) ; réveillés, ils tirent en ligne de vue.
- **Action joueur** — neutraliser les 2 tireurs.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Tony : ils deviennent nerveux. Merci, petit. »*

## Clôture
→ `finishMission()` : **+350 $**, **MISSION ACCOMPLIE**, `campaignStep → 8`.
