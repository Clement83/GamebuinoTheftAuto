# GTA Gamebuino Demake — Plan M1 : Pipeline d'assets

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convertir les BMP GTA1 en un tileset 8×8 RGB565 + un sprite perso, nommés et exposés en headers C++ Gamebuino, avec un catalogue navigable des 577 images.

**Architecture:** Pipeline Python déterministe. `catalog.py` inventorie et range les 577 BMP ; une étape de **curation (vision)** nomme le sous-ensemble requis dans `assets/tileset.csv` + `assets/named/**` ; `build_assets.py` convertit en `gta/assets.h` + `gta/assets_data.cpp` (tileset RGB565, flags de collision, frames perso) et en PNG 8×8 pour la future preview.

**Tech Stack:** Python 3, Pillow (PIL), pytest. Sortie C++ pour Gamebuino-META en **tableaux `const uint16_t` RGB565 simples** (rendu via `drawPixel` côté sketch → aucune dépendance au format binaire `Image` de la lib).

**Portée :** ce plan couvre **M1 uniquement** (spec §5.1, §5.2, §5.3). M2 (compilo ville), M3 (preview), M4 (sketch) sont des plans séparés.

**Convention d'exécution :** toutes les commandes se lancent depuis la **racine du dépôt** (`/home/clement/Documents/private/projet/gamebuino/gta`). Les modules s'invoquent avec `python3 -m tools.<module>`.

---

## Structure de fichiers (M1)

- `conftest.py` — ajoute la racine au `sys.path` pour les tests.
- `tools/__init__.py` — marque `tools` comme package (vide).
- `tools/tests/__init__.py` — vide.
- `tools/gbimg.py` — helpers conversion : `rgb565()`, `load_rgb()`, `downscale()`.
- `tools/catalog.py` — scan BMP → `assets/catalog.csv` + planches-contact.
- `tools/validate_tileset.py` — vérifie que la curation est complète.
- `tools/build_assets.py` — `assets/tileset.csv` + `named/**` → `gta/assets.{h,cpp}` + `assets/tiles8/`.
- `tools/tests/test_gbimg.py`, `test_catalog.py`, `test_build_assets.py`, `test_tileset_valid.py`.
- **Données produites par la curation :** `assets/tileset.csv`, `assets/named/tiles/*.png`, `assets/named/sprites/player_*.png`.
- **Sorties générées :** `assets/catalog.csv`, `assets/contact/*.png`, `gta/assets.h`, `gta/assets_data.cpp`, `assets/tiles8/*.png`.

**Contrat de noms de tuiles (partagé avec M2/M4) — ordre = index `enum` :**
`grass, road_h, road_v, road_cross, pavement, water, building_a, building_b` (+ variantes optionnelles ensuite).
**Tuiles solides (collision) :** `water, building_a, building_b`. Les autres sont praticables.

---

## Task 0 : Scaffold + vérif environnement

**Files:**
- Create: `conftest.py`, `tools/__init__.py`, `tools/tests/__init__.py`

- [ ] **Step 1 : Vérifier Python, PIL, pytest**

Run:
```bash
python3 -c "import PIL, sys; print('PIL', PIL.__version__, 'py', sys.version.split()[0])"
python3 -m pytest --version || pip install --user pytest
```
Expected : `PIL 10.2.0 ...` puis une version de pytest (sinon installation).

- [ ] **Step 2 : Créer le scaffold de package**

```bash
mkdir -p tools/tests assets
```

`conftest.py` (racine) :
```python
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
```

`tools/__init__.py` : *(fichier vide)*
`tools/tests/__init__.py` : *(fichier vide)*

- [ ] **Step 3 : Commit**

```bash
git add conftest.py tools/__init__.py tools/tests/__init__.py
git commit -m "chore(m1): scaffold du package tools + conftest"
```

---

## Task 1 : `gbimg.py` — conversion RGB565 + downscale (TDD)

**Files:**
- Create: `tools/gbimg.py`
- Test: `tools/tests/test_gbimg.py`

- [ ] **Step 1 : Écrire le test qui échoue**

`tools/tests/test_gbimg.py` :
```python
from tools.gbimg import rgb565

def test_rgb565_primaries():
    assert rgb565(255, 0, 0) == 0xF800   # rouge pur
    assert rgb565(0, 255, 0) == 0x07E0   # vert pur
    assert rgb565(0, 0, 255) == 0x001F   # bleu pur
    assert rgb565(0, 0, 0) == 0x0000
    assert rgb565(255, 255, 255) == 0xFFFF

def test_rgb565_truncation():
    # 5/6/5 bits : les bits de poids faible sont jetés
    assert rgb565(7, 3, 7) == 0x0000
    assert rgb565(8, 4, 8) == ((1 << 11) | (1 << 5) | 1)
```

