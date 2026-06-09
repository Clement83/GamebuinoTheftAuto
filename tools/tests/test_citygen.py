import pytest, random
from collections import Counter
from tools.citydsl import CompiledCity
from tools import citygen
from tools.citygen import noise_field, _value_noise, _hash01, _quantile_threshold
from tools.citygen import (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL, build_zones)


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
    z = build_zones(7, 96, 96, water=0.18, parks=0.10, districts=8)
    assert len(z) == 96 * 96
    n = Counter(z)
    for t in (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL):
        assert n[t] > 0, "zone %d absente" % t
    assert all(v in (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL) for v in z)


def test_build_zones_deterministic():
    a = build_zones(7, 64, 64, 0.18, 0.10, 8)
    assert a == build_zones(7, 64, 64, 0.18, 0.10, 8)
    assert a != build_zones(9, 64, 64, 0.18, 0.10, 8)


def test_water_fraction_roughly_matches():
    z = build_zones(3, 96, 96, water=0.20, parks=0.05, districts=8)
    frac = Counter(z)[Z_WATER] / (96 * 96)
    assert 0.18 <= frac <= 0.40


def test_downtown_more_central_than_residential():
    z = build_zones(7, 96, 96, 0.15, 0.05, 10)
    cx = cy = 48
    def mean_dist(t):
        ds = [((x-cx)**2+(y-cy)**2)**0.5 for y in range(96) for x in range(96)
              if z[y*96+x] == t]
        return sum(ds)/len(ds)
    assert mean_dist(Z_DOWNTOWN) < mean_dist(Z_RESIDENTIAL)
