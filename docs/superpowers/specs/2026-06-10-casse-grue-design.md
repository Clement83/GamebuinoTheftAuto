# La Casse — grue de broyage déclenchée à pied (design)

Date : 2026-06-10

## Contexte

La Casse (district thématique `THEME_JUNKYARD`, nom HUD « La Casse ») broie la
voiture du joueur contre une prime. Le code existant (commit `589cfd0`) fournit
déjà : zone hazard jaune/noir, animation grue (pince qui descend par câble),
animation broyage, prime selon les PV (~40..160 $), sons, éjection du joueur.

**Deux problèmes à corriger :**

1. **Déclenchement inversé.** Le broyage se lance en *roulant* sur la zone
   (`driving && !carIsMission` → `onCasse && !casseInside`). On veut l'inverse :
   garer la voiture, **sortir**, et alors la grue vient la chercher. Si on est
   *dans* la voiture, la grue ne se met pas en route.
2. **Zone non dédiée.** La zone = point-cible générique du district (centroïde
   → cellule marchable la plus proche), qui « bouge » selon la forme du
   quartier. On veut un **point précis émis par le générateur Python**, comme
   les services `citySprays[]` / `cityAmmus[]`.

## Comportement cible

### Point Casse (généré)

- Une tuile précise dans le district `THEME_JUNKYARD`, **carrossable et
  accessible depuis la route** (case route/pavée du quartier ou bordant
  celui-ci), choisie de façon **déterministe** par seed.
- Si le thème junkyard n'est pas placé (tuiles absentes / district non
  attribué) → pas de point → la feature est absente (comme aujourd'hui quand
  `findPoi("La Casse")` renvoie -1).

### Déclenchement (firmware)

Évalué par frame quand `seqKind == SEQ_NONE` :

- **Condition d'amorçage** : `!carGone` ET `!carIsMission` ET `!driving`
  (joueur à pied) ET centre de la voiture **dans la zone** Casse ET joueur **à
  portée** de la zone (`CASSE_REACH`).
- **Amorçage** : tant que la condition est vraie, un compteur `casseArm`
  décompte (~30 frames ≈ 1 s). Petit son d'amorçage (la grue « se réveille »).
- **Annulation** : si la condition redevient fausse (on remonte → `driving`,
  on s'éloigne hors `CASSE_REACH`, ou la voiture sort de la zone), `casseArm`
  est remis à zéro → « la grue s'arrête ou ne se met pas en route ».
- **Déclenchement** : `casseArm` arrivé à 0 → `startCrushSeq()` existante
  (PH_CRANE → PH_CRUSH → prime + cha-ching → PH_EJECT). Le joueur étant déjà à
  pied, on retire l'éjection/repositionnement forcé en fin de séquence.

### Inchangé

Animations grue/broyeur, sons, prime selon PV, marqueur hazard. L'accès route
est garanti par construction (point choisi carrossable).

## Découpage technique

**Python (`tools/`)** — TDD :
- `pois.py` : `place_casse(grid, district_id, assign, tile_index, solid_index,
  w, h, seed)` → `(tx, ty)` ou `None`. Tuile carrossable du district junkyard
  bordant une route, déterministe.
- `citygen.py` : remplit `city.casse`.
- `citydsl.py` : champ `self.casse = None` sur `CompiledCity`.
- `build_city.py` : exporte `CITY_HAS_CASSE` + `CITY_CASSE_TX/TY` dans
  `citymap.h` ; marqueur sur le PNG de preview.
- `tests/test_pois.py` : point dans le district junkyard, tuile carrossable,
  accès route, déterminisme, absence propre si thème non placé.

**Firmware (`gta/gta.ino`)** :
- Utilise `cityCasse` (px monde) au lieu de `findPoi("La Casse")` pour la zone,
  l'animation et le trigger.
- Nouveau `static int casseArm` ; logique d'amorçage/annulation à pied.
- Retrait du trigger « au volant » et de l'éjection forcée de fin de séquence.
- Garde `#if CITY_HAS_CASSE` pour neutraliser proprement si absent.

## Validation

- Tests host Python (`.venv/bin/pytest tools/tests/`).
- Compilation firmware (FQBN + `FOLDER_NAME`) : surveiller la taille flash.
- Flash sur Gamebuino META et test manuel du scénario complet.
