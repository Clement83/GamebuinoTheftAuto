# M2 — Les assurances

*ACTE I — Marco (M1–M4) · tutoriel déguisé*

> Script fidèle à l'implémentation (`OBJS_M2`, `game_state.h` ; glue
> `mod_mission.h`).

- **Entrée** — `MISSIONS[17]`, `STORY_SEQ[1]`. `isStory`.
- **Déclencheur** — téléphone **rouge** à la Planque (`campaignStep == 1`).
  Décrocher → `startMission(17)`.
- **Prime** — **150 $**.
- **Échec sélectif (règle racket)** — les stops coopératifs (Chinatown, Le Bar)
  posent un **client** (`EV_CLIENT`, PNJ tan). **Le tuer** (poing, balle ou
  voiture) → **MISSION RATÉE** (*« On rackette, on ne tue pas les clients ! »*).
  `sceneNpcDead` capté par `missionProgress` ; coup du joueur détecté dans
  `mod_combat.h` (cône + balle).
- **Note** — Marco n'est ici **que narratif** : aucun PNJ allié n'est posé (seuls
  M1/M4/M13 spawnent un compagnon). Ses répliques passent par le bandeau.

## Setup au décrochage

- Coords POI résolues. Pas de caisse de mission (aucun `OBJ_ENTER_CAR`, aucun
  `EV_MARCO_JOIN` à pied). Tout se joue **à pied**.
- Bandeau *« Les assurances »* puis narration de l'objectif 1.

## Objectifs (séquence moteur)

### 1. `OBJ_GOTO` → Les Commerces *(à pied, rayon 14)*
- **Narration** — *« Jour de tournée. Marco t'emmène encaisser le loyer aux
  Commerces. »*
- **Action joueur** — rejoindre le marqueur aux Commerces.
- **Atteint** — *« Marco : ce commerçant fait le difficile. Regarde et apprends,
  petit. »*

### 2. `OBJ_SUBDUE` → le commerçant *(count 3)*
- **Narration** — *« Le commerçant refuse et te saute dessus. Mate-le, mais le
  tue pas. »*
- **Script** — une cible nommée (rouge) est posée de façon déterministe près du
  marqueur (`T_EMERGE`), **sort**, puis **fonce et frappe le joueur** sans jamais
  mourir (`missionUpdate`, branche `OBJ_SUBDUE`).
- **Action joueur** — la **tabasser au poing** (à pied uniquement : le coup en
  voiture ne compte pas). Chaque coup porté incrémente `objSubdue`.
- **Complétion** — `objSubdue >= 3` : elle **cède** (ne meurt pas).
- **Atteint** — *« Il crache l'argent. Marco : voilà comment on fait. »*

### 3. `OBJ_GOTO` → Chinatown *(rayon 14)* — `EV_CLIENT`
- **Narration** — *« Client suivant : une échoppe de Chinatown. »*
- **Atteint** — *« Encaisse. Sans histoire, celui-là. »*

### 4. `OBJ_GOTO` → Le Bar *(rayon 14)* — `EV_CLIENT`
- **Narration** — *« Encore un : le vieux du Bar paie toujours rubis sur
  l'ongle. »*
- **Atteint** — *« Le vieux paie, et t'offre même un verre. »*

### 5. `OBJ_GOTO` → Le Chantier *(rayon 16)*
- **Narration** — *« Dernier client, au Chantier. Marco : celui-là... je le sens
  pas. »*
- **Atteint** — *« Personne en vue. Trop calme. »* (amorce de l'embuscade)

### 6. `OBJ_BEAT` → embuscade au Chantier *(3 × `EK_THUG`, `SP_AMBUSH`)*
- **Narration** — *« Embuscade ! Ils t'attendaient. Défends-toi ! »*
- **Script** — 3 gros bras posés en anneau déterministe autour du point
  (`spawnEnemiesForObjective`), **passifs** (`EN_IDLE`) jusqu'à ce que le joueur
  entre dans `ENEMY_AGGRO_RANGE` ; réveillés, ils **foncent et cognent** au
  corps-à-corps.
- **Action joueur** — mettre les **3** au tapis (poing, arme, ou voiture lancée).
- **Complétion** — `enemiesAlive == 0` (les passants ne comptent jamais).
- **Atteint** — *« Marco : quelqu'un nous a vendus. On réglera ça. »*

## Clôture
Dernier objectif franchi → `finishMission()` : **+150 $**, bandeau **MISSION
ACCOMPLIE**, `campaignStep → 2`.
