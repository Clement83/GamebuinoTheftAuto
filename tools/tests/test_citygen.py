import pytest
from tools.citygen import noise_field, _value_noise, _hash01, _quantile_threshold
from tools.citydsl import CompiledCity
from tools import citygen
from collections import Counter

TI = {"grass":0,"road_h":1,"road_v":2,"road_cross":3,"pavement":4,"water":5,"building_a":6,"building_b":7}
SI = {5, 6, 7}

def _gen(seed=7, w=96, h=96, **kw):
    c = CompiledCity(w, h)
    citygen.generate_into(c, seed, TI, SI, **kw)
    return c

def _counts(c):
    return Counter(c.grid)


def test_generate_water_and_land_present():
    c = _gen(water=0.25)
    n = _counts(c); total = c.w * c.h
    assert n[5] > 0
    assert n[5] < total * 0.6
    assert (total - n[5]) > total * 0.3


def test_generate_has_roads_and_pavement():
    c = _gen()
    n = _counts(c)
    assert n[1] + n[2] + n[3] > 0
    assert n[4] > 0


def test_generate_deterministic_same_seed():
    assert _gen(seed=7).grid == _gen(seed=7).grid
    assert _gen(seed=7).grid != _gen(seed=9).grid


def test_missing_tile_raises():
    c = CompiledCity(8, 8)
    with pytest.raises(ValueError):
        citygen.generate_into(c, 1, {"grass": 0}, set())


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


def test_generate_has_buildings_and_parks():
    c = _gen(density=0.6, parks=0.15)
    n = _counts(c)
    assert n[6] + n[7] > 0
    assert n[0] > 0


def test_density_center_denser_than_edges():
    c = _gen(density=0.7)
    def build_frac(x0, y0, x1, y1):
        cells = [c.get(x, y) for y in range(y0, y1) for x in range(x0, x1)]
        b = sum(1 for v in cells if v in (6, 7))
        return b / len(cells)
    center = build_frac(c.w//2 - 12, c.h//2 - 12, c.w//2 + 12, c.h//2 + 12)
    corner = build_frac(0, 0, 20, 20)
    assert center >= corner


def test_spawn_is_walkable_and_in_bounds():
    c = _gen()
    assert c.spawn is not None
    sx, sy, sd = c.spawn
    assert 0 <= sx < c.w and 0 <= sy < c.h
    assert c.get(sx, sy) not in SI
    assert sd == 2


def test_full_generation_deterministic():
    assert _gen(seed=11).grid == _gen(seed=11).grid
    assert _gen(seed=11).spawn == _gen(seed=11).spawn
