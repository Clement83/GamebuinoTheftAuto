# M11 — Rico le Loup *(retournement)*

*ACTE II — La vengeance manipulée (M5–M11) · fausse piste, les Loups*

> Script fidèle à l'implémentation (`OBJS_M11`, `game_state.h` ; taunt de boss
> `bossTauntLines` / `startTauntCut`). Pivot narratif : premier **boss à PV
> élevés**.

- **Entrée** — `MISSIONS[25]`, `STORY_SEQ[10]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 10`).
- **Prime** — **450 $**.
- **Échec** — aucun (Rico est un KILL **non poursuivant** : pas de fuite hors map).

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Chantier *(rayon 16)*
- **Narration** — *« Tony : Rico, le lieutenant des Loups, se terre à l'ancienne
  usine. Finis-le. »*
- **Action joueur** — rejoindre le marqueur au Chantier.
- **Atteint** — *« Ses hommes te coupent la route. »*

### 2. `OBJ_BEAT` → garde rapprochée *(2 × `EK_GUNNER`, `SP_PRESENT`)*
- **Narration** — *« Écarte sa garde rapprochée. »*
- **Script** — 2 **tireurs** visibles d'emblée.
- **Action joueur** — les neutraliser.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Reste Rico. Il ne tombera pas en un coup. »*

### 3. `OBJ_KILL` → Rico *(count 5 = boss, `targetHp = 5`)*
- **Script (à l'activation)** — `count > 1` → boss : `targetHp = 5`, posé via
  `spawnTargetAt` (AGGRO, **fonce et frappe**). Comme on est **à pied** et que le
  titre est « Rico le Loup », un **face-à-face** se déclenche
  (`startTauntCut`, joueur figé) :
  - *« Rico : Tony t'envoie crever à ma place, gamin ? »*
  - *« Rico : approche, que je t'apprenne le respect. »*
  Fin du taunt → Rico charge.
- **Narration (objectif)** — *« Rico est coriace. Acharne-toi. »*
- **Action joueur** — lui porter **5 coups** (encaisse plusieurs frappes avant de
  tomber).
- **Complétion** — Rico mort → **death-beat** (`CUT_BOSS_DOWN`, joueur figé sur
  le corps ~4,5 s) pendant que défile la révélation, avant la bannière de fin.
- **Atteint (death-beat)** — *« Rico, à terre : tu crois qu'on a tué Marco ? Tu
  bosses pour le vrai coupable... »* (le retournement : la vengeance était
  manipulée). Déclenché car `OBJ_KILL` `count >= 5`.

## Clôture
→ `finishMission()` : **+450 $**, **MISSION ACCOMPLIE**, `campaignStep → 11`
(ouverture de l'Acte III).
