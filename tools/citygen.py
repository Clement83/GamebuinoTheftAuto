"""Generateur de ville organique par value noise deterministe (pur, aucune I/O)."""
import math
import random


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


# --- couche ZONES ------------------------------------------------------------

Z_WATER = 0
Z_PARK = 1
Z_DOWNTOWN = 2
Z_RESIDENTIAL = 3


def build_zones(seed, w, h, water, parks, districts):
    """Carte de zones (eau, fleuve, quartiers Voronoi, parcs) -> liste plate w*h."""
    zones = [Z_RESIDENTIAL] * (w * h)

    # 1. blob d'eau (noise)
    nw = noise_field(seed + 1, w, h, scale=max(6.0, w / 8.0))
    thr = _quantile_threshold(nw, water)
    for y in range(h):
        for x in range(w):
            if nw[y][x] < thr:
                zones[y * w + x] = Z_WATER

    # 2. fleuve serpentant
    xf = w * 0.5
    rw = max(3, w // 28)
    for y in range(h):
        xf += (_value_noise(seed + 2, y * 0.15, 0.0) - 0.5) * 2.2
        cx = int(xf)
        for dx in range(-(rw // 2), rw - rw // 2):
            if 0 <= cx + dx < w:
                zones[y * w + (cx + dx)] = Z_WATER

    # 3. quartiers Voronoi (sur la terre)
    rng = random.Random(seed + 3)
    seeds = [(rng.randrange(w), rng.randrange(h)) for _ in range(districts)]
    ccx, ccy = w / 2.0, h / 2.0
    order = sorted(
        range(districts),
        key=lambda i: (seeds[i][0] - ccx) ** 2 + (seeds[i][1] - ccy) ** 2,
    )
    n_dt = math.ceil(districts * 0.4)
    seed_type = [Z_RESIDENTIAL] * districts
    for rank, i in enumerate(order):
        seed_type[i] = Z_DOWNTOWN if rank < n_dt else Z_RESIDENTIAL
    for y in range(h):
        for x in range(w):
            if zones[y * w + x] == Z_WATER:
                continue
            best_i, best_d = 0, None
            for i, (gx, gy) in enumerate(seeds):
                d = (gx - x) ** 2 + (gy - y) ** 2
                if best_d is None or d < best_d:
                    best_d, best_i = d, i
            zones[y * w + x] = seed_type[best_i]

    # 4. parcs (taches)
    npf = noise_field(seed + 4, w, h, scale=max(5.0, w / 12.0))
    thr = _quantile_threshold(npf, parks)
    for y in range(h):
        for x in range(w):
            if zones[y * w + x] != Z_WATER and npf[y][x] < thr:
                zones[y * w + x] = Z_PARK

    return zones


def generate_into(city, seed, tile_index, solid_index, water=0.22, parks=0.12, density=0.85):
    """Genere la geographie de la ville dans `city` (grass/eau/grille/trottoirs)."""
    w, h = city.w, city.h

    # 1. resolution des tuiles requises
    def _ti(name):
        if name not in tile_index:
            raise ValueError("citygen: tuile manquante: '%s'" % name)
        return tile_index[name]

    grass = _ti("grass")
    road_h = _ti("road_h")
    road_v = _ti("road_v")
    road_cross = _ti("road_cross")
    pavement = _ti("pavement")
    water_t = _ti("water")
    building_a = _ti("building_a")
    building_b = _ti("building_b")
    road_ids = {road_h, road_v, road_cross}

    # 2. base grass
    city.grid = [grass] * (w * h)

    # 3. blob d'eau (noise)
    nw = noise_field(seed + 1, w, h, scale=max(6.0, w / 8.0), octaves=3)
    thr = _quantile_threshold(nw, water)
    for y in range(h):
        for x in range(w):
            if nw[y][x] < thr:
                city.set(x, y, water_t)

    # 4. riviere serpentante
    xf = w * 0.5
    rw = max(3, w // 28)
    for y in range(h):
        xf += (_value_noise(seed + 2, y * 0.15, 0.0) - 0.5) * 2.2
        cx = int(xf)
        for dx in range(-(rw // 2), rw - rw // 2):
            city.set(cx + dx, y, water_t)

    # 5. grille de rues irreguliere (sur la terre)
    rng = random.Random(seed + 3)
    cols = set()
    pos = 3
    while pos < w:
        wd = 2 if rng.random() < 0.35 else 1
        for i in range(pos, pos + wd):
            if 0 <= i < w:
                cols.add(i)
        step = 10 + rng.randint(-3, 4)
        pos += max(4, step)
    rows = set()
    pos = 3
    while pos < h:
        wd = 2 if rng.random() < 0.35 else 1
        for i in range(pos, pos + wd):
            if 0 <= i < h:
                rows.add(i)
        step = 10 + rng.randint(-3, 4)
        pos += max(4, step)
    for y in range(h):
        on_row = y in rows
        for x in range(w):
            if city.get(x, y) == water_t:
                continue
            on_col = x in cols
            if on_col and on_row:
                city.set(x, y, road_cross)
            elif on_col:
                city.set(x, y, road_v)
            elif on_row:
                city.set(x, y, road_h)

    # 6. trottoirs (autour des rues, hors eau, sans chainage)
    to_pave = []
    for y in range(h):
        for x in range(w):
            t = city.get(x, y)
            if t in road_ids or t == water_t:
                continue
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if 0 <= nx < w and 0 <= ny < h and city.get(nx, ny) in road_ids:
                    to_pave.append((x, y))
                    break
    for x, y in to_pave:
        city.set(x, y, pavement)

    # 7. batiments (densite variable: gradient centre + bruit)
    nd = noise_field(seed + 4, w, h, scale=max(5.0, w / 10.0))
    brng = random.Random(seed + 5)
    cx, cy = w / 2.0, h / 2.0
    maxd = math.hypot(cx, cy)
    for y in range(h):
        for x in range(w):
            if city.get(x, y) != grass:
                continue
            g = 1.0 - (math.hypot(x - cx, y - cy) / maxd)
            p = density * (0.4 + 0.6 * g) * nd[y][x]
            if brng.random() < p:
                t = building_b if _value_noise(seed + 6, x * 0.2, y * 0.2) > 0.5 else building_a
                city.set(x, y, t)

    # 8. parcs (taches vertes dispersees a travers les blocs)
    npf = noise_field(seed + 7, w, h, scale=max(5.0, w / 12.0))
    pthr = _quantile_threshold(npf, parks)
    block_ids = {grass, building_a, building_b}
    for y in range(h):
        for x in range(w):
            if npf[y][x] < pthr and city.get(x, y) in block_ids:
                city.set(x, y, grass)

    # 9. spawn deterministe (spirale depuis le centre, tier de preference)
    ox, oy = w // 2, h // 2
    found = {}
    rmax = max(w, h)
    for r in range(rmax + 1):
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if max(abs(dx), abs(dy)) != r:
                    continue
                sx, sy = ox + dx, oy + dy
                if not (0 <= sx < w and 0 <= sy < h):
                    continue
                t = city.get(sx, sy)
                if t in solid_index:
                    continue
                if t == pavement:
                    tier = 0
                elif t in road_ids:
                    tier = 1
                elif t == grass:
                    tier = 2
                else:
                    tier = 3
                if tier not in found:
                    found[tier] = (sx, sy)
        if found:
            break
    spawn = None
    for tier in (0, 1, 2, 3):
        if tier in found:
            sx, sy = found[tier]
            spawn = (sx, sy, 2)
            break
    city.spawn = spawn
    return spawn
