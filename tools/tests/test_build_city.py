import csv
from tools import build_city as bc
from PIL import Image
from tools.citydsl import compile_city


def _write_tileset(path):
    rows = [
        ("grass", "g.png", "0"), ("road_h", "rh.png", "0"),
        ("road_v", "rv.png", "0"), ("road_cross", "rc.png", "0"),
        ("pavement", "pv.png", "0"), ("water", "w.png", "1"),
        ("building_a", "ba.png", "1"), ("building_b", "bb.png", "1"),
    ]
    with open(path, "w", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(["name", "source", "solid"])
        wr.writerows(rows)


def test_load_tiles_maps_index_and_solid(tmp_path):
    ts = tmp_path / "tileset.csv"
    _write_tileset(ts)
    tile_index, solid_index, names = bc.load_tiles(str(ts))
    assert tile_index["grass"] == 0 and tile_index["building_b"] == 7
    assert solid_index == {5, 6, 7}
    assert names[3] == "road_cross"


def test_emit_headers(tmp_path):
    ts = tmp_path / "tileset.csv"; _write_tileset(ts)
    tile_index, solid_index, _ = bc.load_tiles(str(ts))
    from tools.citydsl import compile_city
    city = compile_city("size 3 2\nfill grass\nrect water 0 0 0 0\n"
                         "player 2 1 east\n", tile_index, solid_index)
    out_h = tmp_path / "citymap.h"
    out_cpp = tmp_path / "citymap_data.cpp"
    bc.emit_headers(city, str(out_h), str(out_cpp))

    h = out_h.read_text()
    assert "#define CITY_W 3" in h and "#define CITY_H 2" in h
    assert "#define PLAYER_START_X 2" in h
    assert "#define PLAYER_START_Y 1" in h
    assert "#define PLAYER_START_DIR DIR_EAST" in h
    assert "extern const uint8_t cityMap[CITY_H*CITY_W];" in h

    cpp = out_cpp.read_text()
    assert '#include "citymap.h"' in cpp
    assert "const uint8_t cityMap[CITY_H*CITY_W] = {" in cpp
    assert cpp.count("5") >= 1 and "0, 0" in cpp


def test_render_png_dimensions(tmp_path):
    # mini tiles8 : un PNG 8x8 par nom
    tiles8 = tmp_path / "tiles8"; tiles8.mkdir()
    names = ["grass", "road_h", "road_v", "road_cross",
             "pavement", "water", "building_a", "building_b"]
    colors = [(0, 200, 0), (80, 80, 80), (80, 80, 80), (60, 60, 60),
              (180, 180, 180), (0, 0, 200), (150, 60, 60), (60, 60, 150)]
    for n, col in zip(names, colors):
        Image.new("RGB", (8, 8), col).save(tiles8 / (n + ".png"))

    tile_index = {n: i for i, n in enumerate(names)}
    city = compile_city("size 4 3\nfill grass\nplayer 0 0 south\n",
                        tile_index, {5, 6, 7})
    out_png = tmp_path / "citymap.png"
    bc.render_png(city, names, str(tiles8), str(out_png))

    assert out_png.exists()
    with Image.open(out_png) as im:
        assert im.size == (4 * 8, 3 * 8)   # 32x24
