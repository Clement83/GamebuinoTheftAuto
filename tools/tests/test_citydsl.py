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
                "size 3 3\nseed\n",
                "size 3 3\nfill grass\nhline pavement 1 3\nplayer 0 0 south\n",
                "size 3 3\nfill grass\nvline pavement 0\nplayer 0 0 south\n"):
        with pytest.raises(CityError):
            _compile(src)


def test_double_size_is_error():
    with pytest.raises(CityError):
        _compile("size 3 3\nsize 4 4\nfill grass\nplayer 0 0 south\n")


def test_hline_isolated():
    c = _compile("size 5 5\nfill grass\nhline pavement 1 3 2\nplayer 0 0 south\n")
    # hline peint x=1..3 inclus ; x=0 et x=4 restent grass
    assert [c.get(x, 2) for x in range(5)] == [0, 4, 4, 4, 0]


def test_vline_isolated():
    c = _compile("size 5 5\nfill grass\nvline pavement 0 4 4\nplayer 0 0 south\n")
    assert [c.get(4, y) for y in range(5)] == [4, 4, 4, 4, 4]
    assert [c.get(x, 2) for x in range(4)] == [0, 0, 0, 0]   # colonnes 0..3 intactes


def test_river_vertical_and_horizontal():
    c = _compile("size 6 6\nfill grass\nriver vertical 2 width 2\n"
                 "river horizontal 4 width 1\nplayer 0 0 south\n")
    # bande verticale water sur colonnes 2 et 3, toute la hauteur
    assert c.get(2, 0) == 5 and c.get(3, 5) == 5 and c.get(1, 0) == 0
    # bande horizontale water sur la ligne 4, toute la largeur
    assert c.get(0, 4) == 5 and c.get(5, 4) == 5


def test_roadgrid_roads_crossings_pavement():
    # spacing 4, width 1, margin 0 -> routes aux x/y = 0,4,8...
    c = _compile("size 9 9\nfill grass\nroadgrid spacing 4 width 1\n"
                 "player 1 1 south\n")
    # croisement la ou colonne ET ligne sont des routes
    assert c.get(0, 0) == 3          # road_cross
    assert c.get(4, 4) == 3
    # colonne de route seule -> road_v
    assert c.get(0, 1) == 2          # road_v
    # ligne de route seule -> road_h
    assert c.get(1, 0) == 1          # road_h
    # case interieure adjacente a une route -> pavement
    assert c.get(1, 1) == 4          # pavement (voisin de (0,1) et (1,0))
    # case loin de toute route reste grass
    assert c.get(2, 2) == 0


def test_blocks_only_on_base_tile_and_deterministic():
    src = ("size 8 8\nseed 42\nfill grass\n"
           "rect water 0 0 7 0\n"                # ligne d'eau en haut
           "blocks building_a density 0.5 on grass\nplayer 0 1 south\n")
    c1 = _compile(src)
    c2 = _compile(src)
    assert c1.grid == c2.grid, "compilation non deterministe"
    # aucune eau n'a ete ecrasee par un building
    assert all(c1.get(x, 0) == 5 for x in range(8))
    # au moins un building a ete pose sur du grass
    assert any(v == 6 for v in c1.grid)


def test_blocks_missing_tile_raises_cityerror():
    with pytest.raises(CityError) as e:
        _compile("size 4 4\nfill grass\nblocks\nplayer 0 0 south\n")
    assert e.value.line_no == 3


def test_blocks_default_base_is_grass():
    # density 1.0 -> tout le grass devient building_b (solide) ; on rouvre (0,0)
    # en grass apres coup pour pouvoir y spawner.
    c = _compile("size 6 6\nseed 1\nfill grass\n"
                 "blocks building_b density 1.0\n"
                 "rect grass 0 0 0 0\nplayer 0 0 south\n")
    assert c.get(0, 0) == 0                        # case rouverte en grass
    assert all(c.get(x, y) == 7 for y in range(6) for x in range(6)
               if not (x == 0 and y == 0))          # le reste = building_b