- [ ] **Step 2 : Lancer le test (doit échouer)**

Run: `python3 -m pytest tools/tests/test_gbimg.py -v`
Expected: FAIL (`ModuleNotFoundError: No module named 'tools.gbimg'`).

- [ ] **Step 3 : Implémenter `gbimg.py`**

`tools/gbimg.py` :
```python
"""Helpers de conversion d'images pour le pipeline Gamebuino."""
from PIL import Image


def rgb565(r, g, b):
    """Pack un triplet RGB 8 bits en RGB565 (entier 16 bits)."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def load_rgb(path):
    """Ouvre une image (BMP/PNG, palettisée ou non) en mode RGB."""
    with Image.open(path) as im:
        return im.convert("RGB")


def load_rgba_keyed(path):
    """Ouvre en RGBA en rendant transparente la couleur du pixel (0,0)."""
    with Image.open(path) as im:
        rgba = im.convert("RGBA")
    bg = rgba.getpixel((0, 0))
    px = rgba.load()
    w, h = rgba.size
    for y in range(h):
        for x in range(w):
            if px[x, y][:3] == bg[:3]:
                px[x, y] = (0, 0, 0, 0)
    return rgba


def downscale(img, w, h):
    """Réduit en (w,h) par moyenne de zone (BOX) — propre pour du /8."""
    return img.resize((w, h), Image.Resampling.BOX)
```

- [ ] **Step 4 : Lancer le test (doit passer)**

Run: `python3 -m pytest tools/tests/test_gbimg.py -v`
Expected: PASS (2 tests).

- [ ] **Step 5 : Commit**

```bash
git add tools/gbimg.py tools/tests/test_gbimg.py
git commit -m "feat(m1): helpers gbimg (rgb565, load_rgb, downscale)"
```

---

## Task 2 : `catalog.py` — inventaire + planches-contact

**Files:**
- Create: `tools/catalog.py`
- Test: `tools/tests/test_catalog.py`

- [ ] **Step 1 : Écrire le test qui échoue (fonctions pures)**

`tools/tests/test_catalog.py` :
```python
from tools.catalog import id_of, guess_category

def test_id_of():
    assert id_of("a/b/GTASPR1_1000.bmp") == 1000
    assert id_of("GTATIL1_7.bmp") == 7

def test_guess_category():
    assert guess_category("tiles", 64, 64, 5) == "tile"
    assert guess_category("tiles", 32, 32, 5) == "tile_other"
    assert guess_category("sprites", 220, 18, 2) == "banner"
    assert guess_category("sprites", 24, 24, 1000) == "fx_explosion"
    assert guess_category("sprites", 20, 24, 5) == "small_object_or_ped"
    assert guess_category("sprites", 60, 50, 5) == "sprite"
```

- [ ] **Step 2 : Lancer le test (doit échouer)**

Run: `python3 -m pytest tools/tests/test_catalog.py -v`
Expected: FAIL (`No module named 'tools.catalog'`).

- [ ] **Step 3 : Implémenter `catalog.py`**

