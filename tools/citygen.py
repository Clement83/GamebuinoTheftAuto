"""Generateur de ville organique par value noise deterministe (pur, aucune I/O)."""
import math


def _hash01(seed, ix, iy):
    """Float pseudo-aleatoire deterministe dans [0,1) depuis des coords entieres de lattice."""
    h = (ix * 374761393 + iy * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
    h ^= (h >> 16)
    return (h & 0xFFFFFFFF) / 4294967296.0


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def _value_noise(seed, x, y):
    """Value noise bilineaire lisse en (x,y) flottants (espace lattice)."""
    x0, y0 = math.floor(x), math.floor(y)
    fx, fy = x - x0, y - y0
    v00 = _hash01(seed, x0, y0);     v10 = _hash01(seed, x0 + 1, y0)
    v01 = _hash01(seed, x0, y0 + 1); v11 = _hash01(seed, x0 + 1, y0 + 1)
    sx, sy = _smooth(fx), _smooth(fy)
    a = v00 + (v10 - v00) * sx
    b = v01 + (v11 - v01) * sx
    return a + (b - a) * sy


def noise_field(seed, w, h, scale, octaves=3, persistence=0.5):
    """Champ [h][w] de flottants dans [0,1], somme d'octaves de value noise."""
    field = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            amp, freq, total, norm = 1.0, 1.0 / scale, 0.0, 0.0
            for o in range(octaves):
                total += amp * _value_noise(seed + o * 101, x * freq, y * freq)
                norm += amp
                amp *= persistence
                freq *= 2.0
            field[y][x] = total / norm
    return field


def _quantile_threshold(field, frac):
    """Seuil tel qu'environ `frac` des cellules soient < seuil."""
    vals = sorted(v for row in field for v in row)
    if not vals:
        return 0.0
    i = max(0, min(len(vals) - 1, int(frac * len(vals))))
    return vals[i]
