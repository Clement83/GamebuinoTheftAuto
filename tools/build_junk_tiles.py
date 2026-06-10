"""Genere les 4 tuiles 8x8 de La Casse (assets/named/tiles/junk_*.png).

Sources pixel-art dessinees a la main ici (grilles d'une grille de pixels 8x8)
plutot que par bruit : on veut une lecture nette a 8 px (chain-link, epave,
base de grue) la ou un downscale d'image serait boueux. build_assets.py les
relit ensuite comme n'importe quelle tuile source (BOX 8->8 = identite).

Tuiles produites (solidite definie dans assets/tileset.csv) :
  junk_ground  sol de casse : terre/cambouis, NON solide (carrossable a pied/voiture)
  junk_fence   grille chain-link sur terre, SOLIDE (enceinte de la casse)
  junk_wreck   epave de voiture vue de dessus, SOLIDE (decor statique)
  junk_crane   base/cabine de la grue, SOLIDE (pivot du bras, rendu par-dessus)
"""
import os

# palette (RGB) -- calee sur junk_facade_a (#4a4231) pour la continuite du sol
PAL = {
    'd': (74, 66, 49),    # terre / cambouis (DIRT)
    'k': (49, 41, 33),    # terre sombre
    'o': (33, 28, 24),    # tache d'huile
    'g': (107, 99, 82),   # gravier clair
    'S': (140, 140, 148),  # acier (mesh / rail)
    'M': (74, 74, 82),    # acier sombre (poteau / chassis machine)
    'R': (122, 74, 58),   # carrosserie rouillee
    'r': (74, 41, 33),    # rouille sombre (habitacle)
    'G': (90, 120, 130),  # vitre brisee
    'O': (214, 130, 40),  # orange machine (cabine de grue)
    'C': (60, 60, 66),    # contrepoids
}

TILES = {
    # sol de casse : terre uniforme mouchetee d'huile et de gravier
    'junk_ground': (
        "ddgddodd",
        "doddddkd",
        "ddddgddd",
        "gddoddkg",
        "ddddddod",
        "dkgddddd",
        "oddddgdd",
        "dddoddkd",
    ),
    # grille chain-link : poteaux d'acier + mailles, sur fond de terre
    'junk_fence': (
        "MdSdMdSd",
        "MdmdMdmd",
        "MSdSMSdS",
        "MdmdMdmd",
        "MdSdMdSd",
        "MdmdMdmd",
        "MSdSMSdS",
        "MdSdMdSd",
    ),
    # epave vue de dessus : carrosserie rouillee, habitacle, vitre brisee
    'junk_wreck': (
        "dRRRRRRd",
        "RRrrrrRR",
        "RrGGGGrR",
        "Rrr GGrR",
        "RrGG rrR",
        "RrGGGGrR",
        "RRrrrrRR",
        "dRRddRRd",
    ),
    # base de grue : chassis d'acier, cabine orange, contrepoids
    'junk_crane': (
        "MMMMMMMM",
        "MCCSSCCM",
        "MCOOOOCM",
        "MSOOOOSM",
        "MSOOOOSM",
        "MCOOOOCM",
        "MCCSSCCM",
        "MMMMMMMM",
    ),
}

OUT_DIR = "assets/named/tiles"


def build(out_dir=OUT_DIR):
    from PIL import Image
    os.makedirs(out_dir, exist_ok=True)
    for name, rows in TILES.items():
        im = Image.new("RGB", (8, 8))
        px = im.load()
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                # ' ' (mailles ajourees de l'epave) -> terre, pour rester tileable
                px[x, y] = PAL.get(ch, PAL['k'] if ch == ' ' else PAL['d'])
        im.save(os.path.join(out_dir, name + ".png"))
    return list(TILES)


if __name__ == "__main__":
    made = build()
    print("genere: %s" % ", ".join("%s/%s.png" % (OUT_DIR, n) for n in made))