`tools/catalog.py` :
```python
"""Inventaire des BMP source -> assets/catalog.csv + planches-contact."""
import os, glob, csv
from PIL import Image, ImageDraw, ImageFont

TILES_DIR = "GTA_textures_LC/LC/tiles"
SPR_DIR = "GTA_textures_LC/LC/sprites"
OUT_CSV = "assets/catalog.csv"
CONTACT_DIR = "assets/contact"


def id_of(path):
    base = os.path.basename(path)
    digits = "".join(c for c in base.split("_")[-1] if c.isdigit())
    return int(digits or 0)


def guess_category(folder, w, h, idnum):
    if folder == "tiles":
        return "tile" if (w, h) == (64, 64) else "tile_other"
    if 992 <= idnum <= 1030:
        return "fx_explosion"
    if w >= 200 or (h and w / h >= 3):
        return "banner"
    if max(w, h) <= 28:
        return "small_object_or_ped"
    return "sprite"


def _font(sz):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


def scan():
    rows = []
    for folder, d in (("tiles", TILES_DIR), ("sprites", SPR_DIR)):
        for p in sorted(glob.glob(d + "/*.bmp"), key=id_of):
            with Image.open(p) as im:
                w, h = im.size
            rows.append({"id": id_of(p), "folder": folder, "w": w, "h": h,
                         "category_guess": guess_category(folder, w, h, id_of(p)),
                         "name": ""})
    return rows


def write_csv(rows):
    os.makedirs(os.path.dirname(OUT_CSV), exist_ok=True)
    with open(OUT_CSV, "w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=["id", "folder", "w", "h",
                                           "category_guess", "name"])
        wr.writeheader()
        wr.writerows(rows)


def _sheet(files, out, cols=6, cell=96):
    font = _font(12)
    rows = (len(files) + cols - 1) // cols
    lab = 15
    canvas = Image.new("RGB", (cols * (cell + 6), rows * (cell + lab + 6)),
                       (128, 128, 128))
    d = ImageDraw.Draw(canvas)
    for i, fpath in enumerate(files):
        r, c = divmod(i, cols)
        x, y = c * (cell + 6) + 3, r * (cell + lab + 6) + 3
        with Image.open(fpath) as im:
            im = im.convert("RGB")
            w, h = im.size
            if w > cell or h > cell:
                s = min(cell / w, cell / h)
                im = im.resize((max(1, int(w * s)), max(1, int(h * s))),
                               Image.Resampling.NEAREST)
                w, h = im.size
            canvas.paste(im, (x + (cell - w) // 2, y + (cell - h) // 2))
        d.text((x, y + cell + 1), str(id_of(fpath)), fill=(0, 0, 0), font=font)
    canvas.save(out)


def contact_sheets(per_sheet=36):
    os.makedirs(CONTACT_DIR, exist_ok=True)
    for folder, d in (("tiles", TILES_DIR), ("sprites", SPR_DIR)):
        files = sorted(glob.glob(d + "/*.bmp"), key=id_of)
        for s in range(0, len(files), per_sheet):
            _sheet(files[s:s + per_sheet],
                   f"{CONTACT_DIR}/{folder}_{s // per_sheet:02d}.png")


if __name__ == "__main__":
    rows = scan()
    write_csv(rows)
    contact_sheets()
    print(f"catalog: {len(rows)} lignes -> {OUT_CSV} ; planches -> {CONTACT_DIR}/")
```

- [ ] **Step 4 : Lancer le test (doit passer)**

Run: `python3 -m pytest tools/tests/test_catalog.py -v`
Expected: PASS (2 tests).

- [ ] **Step 5 : Générer le catalogue réel**

Run: `python3 -m tools.catalog`
Expected: `catalog: 577 lignes -> assets/catalog.csv ; planches -> assets/contact/`
Vérif: `wc -l assets/catalog.csv` → 578 (header + 577) ; `ls assets/contact/ | wc -l` → ~17 planches.

- [ ] **Step 6 : Commit**

```bash
git add tools/catalog.py tools/tests/test_catalog.py assets/catalog.csv assets/contact
git commit -m "feat(m1): catalog.py (inventaire 577 BMP + planches-contact)"
```

---

## Task 3 : Curation — nommer le sous-ensemble (vision)

**But :** produire `assets/tileset.csv` + les PNG nommés. **Tâche guidée par la vision** (pas de TDD ; verrouillée par une validation à la fin).

**Files:**
- Create: `assets/tileset.csv`
- Create: `assets/named/tiles/*.png`, `assets/named/sprites/player_a.png`, `player_b.png`
- Create: `tools/validate_tileset.py`, `tools/tests/test_tileset_valid.py`

- [ ] **Step 1 : Regarder les planches-contact**

Ouvrir chaque `assets/contact/tiles_*.png` et `sprites_*.png` (outil Read sur les PNG). Repérer, par leur **id**, des tuiles 64×64 correspondant aux catégories du contrat de noms :
- `grass` (herbe/parc uni), `pavement` (trottoir/béton),
- `road_h` (route + ligne horizontale), `road_v` (route + ligne verticale), `road_cross` (croisement),
- `water` (eau/mer), `building_a` et `building_b` (deux façades/toits distincts).
- Pour le perso : deux sprites de **piéton** lisibles (`player_a`, `player_b`).

- [ ] **Step 2 : Copier les sources choisies en PNG nommés**

