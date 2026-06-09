import pytest, random
from collections import Counter
from tools.citydsl import CompiledCity
from tools import citygen
from tools.citygen import noise_field, _value_noise, _hash01, _quantile_threshold
from tools.citygen import (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL, Z_SAND,
                           build_zones)


def test_hash_deterministic_and_range():
    assert _hash01(7, 3, 4) == _hash01(7, 3, 4)
    assert _hash01(7, 3, 4) != _hash01(7, 3, 5)
    assert 0.0 <= _hash01(1, 10, 20) < 1.0


def test_noise_field_shape_range_deterministic():
    f1 = noise_field(7, 16, 12, scale=4.0)
    assert len(f1) == 12 and len(f1[0]) == 16
    assert all(0.0 <= v <= 1.0 for row in f1 for v in row)
    f2 = noise_field(7, 16, 12, scale=4.0)
    assert f1 == f2
    f3 = noise_field(8, 16, 12, scale=4.0)
    assert f1 != f3


def test_noise_is_smooth_not_white():
    f = noise_field(3, 40, 40, scale=8.0)
    diffs = [abs(f[y][x] - f[y][x + 1]) for y in range(40) for x in range(39)]
    assert sum(diffs) / len(diffs) < 0.2


def test_quantile_threshold_fraction():
    f = noise_field(5, 30, 30, scale=6.0)
    thr = _quantile_threshold(f, 0.3)
    below = sum(1 for row in f for v in row if v < thr)
    assert 0.2 * 900 <= below <= 0.4 * 900


def test_build_zones_all_types_present_and_labeled():
    z, _ = build_zones(7, 96, 96, water=0.18, parks=0.10, districts=8)
    assert len(z) == 96 * 96
    n = Counter(z)
    for t in (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL):
        assert n[t] > 0, "zone %d absente" % t
    assert all(v in (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL, Z_SAND) for v in z)


def test_build_zones_deterministic():
    a = build_zones(7, 64, 64, 0.18, 0.10, 8)
    assert a == build_zones(7, 64, 64, 0.18, 0.10, 8)
    assert a != build_zones(9, 64, 64, 0.18, 0.10, 8)
    assert len(a) == 2 and len(a[0]) == 64 * 64


def test_water_fraction_roughly_matches():
    z, _ = build_zones(3, 96, 96, water=0.20, parks=0.05, districts=8)
    frac = Counter(z)[Z_WATER] / (96 * 96)
    assert 0.18 <= frac <= 0.50          # +mer cotiere forcee


def test_downtown_more_central_than_residential():
    z, _ = build_zones(7, 96, 96, 0.15, 0.05, 10)
    cx = cy = 48
    def mean_dist(t):
        ds = [((x-cx)**2+(y-cy)**2)**0.5 for y in range(96) for x in range(96)
              if z[y*96+x] == t]
        return sum(ds)/len(ds)
    assert mean_dist(Z_DOWNTOWN) < mean_dist(Z_RESIDENTIAL)


from tools.citygen import draw_roads, add_bridges

def _zone_and_roads(seed=7, w=96, h=96):
    z, _ = build_zones(seed, w, h, 0.15, 0.05, 10)
    r = draw_roads(z, seed, w, h)
    return z, r


def test_roads_present_and_not_on_border():
    z, r = _zone_and_roads()
    assert any(v != '.' for v in r)
    m = 3
    assert all(r[y*96+x] == '.' for y in range(96) for x in range(96)
               if x < m or x >= 96-m or y < m or y >= 96-m)


def test_downtown_denser_roads_than_residential():
    z, r = _zone_and_roads()
    def road_frac(t):
        idx = [i for i, zz in enumerate(z) if zz == t]
        return sum(1 for i in idx if r[i] != '.') / max(1, len(idx))
    assert road_frac(Z_DOWNTOWN) > road_frac(Z_RESIDENTIAL)


def test_roads_not_on_water_before_bridges():
    z, r = _zone_and_roads()
    assert all(not (r[i] != '.' and z[i] == Z_WATER) for i in range(len(z)))


