# M7 — Voiture volée

*ACTE II — La vengeance manipulée (M5–M11) · fausse piste, les Loups*

> Script fidèle à l'implémentation (`OBJS_M7`, `game_state.h`). Premier dialogue
> signé **Tony** : l'« Inconnu » prend un nom.

- **Entrée** — `MISSIONS[21]`, `STORY_SEQ[6]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 6`).
- **Prime** — **350 $**.
- **Échec** — aucun (pas de chrono, pas de cible qui s'échappe).

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Les Quais *(rayon 18)*
- **Narration** — *« Tony - oui, l'Inconnu a un nom : une caisse des Loups dort
  aux Quais. »*
- **Action joueur** — rejoindre le marqueur aux Quais.
- **Atteint** — *« Deux gardes armés la surveillent. »*

### 2. `OBJ_KILL` → les deux gardes *(2 × `EK_GUNNER`, `SP_PRESENT`)*
- **Narration** — *« Occupe-toi des deux gardes. »*
- **Script** — `enemyCount == 2`, donc **aucune cible nommée** n'est posée : le
  KILL est rempli quand **tous les ennemis scénarisés sont à terre**. Deux
  **tireurs** posés et visibles : ils se rapprochent puis **font feu** en ligne
  de vue (`fireCopBullet`, balles esquivables).
- **Action joueur** — neutraliser les 2 tireurs (arme conseillée).
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« La voie est libre. La caisse est à toi. »*

### 3. `OBJ_ENTER_CAR` → la caisse des Loups *(aux Quais)*
- **Narration** — *« Embarque la caisse des Loups. »*
- **Script** — `mCar` posée aux coords des Quais.
- **Action joueur** — **monter dans la caisse de mission**.
- **Complétion** — `inMissionCar`.

### 4. `OBJ_GOTO` → Commissariat *(`requireCar`, rayon 18)*
- **Narration** — *« Planque-la derrière le Commissariat. Roule peinard. »*
- **Action joueur** — conduire la caisse jusqu'aux abords du Commissariat.
- **Complétion** — voiture dans les 18 px du Commissariat.
- **Atteint** — *« Tony : parfait. Tu montes en grade, petit. »*

## Clôture
→ `finishMission()` : **+350 $**, **MISSION ACCOMPLIE**, `campaignStep → 7`.
