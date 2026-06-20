# M15 — Les dossiers

*ACTE III — La vérité (M12–M15) · Sarah, les preuves*

> Script fidèle à l'implémentation (`OBJS_M15`, `game_state.h`). Les
> « dossiers » sont **narratifs** (pas de `OBJ_PICKUP`) : on les obtient via un
> beat de ramassage scripté (`OBJ_GOTO` rayon 10) après avoir nettoyé le hall,
> puis on force la sortie.

- **Entrée** — `MISSIONS[29]`, `STORY_SEQ[14]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 14`).
- **Prime** — **500 $**.
- **Échec** — aucun.
- **Note** — Les Bureaux : l'interaction libre (`bureauVisit`) n'est possible
  **qu'hors mission** ; pendant M15, l'entrée ne déclenche pas le braquage.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Les Bureaux *(rayon 14)*
- **Narration** — *« Les Bureaux de Victor. Les preuves sont à l'intérieur.
  Entre. »*
- **Action joueur** — rejoindre le marqueur aux Bureaux.
- **Atteint** — *« Des gardes patrouillent le hall. »*

### 2. `OBJ_KILL` → gardes du hall *(2 × `EK_GUNNER`, `SP_PRESENT`)*
- **Narration** — *« Neutralise les gardes du hall. »*
- **Script** — `enemyCount == 2`, aucune cible nommée ; 2 **tireurs** visibles
  d'emblée. KILL rempli quand tous à terre.
- **Face-à-face** — flag `taunt = 1` : à pied, le joueur est figé le temps de
  deux répliques (`ambushTauntLines`, keyé `"Les dossiers"`) avant que les
  gardes ne chargent.
  - *« Un garde : t'as rien a faire dans les Bureaux de M. Victor. »*
  - *« Il degaine. Les autres rappliquent. »*
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Hall degage. Les dossiers sont dans le coffre. »*

### 3. `OBJ_GOTO` → Les Bureaux *(rayon 10)*
- **Narration** — *« Rafle les dossiers de Victor. »*
- **Action joueur** — beat de ramassage scripté : rejoindre le marqueur dans
  les Bureaux pour rafler les dossiers.
- **Atteint** — *« Tu rafles les dossiers. Maintenant, sors. »*

### 4. `OBJ_BEAT` → renforts à la sortie *(3 × `EK_THUG`, `SP_AMBUSH`)*
- **Narration** — *« Des renforts te coupent la sortie. Force le passage. »*
- **Script** — 3 gros bras passifs jusqu'à l'approche, puis foncent.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Sarah : 'Victor a ordonné l'assassinat de Marco.' On le
  tient. »*

## Clôture
→ `finishMission()` : **+500 $**, **MISSION ACCOMPLIE**, `campaignStep → 15`
(ouverture de l'Acte IV).