Pour chaque tuile retenue (exemple, à adapter aux id réellement repérés) :
```bash
mkdir -p assets/named/tiles assets/named/sprites
# Gabarit (remplacer <ID> par l'id repéré) :
python3 -c "from PIL import Image; Image.open('GTA_textures_LC/LC/tiles/GTATIL1_<ID>.bmp').convert('RGB').save('assets/named/tiles/grass.png')"
# ... idem pour road_h, road_v, road_cross, pavement, water, building_a, building_b
# Perso :
python3 -c "from PIL import Image; Image.open('GTA_textures_LC/LC/sprites/GTASPR1_<ID>.bmp').convert('RGB').save('assets/named/sprites/player_a.png')"
python3 -c "from PIL import Image; Image.open('GTA_textures_LC/LC/sprites/GTASPR1_<ID>.bmp').convert('RGB').save('assets/named/sprites/player_b.png')"
```

- [ ] **Step 3 : Écrire `assets/tileset.csv`**

Colonnes `name,source,solid`. **L'ordre des lignes fixe l'index `enum`.** Les noms requis d'abord :
```csv
name,source,solid
grass,assets/named/tiles/grass.png,0
road_h,assets/named/tiles/road_h.png,0
road_v,assets/named/tiles/road_v.png,0
road_cross,assets/named/tiles/road_cross.png,0
pavement,assets/named/tiles/pavement.png,0
water,assets/named/tiles/water.png,1
building_a,assets/named/tiles/building_a.png,1
building_b,assets/named/tiles/building_b.png,1
```

- [ ] **Step 4 : Écrire la validation + son test**

`tools/validate_tileset.py` :
```python
"""Valide que la curation du tileset est complète et cohérente."""
import os, csv

TILESET_CSV = "assets/tileset.csv"
REQUIRED = ["grass", "road_h", "road_v", "road_cross",
            "pavement", "water", "building_a", "building_b"]
SOLID_EXPECTED = {"water", "building_a", "building_b"}
PLAYER_FRAMES = ["assets/named/sprites/player_a.png",
                 "assets/named/sprites/player_b.png"]


def validate():
    errors = []
    if not os.path.exists(TILESET_CSV):
        return [f"{TILESET_CSV} manquant"]
    with open(TILESET_CSV) as f:
        rows = list(csv.DictReader(f))
    names = [r["name"] for r in rows]
    for req in REQUIRED:
        if req not in names:
            errors.append(f"tuile requise absente: {req}")
    for r in rows:
        if not os.path.exists(r["source"]):
            errors.append(f"source introuvable: {r['source']} ({r['name']})")
        if r["name"] in SOLID_EXPECTED and r["solid"] != "1":
            errors.append(f"{r['name']} devrait etre solide (solid=1)")
    for p in PLAYER_FRAMES:
        if not os.path.exists(p):
            errors.append(f"frame perso manquante: {p}")
    return errors


if __name__ == "__main__":
    errs = validate()
    if errs:
        print("INVALIDE:\n  " + "\n  ".join(errs))
        raise SystemExit(1)
    print("tileset.csv valide")
```

`tools/tests/test_tileset_valid.py` :
```python
from tools.validate_tileset import validate

def test_curation_complete():
    errs = validate()
    assert errs == [], "Curation incomplete:\n" + "\n".join(errs)
```

- [ ] **Step 5 : Lancer la validation**

Run: `python3 -m pytest tools/tests/test_tileset_valid.py -v`
Expected: PASS (échoue tant que la curation n'est pas complète → corriger les id/sources jusqu'au vert).

- [ ] **Step 6 : Commit**

```bash
git add assets/tileset.csv assets/named tools/validate_tileset.py tools/tests/test_tileset_valid.py
git commit -m "feat(m1): curation du sous-ensemble (tileset.csv + PNG nommes + perso)"
```

---

## Task 4 : `build_assets.py` — génération des headers C++ + tiles8

**Files:**
- Create: `tools/build_assets.py`
- Test: `tools/tests/test_build_assets.py`
- Generate: `gta/assets.h`, `gta/assets_data.cpp`, `assets/tiles8/*.png`

- [ ] **Step 1 : Écrire le test qui échoue**

