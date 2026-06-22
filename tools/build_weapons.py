"""Genere les sprites d'armes en flash (gta/weapons_gfx.{h,cpp}).

Vues "icone" 6x6, lisibles a la fois au sol (objet a ramasser) et dans le HUD
(arme selectionnee). Pre-rendus sur PC, blittes tels quels (comme la voiture et
le pieton). A 6 px on dessine a la main une silhouette par arme (pixel-art) :
plus net qu'un rendu procedural mathematique a cette taille. L'ordre des sprites
suit l'enum WeaponId de gta/weapons.h (FIST..GRENADE).

Lancer depuis la racine : .venv/bin/python -m tools.build_weapons
"""

WEAPON_BOX = 6
WEAPON_TRANSP = 0xF81F   # magenta = transparent (meme convention que car/player)

# Palette des caracteres de grille -> RGB888.
PAL = {
    '.': None,            # transparent
    'k': (30, 30, 34),    # metal sombre / canon
    'g': (90, 90, 100),   # metal clair / poignee
    'b': (120, 80, 40),   # bois (crosse)
    'd': (60, 90, 55),    # vert kaki (lance-roquette / grenade)
    's': (238, 205, 165), # peau (poing)
    'y': (255, 220, 120), # accent (bouche / detente / cuillere)
    'r': (180, 40, 40),   # rouge (ogive roquette)
    'c': (190, 220, 210), # verre (bouteille molotov)
    'o': (240, 130, 30),  # flamme (chiffon enflamme)
}

# Une grille 6x6 par arme. Top-down "de cote", oriente vers la droite.
SPRITES = [
    # FIST -- petit poing (peau)
    ["......",
     "..ss..",
     ".ssss.",
     ".ssss.",
     "..ss..",
     "......"],
    # PISTOL -- canon court + poignee
    ["......",
     "kkkk..",
     "kkkky.",
     "kg....",
     "gg....",
     "......"],
    # SMG (pistolet-mitrailleur) -- corps + chargeur dessous
    ["......",
     "gkkkk.",
     "kkkkky",
     ".k....",
     ".kk...",
     "..k..."],
    # SHOTGUN (fusil a pompe) -- long canon + crosse bois
    ["......",
     "bgkkkk",
     "bkkkky",
     "bb....",
     "......",
     "......"],
    # BAZOOKA -- gros tube + ogive rouge a l'avant, event arriere
    ["......",
     "kk....",
     "ydddr.",
     "ydddr.",
     "..k...",
     "......"],
    # GRENADE -- ovoide kaki, cuillere/goupille jaune en haut
    ["..y...",
     "..yd..",
     ".dddd.",
     ".dddd.",
     ".dddd.",
     "..dd.."],
    # MOLOTOV -- bouteille de verre, chiffon enflamme au goulot
    ["..o...",
     ".oo...",
     "..c...",
     ".ccc..",
     ".ccc..",
     "..c..."],
]


def _rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def _row_pixels(row):
    """Normalise une ligne a WEAPON_BOX cases -> liste de RGB565/transp."""
    out = []
    for i in range(WEAPON_BOX):
        ch = row[i] if i < len(row) else '.'
        col = PAL.get(ch)
        out.append(WEAPON_TRANSP if col is None else _rgb565(col))
    return out


def build():
    n = len(SPRITES)
    frames = []
    for grid in SPRITES:
        flat = []
        for y in range(WEAPON_BOX):
            row = grid[y] if y < len(grid) else ""
            flat.extend(_row_pixels(row))
        frames.append(flat)

    with open("gta/weapons_gfx.h", "w") as f:
        f.write("// genere par tools/build_weapons.py -- NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define WEAPON_BOX %d\n" % WEAPON_BOX)
        f.write("#define WEAPON_GFX_COUNT %d\n" % n)
        f.write("#define WEAPON_TRANSP 0x%04X\n\n" % WEAPON_TRANSP)
        f.write("extern const uint16_t weaponSprites[WEAPON_GFX_COUNT][WEAPON_BOX*WEAPON_BOX];\n")

    with open("gta/weapons_gfx_data.cpp", "w") as f:
        f.write("// genere par tools/build_weapons.py -- NE PAS editer\n")
        f.write('#include "weapons_gfx.h"\n\n')
        f.write("const uint16_t weaponSprites[WEAPON_GFX_COUNT][WEAPON_BOX*WEAPON_BOX] = {\n")
        for fr in frames:
            f.write("  {" + ",".join("0x%04X" % v for v in fr) + "},\n")
        f.write("};\n")

    print("OK: %d armes %dx%d -> gta/weapons_gfx.{h,cpp} (%d o flash)"
          % (n, WEAPON_BOX, WEAPON_BOX, n * WEAPON_BOX * WEAPON_BOX * 2))


if __name__ == "__main__":
    build()
