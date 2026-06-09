# Port côtier forcé — design

Date : 2026-06-09

## Problème

Le thème `THEME_PORT` (`tools/pois.py`) s'accroche à n'importe quel district
adjacent à de l'eau. Avec une carte dont l'eau n'est qu'un petit lac de noise,
le port se retrouve sur une flaque : pas crédible. On veut une vraie façade
maritime (mer le long d'un bord), avec plage de sable et port à quais, tout en
gardant le reste de la génération procédurale.

## Décisions

1. **Bord choisi par la seed** : la mer est garantie le long d'un des 4 bords
   (N/E/S/O), choisi via `Random(seed + 6)`. Elle ne couvre qu'un **segment**
   (~70-82 %) du bord, place par la seed, aux extremites adoucies (baie) ; le
   reste de la face reste de la ville jusqu'au bord.
2. **Nouvelles tuiles** : `TILE_SAND` (plage, marchable) et `TILE_DOCK` (quai en
   planches, marchable). Dessinées dans `tools/build_assets.py`,
   régénère `gta/assets.h` + `gta/assets_data.cpp`.
3. **Composition côtière** : mer → fine bande de sable continue → le port occupe
   *un segment* de cette côte (quais avançant dans l'eau + façades port
   derrière) ; le reste de la côte reste plage ouverte.
4. **Cohabitation** : on garde l'eau de noise (lacs) et le fleuve ; la mer
   côtière s'ajoute. Lacs/fleuve restent décoratifs.
5. **Port réservé à la mer** : un masque `sea` distingue la mer des lacs ; le
   thème port ne peut choisir qu'un district adjacent à la mer.

## Architecture

### Couche zones (`tools/citygen.py`)

- `build_zones` renvoie désormais `(zones, sea)` où `sea` est un masque booléen
  `w*h` marquant uniquement la mer côtière.
- Nouvelle étape (après blob/fleuve, avant Voronoi) `force_coast(seed, w, h, zones, sea)` :
  - `rng = Random(seed + 6)` choisit le bord.
  - profondeur = bande depuis le bord, limite ondulée par un value-noise 1D le
    long du bord (trait de côte irrégulier). Profondeur ~6–9 selon la carte.
  - cellules → `Z_WATER` **et** `sea[i] = True`.
- Nouvelle zone `Z_SAND` : cellules de terre à 1–2 tuiles de la mer (côté
  intérieur) passent en `Z_SAND`. Rendue par la tuile `sand`, sans bâtiment
  (traitée comme un parc par `fill_blocks`), exclue des routes.

### Couche routes

- `draw_roads` / `add_bridges` : la mer est `Z_WATER` → déjà ignorée. `Z_SAND`
  est ajouté aux zones « sans route » pour garder la plage dégagée.

### Couche POI (`tools/pois.py`)

- Placement du thème port restreint aux districts adjacents au masque `sea`
  (et non à toute l'eau). Repli : si aucun district côtier libre, comportement
  actuel (n'importe quelle eau adjacente).
- Après le remplissage des façades port, pose 2-3 **jetées** (`place_docks`) :
  chaque jetée part d'une cellule de mer bordant la terre du district port et
  avance perpendiculairement vers le large sur au plus `max_len` cellules, en
  laissant toujours ≥ 1 cellule de mer au-delà (jamais « jusqu'au bout ») ;
  amorces espacées d'au moins `gap`. `TILE_DOCK` marchable. Déterministe.

### Assets (`tools/build_assets.py`)

- Ajoute `sand` et `dock` au tileset. `dock` non-solide, `sand` non-solide,
  `water` reste solide. Régénère `assets.h` (NUM_TILES, enum) et `assets_data.cpp`.

## Tests (`tools/tests/`)

- `force_coast` : la mer touche le bord choisi sur toute sa longueur ;
  déterministe pour une seed ; bord varie selon la seed.
- `sea` mask : disjoint des lacs de noise.
- plage : au moins une bande `Z_SAND` entre mer et terre.
- port : district port adjacent au masque `sea` ; au moins une tuile `dock`
  posée et marchable (non solide).
- non-régression : tileset minimal (8 tuiles) → génération inchangée.

## Hors scope

- Bateaux / entités sur l'eau.
- Fleuve débouchant proprement dans la mer (les deux cohabitent simplement).
