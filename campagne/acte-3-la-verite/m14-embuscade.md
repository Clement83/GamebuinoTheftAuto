# M14 — Embuscade

*ACTE III — La vérité (M12–M15) · Sarah, les preuves*

> Script fidèle à l'implémentation (`OBJS_M14`, `game_state.h`).

- **Entrée** — `MISSIONS[28]`, `STORY_SEQ[13]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 13`).
- **Prime** — **450 $**.
- **Échec sélectif** — **Sarah est une alliée DÉFENDUE** (`failOnAllyDeath`,
  `MISSIONS[28]`) : entité stationnaire posée à la Planque (magenta,
  `SARAH_COLOR`, `ALLY_HP = 5`). Les assaillants les plus proches d'**elle** la
  ciblent ; le joueur doit **s'interposer**. **Si elle tombe → MISSION RATÉE**
  (*« Sarah est morte ! Mission ratée. »*). Même mécanique que Tony en M8
  (`missionAllyDeathFail` / `hurtAlly`).

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Planque *(rayon 14)*
- **Narration** — *« Sarah : ils nous ont trouvés ! Reviens à la planque,
  vite ! »*
- **Action joueur** — rejoindre le marqueur à la Planque.
- **Atteint** — *« Les hommes de Victor encerclent la planque. »*

### 2. `OBJ_BEAT` → 1re vague *(3 × `EK_THUG`, `SP_PRESENT`)*
- **Narration** — *« Défends Sarah : première vague ! »*
- **Script** — 3 gros bras visibles d'emblée, qui foncent.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Ils refluent... non, d'autres arrivent ! »*

### 3. `OBJ_BEAT` → 2e vague *(2 × `EK_GUNNER`, `SP_AMBUSH`)*
- **Narration** — *« Des tireurs, cette fois. Tiens bon ! »*
- **Script** — 2 **tireurs** passifs jusqu'à l'approche, puis tirent.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Sarah : je sais où il garde les dossiers. Ses Bureaux. »*

## Clôture
→ `finishMission()` : **+450 $**, **MISSION ACCOMPLIE**, `campaignStep → 14`.
