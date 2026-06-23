# Carte « Marseille » — Design (branche `gta-marseille`)

## But

Remplacer la ville aléatoire (Perlin) par une carte **stylisée de Marseille** :
Vieux-Port au sud, quartiers nord, anneau de collines, Corniche au bord de
l'eau, la Bonne Mère, et des **routes organiques** (pas une grille
horizontale/verticale). Le tout sans casser la campagne ni l'IA.

## Décisions validées

- **Approche** : générateur déterministe dédié + commande DSL `marseille`,
  réutilisant TOUTE la machinerie POI/thèmes (`tools/pois.py`). Aucune logique
  C++ nouvelle.
- **Taille** : 220×220 (la `cityMap` vit en flash, pas en RAM).
- **Repères** : Vieux-Port + Quais, Bonne Mère, Quartiers Nord, Corniche,
  anneau de collines, **Le Panier**, **La Canebière**, **Calanques à l'est**.
- **Noms** : pool cosmétique `DISTRICT_NAMES` → noms marseillais. Les noms
  **fonctionnels** des POI (référencés par les missions via `findPoi`/`strcmp`)
  restent intacts.

## Contraintes dures

1. **Campagne** : les missions retrouvent les POI par nom (`cityPois[].name`).
   Ne renommer QUE le pool `DISTRICT_NAMES`. Tous les POI de campagne doivent
   rester présents (Planque, Casino, Garages, La Casse, Chantier, Police,
   Hôpital, Pompiers, Bar, Bureaux, Commerces, Les Quais).
2. **Routes** : uniquement `road_h`/`road_v`/`road_cross`, IA trafic en
   4-connexité orthogonale stricte. « Organique » = segments orthogonaux
   suivant des courbes (escalier), réseau connexe garanti.
3. **Top-down sans relief** : collines = zones vertes (`Z_PARK`) ; Bonne Mère =
   POI-repère sur une colline.

## Architecture

- `tools/citygen.py` refactoré : extraction d'un `_assemble(...)` partagé
  (fill blocs + routes + trottoirs + tous les POI + spawn). `generate_into`
  par défaut = terrain/routes Perlin + `_assemble`. Comportement par défaut
  inchangé (verrouillé par pytest + host_test).
- `tools/citygen_marseille.py` : terrain (zones) + routes organiques propres à
  Marseille, puis `_assemble(...)`.
- `tools/citydsl.py` : commande `marseille` (parallèle à `organic`).
- `city/city.txt` : `size 220 220` / `seed N` / `marseille`.

## Géographie (couche zones)

Positions ancrées en proportions de la map (indépendantes de la taille) ;
le seed ne jitte que les détails.

- Mer au sud (côte irrégulière), Vieux-Port = entaille d'eau remontant au nord
  avec une passe étroite vers la mer, bordée de quais (`dock`, thème PORT).
- Calanques à l'est (côte rocheuse découpée).
- Anneau de collines (`Z_PARK`) nord & est.
- Bonne Mère = colline (park) au sud du port + cluster bâti → POI repère.
- Districts : centre dense (Panier) autour du port, Quartiers Nord au nord.

## Routes organiques

1. Graphe de nœuds (tête du port, centres de districts, cols, points de côte)
   → arbre couvrant + boucles (rocades) → réseau connexe.
2. Rasterisation en escalier perturbée par bruit (largeur 1–2, `road_cross`
   aux intersections).
3. La Canebière : avenue large/droite depuis la tête du port vers l'intérieur.
4. La Corniche : route côtière une tuile en retrait du rivage + plage.
5. Le Panier : maille très serrée et tortueuse près du port.
6. Résidentiel : segments lâches branchés sur les artères.
7. Trottoirs : `add_pavement` existant.

## POI & noms

- Chaîne `pois.py` réutilisée telle quelle → POI fonctionnels intacts.
- `DISTRICT_NAMES` → marseillais (≤12 car.) : Le Panier, Noailles, La Joliette,
  Endoume, Le Pharo, La Plaine, Quart. Nord, Vauban, St-Victor…
- La Bonne Mère : MVP = POI-repère nommé (pas d'art dédié). Stretch = tuile
  dédiée (toucherait l'enum `assets.h` généré).

## Tests & build

- pytest : déterminisme, réseau connexe, présence de tous les POI, spawn
  non-solide, port adjacent à la mer.
- host_test inchangés, relancés après build.
- `build_city.py` → `gta/citymap.{h,cpp}` + `city/citymap.png` (copié dans
  `previews/`). Recompiler firmware + rafraîchir `GTADEMAKE/*.bin`.

## Risque

La génération des routes organiques (§ Routes) est la partie la plus
expérimentale : itérations attendues pour le « ressenti » Marseille sans casser
le trafic.
