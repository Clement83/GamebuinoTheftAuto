# M10 — L'entrepôt

*ACTE II — La vengeance manipulée (M5–M11) · fausse piste, les Loups*

> Script fidèle à l'implémentation (`OBJS_M10`, `game_state.h`).

- **Entrée** — `MISSIONS[24]`, `STORY_SEQ[9]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 9`).
- **Prime** — **400 $**.
- **Échec** — aucun.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Les Quais *(rayon 16)*
- **Narration** — *« Tony : on a trouvé le stock des Loups, aux entrepôts des
  Quais. »*
- **Action joueur** — rejoindre le marqueur aux Quais.
- **Atteint** — *« Des gardes verrouillent l'entrée. »*

### 2. `OBJ_KILL` → gardes de la porte *(2 × `EK_GUNNER`, `SP_PRESENT`)*
- **Narration** — *« Force le passage : deux gardes armés à la porte. »*
- **Script** — `enemyCount == 2`, aucune cible nommée : KILL rempli quand les 2
  **tireurs** sont à terre (visibles d'emblée).
- **Action joueur** — neutraliser les 2 tireurs.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Entrée dégagée. »*

### 3. `OBJ_KILL` → nettoyer l'entrepôt *(3 × `EK_THUG`, `SP_AMBUSH`)*
- **Narration** — *« Nettoie l'entrepôt. Que personne ne ressorte. »*
- **Script** — 3 gros bras posés mais **passifs** (`SP_AMBUSH`) jusqu'à
  l'approche, puis foncent. KILL rempli quand tous à terre.
- **Action joueur** — les neutraliser tous.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Le patron des Loups va nous tuer pour ça... tant pis. »*

## Clôture
→ `finishMission()` : **+400 $**, **MISSION ACCOMPLIE**, `campaignStep → 10`
(prochaine : le boss Rico).
