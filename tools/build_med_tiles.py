"""Tuiles 8x8 "mediterraneennes" (Marseille / Provence / garrigue).

Reskin des surfaces dominantes de la carte pour un rendu sud :
  building_a/b : toits de TUILES en terre cuite (le "Panier" vu d'en haut),
  grass        : GARRIGUE seche (sauge/olive + cailloux calcaire),
  pavement     : pierre chaude (calcaire), au lieu du gris.

Meme principe que build_junk_tiles : pixel-art 8x8 dessine a la main (lecture
nette a 8 px), relu tel quel par build_assets.py. Ecrit dans assets/named/tiles/.
Lancer build_assets.py puis build_city.py ensuite.
"""
import os

PAL = {
    # terre cuite (toits)
    'T': (196, 92, 58), 't': (168, 76, 48), 'h': (216, 118, 80), 'R': (138, 58, 42),
    # ocre / facade Provence
    'O': (214, 176, 108), 'o': (186, 148, 86), 'c': (228, 200, 142),
    'e': (190, 88, 54),
    # garrigue
    's': (150, 162, 98), 'd': (122, 138, 80), 'L': (206, 196, 158), 'b': (100, 122, 70),
    # pierre chaude (trottoir)
    'P': (204, 190, 162), 'p': (180, 166, 138), 'j': (156, 142, 118),
}

TILES = {
    # toit de tuiles a deux pans : rangees de tuiles + faitiere centrale + avant-toit
    'building_a': (
        "hThThThT",
        "TtTtTtTt",
        "hThThThT",
        "RRRRRRRR",
        "hThThThT",
        "TtTtTtTt",
        "hThThThT",
        "tttttttt",
    ),
    # maison provencale : mur ocre, liseré de tuiles terre cuite (toit-terrasse)
    'building_b': (
        "eeeeeeee",
        "eOcOcOoe",
        "eoOcOcOe",
        "ecOoOcOe",
        "eOcOoOce",
        "eoOcOcOe",
        "ecOoOcOe",
        "eeeeeeee",
    ),
    # garrigue : sauge/olive mouchete de cailloux calcaire et de buissons
    'grass': (
        "sdsssLss",
        "ssbsssds",
        "dsssssLs",
        "ssLssbss",
        "bssssdss",
        "sdsssssb",
        "ssssLsss",
        "dssbsssd",
    ),
    # trottoir : dalles de pierre calcaire chaude, joints clairs
    'pavement': (
        "PPpPPPpP",
        "PPpPPPpP",
        "jjjjjjjj",
        "pPPPpPPP",
        "pPPPpPPP",
        "jjjjjjjj",
        "PPpPPPpP",
        "PPpPPPpP",
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
                px[x, y] = PAL[ch]
        im.save(os.path.join(out_dir, name + ".png"))
    return list(TILES)


if __name__ == "__main__":
    made = build()
    print("genere: %s" % ", ".join("%s/%s.png" % (OUT_DIR, n) for n in made))
