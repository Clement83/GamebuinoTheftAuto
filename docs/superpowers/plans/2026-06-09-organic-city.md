# GTA Gamebuino Demake — Plan : Génération organique de ville (bruit)

> **For agentic workers:** Implémenté via superpowers:subagent-driven-development (TDD par tâche, review spec puis qualité). Cases `- [ ]` pour le suivi.

**Goal:** Remplacer la grille régulière par une ville **organique** style GTA1 (eau qui serpente, blocs de tailles irrégulières, parcs dispersés, densité variable), générée par **value noise déterministe** (graine fixe). Le générateur tourne au build (PC) et produit le `cityMap[]` figé — aucun coût/portage côté Gamebuino.

**Architecture:**
- `tools/citygen.py` — module **pur** déterministe : primitives de bruit (`noise_field`, `_value_noise`, `_hash01`, `_smooth`) + `generate_into(city, seed, tile_index, solid_index, water, parks, density)` qui remplit `city.grid` et fixe `city.spawn`. Réutilise `CompiledCity` de `citydsl`. Aucune I/O.
- `tools/citydsl.py` — nouvelle commande DSL `organic [water <w>] [parks <p>] [density <d>]` qui délègue à `citygen.generate_into` (utilise la graine du `seed`/défaut 1). `player` devient **optionnel** quand `organic` a déjà fixé un spawn valide.
- `city/city.txt` — réécrit pour utiliser `organic` (LA ville de démarrage). `build_city` émet toujours `citymap.{h,cpp,png}` ; le viewer M3 affiche la nouvelle ville sans modif.

**Convention d'exécution :** racine du dépôt ; pytest = `.venv/bin/python -m pytest`. Map 96×96.

**Contrat tuiles (noms requis dans tile_index) :** `grass, road_h, road_v, road_cross, pavement, water, building_a, building_b`. Solides = `solid_index` (= {water, building_a, building_b}).

**Déterminisme :** tout dérive de `seed` (offsets de sel différents par champ de bruit). Même `(w,h,seed,params)` → même grille. Vérifié par test.

---

## Implémentation de référence du bruit (à utiliser telle quelle en Task 1)

```python
import math

def _hash01(seed, ix, iy):
    """Float pseudo-aléatoire déterministe dans [0,1) depuis des coords entières de lattice."""
    h = (ix * 374761393 + iy * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
    h ^= (h >> 16)
    return (h & 0xFFFFFFFF) / 4294967296.0

def _smooth(t):
    return t * t * (3.0 - 2.0 * t)

def _value_noise(seed, x, y):
    """Value noise bilinéaire lissé en (x,y) flottants (espace lattice)."""
    x0, y0 = math.floor(x), math.floor(y)
    fx, fy = x - x0, y - y0
    v00 = _hash01(seed, x0, y0);     v10 = _hash01(seed, x0 + 1, y0)
    v01 = _hash01(seed, x0, y0 + 1); v11 = _hash01(seed, x0 + 1, y0 + 1)
    sx, sy = _smooth(fx), _smooth(fy)
    a = v00 + (v10 - v00) * sx
    b = v01 + (v11 - v01) * sx
    return a + (b - a) * sy

def noise_field(seed, w, h, scale, octaves=3, persistence=0.5):
    """Champ [h][w] de flottants dans [0,1], somme d'octaves de value noise."""
    field = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            amp, freq, total, norm = 1.0, 1.0 / scale, 0.0, 0.0
            for o in range(octaves):
                total += amp * _value_noise(seed + o * 101, x * freq, y * freq)
                norm += amp
                amp *= persistence
                freq *= 2.0
            field[y][x] = total / norm
    return field

def _quantile_threshold(field, frac):
    """Seuil tel qu'environ `frac` des cellules soient < seuil."""
    vals = sorted(v for row in field for v in row)
    if not vals:
        return 0.0
    i = max(0, min(len(vals) - 1, int(frac * len(vals))))
    return vals[i]
```

---

## Task 1 : `citygen.py` — primitives de bruit (TDD)

**Files:** Create `tools/citygen.py`, `tools/tests/test_citygen.py`.

