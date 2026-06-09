# GTA Gamebuino Demake — Plan M2 : Compilateur de ville

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compiler un fichier de configuration éditable `city/city.txt` (DSL ligne-à-ligne, déterministe) en une carte de tuiles exposée en C++ Gamebuino (`gta/citymap.{h,cpp}`) plus un rendu PNG de debug (`city/citymap.png`).

**Architecture:** Pipeline Python déterministe en deux modules. `tools/citydsl.py` = le **compilateur pur** (texte → objet `CompiledCity`, aucune I/O fichier de sortie, lève `CityError(line_no, msg)`) ; il est testé exhaustivement (commandes, bornes, erreurs avec n° de ligne, déterminisme PRNG). `tools/build_city.py` = l'**orchestration I/O** : charge le mapping nom→index depuis `assets/tileset.csv` (l'ordre des lignes = l'index `enum` de `gta/assets.h`), lit `city/city.txt`, appelle le compilateur, émet les headers C++ et rend `city/citymap.png` depuis `assets/tiles8/`.

**Tech Stack:** Python 3, Pillow (PIL) — déjà présents. pytest. Sortie C++ : `const uint8_t cityMap[]` row-major + `#define` de dimensions/spawn. Pas de nouvelle dépendance.

**Portée :** M2 uniquement (spec §5.4). M3 (viewer pygame) et M4 (sketch) sont des plans séparés.

**YAGNI — hors M2 :** les commandes `alias` + `stamp` (zones en ASCII-art) de la spec sont **reportées** : `roadgrid` + `river` + `blocks` produisent déjà une ville scrollante cohérente. Architecture inchangée si on les ajoute plus tard (une commande de plus dans le parser).

**Convention d'exécution :** toutes les commandes se lancent depuis la **racine du dépôt** (`/home/clement/Documents/private/projet/gamebuino/gta`). Modules invoqués via `python3 -m tools.<module>`.

---

## Contrat partagé (avec M1 déjà livré, et M4 à venir)

