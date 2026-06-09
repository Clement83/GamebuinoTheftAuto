import pytest
from tools.citydsl import compile_city, CityError

# Mapping de test calé sur le contrat M1 (ordre = index enum).
TILE_INDEX = {
    "grass": 0, "road_h": 1, "road_v": 2, "road_cross": 3,
    "pavement": 4, "water": 5, "building_a": 6, "building_b": 7,
}
SOLID_INDEX = {5, 6, 7}  # water, building_a, building_b


def _compile(text):
    return compile_city(text, TILE_INDEX, SOLID_INDEX)


def test_size_and_fill():
    c = _compile("size 4 3\nfill grass\nplayer 0 0 south\n")
    assert (c.w, c.h) == (4, 3)
    assert c.grid == [0] * 12          # tout grass
    assert c.spawn == (0, 0, 2)        # south == 2


def test_rect_inclusive_and_clamped():
    c = _compile("size 5 5\nfill grass\nrect water 1 1 3 3\n"
                 "rect building_a 4 4 99 99\nplayer 0 0 east\n")
    # water sur le carré [1..3]x[1..3]
    assert c.get(1, 1) == 5 and c.get(3, 3) == 5 and c.get(0, 0) == 0
    # rect clampé : seule (4,4) existe
    assert c.get(4, 4) == 6


def test_size_must_be_first():
    with pytest.raises(CityError) as e:
        _compile("fill grass\nsize 4 4\n")
    assert e.value.line_no == 1


def test_unknown_tile_reports_line():
    with pytest.raises(CityError) as e:
        _compile("size 3 3\nfill lava\nplayer 0 0 south\n")
    assert e.value.line_no == 2
    assert "lava" in str(e.value)


def test_size_out_of_range():
    with pytest.raises(CityError):
        _compile("size 0 5\n")
    with pytest.raises(CityError):
        _compile("size 5 999\n")


def test_malformed_commands_raise_cityerror():
    # arite invalide -> CityError ligne, jamais IndexError
    for src in ("size 3 3\nfill\n",
                "size 3 3\nfill grass\nrect water 0 0 1\n",
                "size 3 3\nseed\n"):
        with pytest.raises(CityError):
            _compile(src)


def test_double_size_is_error():
    with pytest.raises(CityError):
        _compile("size 3 3\nsize 4 4\nfill grass\nplayer 0 0 south\n")


def test_hline_vline():
    c = _compile("size 5 5\nfill grass\nhline pavement 1 3 2\n"
                 "vline pavement 0 4 4\nplayer 0 0 south\n")
    assert [c.get(x, 2) for x in range(5)] == [0, 4, 4, 4, 4]
    assert [c.get(4, y) for y in range(5)] == [4, 4, 4, 4, 4]


def test_river_vertical_and_horizontal():
    c = _compile("size 6 6\nfill grass\nriver vertical 2 width 2\n"
                 "river horizontal 4 width 1\nplayer 0 0 south\n")
    # bande verticale water sur colonnes 2 et 3, toute la hauteur
    assert c.get(2, 0) == 5 and c.get(3, 5) == 5 and c.get(1, 0) == 0
    # bande horizontale water sur la ligne 4, toute la largeur
    assert c.get(0, 4) == 5 and c.get(5, 4) == 5
