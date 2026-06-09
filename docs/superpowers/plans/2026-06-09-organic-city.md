# GTA Gamebuino Demake — Plan : Génération organique de ville (pipeline en couches)

> **For agentic workers:** Implémenté via superpowers:subagent-driven-development (TDD par tâche, review spec puis qualité). Cases `- [ ]` pour le suivi.

**Goal:** Ville organique style GTA1 générée par une **pipeline déterministe en couches** : zones (Voronoi de quartiers) → routes (grille hiérarchique jittée, denses en downtown) → ponts → remplissage des blocs par zone → trottoirs/spawn. Value noise déterministe (graine fixe). Tourne au build (PC) → produit le `cityMap[]` figé, aucun coût device.

**Architecture (`tools/citygen.py`, pur, aucune I/O) :**
- Primitives de bruit (déjà livrées Task 1) : `_hash01`, `_smooth`, `_value_noise`, `noise_field`, `_quantile_threshold`.
- Constantes de zone : `Z_WATER=0, Z_PARK=1, Z_DOWNTOWN=2, Z_RESIDENTIAL=3`.
- Couches pures et **testables isolément** :
  - `build_zones(seed, w, h, water, parks, districts) -> zone_grid` (liste plate `[h*w]`).
  - `draw_roads(zone_grid, seed, w, h, margin) -> road_grid` (plate ; 0=pas route, sinon road_h/road_v/road_cross via index passés OU codes internes 'h'/'v'/'x').
  - `add_bridges(zone_grid, road_grid, seed, w, h, max_span) -> road_grid` (modifie/retourne).
  - `fill_blocks(grid, zone_grid, road_grid, seed, w, h, density, idx)` (pose bâtiments par zone).
  - `add_pavement(grid, w, h, idx)`, `pick_spawn(grid, w, h, solid_index, zone_grid) -> (x,y,2)`.
  - `generate_into(city, seed, tile_index, solid_index, water=0.18, parks=0.10, density=0.85, districts=8)` — orchestre tout, écrit `city.grid` + `city.spawn`.

**IMPORTANT — refactor :** le `generate_into` monolithique actuel (issu des commits `f7da7b7`/`5eb3fbf`) est **remplacé** par cette pipeline en couches. Les anciens tests de `test_citygen.py` qui supposaient le monolithe sont remplacés par des tests par couche (ci-dessous). Garder les tests des **primitives de bruit** (Task 1) intacts.

**Contrat tuiles :** noms requis `grass, road_h, road_v, road_cross, pavement, water, building_a, building_b` dans `tile_index`. `solid_index` = {water, building_a, building_b}. Manquant → `ValueError` clair.

**Convention :** racine du dépôt ; pytest = `.venv/bin/python -m pytest`. Map 96×96.

---

## Task 1 : primitives de bruit — ✅ DÉJÀ LIVRÉE (commit 2fe619c)

`_hash01`, `_smooth`, `_value_noise`, `noise_field`, `_quantile_threshold` + tests. Ne pas y toucher.

---

## Task A : `build_zones` — eau + fleuve + Voronoi de quartiers + parcs (TDD)

**Files:** Modify `tools/citygen.py`, rewrite the non-noise part of `tools/tests/test_citygen.py`.

`build_zones(seed, w, h, water, parks, districts) -> zone_grid` (liste plate de longueur `w*h`, valeurs ∈ {Z_WATER,Z_PARK,Z_DOWNTOWN,Z_RESIDENTIAL}) :
1. Tout `Z_RESIDENTIAL` par défaut.
2. **Eau blob :** `nw = noise_field(seed+1, w, h, scale=max(6.0, w/8.0))` ; `thr = _quantile_threshold(nw, water)` ; `nw[y][x] < thr` → `Z_WATER`.
3. **Fleuve serpentant :** `xf=w*0.5` ; pour `y` : `xf += (_value_noise(seed+2, y*0.15, 0)-0.5)*2.2` ; bande `Z_WATER` largeur `max(3, w//28)` centrée `int(xf)` (clampée).
4. **Voronoi quartiers (terre seulement) :** `rng=random.Random(seed+3)` ; tirer `districts` graines `(gx,gy)` dans `[0,w)×[0,h)`. Typage : trier les graines par distance au centre ; les `ceil(districts*0.4)` plus proches du centre = `Z_DOWNTOWN`, le reste = `Z_RESIDENTIAL`. Pour chaque case **non-eau**, lui donner le type de la graine la plus proche (distance euclidienne au carré).
5. **Parcs (taches) :** `npf = noise_field(seed+4, w, h, scale=max(5.0, w/12.0))` ; `thr=_quantile_threshold(npf, parks)` ; case **non-eau** avec `npf[y][x] < thr` → `Z_PARK` (override quartier).

