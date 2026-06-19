# M15 — Les dossiers

*ACTE III — La vérité (M12–M15) · Sarah, les preuves*

> Script fidèle à l'implémentation (`OBJS_M15`, `game_state.h`). Les
> « dossiers » sont **narratifs** (pas de `OBJ_PICKUP`) : on les obtient en
> nettoyant le hall puis en forçant la sortie.

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
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Tu tiens les dossiers. Maintenant, sors. »*

### 3. `OBJ_BEAT` → renforts à la sortie *(3 × `EK_THUG`, `SP_AMBUSH`)*
- **Narration** — *« Des renforts te coupent la sortie. Force le passage. »*
- **Script** — 3 gros bras passifs jusqu'à l'approche, puis foncent.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Sarah : 'Victor a ordonné l'assassinat de Marco.' On le
  tient. »*

## Clôture
→ `finishMission()` : **+500 $**, **MISSION ACCOMPLIE**, `campaignStep → 15`
(ouverture de l'Acte IV).
