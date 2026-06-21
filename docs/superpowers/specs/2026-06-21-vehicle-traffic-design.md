# Trafic véhicules cohérent — design

## Contexte / problème

Le trafic IA (`AiCar`, `gta/ai.h` ↔ `tools/ai.py`, glue dans `gta/mod_ai.h`)
roule déjà avec un décalage de voie à droite et une pondération
tout-droit/droite/gauche aux intersections, mais le résultat perçu en jeu est
incohérent :

- **Virages en angle droit instantané** : à l'arrivée sur une tuile, la
  voiture change de direction de mouvement (et de frame de sprite) d'un coup,
  sans aucune courbe.
- **Voitures qui se traversent** : `aiUpdate` ne traite QUE le joueur, les
  épaves et les piétons comme obstacles « devant » (cf. `STOP_AHEAD`/
  `STOP_SIDE`) ; les autres `AiCar` ne sont jamais évités entre elles. Sur une
  avenue large (2 tuiles), rien n'empêche un véhicule de zigzaguer entre les
  deux colonnes et de croiser le trafic en sens inverse.
- **Pas de ralentissement aux intersections.**

Mesures de référence : route = 1 tuile (8px) ; voiture = ~4px de large, 12px
de long (`tools/build_car.py`, `L=6.0, W=2.0`) ; avenues majeures = exactement
2 tuiles de large (`tools/citygen.py:_major_lines`, `range(pos, pos+2)`), pas
de cas 3 voies dans le générateur actuel.

## Périmètre

Réécriture du comportement de conduite IA. Le code existant (`ai.h`,
`mod_ai.h`, partie routage de `tools/ai.py`) peut être entièrement remplacé —
rien n'est à préserver par principe, seule la parité `.h`/`.py` et les points
d'intégration (signatures appelées depuis `mod_ai.h`, `AiCar`, rendu) doivent
rester compatibles ou être mis à jour partout où ils sont utilisés.

Hors périmètre : ne pas ajouter de cas « avenue 3 voies » spécifique (aucune
avenue de ce type n'est générée aujourd'hui) — mais la structure de données
(masque de directions par tuile) doit rester générique pour ne pas avoir à
tout refaire si une avenue plus large apparaît un jour. Pas de changement du
nombre de voitures actives (`NUM_AI_CARS`) ni du tuning fin (vitesses,
distances) au-delà de ce qui est nécessaire pour les comportements décrits
ici — à ajuster empiriquement après implémentation, pas à fixer dans cette
spec.

## 1. Données de sens de circulation (masque par tuile)

Aujourd'hui une tuile route est juste « roulable » (`aiIsDrivable`), sans
notion de sens. On ajoute, calculé au **build** (`tools/build_city.py`, même
étage que le calcul de solidité), un **masque de 4 bits** (N/E/S/W) par tuile
route : quelles directions de sortie sont légales depuis cette tuile.

Règle de génération (schéma « damier », garanti sans piège par
construction — pas de validation de connectivité au runtime nécessaire) :

- **Rue à 1 voie** (mineure) : sens unique déterminé par la parité de la
  rangée/colonne de la rue dans la grille de routes (rangée paire → sens A,
  impaire → sens B ; colonne paire → sens C, impaire → sens D).
- **Avenue à 2 voies** (majeure) : chaque colonne/rangée de l'avenue reçoit un
  sens fixe, opposé à l'autre colonne/rangée (calculé déterministiquement à
  partir de la géométrie : quelle colonne est « à droite » de quel sens).
