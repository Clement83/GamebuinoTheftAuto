from tools.citygen import noise_field, _value_noise, _hash01, _quantile_threshold


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