- [ ] **Step 1 : tests** (remplacer la partie non-noise de `test_citygen.py` ; garder les 4 tests de primitives). Helpers + tests :
```python
import pytest, random
from collections import Counter
from tools.citydsl import CompiledCity
from tools import citygen
from tools.citygen import (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL,
                           build_zones)

def test_build_zones_all_types_present_and_labeled():
    z = build_zones(7, 96, 96, water=0.18, parks=0.10, districts=8)
    assert len(z) == 96 * 96
    n = Counter(z)
    for t in (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL):
        assert n[t] > 0, "zone %d absente" % t
    assert all(v in (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL) for v in z)

def test_build_zones_deterministic():
    a = build_zones(7, 64, 64, 0.18, 0.10, 8)
    assert a == build_zones(7, 64, 64, 0.18, 0.10, 8)
    assert a != build_zones(9, 64, 64, 0.18, 0.10, 8)

def test_water_fraction_roughly_matches():
    z = build_zones(3, 96, 96, water=0.20, parks=0.05, districts=8)
    frac = Counter(z)[Z_WATER] / (96 * 96)
    assert 0.18 <= frac <= 0.40   # blob ~0.20 + fleuve ajoute un peu

def test_downtown_more_central_than_residential():
    z = build_zones(7, 96, 96, 0.15, 0.05, 10)
    cx = cy = 48
    def mean_dist(t):
        ds = [((x-cx)**2+(y-cy)**2)**0.5 for y in range(96) for x in range(96)
              if z[y*96+x] == t]
        return sum(ds)/len(ds)
    assert mean_dist(Z_DOWNTOWN) < mean_dist(Z_RESIDENTIAL)
```

