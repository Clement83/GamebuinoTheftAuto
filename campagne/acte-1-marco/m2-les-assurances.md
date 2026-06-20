# M2 — Les assurances

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

> Script fidèle à l'implémentation (`OBJS_M2`, `game_state.h` ; glue
> `mod_mission.h`).

- **Entrée** — `MISSIONS[17]`, `STORY_SEQ[1]`, **9 objectifs**. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 1`).
- **Prime** — **150 $**.
- **Échec sélectif (règle racket)** — les stops coopératifs (Commerces, Le Bar)
  posent un **client**. **Le tuer** (poing, balle ou voiture) → **MISSION RATÉE**
  (*« On rackette, on ne tue pas les clients ! »*) — `sceneNpcDead` capté par
  `missionProgress`, coup du joueur détecté dans `mod_combat.h`.
- **Marco compagnon physique** — Marco est désormais une **vraie entité** : on
  **va le chercher** au Garage, il **te suit à pied** toute la tournée, et on le
  **redépose** au Garage à la fin. Il est **invulnérable** ici (pas de
  `failOnAllyDeath`) : les assaillants ne ciblent que le joueur.

## Setup au décrochage
Coords POI résolues. La **caisse de Marco** est garée près du Garage dès le
décrochage (comme M1) : la tournée se fait **à pied ou en voiture**, au choix —
les scènes s'adaptent (`sceneHomeX/Y`). Seul le `SUBDUE` impose de **descendre**
(coup de poing → à pied ; Marco descend avec toi). Bandeau *« Les assurances »*.

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

### 3. `OBJ_GOTO` → Commerces *(rayon 14)* — `EV_DELIVERY` (collecte démo)
- **Narration** — *« Premier client : les Commerces. Regarde Marco faire. »*
- **Scène de collecte** (`CUT_DELIVERY`, compagnon à pied) — Marco s'approche du
  commerçant, encaisse sans heurt. Client **coopératif** : le frapper/tuer →
  MISSION RATÉE.
- **Atteint** — *« Marco : tu vois ? Facile. ...Le suivant, lui, fait le
  difficile. »* (annonce le récalcitrant)

### 4. `OBJ_GOTO` → Les Bureaux *(rayon 14)*
- **Narration** — *« Le difficile tient un bureau aux Bureaux. Rejoins-le avec
  Marco. »*
- **Script** — trajet vers le POI suivant ; pas de scène ni d'ennemi posé ici (le
  récalcitrant n'apparaît qu'à l'objectif `SUBDUE`).

### 5. `OBJ_SUBDUE` → le gérant récalcitrant *(POI Les Bureaux, count 3)*
- **Narration** — *« Ce gérant-là refuse et te saute dessus. Mate-le, mais le tue
  pas. »*
- **Script** — cible nommée posée (`T_EMERGE`), sort, **fonce et frappe** sans
  mourir. Désormais à **un POI distinct** des Commerces : le client coopératif
  (obj. 3) et ce récalcitrant ne sont plus le même bonhomme.
- **Action joueur** — le **tabasser au poing** (3 coups, `objSubdue`).
- **Complétion** — `objSubdue >= 3` : il cède.
- **Atteint** — *« Il crache l'argent. Marco : voilà comment on fait. »*

### 6. `OBJ_GOTO` → Le Bar *(rayon 14)* — `EV_DELIVERY` (collecte)
- **Narration** — *« Encore un : le vieux du Bar paie toujours rubis sur
  l'ongle. »*
- **Scène de collecte** — le vieux paie sans histoire. Client **coopératif** :
  le frapper/tuer → MISSION RATÉE.
- **Soin** — le **verre offert te rend toute ta vie** (`playerHearts` →
  `PLAYER_HEARTS_MAX`, sans toucher au gilet ; glouglou). Détecté dans
  `missionProgress` quand l'objectif `EV_DELIVERY` est au POI « Le Bar ».
- **Atteint** — *« Le vieux paie et t'offre un verre. Ça requinque : pleine
  forme ! »*

### 7. `OBJ_GOTO` → Chantier *(rayon 14)*
- **Narration** — *« Dernier client, au Chantier. Marco : celui-la... je le sens
  pas. Avance, doucement. »*
- **Script** — trajet d'approche vers l'embuscade ; les gros bras y sont déjà
  posés (`SP_PRESENT`) mais figés tant que l'objectif `BEAT` n'est pas activé.

### 8. `OBJ_BEAT` → embuscade au Chantier *(3 × `EK_THUG`, `SP_PRESENT`, `taunt`)*
- **Narration** — *« Un type t'attend, plante entre deux gros bras. »*
- **Face-à-face scénarisé** (flag `o.taunt`, à pied) — à l'activation,
  `enterObjective` appelle `ambushTauntLines` (keyé par le titre *« Les
  assurances »*) et lance `startTauntCut` : joueur figé le temps de deux
  répliques —
  *« Le type : Marco t'envoie au charbon, hein ? Mauvaise pioche, petit. »* →
  *« Marco : c'est un piege ! Sors les poings, vite ! »* — puis les 3 `EK_THUG`
  **déjà posés** (`SP_PRESENT`) chargent.
- **Complétion** — `enemiesAlive == 0`.
- **Atteint** — *« Marco : quelqu'un nous a vendus. On reglera ca. »*

### 9. `OBJ_GOTO` → Le Garage *(rayon 14)* — `EV_MARCO_LEAVE`
- **Narration** — *« Tournée finie. Ramène Marco au Garage. »*
- **Cinématique** (`CUT_MARCO_LEAVE`, variante **à pied** : Marco part de sa
  position de filature) : il marche jusqu'à sa porte, *« Marco : bon boulot,
  petit. On remet ça bientôt. »*, disparaît → fin.

## Clôture
→ `finishMission()` : **+150 $**, **MISSION ACCOMPLIE**, `campaignStep → 2`.
