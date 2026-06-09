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