- [ ] **Step 2 :** lancer → FAIL.
- [ ] **Step 3 :** implémenter `build_zones` + constantes de zone. (Si le monolithe `generate_into` existe encore, le laisser pour l'instant — il sera réécrit en Task C ; mais SUPPRIMER les anciens tests qui le ciblent et que ces nouveaux tests remplacent.)
- [ ] **Step 4 :** `.venv/bin/python -m pytest tools/tests/test_citygen.py -v` → PASS.
- [ ] **Step 5 :** commit `feat(organic): build_zones (eau + fleuve + Voronoi quartiers + parcs)`.

---

## Task B : `draw_roads` (grille hiérarchique jittée) + `add_bridges` (TDD)

**Files:** Modify `tools/citygen.py`, `tools/tests/test_citygen.py`.

`draw_roads(zone_grid, seed, w, h, margin=3) -> road_grid` (liste plate `w*h`, valeurs `'.'`/`'h'`/`'v'`/`'x'`) :
- **Avenues majeures :** `rng=random.Random(seed+5)`. Colonnes : `pos=margin` ; `while pos < w-margin` : largeur `2` ; ajouter `[pos, pos+2)` à `major_cols` ; `pos += 14 + rng.randint(-3,4)` (≥6). Idem `major_rows` sur la hauteur (même rng). 
- **Rues mineures (downtown only) :** candidats fins : `minor_cols` via `pos=margin; step=7+rng.randint(-2,2)`, idem rows. Une cellule de rue mineure n'est tracée que si `zone_grid[cell] == Z_DOWNTOWN`.
- **Tracé :** pour chaque (x,y) dans `[margin, w-margin)`×`[margin, h-margin)` : si zone==Z_WATER → `'.'` (pas de route sur l'eau). Sinon déterminer `on_col` = (x dans major_cols) OU (x dans minor_cols ET zone downtown) ; `on_row` analogue. `on_col&on_row`→`'x'` ; `on_col`→`'v'` ; `on_row`→`'h'`. Bords (`< margin` ou `>= dim-margin`) → jamais de route.

`add_bridges(zone_grid, road_grid, seed, w, h, max_span=14) -> road_grid` :
- Pour chaque avenue majeure (chaque colonne de `major_cols`, chaque ligne de `major_rows`) : parcourir la ligne ; pour chaque segment **contigu d'eau** traversé de longueur `<= max_span`, le remplir de route (`'v'` pour une colonne, `'h'` pour une ligne) → pont. Spans plus longs (grands lacs) non pontés.

- [ ] **Step 1 : tests** :
```python
from tools.citygen import draw_roads, add_bridges

def _zone_and_roads(seed=7, w=96, h=96):
    z = build_zones(seed, w, h, 0.15, 0.05, 10)
    r = draw_roads(z, seed, w, h)
    return z, r

def test_roads_present_and_not_on_border():
    z, r = _zone_and_roads()
    assert any(v != '.' for v in r)
    m = 3
    # marge: aucune route dans la bande de bord
    assert all(r[y*96+x] == '.' for y in range(96) for x in range(96)
               if x < m or x >= 96-m or y < m or y >= 96-m)

def test_downtown_denser_roads_than_residential():
    z, r = _zone_and_roads()
    def road_frac(t):
        idx = [i for i, zz in enumerate(z) if zz == t]
        return sum(1 for i in idx if r[i] != '.') / max(1, len(idx))
    assert road_frac(Z_DOWNTOWN) > road_frac(Z_RESIDENTIAL)

def test_roads_not_on_water_before_bridges():
    z, r = _zone_and_roads()
    assert all(not (r[i] != '.' and z[i] == Z_WATER) for i in range(len(z)))

def test_bridges_add_road_over_water():
    z, r = _zone_and_roads()
    rb = add_bridges(z, list(r), 7, 96, 96)
    before = sum(1 for i in range(len(z)) if z[i] == Z_WATER and r[i] != '.')
    after = sum(1 for i in range(len(z)) if z[i] == Z_WATER and rb[i] != '.')
    assert after > before   # au moins un pont pose sur l'eau

def test_draw_roads_deterministic():
    z, r = _zone_and_roads(); _, r2 = _zone_and_roads()
    assert r == r2
```

- [ ] **Step 2 :** FAIL. **Step 3 :** implémenter. **Step 4 :** PASS. **Step 5 :** commit `feat(organic): draw_roads (grille hierarchique jittee, dense en downtown) + add_bridges`.

---

## Task C : `fill_blocks` par zone + trottoirs + spawn + `generate_into` orchestration (TDD)

**Files:** Modify `tools/citygen.py`, `tools/tests/test_citygen.py`.

- `fill_blocks(grid, zone_grid, road_grid, seed, w, h, density, idx)` : `idx` = dict des index tuiles. `rng=random.Random(seed+6)`. Pour chaque (x,y) où `road_grid==' .'`/`'.'` (pas route) ET `zone != Z_WATER` :
  - `Z_WATER` → `water` ; `Z_PARK` → `grass` ; sinon (downtown/résidentiel) c'est un intérieur d'îlot :
    - downtown : `pbuild = density` (≈0.85→0.9) ; type = `building_b` si `_value_noise(seed+7, x*0.2, y*0.2) > 0.35` sinon `building_a` (majorité tours).
    - résidentiel : `pbuild = density * 0.5` ; type = `building_a` si `_value_noise(seed+7, x*0.2, y*0.2) > 0.4` sinon `building_b` (majorité maisons, cours d'herbe = les non-tirés restent grass).
  - `rng.random() < pbuild` → poser bâtiment, sinon `grass`.
  (Les cases route gardées telles quelles et converties en index road via `road_grid` → géré dans `generate_into`.)
- `add_pavement(grid, w, h, idx)` : toute case non-route/non-eau avec voisin 4-connexe route → `pavement` (snapshot, ne chaîne pas).
- `pick_spawn(grid, w, h, solid_index, zone_grid)` : spirale déterministe depuis le centre, 1ère case non-solide, préférence pavement>route>grass ; retourne `(x,y,2)`.
- `generate_into(city, seed, tile_index, solid_index, water=0.18, parks=0.10, density=0.85, districts=8)` : résoudre les 8 index (manquant→ValueError) ; `z=build_zones(...)` ; `r=draw_roads(z,...)` ; `r=add_bridges(z,r,...)` ; écrire `city.grid` : eau d'abord depuis z, puis routes depuis r (`'h'`→road_h, `'v'`→road_v, `'x'`→road_cross), puis `fill_blocks`, puis `add_pavement` ; `city.spawn = pick_spawn(...)`.

- [ ] **Step 1 : tests** :
```python
TI = {"grass":0,"road_h":1,"road_v":2,"road_cross":3,"pavement":4,"water":5,"building_a":6,"building_b":7}
SI = {5, 6, 7}
def _gen(seed=7, w=96, h=96, **kw):
    c = CompiledCity(w, h); citygen.generate_into(c, seed, TI, SI, **kw); return c

def test_full_all_tiles_present():
    n = Counter(_gen().grid)
    for t in (0,1,2,3,4,5,6,7):
        assert n[t] > 0, "tuile %d absente" % t

def test_downtown_denser_buildings_than_residential():
    c = _gen(); z = build_zones(7, 96, 96, 0.18, 0.10, 8)
    def bfrac(t):
        idx = [i for i,zz in enumerate(z) if zz==t]
        return sum(1 for i in idx if c.grid[i] in (6,7))/max(1,len(idx))
    assert bfrac(Z_DOWNTOWN) > bfrac(Z_RESIDENTIAL)

def test_spawn_walkable():
    c = _gen(); sx,sy,sd = c.spawn
    assert 0<=sx<c.w and 0<=sy<c.h and c.get(sx,sy) not in SI and sd==2

def test_generate_deterministic():
    assert _gen(seed=11).grid == _gen(seed=11).grid
    assert _gen(seed=11).spawn == _gen(seed=11).spawn

def test_missing_tile_raises():
    with pytest.raises(ValueError):
        citygen.generate_into(CompiledCity(8,8), 1, {"grass":0}, set())
```

- [ ] **Step 2 :** FAIL. **Step 3 :** implémenter + **supprimer l'ancien corps monolithique** de `generate_into`. **Step 4 :** `.venv/bin/python -m pytest tools/tests -q` → tout vert. **Step 5 :** commit `feat(organic): fill_blocks par zone + trottoirs + spawn + generate_into en couches`.

---

## Task D : commande DSL `organic` + `player` optionnel (TDD)

**Files:** Modify `tools/citydsl.py`, `tools/tests/test_citydsl.py`.

- À `seed <N>` : stocker aussi `seed_int = N` (défaut 1).
- Commande `organic` : `kv=_kwargs(tok[1:])` ; lire `water/parks/density` floats (défauts 0.18/0.10/0.85) + `districts` int (défaut 8), erreurs lignées ; `from tools import citygen` (module-level) ; `citygen.generate_into(city, seed_int, tile_index, solid_index, water, parks, density, districts)`.
- `_finalize` : `player` optionnel SI `city.spawn` déjà fixé (par organic) ; sinon erreur `player` manquante comme avant (tests M2 inchangés gardent leur `player`).

- [ ] **Step 1 : tests** (ajouter à `test_citydsl.py`) :
```python
def test_organic_fills_and_autospawn():
    c = _compile("size 64 64\nseed 7\norganic water 0.18 parks 0.1 density 0.85\n")
    assert c.spawn is not None
    from collections import Counter
    n = Counter(c.grid)
    assert n[5]>0 and (n[6]+n[7])>0 and (n[1]+n[2]+n[3])>0

def test_organic_deterministic():
    src="size 48 48\nseed 3\norganic\n"
    assert _compile(src).grid == _compile(src).grid

def test_player_still_required_without_spawn():
    with pytest.raises(CityError):
        _compile("size 8 8\nfill grass\n")
```

- [ ] **Step 2 :** FAIL. **Step 3 :** implémenter. **Step 4 :** `.venv/bin/python -m pytest tools/tests -v` → tout vert. **Step 5 :** commit `feat(organic): commande DSL 'organic' (couches citygen) + player optionnel`.

---

## Task E : `city/city.txt` organique + régénération + tuning visuel

**Files:** Overwrite `city/city.txt` ; regenerate `gta/citymap.{h,cpp}`, `city/citymap.png`.

- [ ] **Step 1 :** `city/city.txt` :
```
; Liberty City (demake) — ville organique en couches (deterministe)
size 96 96
seed 7
organic water 0.18 parks 0.10 density 0.85 districts 8
```
- [ ] **Step 2 :** `.venv/bin/python -m tools.build_city` → OK.
- [ ] **Step 3 : tuning visuel** — ouvrir `city/citymap.png` (Read). Attendu proche de la réf GTA1 : quartiers distincts (downtown petits blocs très bâtis vs résidentiel grands blocs aérés), eau qui serpente + ponts sur les bras étroits, parcs verts en taches, trottoirs, spawn marchable. **Itérer `water/parks/density/districts/seed`** jusqu'à un rendu convaincant. (Le contrôleur peut aussi rendre la carte de zones via `build_zones` pour diagnostiquer.) Décrire le rendu final.
- [ ] **Step 4 :** `.venv/bin/python -m pytest tools/tests -v` → tout vert.
- [ ] **Step 5 :** commit `feat(organic): city.txt organique en couches + regeneration citymap.{h,cpp,png}`.

---

## Vérification finale

- [ ] Suite complète verte (M1+M2+M3+organic).
- [ ] `citymap.png` : quartiers lisibles, densité contrastée downtown/résidentiel, eau organique + ponts, parcs dispersés, spawn marchable.
- [ ] `citygen.py` pur, en couches, déterministe (tests par couche).
- [ ] Connectivité raisonnable (le viewer permet de circuler ; vérif manuelle écran : `.venv/bin/python -m tools.viewer`).

**Livrable :** `tools/citygen.py` en couches + commande DSL `organic` + `city.txt` organique + `citymap.{h,cpp,png}`. Viewer M3 affiche la ville sans modif.