- **Carrefour** (`ROAD_CROSS` ou tuile à l'intersection de deux rues/avenues) :
  masque = union des sens légaux des deux rues qui se croisent (toujours 2
  bits posés au moins, jamais 0 — pas d'impasse à un carrefour).
- **Tuile à largeur N générique** (futur-proof, pas implémenté tant
  qu'aucune avenue de ce type n'existe) : toute tuile ayant 2+ voisins
  roulables sur le même axe doit permettre de se rabattre vers une voie
  adjacente (gauche ou droite) — propriété déjà garantie par construction du
  schéma en damier décrit ci-dessus, donc rien de spécial à coder.

Conséquence sur l'algorithme de routage (`aiPickExit` / équivalent
réécrit) : en plein milieu d'un bloc (masque à 1 seul bit), il n'y a plus de
choix aléatoire — la voiture continue tout droit, point. Le choix pondéré
(tout droit / droite / gauche, RNG) ne s'applique plus qu'aux tuiles où le
masque autorise réellement plusieurs sorties (carrefours), filtré par le
masque (on ne propose plus que les directions légales).

Format de stockage : un tableau `const uint8_t laneMask[CITY_H*CITY_W]`
généré par `tools/build_city.py` à côté de `cityMap`, même style
d'émission (fichier marqué « généré — NE PAS éditer »).

## 2. Anti-chevauchement véhicule-véhicule

`aiUpdate` (`mod_ai.h`) calcule déjà, pour chaque `AiCar`, si un obstacle
(joueur, épave, piéton) est « devant » dans sa voie (`fwd`/`lat` projetés sur
`AI_DX[c.dir]`/`AI_RIGHT[c.dir]`, comparés à `STOP_AHEAD`/`STOP_SIDE`) pour
décider `blocked`. On étend cette même vérification aux **autres `AiCar`
actives** : une voiture en fuite/bélier (`ignorePeople`) continue d'ignorer
cette règle (cohérent avec le comportement actuel vis-à-vis des piétons),
toutes les autres s'arrêtent/ralentissent derrière la voiture qui les
précède au lieu de la traverser.

## 3. Ralentissement aux intersections

Quand une voiture approche d'une tuile dont le masque de sens autorise un
virage (carrefour, plusieurs bits), elle réduit sa vitesse cible sur les
derniers pixels avant le point de décision, et la reprend une fois la
nouvelle direction engagée. Réutilise le même mécanisme de vitesse que
`CAR_FLEE_SPEED`/`AI_CAR_SPEED` (une vitesse `AI_CAR_TURN_SPEED` plus basse,
appliquée uniquement à l'approche d'un carrefour).

## 4. Virage en courbe (pas de snap à 90°)

La position `(x,y)` ne doit plus sauter d'un segment droit à un autre avec un
angle vif. À l'approche d'une tuile où la direction va changer (connue en
avance via le masque, donc pas besoin d'attendre l'arrivée exacte pour
décider), la trajectoire suit une courbe (ex. Bézier quadratique : point de
contrôle au coin, départ avant le coin, arrivée après, dans la nouvelle
direction) au lieu d'un trajet rectiligne vers un point cible unique. Le
sprite (4 frames cardinales existantes, pas de nouvelles frames diagonales)
bascule vers la nouvelle direction au moment approprié de la courbe (ex. à
mi-parcours), donc le changement de frame reste un saut (limite du sprite 4
directions, accepté), mais la trajectoire elle-même est lisse — c'est le
défaut principal pointé par le joueur qui disparaît.

## Parité .h/.py et tests

`gta/ai.h` ↔ `tools/ai.py` doivent rester en parité fonctionnelle (cf.
CLAUDE.md). Toute nouvelle fonction de routage/masque doit exister dans les
deux, et les `host_test/` (g++ natif) doivent couvrir au minimum :

- une rue 1-voie : la voiture ne choisit jamais le sens interdit ;
- un carrefour : les deux sens permis restent atteignables (pas de piège) ;
- deux voitures qui se suivent sur la même voie : la seconde s'arrête
  derrière sans chevauchement ;
- un virage : la position suit une courbe (pas de discontinuité brutale du
  vecteur vitesse d'une frame à l'autre au-delà d'une tolérance).

## Hors scope / suivi

- Tuning fin des constantes (vitesses, distances, taille du masque pour les
  rues mineures) : à ajuster en testant en jeu après implémentation.
- Avenue 3+ voies avec voie centrale dédiée : pas de cas réel aujourd'hui,
  non implémenté ; la structure de données générique n'empêche pas de
  l'ajouter plus tard.
- Augmentation de `NUM_AI_CARS` : indépendant de ce design, à considérer
  séparément si le trafic reste trop clairsemé une fois la cohérence
  corrigée.
