"""Genere les frames de rotation du bateau generique en flash
(gta/boat.{h,cpp}).

Meme principe que build_truck.py : un seul sprite pre-rendu a N orientations,
blitte tel quel a l'execution (aucun calcul flottant par pixel). La variete
(vedette / yacht / peche / zodiac / taxi de mer / police maritime) ne cree PAS
6 sprites : une seule coque partagee porte 3 "cles" de recolorisation (coque,
pont, liseree) ; chaque livree choisit une couleur par cle. Pare-brise et
moteur hors-bord = couleurs fixes (baked). Le gyrophare police reste un overlay
anime dessine a l'execution (cf. drawGyro), pas un pixel baked.

Bateaux plus petits/legers que le camion -> 16 frames (22.5 deg) x 16 px pour
economiser la flash (firmware deja a ~92 %). Les bateaux tournent lentement, le
snap est invisible.

Lancer depuis la racine : .venv/bin/python -m tools.build_boat
"""
import math
import os

BOAT_FRAMES = 16          # pas de rotation (22.5 deg) -- bateaux tournent lentement
BOAT_BOX = 16             # boite englobante (diagonale coque ~14.2 px)
BOAT_TRANSP = 0xF81F      # magenta = transparent (idem voiture)
BOAT_HULL_KEY = 0x07E0    # vert pur = coque / plat-bord (recolore = couleur entite)
BOAT_DECK_KEY = 0x001F    # bleu pur = pont / cockpit -- recolore (secondaire)
BOAT_TRIM_KEY = 0xFFE0    # jaune pur = liseree de coque + console -- recolore (accent)
HDR = "gta/boat.h"
SRC = "gta/boat_data.cpp"

L, W = 7.0, 2.5           # demi-longueur / demi-largeur (coque longue et etroite)


def _rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


_WINDSHIELD = _rgb565(120, 200, 235)   # pare-brise (fixe)
_MOTOR = _rgb565(45, 45, 52)           # moteur hors-bord a la poupe (fixe)


def boat_pixel(x, y):
    """Couleur ou cle de recolorisation d'un point local (x avant+, y largeur),
    ou None hors silhouette. Proue effilee (triangle a l'avant), poupe carree
    avec moteur hors-bord."""
    ay = abs(y)
    # proue effilee : la largeur max se reduit a une pointe vers l'avant
    maxw = W
    if x > 2.0:
        maxw = W - (x - 2.0) * (W - 0.35) / (L - 2.0)
    if x < -L or x > L:
        return None
    if ay > maxw:
        return None
    # moteur hors-bord : petit bloc fixe a la poupe, derriere le tableau arriere
    if x < -L + 1.3 and ay < 1.2:
        return _MOTOR
    # plat-bord / contour : gunwale recolore (coque)
    edge = (ay > maxw - 0.8) or (x < -L + 0.8) or (x > L - 1.0)
    if edge:
        return BOAT_HULL_KEY
    # pare-brise : bande avant du cockpit (verre fixe)
    if 2.5 < x < 3.6 and ay < maxw - 0.8:
        return _WINDSHIELD
    # liseree de coque : fine bande mediane sur les flancs (accent)
    if maxw - 1.5 < ay <= maxw - 0.8 and -5.0 < x < 2.5:
        return BOAT_TRIM_KEY
    # cockpit / pont interieur : recolore (secondaire)
    if -5.0 < x < 2.6 and ay < maxw - 0.8:
        return BOAT_DECK_KEY
    # reste de la coque (proue, poupe pleine) : recolore (coque)
    return BOAT_HULL_KEY


def render_frame(angle):
    """Rasterise une frame BOAT_BOX x BOAT_BOX (rotation inverse)."""
    cs, sn = math.cos(angle), math.sin(angle)
    c = BOAT_BOX / 2.0
    out = []
    for sy in range(BOAT_BOX):
        for sx in range(BOAT_BOX):
            dx, dy = sx - c, sy - c
            lx = dx * cs + dy * sn
            ly = -dx * sn + dy * cs
            col = boat_pixel(lx, ly)
            out.append(BOAT_TRANSP if col is None else col)
    return out


