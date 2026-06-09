# Trafic IA — véhicules & piétons errants (GTA demake Gamebuino)

Date : 2026-06-09
Statut : approuvé (approche A), implémentation en cours

## Objectif

Peupler la ville d'une circulation vivante : ~6 voitures IA qui suivent les
routes (côté droit) et ~6 piétons qui marchent sur les trottoirs. On réutilise
les sprites existants (voiture 24 frames, piéton 4 dir × 2 frames) **recolorés**
avec une teinte aléatoire par entité.

Contrainte maîtresse : Gamebuino META (Cortex-M0+ 48 MHz, ~32 Ko RAM). Le rendu
+ joueur + 1 voiture sont déjà à ~56 % CPU. L'IA doit rester très légère.

## Choix validés (brainstorming)

- **Modèle IA** : errance simple + respect des voies (roulage à droite, dans sa voie).
- **Densité** : ~6 voitures + ~6 piétons, *culling agressif* (update/draw seulement
  autour de la caméra).
- **Recolorage** : 1 zone teintable par entité (carrosserie / t-shirt).
- **Interaction joueur** : voitures solides entre elles et avec la voiture joueur
  (collision AABB) ; le joueur en voiture peut renverser les piétons.

## Approche retenue : A — agents sur grille, tuile par tuile

Le seul modèle qui tient dans le budget et s'inscrit dans la discipline existante
(moteur pur `engine.py` ↔ `engine.h`, parité testée en host).

### Couche pure (engine.py source de vérité, portée en engine.h)

Classification de tuiles :
- `is_drivable(grid,w,h,tx,ty)` → tuile route (ROAD_H/V/CROSS) et en bornes.
- `is_walkable(grid,w,h,tx,ty)` → tuile PAVEMENT et en bornes.

Directions cardinales (ordre enum `Dir`) : `N E S W` = `(0,-1)(1,0)(0,1)(-1,0)`.
Tables : `RIGHT[d]`, `LEFT[d]`, `BACK[d]`.

Voie à droite : décalage latéral `LANE` (px) perpendiculaire au sens de marche,
vers la droite. `lane_point(tx,ty,d)` = centre de tuile + `LANE * dir(RIGHT[d])`.
- Va vers l'Est → moitié Sud ; Sud → moitié Ouest ; Ouest → moitié Nord ; Nord → moitié Est.

RNG déterministe partagé (xorshift32) pour parité et reproductibilité :
`rng_next(state) -> (val, state)`.

Choix de sortie à une intersection / fin de voie :
`pick_exit(grid,w,h,tx,ty,d, classify, state)` → direction choisie parmi
`{tout droit (poids 4), droite (1), gauche (1)}` filtrée aux tuiles valides ;
demi-tour seulement si cul-de-sac.

### Modèle d'avance (« drive toward lane-point »)

Chaque agent vise un point-cible (lane_point de la tuile suivante) et avance en
ligne droite à vitesse constante vers lui. Arrivé (à epsilon près) il appelle
`pick_exit` pour fixer la nouvelle direction et le nouveau point-cible. Le chemin
est donc une succession de segments axiaux avec coins à 90° — virage instantané
acceptable à cette résolution. Voiture : on blitte la frame de rotation la plus
proche de la cardinale. Piéton : idem avec `playerFrames[dir]`.

Tests (invariants, comme la physique voiture actuelle) :
- un agent placé sur une route reste toujours sur des tuiles drivables/walkables ;
- `pick_exit` déterministe à graine fixe ne renvoie jamais une tuile non valide ;
- `lane_point` décale du bon côté pour les 4 directions.

### Glue device (gta.ino, non host-testé — dépend de Gamebuino)

- **Pool** statique `AiCar cars[N]`, `AiPed peds[N]` (positions float, dir, couleur,
  pour piéton : état WALKING/DOWN + timer + frame anim).
- **Spawn / recyclage** : au reset, placer chaque agent sur une tuile route/trottoir
  proche du spawn. Chaque frame, si un agent est trop loin de la caméra (> marge),
  le recycler sur une tuile valide juste hors écran (anneau autour de la vue).
  Update seulement les agents proches (culling).
- **Couleur** : petite palette fixe de teintes RGB565 agréables ; index tiré au spawn.
- **Recolorage au blit** : sprites bakés avec la zone teintable en couleur-clé
  `KEY` ; au blit `if (px==KEY) px = agent.color;` (une comparaison/pixel de plus).
  `build_car.py` / `build_player.py` émettent la carrosserie / le t-shirt en `KEY`.
- **Collisions** :
  - voiture joueur ↔ voitures IA : AABB ; bloque (réutilise le rebond amorti).
  - voiture joueur (en mouvement) ↔ piéton : si chevauchement et vitesse > seuil →
    piéton passe DOWN (sprite au sol / disparition après timer puis recyclage).
  - IA ↔ IA : ignorées (pas d'évitement) — conforme à l'errance, coût nul.

## Budget estimé

RAM : ~210 o pour les deux pools. CPU : par agent, avance flottante + lookup tuile
seulement aux frontières + 12 tests AABB/frame en voiture. Objectif : rester < ~80 %.
On mesure étape par étape (debug série `gb.getCpuLoad()`), on réduit N si besoin.

## Itération 1 (après test sur device, 2026-06-09)

- **Densité voitures 6 → 3** (`NUM_AI_CARS`) : 6 trop chargé. Piétons restent à 6.
- **Arrêt sur obstacle** : une voiture IA s'arrête si l'entité joueur active (perso
  à pied OU voiture pilotée) est devant elle, dans la voie, à moins de `STOP_AHEAD`
  px (tolérance latérale `STOP_SIDE`). Projection avant/latérale via les vecteurs
  `dir`/`RIGHT[dir]` — quelques multiplications par voiture, négligeable. Pas
  d'arrêt IA↔IA ni pour les voitures garées (seule l'entité active bloque).
- **Vol de véhicule** : à pied, `A` prend la voiture la plus proche à portée —
  voiture du joueur (`ENTER_DIST`) ou voiture IA (`ENTER_AI_DIST`). Pour une IA :
  le conducteur descend (un piéton apparaît sur un trottoir proche via
  `aiEjectDriver`), la voiture quitte le pool IA et devient la voiture pilotée,
  sa teinte est conservée (`carColor`, repris par `drawCar`). Limite assumée : un
  seul créneau « voiture joueur » — voler une voiture écrase la précédente.

## Hors scope (v1)

- Fuite/panique des piétons, feux tricolores, klaxons, IA d'évitement,
  destruction/score. Possibles itérations futures.
