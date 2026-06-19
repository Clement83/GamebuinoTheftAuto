# M2 — Les assurances

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

> Script fidèle à l'implémentation (`OBJS_M2`, `game_state.h` ; glue
> `mod_mission.h`).

- **Entrée** — `MISSIONS[17]`, `STORY_SEQ[1]`, **8 objectifs**. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 1`).
- **Prime** — **150 $**.
- **Échec sélectif (règle racket)** — les stops coopératifs (Chinatown, Le Bar)
  posent un **client**. **Le tuer** (poing, balle ou voiture) → **MISSION RATÉE**
  (*« On rackette, on ne tue pas les clients ! »*) — `sceneNpcDead` capté par
  `missionProgress`, coup du joueur détecté dans `mod_combat.h`.
- **Marco compagnon physique** — Marco est désormais une **vraie entité** : on
  **va le chercher** au Garage, il **te suit à pied** toute la tournée, et on le
  **redépose** au Garage à la fin. Il est **invulnérable** ici (pas de
  `failOnAllyDeath`) : les assaillants ne ciblent que le joueur.

## Setup au décrochage
Coords POI résolues. **Pas de caisse** posée : M2 n'a aucun objectif
`requireCar`, donc la tournée se fait **à pied** (le `startMission` ne pose la
caisse-compagnon que si la mission demande de conduire — sinon le `SUBDUE` au
poing et les scènes à pied seraient cassés). Bandeau *« Les assurances »*.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Le Garage *(à pied, rayon 12)*
- **Narration** — *« Jour de tournée. Marco t'attend au Garage. Vas-y à pied le
  chercher. »*

### 2. `OBJ_TALK` → Marco *(rayon 8)* — `EV_MARCO_JOIN`
- **Narration** — *« Marco : deux secondes petit, j'arrive ! »*
- **Script** — Marco (cyan) émerge du Garage et te rejoint, puis **te suit à
  pied** (`marcoFollow`). S'il prend une caisse volée il monte/descend avec toi.
- **Atteint** — *« Marco : la tournée du loyer. Tu regardes et t'apprends.
  Suis-moi. »*

### 3. `OBJ_GOTO` → Les Commerces *(rayon 14)* — `EV_DELIVERY` (collecte démo)
- **Narration** — *« Premier client : les Commerces. Regarde Marco faire. »*
- **Scène de collecte** (`CUT_DELIVERY`, compagnon à pied) — Marco s'approche du
  commerçant, encaisse sans heurt.
- **Atteint** — *« Marco : tu vois ? Facile. ...Le suivant, lui, fait le
  difficile. »* (annonce le récalcitrant)

### 4. `OBJ_SUBDUE` → le commerçant récalcitrant *(count 3)*
- **Narration** — *« Ce commerçant-là refuse et te saute dessus. Mate-le, mais le
  tue pas. »*
- **Script** — cible nommée posée (`T_EMERGE`), sort, **fonce et frappe** sans
  mourir.
- **Action joueur** — le **tabasser au poing** (3 coups, `objSubdue`).
- **Complétion** — `objSubdue >= 3` : il cède.
- **Atteint** — *« Il crache l'argent. Marco : voilà comment on fait. »*

### 5. `OBJ_GOTO` → Chinatown *(rayon 14)* — `EV_DELIVERY` (collecte)
- **Narration** — *« Client suivant : une échoppe de Chinatown. »*
- **Scène de collecte** (`CUT_DELIVERY`, variante **compagnon à pied** : Marco
  part de ta position, marche jusqu'au client, échange, **revient vers toi**) :
  *« Marco : tu connais la chanson. Le loyer. »* → *« Le commerçant : ...tiens.
  C'est tout ce que j'ai. »*
- **Atteint** — *« Encaisse. Sans histoire, celui-là. »*
- **Fail racket** — frapper/tuer ce client avant la scène → MISSION RATÉE.

### 6. `OBJ_GOTO` → Le Bar *(rayon 14)* — `EV_DELIVERY` (collecte)
- **Narration** — *« Encore un : le vieux du Bar paie toujours rubis sur
  l'ongle. »*
- **Scène de collecte** — mêmes répliques de collecte, le vieux paie.
- **Atteint** — *« Le vieux paie, et t'offre même un verre. »*

### 7. `OBJ_BEAT` → embuscade au Chantier *(3 × `EK_THUG`, `SP_AMBUSH`)*
- **Narration** — *« Dernier client, au Chantier. Marco : celui-là, je le sens
  pas... Embuscade ! »*
- **Script** — 3 gros bras passifs (`SP_AMBUSH`) jusqu'à l'approche, puis foncent.
  Marco présent mais **invulnérable** (ennemis ciblent le joueur).
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Marco : quelqu'un nous a vendus. On réglera ça. »*

### 8. `OBJ_GOTO` → Le Garage *(rayon 14)* — `EV_MARCO_LEAVE`
- **Narration** — *« Tournée finie. Ramène Marco au Garage. »*
- **Cinématique** (`CUT_MARCO_LEAVE`, variante **à pied** : Marco part de sa
  position de filature) : il marche jusqu'à sa porte, *« Marco : bon boulot,
  petit. On remet ça bientôt. »*, disparaît → fin.

## Clôture
→ `finishMission()` : **+150 $**, **MISSION ACCOMPLIE**, `campaignStep → 2`.
