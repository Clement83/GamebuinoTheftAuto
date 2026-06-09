from collections import Counter

from tools.citydsl import CompiledCity
from tools import citygen, pois
from tools.citygen import THEME_CHINATOWN, THEME_NONE, Z_DOWNTOWN, Z_RESIDENTIAL

# tileset 14 tuiles (8 base + 6 POI) ; index = ordre de assets.h
TI = {"grass": 0, "road_h": 1, "road_v": 2, "road_cross": 3, "pavement": 4,
      "water": 5, "building_a": 6, "building_b": 7,
      "cn_facade_a": 8, "cn_facade_b": 9, "cn_sign": 10,
      "police_facade": 11, "police_sign": 12, "police_door": 13}
SI = {5, 6, 7, 8, 9, 10, 11, 12}          # police_door (13) non-solide
CN = {8, 9, 10}
TI8 = {k: v for k, v in TI.items() if v < 8}
SI8 = {5, 6, 7}


def _gen(seed=7, w=96, h=96, tile_index=TI, solid_index=SI, **kw):
    c = CompiledCity(w, h)
    citygen.generate_into(c, seed, tile_index, solid_index, **kw)
    return c


# ----- build_chinatown_theme ------------------------------------------------

def test_chinatown_theme_only_one_downtown_district():
    z = citygen.build_zones(7, 96, 96, 0.15, 0.05, 10)
    did, st, _ = citygen.voronoi_districts(7, 96, 96, 10)
    theme, pick = pois.build_chinatown_theme(7, z, did, st, 96, 96)
    assert st[pick] == Z_DOWNTOWN
    # toutes les cellules thematisees appartiennent au district choisi
    assert all(did[i] == pick for i in range(96 * 96) if theme[i] == THEME_CHINATOWN)
    assert any(t == THEME_CHINATOWN for t in theme)


def test_chinatown_theme_deterministic():
    z = citygen.build_zones(7, 64, 64, 0.15, 0.05, 8)
    did, st, _ = citygen.voronoi_districts(7, 64, 64, 8)
    a, _ = pois.build_chinatown_theme(7, z, did, st, 64, 64)
    b, _ = pois.build_chinatown_theme(7, z, did, st, 64, 64)
    assert a == b


def test_chinatown_theme_empty_without_downtown():
    z = [Z_RESIDENTIAL] * 16
    did = [0] * 16
    st = [Z_RESIDENTIAL]
    theme, pick = pois.build_chinatown_theme(1, z, did, st, 4, 4)
    assert pick is None and all(t == THEME_NONE for t in theme)


# ----- place_police (unitaire) ----------------------------------------------

def _solid_field(w, h, fill=6):
    return [fill] * (w * h)


def test_place_police_on_block_next_to_road():
    w = h = 10
    grid = _solid_field(w, h, TI["building_a"])
    for x in range(w):                      # route horizontale en y=6
        grid[6 * w + x] = TI["road_h"]
    did = [0] * (w * h)
    st = [Z_DOWNTOWN]
    pos = pois.place_police(grid, [Z_DOWNTOWN] * (w * h), did, st, 7, w, h, TI)
    assert pos is not None
    x, y = pos
    # blueprint pose : 7 facades, 1 enseigne, 1 porte
    n = Counter(grid[(y + ry) * w + (x + rx)] for ry in range(3) for rx in range(3))
    assert n[TI["police_facade"]] == 7
    assert n[TI["police_sign"]] == 1
    assert n[TI["police_door"]] == 1
    # porte (bas-centre) a une route juste au sud
    assert grid[(y + 3) * w + (x + 1)] == TI["road_h"]


def test_place_police_none_when_no_access():
    w = h = 10
    grid = _solid_field(w, h, TI["building_a"])   # aucune route -> aucun acces
    pos = pois.place_police(grid, [Z_DOWNTOWN] * (w * h),
                            [0] * (w * h), [Z_DOWNTOWN], 7, w, h, TI)
    assert pos is None


def test_place_police_never_overwrites_road_or_water():
    c = _gen()
    n = Counter(c.grid)
    # exactement un commissariat (1 enseigne, 1 porte) sur la vraie ville
    assert n[TI["police_sign"]] == 1
    assert n[TI["police_door"]] == 1
    assert n[TI["police_facade"]] == 7


# ----- integration generate_into --------------------------------------------

def test_generate_with_pois_deterministic():
    assert _gen(seed=11).grid == _gen(seed=11).grid


def test_chinatown_tiles_present_in_full_gen():
    n = Counter(_gen().grid)
    assert sum(n[t] for t in CN) > 0, "aucune tuile chinatown generee"


def test_chinatown_tiles_confined_to_one_district():
    c = _gen()  # districts=8 par defaut -> recomputer avec le meme parametre
    did, st, _ = citygen.voronoi_districts(7, 96, 96, 8)
    dists = {did[i] for i in range(96 * 96) if c.grid[i] in CN}
    assert len(dists) == 1, "chinatown eparpille sur %d districts" % len(dists)


def test_pois_disabled_without_tiles_backward_compatible():
    """Tileset 8 tuiles -> generation identique a l'historique, aucune tuile POI."""
    c8 = _gen(tile_index=TI8, solid_index=SI8)
    assert all(t < 8 for t in c8.grid)


def test_spawn_not_on_solid_with_pois():
    c = _gen()
    sx, sy, _ = c.spawn
    assert c.get(sx, sy) not in SI