`tools/tests/test_build_assets.py` :
```python
import os, csv, re
from PIL import Image
from tools import build_assets as ba


def _make_solid_png(path, color, size=64):
    Image.new("RGB", (size, size), color).save(path)


def test_build_assets_end_to_end(tmp_path, monkeypatch):
    # Mini-jeu de données : 1 tuile rouge non-solide, 1 bleue solide
    tiles = tmp_path / "named" / "tiles"
    spr = tmp_path / "named" / "sprites"
    tiles.mkdir(parents=True); spr.mkdir(parents=True)
    _make_solid_png(tiles / "grass.png", (255, 0, 0))
    _make_solid_png(tiles / "water.png", (0, 0, 255))
    _make_solid_png(spr / "player_a.png", (0, 255, 0), size=24)
    _make_solid_png(spr / "player_b.png", (0, 255, 0), size=24)
    csv_path = tmp_path / "tileset.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.writer(f); w.writerow(["name", "source", "solid"])
        w.writerow(["grass", str(tiles / "grass.png"), "0"])
        w.writerow(["water", str(tiles / "water.png"), "1"])

    out_h = tmp_path / "assets.h"
    out_cpp = tmp_path / "assets_data.cpp"
    tiles8 = tmp_path / "tiles8"
    ba.build(str(csv_path),
             [str(spr / "player_a.png"), str(spr / "player_b.png")],
             str(out_h), str(out_cpp), str(tiles8))

    h = out_h.read_text()
    assert "enum TileId" in h
    assert "TILE_GRASS" in h and "TILE_WATER" in h
    assert "#define NUM_TILES 2" in h

    cpp = out_cpp.read_text()
    # tuile 0 (grass) : 64 px rouges = 0xF800
    vals = re.findall(r"0x[0-9A-Fa-f]{4}", cpp)
    assert vals, "aucune valeur RGB565 emise"
    assert cpp.count("0xF800") >= 64   # grass tout rouge
    assert "const uint8_t tileFlags[NUM_TILES] = {0, 1};" in cpp

    assert (tiles8 / "grass.png").exists()
    with Image.open(tiles8 / "grass.png") as im:
        assert im.size == (8, 8)
```

- [ ] **Step 2 : Lancer le test (doit échouer)**

Run: `python3 -m pytest tools/tests/test_build_assets.py -v`
Expected: FAIL (`No module named 'tools.build_assets'` ou `AttributeError: build`).

- [ ] **Step 3 : Implémenter `build_assets.py`**

