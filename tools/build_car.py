"""Genere les frames de rotation de la voiture en flash (gta/car.{h,cpp}).

La voiture est un sprite procedural (carrosserie effilee + pare-brise + phares
+ feux + pneus) pre-rendu a N orientations. A l'execution la Gamebuino blitte
juste la frame la plus proche de l'angle courant -> aucun calcul flottant par
pixel (contrairement au raster a la volee, trop lourd sur le M0+).

Lancer depuis la racine : .venv/bin/python -m tools.build_car
"""
import math
import os

CAR_FRAMES = 24          # pas de rotation (15 deg) -- assez fin, snap invisible
CAR_BOX = 16             # boite englobante (diagonale voiture ~15.2 px)
CAR_TRANSP = 0xF81F      # magenta = transparent
HDR = "gta/car.h"
SRC = "gta/car_data.cpp"

L, W = 6.0, 2.0          # demi-longueur / demi-largeur (voiture etroite et courte)


def _rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def car_pixel(x, y):
    """Couleur RGB565 d'un point local (x avant+, y largeur) ou None."""
    ay = abs(y)
    maxw = W
    if x > 3:
        maxw = W - (x - 3) * (W - 1.5) / (L - 3)     # nez effile
    if x < -L or x > L:
        return None
    if ay > maxw:
        return None                                  # pas de pneu qui depasse
    edge = (ay > maxw - 0.9) or (x < -L + 0.9) or (x > L - 0.9)
    if x > L - 0.5 and ay > maxw - 1.0:
        return _rgb565(255, 240, 150)                # phares (2 px a l'avant)
    if x < -L + 1.2 and ay > maxw - 1.6:
        return _rgb565(120, 0, 0)                    # feux AR
    if 1.0 < x < 4.0 and ay < maxw - 1.0:
        return _rgb565(120, 200, 235)                # pare-brise
    if -4.0 < x < -2.0 and ay < W - 1.0:
        return _rgb565(90, 150, 180)                 # lunette AR
    if -2.0 <= x <= 1.0 and ay < W - 1.0:
        return _rgb565(150, 22, 22)                  # toit
    if edge:
        return _rgb565(110, 10, 10)                  # contour
    return _rgb565(205, 30, 30)                      # carrosserie


def render_frame(angle):
    """Rasterise une frame CAR_BOX x CAR_BOX (rotation inverse)."""
    cs, sn = math.cos(angle), math.sin(angle)
    c = CAR_BOX / 2.0
    out = []
    for sy in range(CAR_BOX):
        for sx in range(CAR_BOX):
            dx, dy = sx - c, sy - c
            lx = dx * cs + dy * sn
            ly = -dx * sn + dy * cs
            col = car_pixel(lx, ly)
            out.append(CAR_TRANSP if col is None else col)
    return out


def build():
    frames = [render_frame(f * 2 * math.pi / CAR_FRAMES) for f in range(CAR_FRAMES)]

    with open(HDR, "w") as f:
        f.write("// genere par tools/build_car.py -- NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define CAR_FRAMES %d\n" % CAR_FRAMES)
        f.write("#define CAR_BOX %d\n" % CAR_BOX)
        f.write("#define CAR_TRANSPARENT 0x%04X\n\n" % CAR_TRANSP)
        f.write("extern const uint16_t carFrames[CAR_FRAMES][CAR_BOX*CAR_BOX];\n")

    with open(SRC, "w") as f:
        f.write("// genere par tools/build_car.py -- NE PAS editer\n")
        f.write('#include "car.h"\n\n')
        f.write("const uint16_t carFrames[CAR_FRAMES][CAR_BOX*CAR_BOX] = {\n")
        for fr in frames:
            f.write("  {")
            f.write(",".join("0x%04X" % v for v in fr))
            f.write("},\n")
        f.write("};\n")

    print("OK: %d frames %dx%d -> %s, %s (%d o flash)"
          % (CAR_FRAMES, CAR_BOX, CAR_BOX, HDR, SRC,
             CAR_FRAMES * CAR_BOX * CAR_BOX * 2))


if __name__ == "__main__":
    build()
