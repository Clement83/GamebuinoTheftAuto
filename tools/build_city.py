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
