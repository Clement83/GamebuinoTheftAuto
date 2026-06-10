# Voitures destructibles, feu lié à la caisse, explosifs génériques

Date : 2026-06-10
Statut : design validé (brainstorming)

## Contexte

Une grosse partie du travail vit déjà dans le working tree non commité :
mèche d'auto-destruction (`carFuse`), caisse lancée sans conducteur
(`carRunaway`), fumée/flamme pré-rendue (`smoke.h`/`smoke_data.cpp`), épaves
(`Wreck`), et toute la machinerie de **panique** des PNJ (`AiPed.state == 2`,
`aiPanicStep`/`panic_step` en parité `ai.h`/`ai.py`). Ce spec finit le travail
et corrige le comportement observé en jeu.

Les retours de test ont précisé l'intention :

- La caisse en feu **s'éteignait** quand on remontait dedans (mèche coupée
  volontairement à l'embarquement).
- Les **PV d'une voiture ne doivent jamais se réinitialiser** — c'est une
  propriété persistante de la caisse. Deux exceptions légitimes : le
  **Pay'n'Spray** (réparation complète, payante) et le **dépop** d'une voiture
  (on l'oublie, son info de PV est perdue — normal).
- Les armes **explosives** (bazooka, grenade) doivent faire des **dégâts de
  zone** dont le **centre (impact) dépasse les PV d'un véhicule**, pour
  détruire génériquement le véhicule touché et endommager les voisins. Les
  armes à **balles** (pistolet, PM, pompe) usent les PV par coups successifs.
- Un véhicule détruit par une explosion **déclenche sa propre explosion**
  (chaîne).
- Le **conducteur éjecté** d'une voiture volée part **en panique** vers le
  trottoir.

## Contraintes

- Cible Gamebuino META (SAMD21, RAM/flash serrées) : pas de gros tableaux,
  éviter le flottant par pixel, blitter du pré-rendu.
- Parité `gta/ai.h` ↔ `tools/ai.py` (déjà respectée pour la panique ;
  inchangée par ce spec). Host tests `tools/host_test/` à relancer.
- Modèle **mono-voiture joueur** : il n'existe qu'une seule voiture pilotée
  (`car` + état global `carHp`/`carFuse`/`carRunaway`/`carColor`). Les voitures
  de trafic sont des entités séparées (`aiCars[]`, chacune son `hp`).

## A. PV et feu persistants, liés à la caisse

Principe : `carHp` et `carFuse` ne sont **jamais remis à zéro/plein** par le
seul fait d'entrer ou re-entrer dans une voiture. Ils ne changent que par :
dégâts (accidents, balles, explosions), réparation Pay'n'Spray, ou
remplacement de la caisse (vol d'une autre voiture / voiture de mission /
dépop).

### A.1 Remonter dans SA propre voiture ne réinitialise plus rien

Site : `loop()`, embarquement, ~ligne 2564.

Aujourd'hui :
```cpp
if (best == -3 || best == -1 || best >= 0) { carFuse = 0; carRunaway = false; }
...
} else if (best == -1) {            // remonter dans sa voiture
  driving = true; carIsMission = false;
  carHp = CAR_MAX_HP;               // <-- RESET à supprimer
}
```

Changements :
- **Retirer `best == -1` du reset `carFuse`/`carRunaway`** : remonter dans sa
  caisse en feu la laisse brûler (et elle explosera). Le reset reste pour
  `best == -3` (voiture de mission, neuve) et `best >= 0` (voiture volée, qui
  n'a de toute façon pas de mèche).
- **Supprimer `carHp = CAR_MAX_HP`** dans la branche `best == -1` : on garde
  l'usure courante.

Résultat : on peut re-rentrer dans une caisse amochée/en feu sans la « guérir ».

### A.2 Voiture volée : hérite déjà des PV (inchangé)

`best >= 0` : `carHp = c.hp > 0 ? c.hp : CAR_MAX_HP;` — la caisse volée garde
son usure. On garde. Le reset `carFuse=0/carRunaway=false` reste correct (autre
caisse, non en feu).

### A.3 Voiture de mission : neuve (inchangé)

`best == -3` : `carHp = CAR_MAX_HP;` — entité neuve, ce n'est pas un reset d'une
caisse existante. On garde.

### A.4 Pay'n'Spray répare la voiture

Site : `repaintCar()` (~ligne 1902). Aujourd'hui il repeint + efface la
recherche police + débite, mais **ne répare pas les PV**.

Ajout en fin de `repaintCar()` : réparation complète + extinction du feu.
```cpp
carHp = CAR_MAX_HP;
carFuse = 0;
carRunaway = false;
```

### A.5 Dépop : info perdue (déjà le cas)

Mono-voiture : abandonner une caisse pour en voler une autre écrase `car` et son
état. Aucune entité « voiture abandonnée » persistante (YAGNI). Les voitures de
trafic recyclées repartent à `hp = CAR_MAX_HP` (leur vie de spawn). Aucun
changement.

### A.6 Rendu de la flamme à pied (vérification)

`drawCar` et `drawCarSmoke` sont appelés que l'on conduise ou non ; une caisse
garée en feu (`carFuse > 0`) doit afficher fumée + flamme. À **vérifier au
build** ; corriger si une garde de `drawCarSmoke` masque le cas garé-en-feu.
Note : sauter **en marche** garde le comportement actuel (la caisse part en
`runaway` et peut sortir de l'écran — choix assumé).

## B. Armes explosives = explosion de zone générique

Remplace l'ancien modèle « le tir explosif détruit instantanément la voiture du
cône ». Une explosion devient un **événement radial** unique réutilisé par la
mèche voiture ET par les armes explosives.

### B.1 Fonction d'explosion unifiée

Étendre `explodeCarAt(wx, wy, frameIdx, hopx, hopy)` (ou la renommer
`explodeAt`) pour, en plus de l'existant (boom visuel + son, `spawnWreck`, PNJ
létaux dans `BOOM_HURT_RADIUS`, PNJ paniqués dans `BOOM_PANIC_RADIUS`, joueur à
pied −1 cœur), infliger des **dégâts radiaux aux véhicules** :

- Pour chaque `aiCars[i]` actif dans `BOOM_VEHICLE_RADIUS` : dégât à
  décroissance linéaire
  `dmg = BOOM_CENTER_DMG * (1 - dist / BOOM_VEHICLE_RADIUS)` (clampé ≥ 0).
  `c.hp -= dmg`. Si `c.hp <= 0` : marquer `c.active = false` **avant** de
  rappeler `explodeAt` à sa position (chaîne — voir B.2).
- Voiture du joueur (`car`) dans le rayon, si elle n'est pas le centre source :
  `carHp -= dmg` (à 0, la mèche s'allumera via `updateCarFuse`, géré ailleurs —
  pas de cas spécial).

### B.2 Chaîne d'explosions

Un véhicule tombé à 0 par l'explosion déclenche sa propre `explodeAt`. Garde
anti-récursion infinie : le véhicule est passé `active = false` **avant**
l'appel récursif, donc une explosion en chaîne ne le re-touche pas. La chaîne
est bornée par `NUM_AI_CARS` (= 3) + la voiture joueur. Profondeur acceptable
sur SAMD21.

### B.3 Constantes (réglage)

```cpp
static const int     BOOM_VEHICLE_RADIUS = 24;  // px : portee des degats vehicule
static const int16_t BOOM_CENTER_DMG     = 40;  // > CAR_MAX_HP (30) : detruit a l'impact
```

(`BOOM_HURT_RADIUS = 16`, `BOOM_PANIC_RADIUS = 40` inchangés.)

### B.4 Déclenchement par les armes

`gta/weapons.h` : ajout d'un champ `bool explosive` à `WeaponDef`, `true` pour
**Bazooka** et **Grenade** uniquement, `false` ailleurs. Mettre à jour les 6
lignes de `WEAPONS[]`.

`tryAttack()` :
- **Arme explosive** (`wd.explosive`) : calculer un **point d'impact** puis
  appeler `explodeAt(impactX, impactY, ...)`. **Ne pas** exécuter les boucles
  cône PNJ / cône voiture normales : l'explosion radiale couvre PNJ et
  véhicules.
  - Point d'impact : véhicule **ou** PNJ le plus proche dans le cône
    (`combatInCone`) ; à défaut, un point devant le joueur à `wd.reach` le long
    de `playerDir`.
- **Arme à balles** (`!wd.explosive`) : comportement actuel inchangé —
  PNJ par cône, et voitures du cône `hp -= (wd.area ? CAR_AREA_DMG : CAR_HIT_DMG)`
  avec explosion à 0. Donc :
  - Pistolet / PM : `CAR_HIT_DMG = 6` → ~5 coups (CAR_MAX_HP = 30).
  - Pompe : `CAR_AREA_DMG = 12` → ~3 coups (plus fort, sans one-shot).

La panique « bruit du tir » des civils alentour (boucle `GUNSHOT_PANIC_RANGE`)
reste pour toutes les armes à feu.

## C. Conducteur éjecté en panique

Site : `aiEjectDriver(atx_px, aty_px)` (~ligne 1045). Aujourd'hui place le PNJ
en `state = 0` (marche normale).

Ajout après le placement : `startPanic(p, atx_px, aty_px);` — le conducteur
fuit la position de la caisse volée. Réutilise la machinerie existante
(`state == 2` / `aiPanicStep`) : il traverse la route en courant et ne se calme
que sur un trottoir. Aucun nouveau code IA.

## Parité et tests

- `gta/ai.h` ↔ `tools/ai.py` : **inchangés** (panique déjà en parité). Relancer
  `tools/host_test/` et `tools/tests/test_ai.py` pour non-régression.
- `weapons.h` : le flag `explosive` est de la donnée pure (pas de logique de
  parité).
- L'explosion radiale et l'état voiture vivent dans `gta.ino` (hors header de
  parité) → pas de host test dédié existant. **Vérification manuelle au build**
  (taille flash, comportement) :
  1. Détruire une voiture de trafic d'**un** tir de bazooka/grenade ; un
     véhicule collé explose aussi (chaîne).
  2. Pistolet/PM : plusieurs tirs pour détruire ; pompe : moins de tirs.
  3. Mettre sa caisse à 0 PV, sauter, remonter : elle **brûle toujours**.
  4. Pay'n'Spray : entrer avec une caisse amochée/en feu → ressort réparée,
     feu éteint.
  5. Voler une voiture → le conducteur détale sur la route puis rejoint un
     trottoir.

## Hors scope (YAGNI)

- Pas d'entité « voiture abandonnée » persistante (modèle mono-voiture).
- Pas de nouveau sprite de fumée ni de retouche des rayons PNJ existants.
- Pas de changement au comportement PNJ normal (flânerie/police).
