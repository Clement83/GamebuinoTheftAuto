"""Genere le sprite piéton procedural en flash (gta/player.{h,cpp}).

Piéton vu de dessus (on voit le dessus de la tete + les epaules), 4 directions
(N/E/S/W, ordre de l'enum Dir) x 2 frames de marche. Pre-rendu sur PC, blitte
tel quel par la Gamebuino (comme la voiture). Remplace le sprite importe 8x8 :
celui-ci est plus petit (boite 7) et dessine main.

Lancer depuis la racine : .venv/bin/python -m tools.build_player
"""
import os

PLAYER_BOX = 7
PLAYER_WALK = 2
PLAYER_TRANSP = 0xF81F
PLAYER_SHIRT_KEY = 0x07E0   # vert pur = t-shirt recolore au blit (couleur entite)
DIRS = [(0, -1), (1, 0), (0, 1), (-1, 0)]   # N, E, S, W
HDR = "gta/player.h"
SRC = "gta/player_data.cpp"

SKIN = (238, 205, 165)
HAIR = (50, 38, 30)
PANTS = (40, 40, 50)


def _rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def ped_pixel(u, v, frame):
    """Couleur RGB565 (u = avant+, v = cote) ou None. Vu de dessus.

    Le t-shirt + les bras sont emis en PLAYER_SHIRT_KEY : recolores au blit par
    la couleur de l'entite (joueur = teinte fixe, IA = teinte aleatoire).
    """
    av = abs(v)
    # epaules : larges (cote), courtes (avant/arriere), un peu en arriere
    if ((u + 0.3) / 1.3) ** 2 + (v / 2.2) ** 2 < 1.0:
        return PLAYER_SHIRT_KEY              # t-shirt + bras (recolore)
    # tete : dessus = cheveux, posee sur les epaules vers l'avant
    if (u - 1.0) ** 2 + v * v < 1.7:
        if u > 1.6 and av < 0.7:
            return _rgb565(SKIN)             # nez/visage -> marque la direction
        return _rgb565(HAIR)
    # pieds derriere, alternance de marche
    sw = 0.6 if frame == 0 else -0.6
    if abs(v - 0.7) < 0.55 and abs(u + 1.8 - sw) < 0.6:
        return _rgb565(PANTS)
    if abs(v + 0.7) < 0.55 and abs(u + 1.8 + sw) < 0.6:
        return _rgb565(PANTS)
    return None


def render_frame(fx, fy, frame):
    sx, sy = -fy, fx
    c = PLAYER_BOX / 2.0
    out = []
    for yy in range(PLAYER_BOX):
        for xx in range(PLAYER_BOX):
            dx, dy = xx - c + 0.5, yy - c + 0.5
            u = dx * fx + dy * fy
            v = dx * sx + dy * sy
            col = ped_pixel(u, v, frame)
            out.append(PLAYER_TRANSP if col is None else col)
    return out


def build():
    # frames[dir][frame]
    frames = [[render_frame(fx, fy, fr) for fr in range(PLAYER_WALK)]
              for (fx, fy) in DIRS]

    with open(HDR, "w") as f:
        f.write("// genere par tools/build_player.py -- NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define PLAYER_BOX %d\n" % PLAYER_BOX)
        f.write("#define PLAYER_WALK %d\n" % PLAYER_WALK)
        f.write("#define PLAYER_TRANSP 0x%04X\n" % PLAYER_TRANSP)
        f.write("#define PLAYER_SHIRT_KEY 0x%04X\n\n" % PLAYER_SHIRT_KEY)
        f.write("extern const uint16_t playerFrames[4][PLAYER_WALK][PLAYER_BOX*PLAYER_BOX];\n")

    with open(SRC, "w") as f:
        f.write("// genere par tools/build_player.py -- NE PAS editer\n")
        f.write('#include "player.h"\n\n')
        f.write("const uint16_t playerFrames[4][PLAYER_WALK][PLAYER_BOX*PLAYER_BOX] = {\n")
        names = "NESW"
        for di in range(4):
            f.write("  { // DIR_%s\n" % names[di])
            for fr in range(PLAYER_WALK):
                f.write("    {" + ",".join("0x%04X" % v for v in frames[di][fr]) + "},\n")
            f.write("  },\n")
        f.write("};\n")

    print("OK: piéton 4x%d frames %dx%d -> %s, %s (%d o flash)"
          % (PLAYER_WALK, PLAYER_BOX, PLAYER_BOX, HDR, SRC,
             4 * PLAYER_WALK * PLAYER_BOX * PLAYER_BOX * 2))


if __name__ == "__main__":
    build()
