# M18 — Le dernier appel *(boss final)*

*ACTE IV — Victor (M16–M18 + épilogue)*

> Script fidèle à l'implémentation (`OBJS_M18`, `game_state.h` ; taunt de boss
> `bossTauntLines` / `startTauntCut`). Boss le plus coriace de la campagne.

- **Entrée** — `MISSIONS[32]`, `STORY_SEQ[17]` (dernière mission de trame).
  `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 17`).
- **Prime** — **800 $**.
- **Échec** — aucun.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Casino *(rayon 16)*
- **Narration** — *« Victor : je t'attends au Casino. Viens donc, petit. »*
- **Action joueur** — rejoindre le marqueur au Casino.
- **Atteint** — *« Victor : tu es allé trop loin. Messieurs, occupez-vous de
  lui. »*

### 2. `OBJ_KILL` → gardes du corps *(4 × `EK_GUNNER`, `SP_PRESENT`)*
- **Narration** — *« Ses gardes du corps t'attaquent ! Ouvre-toi un chemin
  jusqu'à Victor. »*
- **Script** — `enemyCount == 4` (= `MAX_ENEMIES`), aucune cible nommée ; 4
  **tireurs** visibles d'emblée. KILL rempli quand tous à terre.
- **Action joueur** — neutraliser les 4 tireurs (le pic d'opposition du jeu).
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« La voie est libre. Reste Victor. »*

### 3. `OBJ_KILL` → Victor *(count 7 = boss final, `targetHp = 7`)*
- **Script (à l'activation)** — boss posé via `spawnTargetAt` (AGGRO, fonce et
  frappe). À pied, titre « Le dernier appel » → **face-à-face** (`startTauntCut`,
  figé) :
  - *« Victor : tu as du cran de venir jusqu'ici. »*
  - *« Victor : ça ne te sauvera pas. Comme Marco. »*
  Fin du taunt → Victor charge.
- **Narration (objectif)** — *« Victor est coriace et bien protégé. Acharne-toi. »*
- **Action joueur** — lui porter **7 coups** (le boss le plus résistant).
- **Complétion** — Victor mort → **death-beat** final (`CUT_BOSS_DOWN`, joueur
  figé sur le corps) avant la bannière de victoire et l'épilogue.
- **Atteint (death-beat)** — *« Tout ça... pour un ami. C'est fini, Victor. »*
  (`OBJ_KILL` `count >= 5`).

## Clôture
→ `finishMission()` : **+800 $**, **MISSION ACCOMPLIE**, `campaignStep → 18`
(= `STORY_LEN`). Le téléphone rouge sonne une **dernière** fois → **Épilogue**.