- **Index des tuiles = ordre des lignes de `assets/tileset.csv`** (identique à l'`enum TileId` de `gta/assets.h`) :
  `grass=0, road_h=1, road_v=2, road_cross=3, pavement=4, water=5, building_a=6, building_b=7`.
- **Tuiles solides :** colonne `solid=1` de `tileset.csv` → `water, building_a, building_b`.
- **Directions :** `north=0, east=1, south=2, west=3` (identique à `enum Dir` de `gta/assets.h` : `DIR_NORTH=0, DIR_EAST, DIR_SOUTH, DIR_WEST`).
- **Carte :** `cityMap[ty*CITY_W + tx]` (row-major), `uint8_t` par tuile.

## Structure de fichiers (M2)

- `tools/citydsl.py` — **compilateur pur** : `CityError`, `CompiledCity`, `compile_city(text, tile_index, solid_index)`. Aucune écriture de fichier.
- `tools/build_city.py` — **I/O** : `load_tiles()`, `emit_headers()`, `render_png()`, `main()`. Lit `tileset.csv` + `city.txt`, écrit headers + PNG.
- `tools/tests/test_citydsl.py` — tests du compilateur (commandes, bornes, erreurs, déterminisme).
- `tools/tests/test_build_city.py` — tests d'I/O (headers bien formés, PNG aux bonnes dimensions, mapping depuis tileset.csv).
- `city/city.txt` — **LA config éditable** (donnée d'auteur, écrite en Task 8).
- **Sorties générées :** `gta/citymap.h`, `gta/citymap_data.cpp`, `city/citymap.png`.

## DSL `city/city.txt`

```
; commentaire (; ou #). <tile> = nom défini dans tileset.csv.
size <W> <H>                    ; OBLIGATOIRE, première commande non-commentaire ; 1..256
seed <N>                        ; défaut 1 (PRNG pour 'blocks')
fill <tile>                     ; remplit toute la map
rect <tile> <x0> <y0> <x1> <y1> ; rectangle plein inclusif (normalisé + clampé aux bornes)
hline <tile> <x0> <x1> <y>      ; ligne horizontale (clampée)
vline <tile> <y0> <y1> <x>      ; ligne verticale (clampée)
roadgrid spacing <S> width <W> [margin <M>]  ; rues (road_h/road_v) + croisements (road_cross) + trottoirs (pavement) auto
river vertical <x> width <W>    ; bande water verticale, colonne de gauche x, largeur W, toute la hauteur
river horizontal <y> width <W>  ; bande water horizontale, ligne du haut y, largeur W, toute la largeur
blocks <tile> density <D> [on <baseTile>]  ; pose <tile> avec proba D∈[0,1] sur chaque case == baseTile (défaut grass)
player <x> <y> <north|south|east|west>     ; spawn (doit être dans les bornes ET sur une tuile non-solide)
```

**Règles :**
- La **première** ligne non-commentaire/non-vide doit être `size`. Sinon `CityError`.
- Mot-clé inconnu, nom de tuile inconnu, direction inconnue, nombre invalide → `CityError(line_no, msg)`.
- `rect`/`hline`/`vline`/`river` : coords **clampées** aux bornes (pas d'erreur).
- `player` hors bornes → erreur ; `player` sur tuile solide → erreur.
- `player` absent à la fin → erreur (`commande 'player' manquante`).
- **Déterminisme :** même texte → même `cityMap` (PRNG = `random.Random(seed)`, itération en ordre fixe).

---

## Task 1 : `citydsl.py` — squelette + `size` / `fill` / `rect` (TDD)

**Files:**
- Create: `tools/citydsl.py`
- Test: `tools/tests/test_citydsl.py`

- [ ] **Step 1 : Écrire les tests qui échouent**

`tools/tests/test_citydsl.py` :
```python
import pytest
from tools.citydsl import compile_city, CityError

# Mapping de test calé sur le contrat M1 (ordre = index enum).
TILE_INDEX = {
    "grass": 0, "road_h": 1, "road_v": 2, "road_cross": 3,
    "pavement": 4, "water": 5, "building_a": 6, "building_b": 7,
}
SOLID_INDEX = {5, 6, 7}  # water, building_a, building_b


def _compile(text):
    return compile_city(text, TILE_INDEX, SOLID_INDEX)


def test_size_and_fill():
    c = _compile("size 4 3\nfill grass\nplayer 0 0 south\n")
    assert (c.w, c.h) == (4, 3)
    assert c.grid == [0] * 12          # tout grass
    assert c.spawn == (0, 0, 2)        # south == 2


def test_rect_inclusive_and_clamped():
    c = _compile("size 5 5\nfill grass\nrect water 1 1 3 3\n"
                 "rect building_a 4 4 99 99\nplayer 0 0 east\n")
    # water sur le carré [1..3]x[1..3]
    assert c.get(1, 1) == 5 and c.get(3, 3) == 5 and c.get(0, 0) == 0
    # rect clampé : seule (4,4) existe
    assert c.get(4, 4) == 6


def test_size_must_be_first():
    with pytest.raises(CityError) as e:
        _compile("fill grass\nsize 4 4\n")
    assert e.value.line_no == 1


def test_unknown_tile_reports_line():
    with pytest.raises(CityError) as e:
        _compile("size 3 3\nfill lava\nplayer 0 0 south\n")
    assert e.value.line_no == 2
    assert "lava" in str(e.value)


def test_size_out_of_range():
    with pytest.raises(CityError):
        _compile("size 0 5\n")
    with pytest.raises(CityError):
        _compile("size 5 999\n")
```

- [ ] **Step 2 : Lancer les tests (doivent échouer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -v`
Expected: FAIL (`ModuleNotFoundError: No module named 'tools.citydsl'`).

- [ ] **Step 3 : Implémenter le squelette + `size`/`fill`/`rect`**

`tools/citydsl.py` :
```python
"""Compilateur pur du DSL de ville : texte -> CompiledCity.

Aucune I/O de fichier ici (lecture city.txt + écriture headers/PNG = build_city.py).
"""
import random

MAX_DIM = 256
DIRS = {"north": 0, "east": 1, "south": 2, "west": 3}


class CityError(Exception):
    """Erreur de compilation avec numéro de ligne (1-based)."""

    def __init__(self, line_no, msg):
        super().__init__("ligne %d: %s" % (line_no, msg))
        self.line_no = line_no
        self.msg = msg


class CompiledCity:
    def __init__(self, w, h):
        self.w = w
        self.h = h
        self.grid = [0] * (w * h)
        self.spawn = None  # (x, y, dir)

    def _i(self, x, y):
        return y * self.w + x

    def get(self, x, y):
        return self.grid[self._i(x, y)]

    def set(self, x, y, t):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.grid[self._i(x, y)] = t


def _int(tok, line_no, what):
    try:
        return int(tok)
    except (ValueError, TypeError):
        raise CityError(line_no, "%s: entier attendu, recu '%s'" % (what, tok))


def _tile(name, tile_index, line_no):
    if name not in tile_index:
        raise CityError(line_no, "tuile inconnue: '%s'" % name)
    return tile_index[name]


def compile_city(text, tile_index, solid_index):
    """Compile le DSL. tile_index: {nom: index}. solid_index: set d'index solides."""
    city = None
    rng = random.Random(1)  # seed par defaut ; 'seed' la reinitialise

    for line_no, raw in enumerate(text.splitlines(), start=1):
        # retirer commentaire (; ou #) puis trim
        line = raw.split(";", 1)[0].split("#", 1)[0].strip()
        if not line:
            continue
        tok = line.split()
        cmd = tok[0]

        if city is None and cmd != "size":
            raise CityError(line_no, "la premiere commande doit etre 'size'")

        if cmd == "size":
            if len(tok) != 3:
                raise CityError(line_no, "usage: size <W> <H>")
            w = _int(tok[1], line_no, "W")
            h = _int(tok[2], line_no, "H")
            if not (1 <= w <= MAX_DIM and 1 <= h <= MAX_DIM):
                raise CityError(line_no, "dimensions hors bornes 1..%d" % MAX_DIM)
            city = CompiledCity(w, h)

        elif cmd == "fill":
            t = _tile(tok[1], tile_index, line_no)
            city.grid = [t] * (city.w * city.h)

        elif cmd == "rect":
            t = _tile(tok[1], tile_index, line_no)
            x0 = _int(tok[2], line_no, "x0"); y0 = _int(tok[3], line_no, "y0")
            x1 = _int(tok[4], line_no, "x1"); y1 = _int(tok[5], line_no, "y1")
            for y in range(min(y0, y1), max(y0, y1) + 1):
                for x in range(min(x0, x1), max(x0, x1) + 1):
                    city.set(x, y, t)

        elif cmd == "seed":
            rng = random.Random(_int(tok[1], line_no, "seed"))

        elif cmd == "player":
            if len(tok) != 4:
                raise CityError(line_no, "usage: player <x> <y> <north|south|east|west>")
            x = _int(tok[1], line_no, "x"); y = _int(tok[2], line_no, "y")
            d = tok[3]
            if d not in DIRS:
                raise CityError(line_no, "direction inconnue: '%s'" % d)
            if not (0 <= x < city.w and 0 <= y < city.h):
                raise CityError(line_no, "spawn hors bornes: (%d,%d)" % (x, y))
            if city.get(x, y) in solid_index:
                raise CityError(line_no, "spawn sur tuile solide: (%d,%d)" % (x, y))
            city.spawn = (x, y, DIRS[d])

        else:
            raise CityError(line_no, "commande inconnue: '%s'" % cmd)

    return _finalize(city, solid_index, tile_index)


def _finalize(city, solid_index, tile_index):
    if city is None:
        raise CityError(0, "fichier vide: 'size' manquant")
    return city
```

> Note : `rng` n'est utilisé qu'à partir de Task 4 (`blocks`) mais on l'initialise et on câble `seed` dès maintenant pour ne pas y revenir. La validation « `player` obligatoire » est ajoutée en Task 5 ; ici `_finalize` accepte encore un spawn `None`, ce qui convient aux tests de Task 1 (qui fournissent tous une ligne `player`).

- [ ] **Step 4 : Lancer les tests (doivent passer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -v`
Expected: PASS (5 tests).

- [ ] **Step 5 : Commit**

```bash
git add tools/citydsl.py tools/tests/test_citydsl.py
git commit -m "feat(m2): citydsl squelette + size/fill/rect/player (compilateur pur)"
```

---

## Task 2 : `hline` / `vline` / `river` (TDD)

**Files:**
- Modify: `tools/citydsl.py`
- Test: `tools/tests/test_citydsl.py`

- [ ] **Step 1 : Ajouter les tests qui échouent**

Ajouter à `tools/tests/test_citydsl.py` :
```python
def test_hline_vline():
    c = _compile("size 5 5\nfill grass\nhline pavement 1 3 2\n"
                 "vline pavement 0 4 4\nplayer 0 0 south\n")
    assert [c.get(x, 2) for x in range(5)] == [0, 4, 4, 4, 0]
    assert [c.get(4, y) for y in range(5)] == [4, 4, 4, 4, 4]


def test_river_vertical_and_horizontal():
    c = _compile("size 6 6\nfill grass\nriver vertical 2 width 2\n"
                 "river horizontal 4 width 1\nplayer 0 0 south\n")
    # bande verticale water sur colonnes 2 et 3, toute la hauteur
    assert c.get(2, 0) == 5 and c.get(3, 5) == 5 and c.get(1, 0) == 0
    # bande horizontale water sur la ligne 4, toute la largeur
    assert c.get(0, 4) == 5 and c.get(5, 4) == 5
```

- [ ] **Step 2 : Lancer (doit échouer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -k "hline or river" -v`
Expected: FAIL (`commande inconnue: 'hline'`).

- [ ] **Step 3 : Implémenter**

Dans `compile_city`, ajouter avant le `else` final :
```python
        elif cmd == "hline":
            t = _tile(tok[1], tile_index, line_no)
            x0 = _int(tok[2], line_no, "x0"); x1 = _int(tok[3], line_no, "x1")
            y = _int(tok[4], line_no, "y")
            for x in range(min(x0, x1), max(x0, x1) + 1):
                city.set(x, y, t)

        elif cmd == "vline":
            t = _tile(tok[1], tile_index, line_no)
            y0 = _int(tok[2], line_no, "y0"); y1 = _int(tok[3], line_no, "y1")
            x = _int(tok[4], line_no, "x")
            for y in range(min(y0, y1), max(y0, y1) + 1):
                city.set(x, y, t)

        elif cmd == "river":
            water = _tile("water", tile_index, line_no)
            if len(tok) != 5 or tok[3] != "width":
                raise CityError(line_no, "usage: river <vertical|horizontal> <pos> width <W>")
            orient = tok[1]
            pos = _int(tok[2], line_no, "pos")
            wd = _int(tok[4], line_no, "width")
            if orient == "vertical":
                for x in range(pos, pos + wd):
                    for y in range(city.h):
                        city.set(x, y, water)
            elif orient == "horizontal":
                for y in range(pos, pos + wd):
                    for x in range(city.w):
                        city.set(x, y, water)
            else:
                raise CityError(line_no, "orientation inconnue: '%s'" % orient)
```

- [ ] **Step 4 : Lancer (doit passer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -v`
Expected: PASS (tous).

- [ ] **Step 5 : Commit**

```bash
git add tools/citydsl.py tools/tests/test_citydsl.py
git commit -m "feat(m2): commandes hline/vline/river"
```

---

## Task 3 : `roadgrid` (rues + croisements + trottoirs auto) (TDD)

**Files:**
- Modify: `tools/citydsl.py`
- Test: `tools/tests/test_citydsl.py`

- [ ] **Step 1 : Ajouter le test qui échoue**

Ajouter à `tools/tests/test_citydsl.py` :
```python
def test_roadgrid_roads_crossings_pavement():
    # spacing 4, width 1, margin 0 -> routes aux x/y = 0,4,8...
    c = _compile("size 9 9\nfill grass\nroadgrid spacing 4 width 1\n"
                 "player 1 1 south\n")
    # croisement la ou colonne ET ligne sont des routes
    assert c.get(0, 0) == 3          # road_cross
    assert c.get(4, 4) == 3
    # colonne de route seule -> road_v
    assert c.get(0, 1) == 2          # road_v
    # ligne de route seule -> road_h
    assert c.get(1, 0) == 1          # road_h
    # case interieure adjacente a une route -> pavement
    assert c.get(1, 1) == 4          # pavement (voisin de (0,1) et (1,0))
    # case loin de toute route reste grass
    assert c.get(2, 2) == 0
```

- [ ] **Step 2 : Lancer (doit échouer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -k roadgrid -v`
Expected: FAIL (`commande inconnue: 'roadgrid'`).

- [ ] **Step 3 : Implémenter**

Ajouter en haut de `tools/citydsl.py` (après les imports) les fonctions utilitaires :
```python
def _kwargs(tokens):
    """Paire les tokens restants en dict {cle: valeur} (ex. ['spacing','4','width','1'])."""
    return {tokens[i]: tokens[i + 1] for i in range(0, len(tokens) - 1, 2)}


def _roadgrid(city, spacing, width, margin, tile_index, line_no):
    rv = _tile("road_v", tile_index, line_no)
    rh = _tile("road_h", tile_index, line_no)
    rc = _tile("road_cross", tile_index, line_no)
    pv = _tile("pavement", tile_index, line_no)
    if spacing < 1 or width < 1:
        raise CityError(line_no, "roadgrid: spacing et width doivent etre >= 1")

    cols, rows = set(), set()
    x0 = margin
    while x0 < city.w:
        for x in range(x0, x0 + width):
            if 0 <= x < city.w:
                cols.add(x)
        x0 += spacing
    y0 = margin
    while y0 < city.h:
        for y in range(y0, y0 + width):
            if 0 <= y < city.h:
                rows.add(y)
        y0 += spacing

    road_ids = {rv, rh, rc}
    for y in range(city.h):
        for x in range(city.w):
            on_col, on_row = x in cols, y in rows
            if on_col and on_row:
                city.set(x, y, rc)
            elif on_col:
                city.set(x, y, rv)
            elif on_row:
                city.set(x, y, rh)
    # trottoirs : toute case non-route adjacente (4-voisinage) a une route
    for y in range(city.h):
        for x in range(city.w):
            if city.get(x, y) in road_ids:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < city.w and 0 <= ny < city.h and city.get(nx, ny) in road_ids:
                    city.set(x, y, pv)
                    break
```

Dans `compile_city`, ajouter avant le `else` final :
```python
        elif cmd == "roadgrid":
            kv = _kwargs(tok[1:])
            if "spacing" not in kv or "width" not in kv:
                raise CityError(line_no, "usage: roadgrid spacing <S> width <W> [margin <M>]")
            spacing = _int(kv["spacing"], line_no, "spacing")
            width = _int(kv["width"], line_no, "width")
            margin = _int(kv.get("margin", "0"), line_no, "margin")
            _roadgrid(city, spacing, width, margin, tile_index, line_no)
```

- [ ] **Step 4 : Lancer (doit passer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -v`
Expected: PASS (tous).

- [ ] **Step 5 : Commit**

```bash
git add tools/citydsl.py tools/tests/test_citydsl.py
git commit -m "feat(m2): roadgrid (routes + croisements + trottoirs auto)"
```

---

## Task 4 : `blocks` + déterminisme PRNG (TDD)

**Files:**
- Modify: `tools/citydsl.py`
- Test: `tools/tests/test_citydsl.py`

- [ ] **Step 1 : Ajouter les tests qui échouent**

Ajouter à `tools/tests/test_citydsl.py` :
```python
def test_blocks_only_on_base_tile_and_deterministic():
    src = ("size 8 8\nseed 42\nfill grass\n"
           "rect water 0 0 7 0\n"                # ligne d'eau en haut
           "blocks building_a density 0.5 on grass\nplayer 0 1 south\n")
    c1 = _compile(src)
    c2 = _compile(src)
    assert c1.grid == c2.grid, "compilation non deterministe"
    # aucune eau n'a ete ecrasee par un building
    assert all(c1.get(x, 0) == 5 for x in range(8))
    # au moins un building a ete pose sur du grass
    assert any(v == 6 for v in c1.grid)


def test_blocks_default_base_is_grass():
    # density 1.0 -> tout le grass devient building_b (solide) ; on rouvre (0,0)
    # en grass apres coup pour pouvoir y spawner.
    c = _compile("size 6 6\nseed 1\nfill grass\n"
                 "blocks building_b density 1.0\n"
                 "rect grass 0 0 0 0\nplayer 0 0 south\n")
    assert c.get(0, 0) == 0                        # case rouverte en grass
    assert all(c.get(x, y) == 7 for y in range(6) for x in range(6)
               if not (x == 0 and y == 0))          # le reste = building_b
```

- [ ] **Step 2 : Lancer (doit échouer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -k blocks -v`
Expected: FAIL (`commande inconnue: 'blocks'`).

- [ ] **Step 3 : Implémenter**

Dans `compile_city`, ajouter avant le `else` final :
```python
        elif cmd == "blocks":
            t = _tile(tok[1], tile_index, line_no)
            kv = _kwargs(tok[2:])
            if "density" not in kv:
                raise CityError(line_no, "usage: blocks <tile> density <D> [on <baseTile>]")
            try:
                density = float(kv["density"])
            except ValueError:
                raise CityError(line_no, "density: nombre attendu, recu '%s'" % kv["density"])
            base_name = kv.get("on", "grass")
            base = _tile(base_name, tile_index, line_no)
            for y in range(city.h):
                for x in range(city.w):
                    if city.get(x, y) == base and rng.random() < density:
                        city.set(x, y, t)
```

> Le `rng` doit être initialisé **avant** la boucle de parsing et réassigné par `seed`. Vérifier que la ligne `rng = random.Random(1)` est bien au début de `compile_city` (posée en Task 1) et que `seed` fait `rng = random.Random(...)`.

- [ ] **Step 4 : Lancer (doit passer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -v`
Expected: PASS (tous).

- [ ] **Step 5 : Commit**

```bash
git add tools/citydsl.py tools/tests/test_citydsl.py
git commit -m "feat(m2): blocks (densite) + determinisme PRNG verifie"
```

---

## Task 5 : Validation finale (`player` obligatoire) + erreurs (TDD)

**Files:**
- Modify: `tools/citydsl.py`
- Test: `tools/tests/test_citydsl.py`

- [ ] **Step 1 : Ajouter les tests qui échouent**

Ajouter à `tools/tests/test_citydsl.py` :
```python
def test_player_required():
    with pytest.raises(CityError) as e:
        _compile("size 3 3\nfill grass\n")
    assert "player" in str(e.value)


def test_player_on_solid_is_error():
    with pytest.raises(CityError) as e:
        _compile("size 3 3\nfill water\nplayer 1 1 south\n")
    assert e.value.line_no == 3


def test_player_out_of_bounds():
    with pytest.raises(CityError):
        _compile("size 3 3\nfill grass\nplayer 9 9 south\n")


def test_bad_direction():
    with pytest.raises(CityError) as e:
        _compile("size 3 3\nfill grass\nplayer 0 0 up\n")
    assert "up" in str(e.value)
```

- [ ] **Step 2 : Lancer (doit échouer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -k player -v`
Expected: FAIL sur `test_player_required` (pas d'erreur levée — spawn `None` accepté).

- [ ] **Step 3 : Implémenter la validation finale**

Dans `tools/citydsl.py`, compléter `_finalize` :
```python
def _finalize(city, solid_index, tile_index):
    if city is None:
        raise CityError(0, "fichier vide: 'size' manquant")
    if city.spawn is None:
        raise CityError(0, "commande 'player' manquante")
    return city
```

> Les autres cas (`player` hors bornes, solide, direction inconnue) sont déjà gérés par le bloc `elif cmd == "player"` ajouté en Task 1 — ces tests passent sans code supplémentaire.

- [ ] **Step 4 : Lancer toute la suite citydsl (doit passer)**

Run: `python3 -m pytest tools/tests/test_citydsl.py -v`
Expected: PASS (tous : size/fill/rect, hline/vline/river, roadgrid, blocks/déterminisme, player/erreurs).

- [ ] **Step 5 : Commit**

```bash
git add tools/citydsl.py tools/tests/test_citydsl.py
git commit -m "feat(m2): validation finale (player obligatoire) + erreurs lignees"
```

---

## Task 6 : `build_city.py` — mapping tileset.csv + émission des headers (TDD)

**Files:**
- Create: `tools/build_city.py`
- Test: `tools/tests/test_build_city.py`

- [ ] **Step 1 : Écrire les tests qui échouent**

`tools/tests/test_build_city.py` :
```python
import csv
from tools import build_city as bc


def _write_tileset(path):
    rows = [
        ("grass", "g.png", "0"), ("road_h", "rh.png", "0"),
        ("road_v", "rv.png", "0"), ("road_cross", "rc.png", "0"),
        ("pavement", "pv.png", "0"), ("water", "w.png", "1"),
        ("building_a", "ba.png", "1"), ("building_b", "bb.png", "1"),
    ]
    with open(path, "w", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(["name", "source", "solid"])
        wr.writerows(rows)


def test_load_tiles_maps_index_and_solid(tmp_path):
    ts = tmp_path / "tileset.csv"
    _write_tileset(ts)
    tile_index, solid_index, names = bc.load_tiles(str(ts))
    assert tile_index["grass"] == 0 and tile_index["building_b"] == 7
    assert solid_index == {5, 6, 7}
    assert names[3] == "road_cross"


def test_emit_headers(tmp_path):
    ts = tmp_path / "tileset.csv"; _write_tileset(ts)
    tile_index, solid_index, _ = bc.load_tiles(str(ts))
    from tools.citydsl import compile_city
    city = compile_city("size 3 2\nfill grass\nrect water 0 0 0 0\n"
                         "player 2 1 east\n", tile_index, solid_index)
    out_h = tmp_path / "citymap.h"
    out_cpp = tmp_path / "citymap_data.cpp"
    bc.emit_headers(city, str(out_h), str(out_cpp))

    h = out_h.read_text()
    assert "#define CITY_W 3" in h and "#define CITY_H 2" in h
    assert "#define PLAYER_START_X 2" in h
    assert "#define PLAYER_START_Y 1" in h
    assert "#define PLAYER_START_DIR DIR_EAST" in h
    assert "extern const uint8_t cityMap[CITY_H*CITY_W];" in h

    cpp = out_cpp.read_text()
    assert '#include "citymap.h"' in cpp
    assert "const uint8_t cityMap[CITY_H*CITY_W] = {" in cpp
    # 6 valeurs (3x2) ; (0,0)=water=5, le reste grass=0
    assert cpp.count("5") >= 1 and "0, 0" in cpp
```

- [ ] **Step 2 : Lancer (doit échouer)**

Run: `python3 -m pytest tools/tests/test_build_city.py -v`
Expected: FAIL (`No module named 'tools.build_city'`).

- [ ] **Step 3 : Implémenter `build_city.py` (sans le PNG, ajouté en Task 7)**

`tools/build_city.py` :
```python
"""city/city.txt -> gta/citymap.{h,cpp} + city/citymap.png.

Lit le mapping nom->index depuis assets/tileset.csv (ordre = enum de assets.h).
"""
import os
import csv
from tools.citydsl import compile_city, CityError

TILESET_CSV = "assets/tileset.csv"
CITY_TXT = "city/city.txt"
OUT_H = "gta/citymap.h"
OUT_CPP = "gta/citymap_data.cpp"
OUT_PNG = "city/citymap.png"
TILES8_DIR = "assets/tiles8"
DIR_NAMES = ["DIR_NORTH", "DIR_EAST", "DIR_SOUTH", "DIR_WEST"]


def load_tiles(tileset_csv):
    """Retourne (tile_index{nom:idx}, solid_index{idx}, names[idx]->nom)."""
    with open(tileset_csv) as f:
        rows = list(csv.DictReader(f))
    tile_index = {r["name"]: i for i, r in enumerate(rows)}
    solid_index = {i for i, r in enumerate(rows) if r["solid"] == "1"}
    names = [r["name"] for r in rows]
    return tile_index, solid_index, names


def emit_headers(city, out_h, out_cpp):
    os.makedirs(os.path.dirname(out_h) or ".", exist_ok=True)
    sx, sy, sdir = city.spawn
    with open(out_h, "w") as f:
        f.write("// genere par tools/build_city.py -- NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define CITY_W %d\n#define CITY_H %d\n" % (city.w, city.h))
        f.write("#define PLAYER_START_X %d\n" % sx)
        f.write("#define PLAYER_START_Y %d\n" % sy)
        f.write("#define PLAYER_START_DIR %s\n\n" % DIR_NAMES[sdir])
        f.write("extern const uint8_t cityMap[CITY_H*CITY_W];\n")
    with open(out_cpp, "w") as f:
        f.write("// genere par tools/build_city.py -- NE PAS editer\n")
        f.write('#include "citymap.h"\n\n')
        f.write("const uint8_t cityMap[CITY_H*CITY_W] = {\n")
        for y in range(city.h):
            row = city.grid[y * city.w:(y + 1) * city.w]
            f.write("  " + ", ".join(str(v) for v in row) + ",\n")
        f.write("};\n")


def main():
    tile_index, solid_index, names = load_tiles(TILESET_CSV)
    with open(CITY_TXT) as f:
        text = f.read()
    try:
        city = compile_city(text, tile_index, solid_index)
    except CityError as e:
        raise SystemExit("ERREUR city.txt %s" % e)
    emit_headers(city, OUT_H, OUT_CPP)
    render_png(city, names, TILES8_DIR, OUT_PNG)  # defini en Task 7
    print("genere: %s, %s, %s (%dx%d, spawn=%s)"
          % (OUT_H, OUT_CPP, OUT_PNG, city.w, city.h, city.spawn))


if __name__ == "__main__":
    main()
```

> `render_png` est appelée par `main()` mais définie en Task 7. Tant que Task 7 n'est pas faite, **ne pas lancer `python3 -m tools.build_city`** ; les tests de Task 6 n'appellent que `load_tiles` et `emit_headers`, donc ils passent.

- [ ] **Step 4 : Lancer (doit passer)**

Run: `python3 -m pytest tools/tests/test_build_city.py -v`
Expected: PASS (2 tests).

- [ ] **Step 5 : Commit**

```bash
git add tools/build_city.py tools/tests/test_build_city.py
git commit -m "feat(m2): build_city load_tiles + emit_headers (citymap.h/cpp)"
```

---

## Task 7 : Rendu `city/citymap.png` (TDD)

**Files:**
- Modify: `tools/build_city.py`
- Test: `tools/tests/test_build_city.py`

- [ ] **Step 1 : Ajouter le test qui échoue**

Ajouter à `tools/tests/test_build_city.py` :
```python
from PIL import Image
from tools.citydsl import compile_city


def test_render_png_dimensions(tmp_path):
    # mini tiles8 : un PNG 8x8 par nom
    tiles8 = tmp_path / "tiles8"; tiles8.mkdir()
    names = ["grass", "road_h", "road_v", "road_cross",
             "pavement", "water", "building_a", "building_b"]
    colors = [(0, 200, 0), (80, 80, 80), (80, 80, 80), (60, 60, 60),
              (180, 180, 180), (0, 0, 200), (150, 60, 60), (60, 60, 150)]
    for n, col in zip(names, colors):
        Image.new("RGB", (8, 8), col).save(tiles8 / (n + ".png"))

    tile_index = {n: i for i, n in enumerate(names)}
    city = compile_city("size 4 3\nfill grass\nplayer 0 0 south\n",
                        tile_index, {5, 6, 7})
    out_png = tmp_path / "citymap.png"
    bc.render_png(city, names, str(tiles8), str(out_png))

    assert out_png.exists()
    with Image.open(out_png) as im:
        assert im.size == (4 * 8, 3 * 8)   # 32x24
```

- [ ] **Step 2 : Lancer (doit échouer)**

Run: `python3 -m pytest tools/tests/test_build_city.py -k render -v`
Expected: FAIL (`AttributeError: module 'tools.build_city' has no attribute 'render_png'`).

- [ ] **Step 3 : Implémenter `render_png`**

Ajouter dans `tools/build_city.py` (avant `main`), et l'import PIL en haut du fichier :
```python
from PIL import Image, ImageDraw
```
```python
def render_png(city, names, tiles8_dir, out_png):
    """Rend la ville entiere en PNG (8 px/tuile) + marqueur de spawn."""
    cache = {}
    for n in names:
        with Image.open(os.path.join(tiles8_dir, n + ".png")) as im:
            cache[n] = im.convert("RGB").resize((8, 8), Image.Resampling.NEAREST)
    canvas = Image.new("RGB", (city.w * 8, city.h * 8))
    for y in range(city.h):
        for x in range(city.w):
            canvas.paste(cache[names[city.get(x, y)]], (x * 8, y * 8))
    # marqueur de spawn : croix magenta sur la tuile de depart
    sx, sy, _ = city.spawn
    d = ImageDraw.Draw(canvas)
    cx, cy = sx * 8 + 4, sy * 8 + 4
    d.line((cx - 3, cy, cx + 3, cy), fill=(255, 0, 255))
    d.line((cx, cy - 3, cx, cy + 3), fill=(255, 0, 255))
    os.makedirs(os.path.dirname(out_png) or ".", exist_ok=True)
    canvas.save(out_png)
```

- [ ] **Step 4 : Lancer (doit passer)**

Run: `python3 -m pytest tools/tests/test_build_city.py -v`
Expected: PASS (3 tests).

- [ ] **Step 5 : Commit**

```bash
git add tools/build_city.py tools/tests/test_build_city.py
git commit -m "feat(m2): render_png (citymap.png + marqueur de spawn)"
```

---

## Task 8 : Écrire `city/city.txt` + générer la vraie ville (donnée d'auteur)

**Files:**
- Create: `city/city.txt`
- Generate: `gta/citymap.h`, `gta/citymap_data.cpp`, `city/citymap.png`

- [ ] **Step 1 : Écrire `city/city.txt`**

```bash
mkdir -p city
```

`city/city.txt` (ville de démarrage : grille de rues, une rivière, des bâtiments, spawn sur trottoir) :
```
; Liberty City (demake) — carte de demarrage
; tuiles disponibles : grass road_h road_v road_cross pavement water building_a building_b
size 96 96
seed 7

fill grass

; le fleuve qui borde l'est de la ville
river vertical 86 width 10

; grille de rues : un bloc tous les 12, rues larges de 2, marge de 3
roadgrid spacing 12 width 2 margin 3

; remplir les ilots de batiments (uniquement sur l'herbe restante)
blocks building_a density 0.55 on grass
blocks building_b density 0.45 on grass

; un petit parc (herbe) au centre pour varier
rect grass 40 40 52 52

; spawn du pieton sur un trottoir, oriente sud
player 5 5 south
```

> Note : la valeur exacte des coordonnées (spawn, parc, fleuve) est ajustable. Si `build_city` signale `spawn sur tuile solide` ou `hors bornes`, déplacer le `player` sur une case `pavement`/`grass` (le marqueur magenta dans `citymap.png` confirme l'emplacement).

- [ ] **Step 2 : Générer la ville**

Run: `python3 -m tools.build_city`
Expected: `genere: gta/citymap.h, gta/citymap_data.cpp, city/citymap.png (96x96, spawn=(5, 5, 2))`
(Si erreur `CityError`, corriger `city/city.txt` d'après le message ligné, puis relancer.)

- [ ] **Step 3 : Vérifications visuelles + structurelles**

```bash
head -n 12 gta/citymap.h
wc -l gta/citymap_data.cpp
```
Vérifs :
- `gta/citymap.h` : `CITY_W 96`, `CITY_H 96`, `PLAYER_START_*` cohérents, `PLAYER_START_DIR DIR_SOUTH`.
- Ouvrir `city/citymap.png` (outil Read) : grille de rues lisible, croisements aux intersections, trottoirs bordant les routes, fleuve à l'est, bâtiments dans les îlots, marqueur magenta sur le spawn (sur trottoir, pas dans un mur/eau).

- [ ] **Step 4 : Lancer toute la suite M2**

Run: `python3 -m pytest tools/tests -v`
Expected: PASS (gbimg, catalog, tileset_valid, build_assets de M1 + citydsl, build_city de M2).

- [ ] **Step 5 : Commit**

```bash
git add city/city.txt gta/citymap.h gta/citymap_data.cpp city/citymap.png
git commit -m "feat(m2): city.txt de demarrage + generation citymap.{h,cpp,png}"
```

---

## Vérification finale M2

- [ ] `python3 -m pytest tools/tests -v` → tout vert (M1 + M2).
- [ ] `tools/citydsl.py` : toutes les commandes (`size, seed, fill, rect, hline, vline, roadgrid, river, blocks, player`) testées ; erreurs avec n° de ligne ; déterminisme prouvé par test.
- [ ] `gta/citymap.h` : `CITY_W/CITY_H`, `PLAYER_START_X/Y/DIR`, déclaration `extern cityMap[]`.
- [ ] `gta/citymap_data.cpp` : `cityMap[CITY_H*CITY_W]` clos (compile-ready), valeurs ∈ [0, NUM_TILES-1].
- [ ] `city/citymap.png` inspecté : ville cohérente, spawn valide.

**Livrable M2 :** `city/city.txt` (config éditable) + `tools/{citydsl,build_city}.py` + `gta/citymap.{h,cpp}` + `city/citymap.png`. Prêt à être consommé par **M3** (le viewer pygame chargera `cityMap` + `tiles8/` + spawn) et **M4** (le sketch inclura `citymap.h/cpp`).
</content>
</invoke>