- [ ] **Step 1 : tests qui échouent** (`tools/tests/test_citygen.py`) :
```python
from tools.citygen import noise_field, _value_noise, _hash01, _quantile_threshold


def test_hash_deterministic_and_range():
    assert _hash01(7, 3, 4) == _hash01(7, 3, 4)
    assert _hash01(7, 3, 4) != _hash01(7, 3, 5)
    assert 0.0 <= _hash01(1, 10, 20) < 1.0


def test_noise_field_shape_range_deterministic():
    f1 = noise_field(7, 16, 12, scale=4.0)
    assert len(f1) == 12 and len(f1[0]) == 16
    assert all(0.0 <= v <= 1.0 for row in f1 for v in row)
    f2 = noise_field(7, 16, 12, scale=4.0)
    assert f1 == f2                       # déterministe
    f3 = noise_field(8, 16, 12, scale=4.0)
    assert f1 != f3                       # graine différente -> champ différent


def test_noise_is_smooth_not_white():
    # value noise lissé : voisins corrélés -> variations locales modestes en moyenne
    f = noise_field(3, 40, 40, scale=8.0)
    diffs = [abs(f[y][x] - f[y][x + 1]) for y in range(40) for x in range(39)]
    assert sum(diffs) / len(diffs) < 0.2  # pas du bruit blanc


def test_quantile_threshold_fraction():
    f = noise_field(5, 30, 30, scale=6.0)
    thr = _quantile_threshold(f, 0.3)
    below = sum(1 for row in f for v in row if v < thr)
    assert 0.2 * 900 <= below <= 0.4 * 900   # ~30% sous le seuil
```

