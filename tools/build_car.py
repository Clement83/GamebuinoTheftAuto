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
CAR_BODY_KEY = 0x07E0    # vert pur = "corps" recolore au blit (couleur entite)
HDR = "gta/car.h"
SRC = "gta/car_data.cpp"

L, W = 6.0, 2.0          # demi-longueur / demi-largeur (voiture etroite et courte)

# --- Fumee de voiture endommagee : petit sprite anime pre-rendu, blitte ancre
#     au capot (cf. drawCarSmoke). 2 paliers (leger / dense) x SMOKE_FRAMES
#     frames qui bouclent. Une bouffee qui enfle puis monte et se dissipe.
SMOKE_BOX = 6
SMOKE_FRAMES = 4
SMOKE_TRANSP = 0xF81F     # magenta = transparent (idem voiture)
SMOKE_HDR = "gta/smoke.h"
SMOKE_SRC = "gta/smoke_data.cpp"
# par frame : (centre y dans la boite, rayon). La bouffee remonte (y decroit) et
# enfle puis maigrit -> boucle de panache vivante mais nette a 6 px.
_SMOKE_SHAPE = [(4.2, 1.5), (3.2, 2.0), (2.1, 2.0), (1.3, 1.4)]
_SMOKE_CX = SMOKE_BOX / 2.0 - 0.5    # centre x = 2.5


def _rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def smoke_pixel(sx, sy, frame, tier):
    """Couleur RGB565 d'un pixel de fumee (ou None = transparent).

    tier 0 = leger (gris clair) ; tier 1 = dense (gris fonce + coeur orange sur
    les frames basses, moteur qui chauffe) et un poil plus gros."""
    cy, r = _SMOKE_SHAPE[frame]
    if tier == 1:
        r += 0.5                                   # panache plus gros
    dx, dy = sx - _SMOKE_CX, sy - cy
    d = math.hypot(dx, dy)
    if d > r + 0.4:
        return None
    edge = d > r - 0.7
    if tier == 0:
        return _rgb565(196, 196, 204) if edge else _rgb565(150, 150, 158)
    if frame <= 1 and d < 1.1:                      # base de flamme (dense)
        return _rgb565(255, 150, 30)
    return _rgb565(112, 112, 120) if edge else _rgb565(58, 58, 66)


def render_smoke(frame, tier):
    return [smoke_pixel(sx, sy, frame, tier) for sy in range(SMOKE_BOX)
            for sx in range(SMOKE_BOX)]


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
        return CAR_BODY_KEY                          # toit (recolore)
    if edge:
        return CAR_BODY_KEY                          # contour (recolore)
    return CAR_BODY_KEY                              # carrosserie (recolore)


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
        f.write("#define CAR_TRANSPARENT 0x%04X\n" % CAR_TRANSP)
        f.write("#define CAR_BODY_KEY 0x%04X\n\n" % CAR_BODY_KEY)
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

    build_smoke()


def build_smoke():
    """Genere les sprites de fumee (gta/smoke.{h,cpp})."""
    tiers = [[render_smoke(fr, t) for fr in range(SMOKE_FRAMES)] for t in range(2)]

    with open(SMOKE_HDR, "w") as f:
        f.write("// genere par tools/build_car.py -- NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define SMOKE_BOX %d\n" % SMOKE_BOX)
        f.write("#define SMOKE_FRAMES %d\n" % SMOKE_FRAMES)
        f.write("#define SMOKE_TRANSPARENT 0x%04X\n\n" % SMOKE_TRANSP)
        f.write("// [tier 0=leger,1=dense][frame][SMOKE_BOX*SMOKE_BOX]\n")
        f.write("extern const uint16_t smokeFrames[2][SMOKE_FRAMES][SMOKE_BOX*SMOKE_BOX];\n")

    with open(SMOKE_SRC, "w") as f:
        f.write("// genere par tools/build_car.py -- NE PAS editer\n")
        f.write('#include "smoke.h"\n\n')
        f.write("const uint16_t smokeFrames[2][SMOKE_FRAMES][SMOKE_BOX*SMOKE_BOX] = {\n")
        for t in range(2):
            f.write("  {\n")
            for fr in tiers[t]:
                vals = (SMOKE_TRANSP if v is None else v for v in fr)
                f.write("    {" + ",".join("0x%04X" % v for v in vals) + "},\n")
            f.write("  },\n")
        f.write("};\n")

    _preview_smoke(tiers)
    print("OK: fumee 2x%d frames %dx%d -> %s, %s (%d o flash)"
          % (SMOKE_FRAMES, SMOKE_BOX, SMOKE_BOX, SMOKE_HDR, SMOKE_SRC,
             2 * SMOKE_FRAMES * SMOKE_BOX * SMOKE_BOX * 2))


def _preview_smoke(tiers, scale=12):
    """Ecrit previews/car_smoke.png : 2 lignes (paliers) x SMOKE_FRAMES, agrandi."""
    try:
        from PIL import Image
    except ImportError:
        return
    pad = 1
    cell = SMOKE_BOX + pad
    im = Image.new("RGB", (cell * SMOKE_FRAMES * scale, cell * 2 * scale), (40, 40, 48))
    px = im.load()
    for t in range(2):
        for fr in range(SMOKE_FRAMES):
            data = tiers[t][fr]
            for sy in range(SMOKE_BOX):
                for sx in range(SMOKE_BOX):
                    v = data[sy * SMOKE_BOX + sx]
                    if v is None:
                        continue
                    r = ((v >> 11) & 0x1F) << 3
                    g = ((v >> 5) & 0x3F) << 2
                    b = (v & 0x1F) << 3
                    bx = (fr * cell + sx) * scale
                    by = (t * cell + sy) * scale
                    for yy in range(scale):
                        for xx in range(scale):
                            px[bx + xx, by + yy] = (r, g, b)
    os.makedirs("previews", exist_ok=True)
    im.save("previews/car_smoke.png")


if __name__ == "__main__":
    build()