`tools/build_assets.py` :
```python
"""tileset.csv + PNG nommes -> gta/assets.{h,cpp} + assets/tiles8/."""
import os, csv
from PIL import Image
from tools.gbimg import rgb565, load_rgb, load_rgba_keyed, downscale

TILESET_CSV = "assets/tileset.csv"
PLAYER_FRAMES = ["assets/named/sprites/player_a.png",
                 "assets/named/sprites/player_b.png"]
OUT_H = "gta/assets.h"
OUT_CPP = "gta/assets_data.cpp"
TILES8_DIR = "assets/tiles8"
TILE = 8
PLAYER = 8
TRANSPARENT = 0xF81F  # magenta = couleur transparente perso
# DIR order: NORTH, EAST, SOUTH, WEST. Source perso suppose orientee SUD.
DIR_ANGLES = [180, 90, 0, 270]  # rotation PIL (CCW) par direction


def _tile_pixels(path):
    img = downscale(load_rgb(path), TILE, TILE)
    return img, [rgb565(*p) for p in img.getdata()]


def _player_frame_pixels(path, angle):
    rgba = load_rgba_keyed(path)
    rgba = rgba.rotate(angle, expand=False)
    rgba = downscale(rgba, PLAYER, PLAYER)
    out = []
    for (r, g, b, a) in rgba.getdata():
        out.append(TRANSPARENT if a < 128 else rgb565(r, g, b))
    return out


def _fmt(vals, per_line=8):
    lines = []
    for i in range(0, len(vals), per_line):
        lines.append("  " + ", ".join("0x%04X" % v for v in vals[i:i + per_line]))
    return ",\n".join(lines)


def build(tileset_csv, player_frames, out_h, out_cpp, tiles8_dir):
    with open(tileset_csv) as f:
        rows = list(csv.DictReader(f))
    os.makedirs(tiles8_dir, exist_ok=True)
    os.makedirs(os.path.dirname(out_h) or ".", exist_ok=True)

    names = [r["name"] for r in rows]
    flags = [1 if r["solid"] == "1" else 0 for r in rows]
    tiles_px = []
    for r in rows:
        img, px = _tile_pixels(r["source"])
        tiles_px.append(px)
        img.save(os.path.join(tiles8_dir, r["name"] + ".png"))

    # Perso : 4 directions x N frames
    player_px = []  # [dir][frame] -> liste de pixels
    for angle in DIR_ANGLES:
        per_dir = [_player_frame_pixels(p, angle) for p in player_frames]
        player_px.append(per_dir)
    nframes = len(player_frames)

    # ---- assets.h ----
    enum = ",\n  ".join("TILE_%s" % n.upper() for n in names)
    with open(out_h, "w") as f:
        f.write("// genere par tools/build_assets.py — NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define NUM_TILES %d\n" % len(rows))
        f.write("#define TILE_W %d\n#define TILE_H %d\n" % (TILE, TILE))
        f.write("#define PLAYER_W %d\n#define PLAYER_H %d\n" % (PLAYER, PLAYER))
        f.write("#define PLAYER_FRAMES %d\n" % nframes)
        f.write("#define PLAYER_TRANSPARENT 0x%04X\n\n" % TRANSPARENT)
        f.write("enum TileId : uint8_t {\n  %s\n};\n\n" % enum)
        f.write("enum Dir : uint8_t { DIR_NORTH=0, DIR_EAST, DIR_SOUTH, DIR_WEST };\n\n")
        f.write("#define TILE_SOLID 0x01\n")
        f.write("extern const uint16_t tileset[NUM_TILES][TILE_W*TILE_H];\n")
        f.write("extern const uint8_t tileFlags[NUM_TILES];\n")
        f.write("extern const uint16_t playerSprite[4][PLAYER_FRAMES][PLAYER_W*PLAYER_H];\n")

    # ---- assets_data.cpp ----
    with open(out_cpp, "w") as f:
        f.write('// genere par tools/build_assets.py — NE PAS editer\n')
        f.write('#include "assets.h"\n\n')
        f.write("const uint16_t tileset[NUM_TILES][TILE_W*TILE_H] = {\n")
        for n, px in zip(names, tiles_px):
            f.write("  { // %s\n%s\n  },\n" % (n, _fmt(px)))
        f.write("};\n\n")
        f.write("const uint8_t tileFlags[NUM_TILES] = {%s};\n\n"
                % ", ".join(str(x) for x in flags))
        f.write("const uint16_t playerSprite[4][PLAYER_FRAMES][PLAYER_W*PLAYER_H] = {\n")
        dirnames = ["NORTH", "EAST", "SOUTH", "WEST"]
        for di, per_dir in enumerate(player_px):
            f.write("  { // DIR_%s\n" % dirnames[di])
            for fr in per_dir:
                f.write("    {\n%s\n    },\n" % _fmt(fr))
            f.write("  },\n")
        f.write("};\n")


if __name__ == "__main__":
    build(TILESET_CSV, PLAYER_FRAMES, OUT_H, OUT_CPP, TILES8_DIR)
    print("genere: %s, %s, %s/" % (OUT_H, OUT_CPP, TILES8_DIR))
```

- [ ] **Step 4 : Lancer le test (doit passer)**

Run: `python3 -m pytest tools/tests/test_build_assets.py -v`
Expected: PASS (1 test).

- [ ] **Step 5 : Générer les assets réels**

Run: `python3 -m tools.build_assets`
Expected: `genere: gta/assets.h, gta/assets_data.cpp, assets/tiles8/`
Vérif: `ls assets/tiles8/` (un PNG 8×8 par tuile) ; ouvrir quelques tiles8 PNG (Read) pour juger la lisibilité à 8×8.

- [ ] **Step 6 : Lancer toute la suite M1**

Run: `python3 -m pytest tools/tests -v`
Expected: PASS (gbimg, catalog, tileset_valid, build_assets).

- [ ] **Step 7 : Commit**

```bash
git add tools/build_assets.py tools/tests/test_build_assets.py gta/assets.h gta/assets_data.cpp assets/tiles8
git commit -m "feat(m1): build_assets.py -> assets.h/cpp + tiles8 (tileset RGB565 + perso)"
```

---

## Vérification finale M1

- [ ] `python3 -m pytest tools/tests -v` → tout vert.
- [ ] `assets/catalog.csv` = 577 lignes ; `assets/contact/` peuplé.
- [ ] `assets/tileset.csv` valide (8 tuiles requises + perso).
- [ ] `gta/assets.h` : `enum TileId` avec `NUM_TILES` ≥ 8 ; `gta/assets_data.cpp` compile-ready (tableaux clos).
- [ ] `assets/tiles8/*.png` inspectés visuellement (tuiles reconnaissables à 8×8 ; sinon, ré-curer une source plus lisible — l'archi ne change pas).

**Livrable M1 :** tileset 8×8 RGB565 + perso, nommés, exposés en C++, + catalogue navigable. Prêt à être consommé par M2 (compilo ville) et M4 (sketch).
