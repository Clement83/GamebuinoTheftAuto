import os, csv, re
from PIL import Image
from tools import build_assets as ba


def _make_solid_png(path, color, size=64):
    Image.new("RGB", (size, size), color).save(path)


def test_build_assets_end_to_end(tmp_path):
    tiles = tmp_path / "named" / "tiles"
    spr = tmp_path / "named" / "sprites"
    tiles.mkdir(parents=True)
    spr.mkdir(parents=True)
    _make_solid_png(tiles / "grass.png", (255, 0, 0))
    _make_solid_png(tiles / "water.png", (0, 0, 255))
    _make_solid_png(spr / "player_a.png", (0, 255, 0), size=24)
    _make_solid_png(spr / "player_b.png", (0, 255, 0), size=24)
    csv_path = tmp_path / "tileset.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["name", "source", "solid"])
        w.writerow(["grass", str(tiles / "grass.png"), "0"])
        w.writerow(["water", str(tiles / "water.png"), "1"])

    out_h = tmp_path / "assets.h"
    out_cpp = tmp_path / "assets_data.cpp"
    tiles8 = tmp_path / "tiles8"
    ba.build(str(csv_path),
             [str(spr / "player_a.png"), str(spr / "player_b.png")],
             str(out_h), str(out_cpp), str(tiles8))

    h = out_h.read_text()
    assert "enum TileId" in h
    assert "TILE_GRASS" in h and "TILE_WATER" in h
    assert "#define NUM_TILES 2" in h

    cpp = out_cpp.read_text()
    vals = re.findall(r"0x[0-9A-Fa-f]{4}", cpp)
    assert vals, "aucune valeur RGB565 emise"
    assert cpp.count("0xF800") >= 64       # grass = 64 px rouges (0xF800)
    assert "const uint8_t tileFlags[NUM_TILES] = {0, 1};" in cpp

    assert (tiles8 / "grass.png").exists()
    with Image.open(tiles8 / "grass.png") as im:
        assert im.size == (8, 8)