def build():
    frames = [render_frame(f * 2 * math.pi / BOAT_FRAMES) for f in range(BOAT_FRAMES)]

    with open(HDR, "w") as f:
        f.write("// genere par tools/build_boat.py -- NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define BOAT_FRAMES %d\n" % BOAT_FRAMES)
        f.write("#define BOAT_BOX %d\n" % BOAT_BOX)
        f.write("#define BOAT_TRANSPARENT 0x%04X\n" % BOAT_TRANSP)
        f.write("#define BOAT_HULL_KEY 0x%04X\n" % BOAT_HULL_KEY)
        f.write("#define BOAT_DECK_KEY 0x%04X\n" % BOAT_DECK_KEY)
        f.write("#define BOAT_TRIM_KEY 0x%04X\n\n" % BOAT_TRIM_KEY)
        f.write("extern const uint16_t boatFrames[BOAT_FRAMES][BOAT_BOX*BOAT_BOX];\n")

    with open(SRC, "w") as f:
        f.write("// genere par tools/build_boat.py -- NE PAS editer\n")
        f.write('#include "boat.h"\n\n')
        f.write("const uint16_t boatFrames[BOAT_FRAMES][BOAT_BOX*BOAT_BOX] = {\n")
        for fr in frames:
            f.write("  {")
            f.write(",".join("0x%04X" % v for v in fr))
            f.write("},\n")
        f.write("};\n")

    print("OK: %d frames %dx%d -> %s, %s (%d o flash)"
          % (BOAT_FRAMES, BOAT_BOX, BOAT_BOX, HDR, SRC,
             BOAT_FRAMES * BOAT_BOX * BOAT_BOX * 2))

    _preview(frames)


# (nom, coque, pont, liseree, gyrophares (g,d) ou None)
_PREVIEW_VARIANTS = [
    ("vedette_rouge",   (210, 40, 40),   (240, 225, 180), (255, 255, 255), None),
    ("yacht_blanc",     (240, 240, 245), (190, 150, 90),  (20, 50, 120),   None),
    ("peche_vert",      (30, 120, 70),   (120, 120, 128), (255, 150, 30),  None),
    ("zodiac_gris",     (70, 75, 85),    (28, 28, 34),    (150, 155, 165), None),
    ("taxi_jaune",      (250, 205, 0),   (35, 35, 40),    (20, 20, 25),    None),
    ("police_maritime", (20, 40, 110),   (235, 235, 235), (255, 255, 255), ((0, 60, 255), (235, 235, 235))),
]


def _preview(frames, scale=10, pad=2):
    """Ecrit previews/boat_variants.png : la frame 0 (face est, pas de
    rotation) de chaque livree, recoloree comme le ferait blitBoat."""
    try:
        from PIL import Image
    except ImportError:
        return
    frame0 = frames[0]
    cell = BOAT_BOX + pad
    sheet = Image.new("RGB", (cell * len(_PREVIEW_VARIANTS) * scale, cell * scale), (28, 40, 60))
    for i, (name, hull, deck, trim, gyro) in enumerate(_PREVIEW_VARIANTS):
        img = Image.new("RGB", (BOAT_BOX, BOAT_BOX), (28, 40, 60))
        px = img.load()
        for sy in range(BOAT_BOX):
            for sx in range(BOAT_BOX):
                v = frame0[sy * BOAT_BOX + sx]
                if v == BOAT_TRANSP:
                    continue
                if v == BOAT_HULL_KEY:
                    col = hull
                elif v == BOAT_DECK_KEY:
                    col = deck
                elif v == BOAT_TRIM_KEY:
                    col = trim
                else:
                    r = ((v >> 11) & 0x1F) << 3
                    g = ((v >> 5) & 0x3F) << 2
                    b = (v & 0x1F) << 3
                    col = (r, g, b)
                px[sx, sy] = col
        if gyro:
            c = BOAT_BOX // 2
            fx = c + 1
            px[fx, c - 2] = gyro[0]
            px[fx, c + 1] = gyro[1]
        frame = img.resize((BOAT_BOX * scale, BOAT_BOX * scale), Image.NEAREST)
        sheet.paste(frame, (i * cell * scale + (pad // 2) * scale, (pad // 2) * scale))
    os.makedirs("previews", exist_ok=True)
    sheet.save("previews/boat_variants.png")
    print("OK: previews/boat_variants.png |", ", ".join(v[0] for v in _PREVIEW_VARIANTS))


if __name__ == "__main__":
    build()
