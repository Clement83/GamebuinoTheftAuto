"""Curation : mappe les ids GTA choisis -> assets/named/** + assets/tileset.csv.

Choix issus de l'inspection visuelle des planches-contact (T3). Reproductible :
relancer ce script régénère les PNG nommés et le tileset.csv à l'identique.
L'ORDRE de TILE_PICKS fixe l'index enum (grass=0, road_h=1, ...).
"""
import os, csv
from PIL import Image, ImageDraw

TILES = "GTA_textures_LC/LC/tiles"
SPR = "GTA_textures_LC/LC/sprites"
NAMED_T = "assets/named/tiles"
NAMED_S = "assets/named/sprites"
TILESET_CSV = "assets/tileset.csv"

# (nom, id_source, rotation_deg, solid)
TILE_PICKS = [
    ("grass",      206, 0, 0),
    ("road_h",     199, 90, 0),   # route verticale 199 pivotée -> lignes horizontales
    ("road_v",     199, 0, 0),
    ("road_cross", 220, 0, 0),    # asphalte (intersection)
    ("pavement",   289, 0, 0),    # dallage beige (trottoir)
    ("water",      375, 0, 1),    # eau cyan
    ("building_a", 43, 0, 1),     # façade brique brun-rouge
    ("building_b", 13, 0, 1),     # façade teal
    # --- POI (append-only : n'altère pas les index 0..7 ci-dessus) ---
    # Chinatown : signature doree (ressort vs marron/teal a 8 px).
    ("cn_facade_a", 135, 0, 1),   # facade doree ornementee
    ("cn_facade_b", 136, 0, 1),   # facade doree, centre sombre
    ("cn_sign",     168, 0, 1),   # accent dore vif (enseigne)
    # Commissariat : signature bleue. police_door non-solide = entree.
    ("police_facade", 140, 0, 1), # facade bleue LCPD
    ("police_sign",   103, 0, 1), # ecusson LCPD bleu
    ("police_door",    49, 0, 0), # arche sombre = entree franchissable
    # Hopital (stamp) : signature claire + croix rouge. door non-solide.
    ("hosp_facade", 152, 0, 1),   # facade claire
    ("hosp_sign",    11, 0, 1),   # croix rouge sur blanc
    ("hosp_door",    84, 0, 0),   # entree franchissable
    # Caserne pompiers (stamp) : signature rouge/saumon. door non-solide.
    ("fire_facade",  26, 0, 1),   # facade saumon
    ("fire_sign",   131, 0, 1),   # neon FIRE rouge vif
    ("fire_door",   145, 0, 0),   # entree (porte rouge)
    # Port (quartier) : industriel gris + enseigne DOCKS verte.
    ("port_facade_a", 56, 0, 1),  # poutrelle/hangar gris
    ("port_facade_b", 94, 0, 1),  # structure industrielle
    ("port_sign",    171, 0, 1),  # panneau DOCKS vert
    # Chantier (quartier) : echafaudage vert + rubalise jaune/noir.
    ("cons_facade_a", 333, 0, 1), # echafaudage vert
    ("cons_facade_b", 351, 0, 1), # rubalise jaune/noir
    ("cons_sign",     350, 0, 1), # rubalise (accent)
    # Casse auto (quartier) : rouille + grillage.
    ("junk_facade_a", 348, 0, 1), # carcasse rouillee (AUTO)
    ("junk_facade_b", 172, 0, 1), # grillage
    ("junk_sign",     260, 0, 1), # barriere (X)
]
PLAYER_ID = 851  # piéton vu de dessus (tête en haut = orienté nord)

# Tuiles generees (absentes du rip GTA) : la cote maritime n'a pas de pendant
# top-down dans la planche. Dessinees procéduralement, deterministes. Append-only
# -> index enum 29 (sand) puis 30 (dock).
GEN_SIZE = 64


def _gen_sand(size):
    """Plage : beige sable chaud, mouchetures fixes (lisible vs trottoir gris)."""
    im = Image.new("RGB", (size, size), (218, 196, 138))
    px = im.load()
    for y in range(size):
        for x in range(size):
            d = (x * 7 + y * 13 + x * y) % 11        # motif deterministe
            if d == 0:
                px[x, y] = (232, 213, 162)           # grain clair
            elif d == 5:
                px[x, y] = (198, 174, 120)           # grain sombre
    return im


def _gen_dock(size):
    """Quai : planches bois verticales, joints sombres (jetee marchable)."""
    im = Image.new("RGB", (size, size), (124, 86, 52))
    d = ImageDraw.Draw(im)
    plank = max(6, size // 6)
    for x in range(0, size, plank):
        d.line([(x, 0), (x, size - 1)], fill=(86, 58, 33), width=2)   # joint
        d.line([(x + plank // 2, 0), (x + plank // 2, size - 1)],
               fill=(140, 100, 62), width=1)                          # veinure
    return im


GEN_TILES = [
    ("sand", _gen_sand, 0),     # plage marchable
    ("dock", _gen_dock, 0),     # quai marchable
]


def main():
    os.makedirs(NAMED_T, exist_ok=True)
    os.makedirs(NAMED_S, exist_ok=True)
    rows = []
    for name, tid, rot, solid in TILE_PICKS:
        im = Image.open(f"{TILES}/GTATIL1_{tid}.bmp").convert("RGB")
        if rot:
            im = im.rotate(rot, expand=False)
        out = f"{NAMED_T}/{name}.png"
        im.save(out)
        rows.append((name, out, solid))
    for name, gen, solid in GEN_TILES:
        out = f"{NAMED_T}/{name}.png"
        gen(GEN_SIZE).save(out)
        rows.append((name, out, solid))
    ped = Image.open(f"{SPR}/GTASPR1_{PLAYER_ID}.bmp").convert("RGB")
    ped.save(f"{NAMED_S}/player_a.png")
    ped.save(f"{NAMED_S}/player_b.png")  # 2e frame identique (anim de marche plus tard)
    with open(TILESET_CSV, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["name", "source", "solid"])
        for name, src, solid in rows:
            w.writerow([name, src, solid])
    print(f"curation: {len(rows)} tuiles -> {NAMED_T}/ ; perso id {PLAYER_ID} -> {NAMED_S}/ ; {TILESET_CSV}")


if __name__ == "__main__":
    main()