def test_bridges_add_road_over_water():
    z, r = _zone_and_roads()
    rb = add_bridges(z, list(r), 7, 96, 96)
    before = sum(1 for i in range(len(z)) if z[i] == Z_WATER and r[i] != '.')
    after = sum(1 for i in range(len(z)) if z[i] == Z_WATER and rb[i] != '.')
    assert after > before


def test_bridges_not_on_border():
    z, r = _zone_and_roads()
    rb = add_bridges(z, list(r), 7, 96, 96)
    m = 3
    assert all(rb[y*96+x] == '.' for y in range(96) for x in range(96)
               if x < m or x >= 96-m or y < m or y >= 96-m)


def test_draw_roads_deterministic():
    z, r = _zone_and_roads(); _, r2 = _zone_and_roads()
    assert r == r2


TI = {"grass":0,"road_h":1,"road_v":2,"road_cross":3,"pavement":4,"water":5,"building_a":6,"building_b":7}
SI = {5, 6, 7}
def _gen(seed=7, w=96, h=96, **kw):
    c = CompiledCity(w, h); citygen.generate_into(c, seed, TI, SI, **kw); return c

def test_full_all_tiles_present():
    n = Counter(_gen().grid)
    for t in (0,1,2,3,4,5,6,7):
        assert n[t] > 0, "tuile %d absente" % t

def test_downtown_denser_buildings_than_residential():
    c = _gen(); z, _ = build_zones(7, 96, 96, 0.18, 0.10, 8)
    roads = (1, 2, 3, 4)            # routes + trottoirs : hors zonage bati
    def bfrac(t):                   # densite de bati parmi les cellules batissables
        idx = [i for i, zz in enumerate(z) if zz == t and c.grid[i] not in roads]
        return sum(1 for i in idx if c.grid[i] in (6, 7)) / max(1, len(idx))
    assert bfrac(Z_DOWNTOWN) > bfrac(Z_RESIDENTIAL)

def test_pavement_single_thickness():
    """Trottoir epaisseur 1 : chaque tuile-trottoir est collee a une route (jamais a 2 cases)."""
    c = _gen(); w = h = 96
    road_ids = {1, 2, 3}
    pavement = 4
    for y in range(h):
        for x in range(w):
            if c.grid[y*w+x] != pavement:
                continue
            touches_road = any(
                0 <= nx < w and 0 <= ny < h and c.grid[ny*w+nx] in road_ids
                for nx, ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1))
            )
            assert touches_road, "trottoir non colle a une route en (%d,%d)" % (x, y)


def test_pavement_only_north_or_west_side():
    """Un seul cote : un trottoir a toujours une route au sud (rue h) ou a l'est (rue v).

    Donc jamais de trottoir cote sud d'une rue horizontale ni cote est d'une rue
    verticale -> chaque rue n'est bordee que d'un cote."""
    c = _gen(); w = h = 96
    g = c.grid
    rh, rv, rx, pavement = 1, 2, 3, 4
    for y in range(h):
        for x in range(w):
            if g[y*w+x] != pavement:
                continue
            south = g[(y+1)*w+x] if y+1 < h else None
            east = g[y*w+(x+1)] if x+1 < w else None
            assert south in (rh, rx) or east in (rv, rx), \
                "trottoir sans route au sud/est (mauvais cote) en (%d,%d)" % (x, y)


def test_spawn_walkable():
    c = _gen(); sx,sy,sd = c.spawn
    assert 0<=sx<c.w and 0<=sy<c.h and c.get(sx,sy) not in SI and sd==2

def test_generate_deterministic():
    assert _gen(seed=11).grid == _gen(seed=11).grid
    assert _gen(seed=11).spawn == _gen(seed=11).spawn

def test_missing_tile_raises():
    with pytest.raises(ValueError):
        citygen.generate_into(CompiledCity(8,8), 1, {"grass":0}, set())
