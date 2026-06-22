"""Genere les frames de rotation du camion/bus generique en flash
(gta/truck.{h,cpp}).

Meme principe que build_car.py : un seul sprite pre-rendu a N orientations,
blitte tel quel a l'execution (aucun calcul flottant par pixel). La variete
(bus scolaire / bus / pompier / ambulance / benne chantier) ne cree PAS 5
sprites : une seule forme partagee porte 3 "cles" de recolorisation en plus
du corps (cabine, fenetres, echelle de toit) ; chaque variante choisit, pour
chaque cle, soit une couleur de detail soit la couleur de carrosserie (= la
cle redevient invisible, fondue dans le corps). Le gyrophare (anime) reste un
overlay dessine a l'execution (cf. drawGyro dans mod_render.h), pas un pixel
baked -- il doit clignoter.

Lancer depuis la racine : .venv/bin/python -m tools.build_truck
"""
import os

TRUCK_FRAMES = 24         # identique a CAR_FRAMES : meme formule d'index de frame
TRUCK_BOX = 20            # boite englobante (diagonale camion ~18.0 px)
TRUCK_TRANSP = 0xF81F     # magenta = transparent (idem voiture)
TRUCK_BODY_KEY = 0x07E0   # vert pur = carrosserie (recoloree au blit)
TRUCK_CAB_KEY = 0x001F    # bleu pur = zone cabine (avant) -- recoloree carrosserie ou gris acier
TRUCK_WINDOW_KEY = 0xF800 # rouge pur = zone fenetres laterales -- recoloree carrosserie ou vitre
TRUCK_LADDER_KEY = 0xFFE0 # jaune pur = echelle de toit -- recoloree carrosserie ou gris clair
HDR = "gta/truck.h"
SRC = "gta/truck_data.cpp"

L, W = 8.0, 3.0           # demi-longueur / demi-largeur (camion/bus : long et carre, pas de nez effile)


def _rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


_LIGHT_F = _rgb565(255, 240, 150)
_LIGHT_R = _rgb565(120, 0, 0)
_WINDSHIELD = _rgb565(120, 200, 235)


def truck_pixel(x, y):
    """Couleur ou cle de recolorisation d'un point local (x avant+, y largeur),
    ou None hors silhouette. Silhouette carree (cabine haute, flanc droit) --
    generique bus/camion, contrairement au nez effile de la voiture."""
    ay = abs(y)
    if x < -L or x > L or ay > W:
        return None
    if x > L - 0.7 and ay > W - 1.3:
        return _LIGHT_F                              # phares avant
    if x < -L + 0.7 and ay > W - 1.3:
        return _LIGHT_R                               # feux AR
    if x > L - 3.0 and ay < W - 1.2:
        return _WINDSHIELD                            # pare-brise (bande avant)
    if ay < 0.7 and -7.0 < x < 5.5:
        return TRUCK_LADDER_KEY                       # echelle de toit (pompier seulement)
    if 1.5 <= ay <= 2.5 and -6.5 < x < 6.0:
        mullion = int(round(x + L)) % 2 == 0          # fenetres segmentees (bus scolaire seulement)
        return TRUCK_BODY_KEY if mullion else TRUCK_WINDOW_KEY
    if x > 3.5:
        return TRUCK_CAB_KEY                          # cabine avant (benne chantier seulement)
    return TRUCK_BODY_KEY                              # carrosserie / contour


def render_frame(angle):
    """Rasterise une frame TRUCK_BOX x TRUCK_BOX (rotation inverse)."""
    import math
    cs, sn = math.cos(angle), math.sin(angle)
    c = TRUCK_BOX / 2.0
    out = []
    for sy in range(TRUCK_BOX):
        for sx in range(TRUCK_BOX):
            dx, dy = sx - c, sy - c
            lx = dx * cs + dy * sn
            ly = -dx * sn + dy * cs
            col = truck_pixel(lx, ly)
            out.append(TRUCK_TRANSP if col is None else col)
    return out


