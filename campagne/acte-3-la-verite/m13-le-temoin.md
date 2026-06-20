# M13 — Le témoin

*ACTE III — La vérité (M12–M15) · Sarah, les preuves*

> Script fidèle à l'implémentation (`OBJS_M13`, `game_state.h`). Réutilise la
> mécanique d'allié de Marco, **avec Sarah** : `EV_MARCO_JOIN` + `count == 1`
> bascule `allyColor` sur **SARAH** (magenta) au lieu de cyan. Depuis la refonte,
> l'escorte comporte une **embuscade en route** (5 objectifs, contre 3 avant).

- **Entrée** — `MISSIONS[27]`, `STORY_SEQ[12]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 12`).
- **Prime** — **400 $**.
- **Échec** — aucun. **PAS de `failOnAllyDeath`** sur M13 (contrairement à M14,
  où Sarah est défendue) : Sarah est ici **compagnon invulnérable embarqué**,
  elle n'a pas de PV propres et ne peut pas mourir. Le mécanisme d'allié défendu
  est **stationnaire** et entrerait en conflit avec une escorte **mobile** ;
  l'embuscade est donc une **montée de tension**, pas une condition d'échec.

## Setup au décrochage
L'objectif 2 est `OBJ_TALK`/`EV_MARCO_JOIN` **sans `requireCar`** → la **caisse
de mission** est garée près du Bar dès le décrochage (comme M1). Bandeau *« Le
témoin »*.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Bar *(à pied, rayon 12)*
- **Narration** — *« Sarah, la journaliste, t'attend au Bar. Vas-y a pied. »*
- **Action joueur** — rejoindre le marqueur au Bar.
- **Atteint** — *(aucun `doneText`.)*

### 2. `OBJ_TALK` → Le Bar *(rayon 8, count 1)* — `EV_MARCO_JOIN`
- **Narration** — *« Sarah : Victor a peur, il efface les preuves. Sors-moi
  d'ici. »*
- **Script** — `count == 1` → `allyColor = SARAH` (magenta). Sarah émerge près du
  Bar et marche jusqu'au marqueur (`marcoWaiting` + `marcoEmergeDelay`).
- **Action joueur** — s'approcher et la laisser arriver.
- **Complétion** — Sarah a fini d'émerger et atteint son poste **et** joueur
  ≤ 8 px. À pied → `marcoFollow = true` : Sarah nous suit.
- **Atteint** — *« Sarah monte. Direction la planque, et vite. »*

### 3. `OBJ_GOTO` → Chinatown *(`requireCar`, rayon 16)* — **NOUVEAU (embuscade en route)**
- **Narration** — *« Prends une caisse et file vers la planque en passant par
  Chinatown. »*
- **Action joueur** — monter dans la caisse de mission (Sarah embarque,
  `marcoAboard`) et rouler jusqu'au waypoint de Chinatown.
- **Complétion** — voiture dans les 16 px du waypoint Chinatown.
- **Atteint** — *« Une berline pile en travers de la route. Embuscade ! »*

### 4. `OBJ_KILL` → Chinatown *(count 2, `EK_GUNNER`, `SP_AMBUSH`)* — **NOUVEAU**
- **Narration** — *« Les hommes de Victor ont repere Sarah ! Ecarte-les. »*
- **Action joueur** — éliminer les **2 tireurs** de l'embuscade. Sarah, embarquée,
  reste invulnérable (aucun fail si elle est touchée).
- **Complétion** — 2 `EK_GUNNER` neutralisés.
- **Atteint** — *« La voie est libre. Sarah, tassee sur le siege : roule,
  roule ! »*

### 5. `OBJ_GOTO` → Planque *(`requireCar`, rayon 16)*
- **Narration** — *« Reprends la route vers la planque, vite ! »*
- **Action joueur** — remonter en caisse et conduire jusqu'à la Planque.
- **Complétion** — voiture dans les 16 px de la Planque.
- **Atteint** — *« Sarah est à l'abri. Pour l'instant. »*

## Clôture
→ `finishMission()` : **+400 $**, **MISSION ACCOMPLIE**, `campaignStep → 13`.
