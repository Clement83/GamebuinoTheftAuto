import csv
from tools import build_city as bc


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
