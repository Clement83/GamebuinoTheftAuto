"""Curation : mappe les ids GTA choisis -> assets/named/** + assets/tileset.csv.

Choix issus de l'inspection visuelle des planches-contact (T3). Reproductible :
relancer ce script régénère les PNG nommés et le tileset.csv à l'identique.
L'ORDRE de TILE_PICKS fixe l'index enum (grass=0, road_h=1, ...).
"""
import os, csv
from PIL import Image

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
]
PLAYER_ID = 851  # piéton vu de dessus (tête en haut = orienté nord)


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