def build():
    import math
    frames = [render_frame(f * 2 * math.pi / TRUCK_FRAMES) for f in range(TRUCK_FRAMES)]

    with open(HDR, "w") as f:
        f.write("// genere par tools/build_truck.py -- NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define TRUCK_FRAMES %d\n" % TRUCK_FRAMES)
        f.write("#define TRUCK_BOX %d\n" % TRUCK_BOX)
        f.write("#define TRUCK_TRANSPARENT 0x%04X\n" % TRUCK_TRANSP)
        f.write("#define TRUCK_BODY_KEY 0x%04X\n" % TRUCK_BODY_KEY)
        f.write("#define TRUCK_CAB_KEY 0x%04X\n" % TRUCK_CAB_KEY)
        f.write("#define TRUCK_WINDOW_KEY 0x%04X\n" % TRUCK_WINDOW_KEY)
        f.write("#define TRUCK_LADDER_KEY 0x%04X\n\n" % TRUCK_LADDER_KEY)
        f.write("extern const uint16_t truckFrames[TRUCK_FRAMES][TRUCK_BOX*TRUCK_BOX];\n")

    with open(SRC, "w") as f:
        f.write("// genere par tools/build_truck.py -- NE PAS editer\n")
        f.write('#include "truck.h"\n\n')
        f.write("const uint16_t truckFrames[TRUCK_FRAMES][TRUCK_BOX*TRUCK_BOX] = {\n")
        for fr in frames:
            f.write("  {")
            f.write(",".join("0x%04X" % v for v in fr))
            f.write("},\n")
        f.write("};\n")

    print("OK: %d frames %dx%d -> %s, %s (%d o flash)"
          % (TRUCK_FRAMES, TRUCK_BOX, TRUCK_BOX, HDR, SRC,
             TRUCK_FRAMES * TRUCK_BOX * TRUCK_BOX * 2))

    _preview(frames)


# (nom, couleur carrosserie, cabine, fenetres, echelle, gyrophares (g,d) ou None)
_PREVIEW_VARIANTS = [
    ("bus_scolaire",    (255, 224, 0),   (255, 224, 0),   (90, 150, 175), (255, 224, 0),   None),
    ("bus_ville",       (40, 90, 220),   (40, 90, 220),   (40, 90, 220),  (40, 90, 220),   None),
    ("pompier",         (220, 20, 20),   (220, 20, 20),   (220, 20, 20),  (200, 200, 205), ((255, 0, 0), (255, 200, 0))),
    ("ambulance",       (235, 235, 235), (235, 235, 235), (235, 235, 235), (235, 235, 235), ((255, 0, 0), (0, 60, 255))),
    ("benne_chantier",  (255, 140, 20),  (110, 110, 120), (255, 140, 20), (255, 140, 20),   None),
]


def _preview(frames, scale=8, pad=2):
    """Ecrit previews/truck_variants.png : la frame 0 (face est, pas de
    rotation) de chaque variante, recoloree comme le ferait blitTruck."""
    try:
        from PIL import Image
    except ImportError:
        return
    frame0 = frames[0]
    cell = TRUCK_BOX + pad
    sheet = Image.new("RGB", (cell * len(_PREVIEW_VARIANTS) * scale, cell * scale), (40, 40, 48))
    for i, (name, body, cab, window, ladder, gyro) in enumerate(_PREVIEW_VARIANTS):
        img = Image.new("RGB", (TRUCK_BOX, TRUCK_BOX), (255, 0, 255))
        px = img.load()
        for sy in range(TRUCK_BOX):
            for sx in range(TRUCK_BOX):
                v = frame0[sy * TRUCK_BOX + sx]
                if v == TRUCK_TRANSP:
                    continue
                if v == TRUCK_BODY_KEY:
                    col = body
                elif v == TRUCK_CAB_KEY:
                    col = cab
                elif v == TRUCK_WINDOW_KEY:
                    col = window
                elif v == TRUCK_LADDER_KEY:
                    col = ladder
                else:
                    r = ((v >> 11) & 0x1F) << 3
                    g = ((v >> 5) & 0x3F) << 2
                    b = (v & 0x1F) << 3
                    col = (r, g, b)
                px[sx, sy] = col
        if gyro:
            c = TRUCK_BOX // 2
            fx = c + int(L) - 3
            px[fx, c - 3] = gyro[0]
            px[fx, c + 2] = gyro[1]
        frame = img.resize((TRUCK_BOX * scale, TRUCK_BOX * scale), Image.NEAREST)
        sheet.paste(frame, (i * cell * scale + (pad // 2) * scale, (pad // 2) * scale))
    os.makedirs("previews", exist_ok=True)
    sheet.save("previews/truck_variants.png")
    print("OK: previews/truck_variants.png |", ", ".join(v[0] for v in _PREVIEW_VARIANTS))


if __name__ == "__main__":
    build()
