"""tileset.csv + PNG nommes -> gta/assets.{h,cpp} + assets/tiles8/."""
import os, csv
from tools.gbimg import rgb565, load_rgb, load_rgba_keyed, downscale

TILESET_CSV = "assets/tileset.csv"
PLAYER_FRAMES = ["assets/named/sprites/player_a.png",
                 "assets/named/sprites/player_b.png"]
OUT_H = "gta/assets.h"
OUT_CPP = "gta/assets_data.cpp"
TILES8_DIR = "assets/tiles8"
SPRITES8_DIR = "assets/sprites8"
TILE = 8
PLAYER = 8
TRANSPARENT = 0xF81F  # magenta = couleur transparente du perso
# Ordre directions: NORTH, EAST, SOUTH, WEST.
# Source perso (851) orientee NORD (tete en haut) -> rotation PIL (CCW) par direction.
DIR_ANGLES = [0, 270, 180, 90]


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
    return rgba, out


def _fmt(vals, per_line=8):
    lines = []
    for i in range(0, len(vals), per_line):
        lines.append("  " + ", ".join("0x%04X" % v for v in vals[i:i + per_line]))
    return ",\n".join(lines)


def build(tileset_csv, player_frames, out_h, out_cpp, tiles8_dir, sprites8_dir):
    with open(tileset_csv) as f:
        rows = list(csv.DictReader(f))
    os.makedirs(tiles8_dir, exist_ok=True)
    os.makedirs(sprites8_dir, exist_ok=True)
    os.makedirs(os.path.dirname(out_h) or ".", exist_ok=True)

    names = [r["name"] for r in rows]
    flags = [1 if r["solid"] == "1" else 0 for r in rows]
    tiles_px = []
    for r in rows:
        img, px = _tile_pixels(r["source"])
        tiles_px.append(px)
        img.save(os.path.join(tiles8_dir, r["name"] + ".png"))

    # Perso : 4 directions x N frames
    nframes = len(player_frames)
    dirnames = ["NORTH", "EAST", "SOUTH", "WEST"]
    player_px = []  # [dir][frame] -> liste de pixels
    for di, angle in enumerate(DIR_ANGLES):
        dir_letter = dirnames[di][0].lower()
        frames_px = []
        for frame_idx, p in enumerate(player_frames):
            rgba, px = _player_frame_pixels(p, angle)
            rgba.save(os.path.join(sprites8_dir, "player_%s_%d.png" % (dir_letter, frame_idx)))
            frames_px.append(px)
        player_px.append(frames_px)

    # ---- assets.h ----
    enum = ",\n  ".join("TILE_%s" % n.upper() for n in names)
    with open(out_h, "w") as f:
        f.write("// genere par tools/build_assets.py -- NE PAS editer\n")
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
        f.write("// genere par tools/build_assets.py -- NE PAS editer\n")
        f.write('#include "assets.h"\n\n')
        f.write("const uint16_t tileset[NUM_TILES][TILE_W*TILE_H] = {\n")
        for n, px in zip(names, tiles_px):
            f.write("  { // %s\n%s\n  },\n" % (n, _fmt(px)))
        f.write("};\n\n")
        f.write("const uint8_t tileFlags[NUM_TILES] = {%s};\n\n"
                % ", ".join(str(x) for x in flags))
        f.write("const uint16_t playerSprite[4][PLAYER_FRAMES][PLAYER_W*PLAYER_H] = {\n")
        for di, per_dir in enumerate(player_px):
            f.write("  { // DIR_%s\n" % dirnames[di])
            for fr in per_dir:
                f.write("    {\n%s\n    },\n" % _fmt(fr))
            f.write("  },\n")
        f.write("};\n")


if __name__ == "__main__":
    build(TILESET_CSV, PLAYER_FRAMES, OUT_H, OUT_CPP, TILES8_DIR, SPRITES8_DIR)
    print("genere: %s, %s, %s/, %s/" % (OUT_H, OUT_CPP, TILES8_DIR, SPRITES8_DIR))
