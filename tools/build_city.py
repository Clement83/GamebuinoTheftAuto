"""city/city.txt -> gta/citymap.{h,cpp} + city/citymap.png.

Lit le mapping nom->index depuis assets/tileset.csv (ordre = enum de assets.h).
"""
import os
import csv
from PIL import Image, ImageDraw
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
        f.write('#pragma once\n#include <stdint.h>\n#include "assets.h"\n\n')
        f.write("#define CITY_W %d\n#define CITY_H %d\n" % (city.w, city.h))
        f.write("#define PLAYER_START_X %d\n" % sx)
        f.write("#define PLAYER_START_Y %d\n" % sy)
        f.write("#define PLAYER_START_DIR %s\n\n" % DIR_NAMES[sdir])
        pois = getattr(city, "pois", []) or []
        f.write("#define CITY_NUM_POIS %d\n\n" % len(pois))
        f.write("// Point d'interet : bbox en TUILES (inclus) pour la detection\n")
        f.write("// HUD, point-cible (tx,ty) en PX monde pour les missions/fleche.\n")
        f.write("struct CityPoi {\n")
        f.write("  const char *name;\n")
        f.write("  uint8_t x0, y0, x1, y1;\n")
        f.write("  int16_t tx, ty;\n")
        f.write("};\n")
        f.write("extern const uint8_t cityMap[CITY_H*CITY_W];\n")
        if pois:
            f.write("extern const CityPoi cityPois[CITY_NUM_POIS];\n")
    with open(out_cpp, "w") as f:
        f.write("// genere par tools/build_city.py -- NE PAS editer\n")
        f.write('#include "citymap.h"\n\n')
        f.write("const uint8_t cityMap[CITY_H*CITY_W] = {\n")
        for y in range(city.h):
            row = city.grid[y * city.w:(y + 1) * city.w]
            f.write("  " + ", ".join(str(v) for v in row) + ",\n")
        f.write("};\n")
        if pois:
            f.write("\nconst CityPoi cityPois[CITY_NUM_POIS] = {\n")
            for p in pois:
                f.write('  { "%s", %d, %d, %d, %d, %d, %d },\n'
                        % (p["name"], p["x0"], p["y0"], p["x1"], p["y1"],
                           p["tx"] * 8 + 4, p["ty"] * 8 + 4))
            f.write("};\n")


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
