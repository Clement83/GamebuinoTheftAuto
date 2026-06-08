"""Valide que la curation du tileset est complète et cohérente."""
import os, csv

TILESET_CSV = "assets/tileset.csv"
REQUIRED = ["grass", "road_h", "road_v", "road_cross",
            "pavement", "water", "building_a", "building_b"]
SOLID_EXPECTED = {"water", "building_a", "building_b"}
PLAYER_FRAMES = ["assets/named/sprites/player_a.png",
                 "assets/named/sprites/player_b.png"]


def validate():
    errors = []
    if not os.path.exists(TILESET_CSV):
        return [f"{TILESET_CSV} manquant"]
    with open(TILESET_CSV) as f:
        rows = list(csv.DictReader(f))
    names = [r["name"] for r in rows]
    for req in REQUIRED:
        if req not in names:
            errors.append(f"tuile requise absente: {req}")
    for r in rows:
        if not os.path.exists(r["source"]):
            errors.append(f"source introuvable: {r['source']} ({r['name']})")
        if r["name"] in SOLID_EXPECTED and r["solid"] != "1":
            errors.append(f"{r['name']} devrait etre solide (solid=1)")
    for p in PLAYER_FRAMES:
        if not os.path.exists(p):
            errors.append(f"frame perso manquante: {p}")
    return errors


if __name__ == "__main__":
    errs = validate()
    if errs:
        print("INVALIDE:\n  " + "\n  ".join(errs))
        raise SystemExit(1)
    print("tileset.csv valide")