- [ ] **Step 2 :** `.venv/bin/python -m pytest tools/tests/test_citygen.py -v` → FAIL (module absent).
- [ ] **Step 3 :** implémenter les primitives (copier l'implémentation de référence ci-dessus dans `tools/citygen.py`).
- [ ] **Step 4 :** `.venv/bin/python -m pytest tools/tests/test_citygen.py -v` → PASS (4).
- [ ] **Step 5 :** commit `feat(organic): citygen primitives de value noise (deterministe)`.

---

## Task 2 : géographie — eau (blob + fleuve serpentant) + grille de rues irrégulière + trottoirs (TDD)

**Files:** Modify `tools/citygen.py`, `tools/tests/test_citygen.py`.

Implémenter `generate_into(city, seed, tile_index, solid_index, water=0.22, parks=0.12, density=0.6)` — pour CETTE task, ne traiter QUE : grass de base → eau → routes → trottoirs. (Bâtiments/parcs/spawn = Task 3.) Le `city` reçu a `.w`, `.h`, `.grid`, `.set(x,y,t)`, `.get(x,y)`.

Étapes internes (toutes seedées) :
1. Vérifier que les 8 noms de tuiles requis sont dans `tile_index`, sinon `raise KeyError`/`ValueError` clair. Récupérer les index (`grass, road_h, road_v, road_cross, pavement, water`).
2. `fill` grass.
3. **Eau blob :** `nw = noise_field(seed + 1, w, h, scale=max(6, w/8), octaves=3)` ; `thr = _quantile_threshold(nw, water)` ; poser `water` là où `nw[y][x] < thr`.
4. **Fleuve serpentant :** partir de `x = w*0.5`, pour chaque `y` faire dériver `x += (_value_noise(seed+2, y*0.15, 0.0) - 0.5)*2.2`, carver une bande `water` de largeur ~`max(3, w//28)` centrée en `int(x)` (clampée). (Garantit une voie d'eau connectée même si le blob est épars.)
5. **Grille irrégulière de rues (sur la terre uniquement) :** générer les positions de colonnes et de lignes par espacement **jitté** : `pos = margin` ; boucle `while pos < dim`: largeur d'axe `wd = 2 if rng.random()<0.35 else 1` (avenue/rue) ; ajouter les indices `[pos, pos+wd)` à l'ensemble `cols`/`rows` ; `pos += base + jitter` où `base≈10`, `jitter = rng.randint(-3, 4)` (clampé ≥4). Utiliser `rng = random.Random(seed + 3)`. Puis pour chaque cellule : si déjà `water` → laisser (le fleuve coupe les rues, réaliste) ; sinon si `on_col and on_row` → `road_cross`, elif `on_col` → `road_v`, elif `on_row` → `road_h`.
6. **Trottoirs :** toute case non-route/non-eau ayant un voisin 4-connexe route → `pavement` (réutiliser la logique de `_roadgrid` de citydsl : adjacence aux `{road_v,road_h,road_cross}`).

- [ ] **Step 1 : tests qui échouent.** Helper de test commun (mapping calé sur le contrat M1) :
```python
import pytest
from tools.citydsl import CompiledCity
from tools import citygen

TI = {"grass":0,"road_h":1,"road_v":2,"road_cross":3,"pavement":4,"water":5,"building_a":6,"building_b":7}
SI = {5, 6, 7}

def _gen(seed=7, w=96, h=96, **kw):
    c = CompiledCity(w, h)
    citygen.generate_into(c, seed, TI, SI, **kw)
    return c

def _counts(c):
    from collections import Counter
    return Counter(c.grid)
```
Tests Task 2 :
```python
def test_generate_water_and_land_present():
    c = _gen(water=0.25)
    n = _counts(c)
    total = c.w * c.h
    assert n[5] > 0                       # de l'eau existe
    assert n[5] < total * 0.6             # mais pas toute la map
    assert (total - n[5]) > total * 0.3   # de la terre reste


def test_generate_has_roads_and_pavement():
    c = _gen()
    n = _counts(c)
    assert n[1] + n[2] + n[3] > 0         # des routes
    assert n[4] > 0                       # des trottoirs


def test_generate_deterministic_same_seed():
    assert _gen(seed=7).grid == _gen(seed=7).grid
    assert _gen(seed=7).grid != _gen(seed=9).grid


def test_roads_not_on_water():
    c = _gen()
    road_ids = {1, 2, 3}
    # aucune cellule n'est à la fois route et eau (trivialement vrai par construction,
    # on vérifie qu'il existe au moins une frontière eau/route cohérente)
    assert all(not (c.get(x, y) in road_ids and c.get(x, y) == 5)
               for y in range(c.h) for x in range(c.w))
```

- [ ] **Step 2 :** lancer → FAIL (`generate_into` absent).
- [ ] **Step 3 :** implémenter les étapes 1-6.
- [ ] **Step 4 :** `.venv/bin/python -m pytest tools/tests/test_citygen.py -v` → PASS.
- [ ] **Step 5 :** commit `feat(organic): generate_into geographie (eau serpentante + grille irreguliere + trottoirs)`.

---

## Task 3 : bâtiments (densité variable) + parcs dispersés + spawn (TDD)

**Files:** Modify `tools/citygen.py`, `tools/tests/test_citygen.py`.

Compléter `generate_into` (après les trottoirs) :
7. **Bâtiments (densité) :** `nd = noise_field(seed+4, w, h, scale=max(5, w/10))`. Gradient centre→bord : `g = 1 - dist_normalisée_au_centre` (centre dense). Pour chaque case encore `grass` (intérieur d'îlot) : proba `p = density * (0.4 + 0.6*g) * nd[y][x]` ; si `rng.random() < p` (rng = `random.Random(seed+5)`), poser un bâtiment. Type : `building_b` si `noise_field`/`_value_noise(seed+6, x*0.2, y*0.2) > 0.5` sinon `building_a` (varie les façades par zone).
8. **Parcs dispersés :** `np_ = noise_field(seed+7, w, h, scale=max(5, w/12))` ; `thr = _quantile_threshold(np_, parks)` ; là où `np_[y][x] < thr` ET la case est un bâtiment ou du grass (pas route/eau/trottoir) → forcer `grass` (taches de verdure qui creusent les îlots).
9. **Spawn :** recherche déterministe en spirale depuis le centre `(w//2, h//2)` de la première case **non-solide** (préférence : `pavement`, sinon `road_*`, sinon `grass`) ; `city.spawn = (x, y, 2)` (south). Doit exister (la map a forcément des trottoirs/routes).

- [ ] **Step 1 : tests qui échouent** :
```python
def test_generate_has_buildings_and_parks():
    c = _gen(density=0.6, parks=0.15)
    n = _counts(c)
    assert n[6] + n[7] > 0                # des bâtiments
    assert n[0] > 0                       # du grass (parcs + reste)


def test_density_center_denser_than_edges():
    c = _gen(density=0.7)
    def build_frac(x0, y0, x1, y1):
        cells = [c.get(x, y) for y in range(y0, y1) for x in range(x0, x1)]
        b = sum(1 for v in cells if v in (6, 7))
        return b / len(cells)
    center = build_frac(c.w//2 - 12, c.h//2 - 12, c.w//2 + 12, c.h//2 + 12)
    corner = build_frac(0, 0, 20, 20)
    assert center >= corner               # centre au moins aussi dense


def test_spawn_is_walkable_and_in_bounds():
    c = _gen()
    assert c.spawn is not None
    sx, sy, sd = c.spawn
    assert 0 <= sx < c.w and 0 <= sy < c.h
    assert c.get(sx, sy) not in SI        # non-solide
    assert sd == 2                        # south


def test_full_generation_deterministic():
    assert _gen(seed=11).grid == _gen(seed=11).grid
    assert _gen(seed=11).spawn == _gen(seed=11).spawn
```

- [ ] **Step 2 :** lancer → FAIL (pas de bâtiments/parcs/spawn).
- [ ] **Step 3 :** implémenter 7-9.
- [ ] **Step 4 :** `.venv/bin/python -m pytest tools/tests/test_citygen.py -v` → PASS (tous).
- [ ] **Step 5 :** commit `feat(organic): batiments (densite gradient+bruit) + parcs disperses + spawn central`.

---

## Task 4 : commande DSL `organic` + `player` optionnel (TDD)

**Files:** Modify `tools/citydsl.py`, `tools/tests/test_citydsl.py`.

- Stocker la graine entière : à `seed <N>`, faire aussi `seed_int = N` (variable locale, défaut 1) en plus du `rng`.
- Nouvelle commande `organic` : `kv = _kwargs(tok[1:])` ; lire `water/parks/density` (floats, défauts 0.22/0.12/0.6) avec messages d'erreur lignés ; importer `from tools import citygen` (au niveau module) ; `citygen.generate_into(city, seed_int, tile_index, solid_index, water, parks, density)`.
- `_finalize` : `player` n'est plus obligatoire **si** un spawn est déjà fixé (par `organic`). Garder : si `city.spawn is None` après tout → erreur `player` manquante. (Donc une ville `organic` sans `player` est valide ; une ville sans `organic` ni `player` reste une erreur. Les tests M2 existants gardent leur ligne `player` → toujours verts.)

- [ ] **Step 1 : tests qui échouent** (ajouter à `test_citydsl.py`) :
```python
def test_organic_fills_and_sets_spawn():
    c = _compile("size 64 64\nseed 7\norganic water 0.2 parks 0.1 density 0.5\n")
    assert c.spawn is not None                  # spawn auto, pas de ligne player
    from collections import Counter
    n = Counter(c.grid)
    assert n[5] > 0 and (n[6] + n[7]) > 0 and (n[1] + n[2] + n[3]) > 0  # eau, bâtiments, routes


def test_organic_deterministic_via_seed():
    src = "size 48 48\nseed 3\norganic\n"
    assert _compile(src).grid == _compile(src).grid


def test_player_still_required_without_spawn():
    with pytest.raises(CityError):
        _compile("size 8 8\nfill grass\n")        # ni organic ni player -> erreur
```

- [ ] **Step 2 :** lancer → FAIL (`commande inconnue: 'organic'`).
- [ ] **Step 3 :** implémenter la commande + `seed_int` + `_finalize` assoupli.
- [ ] **Step 4 :** `.venv/bin/python -m pytest tools/tests -v` → PASS (toute la suite, M1+M2+M3+organic).
- [ ] **Step 5 :** commit `feat(organic): commande DSL 'organic' (delegue citygen) + player optionnel si spawn auto`.

---

## Task 5 : nouveau `city/city.txt` organique + régénération + vérification visuelle

**Files:** Overwrite `city/city.txt` ; regenerate `gta/citymap.{h,cpp}`, `city/citymap.png`.

- [ ] **Step 1 :** réécrire `city/city.txt` :
```
; Liberty City (demake) — ville organique generee par bruit (deterministe)
size 96 96
seed 7
organic water 0.22 parks 0.12 density 0.6
```
- [ ] **Step 2 :** `.venv/bin/python -m tools.build_city` → `genere: ... (96x96, spawn=...)`. Si `CityError`, corriger d'après le message.
- [ ] **Step 3 : vérification visuelle** — ouvrir `city/citymap.png` (outil Read). Attendu : eau qui serpente (fleuve + taches/lac), grille de rues à espacement **irrégulier** (blocs de tailles variées) coupée par l'eau, trottoirs bordant les routes, bâtiments plus denses au centre, parcs verts dispersés, marqueur de spawn (croix magenta) sur une case marchable. **Itérer les params `water/parks/density`/`seed`** dans `city.txt` et régénérer jusqu'à un rendu convaincant proche de la référence GTA1. Décrire le rendu final.
- [ ] **Step 4 :** `.venv/bin/python -m pytest tools/tests -v` → tout vert.
- [ ] **Step 5 :** commit `feat(organic): city.txt organique + regeneration citymap.{h,cpp,png}`.

---

## Vérification finale

- [ ] Suite complète verte (M1+M2+M3+organic).
- [ ] `citymap.png` inspecté : eau organique, blocs irréguliers, parcs dispersés, densité variable, spawn marchable.
- [ ] `citygen.py` pur (aucune I/O), déterministe (tests le prouvent).
- [ ] Vérif interactive utilisateur (écran) : `.venv/bin/python -m tools.viewer` → la nouvelle ville scrolle, collisions sur eau/bâtiments, overlay 80×64 clampé.

**Livrable :** `tools/citygen.py` + commande DSL `organic` + `city/city.txt` organique + `citymap.{h,cpp,png}` régénérés. Le viewer M3 affiche la ville organique sans modification.
