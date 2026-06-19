# M5 — Un nom

*ACTE II — La vengeance manipulée (M5–M11) · fausse piste, les Loups*

> Script fidèle à l'implémentation (`OBJS_M5`, `game_state.h`).

- **Entrée** — `MISSIONS[19]`, `STORY_SEQ[4]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 4`).
- **Prime** — **200 $**.
- **Échec** — aucun.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Bar *(à pied, rayon 12)*
- **Narration** — *« Inconnu : tu veux savoir pour Marco ? Trouve Nico. Il traîne
  au Bar. »*
- **Action joueur** — rejoindre le marqueur au Bar.
- **Atteint** — *(aucun `doneText` : enchaîne directement sur Nico.)*

### 2. `OBJ_SUBDUE` → Nico *(count 3)*
- **Narration** — *« Nico fait le malin et te bouscule. Secoue-le, sans le tuer :
  il parlera. »*
- **Script** — cible nommée posée près du Bar (`T_EMERGE`), sort, **fonce et
  frappe** sans mourir (branche `OBJ_SUBDUE`).
- **Action joueur** — le **tabasser au poing à pied** (3 coups portés,
  `objSubdue`).
- **Complétion** — `objSubdue >= 3`.
- **Atteint** — *« Nico : les Loups cherchent un type depuis des semaines...
  c'est eux, sûrement. »*

## Clôture
→ `finishMission()` : **+200 $**, **MISSION ACCOMPLIE**, `campaignStep → 5`.
