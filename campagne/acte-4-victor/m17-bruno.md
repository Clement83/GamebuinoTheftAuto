# M17 — Bruno

*ACTE IV — Victor (M16–M18 + épilogue)*

> Script fidèle à l'implémentation (`OBJS_M17`, `game_state.h` ; taunt de boss
> `bossTauntLines` / `startTauntCut`).

- **Entrée** — `MISSIONS[31]`, `STORY_SEQ[16]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 16`).
- **Prime** — **600 $**.
- **Échec** — aucun (Bruno est un KILL non poursuivant).

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → La Casse *(rayon 16)*
- **Narration** — *« Tony : Bruno, l'homme de Victor, surveille la Casse.
  Descends-le. »*
- **Action joueur** — rejoindre le marqueur à La Casse.
- **Atteint** — *« Bruno et ses hommes t'attendent. »*

### 2. `OBJ_KILL` → ses gardes *(2 × `EK_THUG`, `SP_PRESENT`)*
- **Narration** — *« Ses gardes d'abord. »*
- **Script** — `enemyCount == 2`, aucune cible nommée ; 2 gros bras visibles
  d'emblée. KILL rempli quand tous à terre.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Reste Bruno. Il ne lâchera pas facilement. »*

### 3. `OBJ_KILL` → Bruno *(count 5 = boss, `targetHp = 5`)*
- **Script (à l'activation)** — boss posé via `spawnTargetAt` (AGGRO, fonce et
  frappe). À pied, titre « Bruno » → **face-à-face** (`startTauntCut`, figé) :
  - *« Bruno : tu es allé trop loin. Victor veut ta tête. »*
  - *« Bruno : et c'est moi qui vais la lui porter. »*
  Fin du taunt → Bruno charge.
- **Narration (objectif)** — *« Bruno encaisse. Ne le lâche pas. »*
- **Action joueur** — lui porter **5 coups**.
- **Complétion** — Bruno mort.
- **Atteint** — *« Bruno, mourant : le vieux casino... Victor t'y attend. »*

## Clôture
→ `finishMission()` : **+600 $**, **MISSION ACCOMPLIE**, `campaignStep → 17`
(prochaine : le boss final, Victor).
