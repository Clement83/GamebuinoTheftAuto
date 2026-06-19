# M13 — Le témoin

*ACTE III — La vérité (M12–M15) · Sarah, les preuves*

> Script fidèle à l'implémentation (`OBJS_M13`, `game_state.h`). Réutilise la
> mécanique d'allié de Marco, **avec Sarah** : `EV_MARCO_JOIN` + `count == 1`
> bascule `allyColor` sur **SARAH** (magenta) au lieu de cyan.

- **Entrée** — `MISSIONS[27]`, `STORY_SEQ[12]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 12`).
- **Prime** — **400 $**.
- **Échec** — aucun (Sarah n'a pas de PV propres : pas de fail « si Sarah
  meurt » ; pas d'embuscade en route dans l'implémentation).

## Setup au décrochage
L'objectif 2 est `OBJ_TALK`/`EV_MARCO_JOIN` **sans `requireCar`** → la **caisse
de mission** est garée près du Bar dès le décrochage (comme M1). Bandeau *« Le
témoin »*.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Bar *(à pied, rayon 12)*
- **Narration** — *« Sarah, la journaliste, t'attend au Bar. Vas-y à pied. »*
- **Action joueur** — rejoindre le marqueur au Bar.
- **Atteint** — *(aucun `doneText`.)*

### 2. `OBJ_TALK` → Sarah *(rayon 8, count 1)* — `EV_MARCO_JOIN`
- **Narration** — *« Sarah : Victor a peur, il efface les preuves. Sors-moi
  d'ici. »*
- **Script** — `count == 1` → `allyColor = SARAH` (magenta). Sarah émerge près du
  Bar et marche jusqu'au marqueur (`marcoWaiting` + `marcoEmergeDelay`).
- **Action joueur** — s'approcher et la laisser arriver.
- **Complétion** — Sarah a fini d'émerger et atteint son poste **et** joueur
  ≤ 8 px. À la validation, à pied → `marcoFollow = true` : Sarah nous suit.
- **Atteint** — *« Sarah monte. Direction la planque, et vite. »*

### 3. `OBJ_GOTO` → Planque *(`requireCar`, rayon 16)*
- **Narration** — *« Conduis Sarah à la planque. Reste sur tes gardes. »*
- **Action joueur** — monter dans la caisse de mission (Sarah embarque,
  `marcoAboard`, *« Sarah : vite, démarre ! »*) et conduire jusqu'à la Planque.
- **Complétion** — voiture dans les 16 px de la Planque.
- **Atteint** — *« Sarah est à l'abri. Pour l'instant. »*

## Clôture
→ `finishMission()` : **+400 $**, **MISSION ACCOMPLIE**, `campaignStep → 13`.
