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


# --- couche ROUTES -----------------------------------------------------------


def _major_lines(seed, w, h, margin=3):
    """Positions des grandes avenues (colonnes, lignes), grille jittee deterministe."""
    rng = random.Random(seed + 5)
    major_cols = set()
    pos = margin
    while pos < w - margin:
        for i in range(pos, pos + 2):
            if i < w - margin:
                major_cols.add(i)
        pos += max(6, 14 + rng.randint(-3, 4))
    major_rows = set()
    pos = margin
    while pos < h - margin:
        for i in range(pos, pos + 2):
            if i < h - margin:
                major_rows.add(i)
        pos += max(6, 14 + rng.randint(-3, 4))
    return major_cols, major_rows


def draw_roads(zone_grid, seed, w, h, margin=3):
    """Grille de routes hierarchique (avenues + rues fines en downtown) -> liste plate de codes."""
    rng = random.Random(seed + 5)
    # avenues majeures (memes positions que _major_lines, meme sequence rng)
    major_cols = set()
    pos = margin
    while pos < w - margin:
        for i in range(pos, pos + 2):
            if i < w - margin:
                major_cols.add(i)
        pos += max(6, 14 + rng.randint(-3, 4))
    major_rows = set()
    pos = margin
    while pos < h - margin:
        for i in range(pos, pos + 2):
            if i < h - margin:
                major_rows.add(i)
        pos += max(6, 14 + rng.randint(-3, 4))

    # rues mineures (downtown uniquement)
    minor_cols = set()
    pos = margin
    while pos < w - margin:
        minor_cols.add(pos)
        pos += max(4, 7 + rng.randint(-2, 2))
    minor_rows = set()
    pos = margin
    while pos < h - margin:
        minor_rows.add(pos)
        pos += max(4, 7 + rng.randint(-2, 2))

    result = ['.'] * (w * h)
    for y in range(margin, h - margin):
        for x in range(margin, w - margin):
            zone = zone_grid[y * w + x]
            if zone == Z_WATER:
                continue
            on_col = (x in major_cols) or (x in minor_cols and zone == Z_DOWNTOWN)
            on_row = (y in major_rows) or (y in minor_rows and zone == Z_DOWNTOWN)
            if on_col and on_row:
                result[y * w + x] = 'x'
            elif on_col:
                result[y * w + x] = 'v'
            elif on_row:
                result[y * w + x] = 'h'
    return result


def add_bridges(zone_grid, road_grid, seed, w, h, max_span=14, margin=3):
    """Ponts: prolonge les avenues majeures sur l'eau si la portee <= max_span.

    Un pont n'est trace que sur une etendue d'eau bordee de terre des deux cotes
    et entierement dans l'interieur (>= margin, <= dim-margin) : pas de route en
    bordure ni de route qui "part nulle part" en touchant le bord de la carte.
    """
    out = list(road_grid)
    major_cols, major_rows = _major_lines(seed, w, h)

    for x in major_cols:
        y = 0
        while y < h:
            if zone_grid[y * w + x] == Z_WATER:
                start = y
                while y < h and zone_grid[y * w + x] == Z_WATER:
                    y += 1
                if (y - start) <= max_span and start >= margin and y <= h - margin:
                    for yy in range(start, y):
                        out[yy * w + x] = 'v'
            else:
                y += 1

    for y in major_rows:
        x = 0
        while x < w:
            if zone_grid[y * w + x] == Z_WATER:
                start = x
                while x < w and zone_grid[y * w + x] == Z_WATER:
                    x += 1
                if (x - start) <= max_span and start >= margin and x <= w - margin:
                    for xx in range(start, x):
                        out[y * w + xx] = 'h'
            else:
                x += 1

    return out


# --- couche BLOCS / TROTTOIRS / SPAWN ---------------------------------------


def fill_blocks(grid, zone_grid, road_grid, seed, w, h, density, idx):
    """Remplit les blocs par zone (eau/parc/batiments/grass) en sautant les routes."""
    rng = random.Random(seed + 6)
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if road_grid[i] != '.':
                continue
            zone = zone_grid[i]
            if zone == Z_WATER:
                grid[i] = idx['water']
            elif zone == Z_PARK:
                grid[i] = idx['grass']
            elif zone == Z_DOWNTOWN:
                pbuild = density
                b = idx['building_b'] if _value_noise(seed + 7, x * 0.2, y * 0.2) > 0.35 else idx['building_a']
                grid[i] = b if rng.random() < pbuild else idx['grass']
            elif zone == Z_RESIDENTIAL:
                pbuild = density * 0.5
                b = idx['building_a'] if _value_noise(seed + 7, x * 0.2, y * 0.2) > 0.4 else idx['building_b']
                grid[i] = b if rng.random() < pbuild else idx['grass']


def add_pavement(grid, w, h, idx):
    """Pose un trottoir d'un seul cote de chaque rue, epaisseur 1.

    Trottoir cote nord des rues horizontales (route juste au sud) et cote ouest
    des rues verticales (route juste a l'est). Pas de trottoir des deux cotes ni
    de double epaisseur : chaque tuile-trottoir est collee a sa chaussee. Les
    batiments de l'autre cote bordent directement la route. L'eau (bord de
    fleuve) n'est jamais convertie."""
    rh, rv, rx = idx['road_h'], idx['road_v'], idx['road_cross']
    road_ids = {rh, rv, rx}
    water = idx['water']
    targets = []
    for y in range(h):
        for x in range(w):
            i = y * w + x
            t = grid[i]
            if t in road_ids or t == water:
                continue
            south = grid[(y + 1) * w + x] if y + 1 < h else None
            east = grid[y * w + (x + 1)] if x + 1 < w else None
            if south in (rh, rx) or east in (rv, rx):
                targets.append(i)
    for i in targets:
        grid[i] = idx['pavement']


def pick_spawn(grid, w, h, solid_index, zone_grid, idx):
    """Spawn deterministe: spirale Chebyshev depuis le centre, tier de preference."""
    ox, oy = w // 2, h // 2
    pavement = idx['pavement']
    road_ids = {idx['road_h'], idx['road_v'], idx['road_cross']}
    grass = idx['grass']
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
                t = grid[sy * w + sx]
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
    for tier in (0, 1, 2, 3):
        if tier in found:
            sx, sy = found[tier]
            return (sx, sy, 2)
    return None


def generate_into(city, seed, tile_index, solid_index,
                  water=0.18, parks=0.10, density=0.85, districts=8):
    """Genere la ville en couches: zones -> routes -> ponts -> blocs -> routes -> trottoirs -> spawn."""
    # 1. resolution des tuiles requises
    names = ('grass', 'road_h', 'road_v', 'road_cross', 'pavement',
             'water', 'building_a', 'building_b')
    idx = {}
    for name in names:
        if name not in tile_index:
            raise ValueError("citygen: tuile manquante: '%s'" % name)
        idx[name] = tile_index[name]

    # 2. dimensions
    w, h = city.w, city.h

    # 3. zones
    z = build_zones(seed, w, h, water, parks, districts)

    # 4. routes + ponts
    r = draw_roads(z, seed, w, h)
    r = add_bridges(z, r, seed, w, h)

    # 5. base grass
    city.grid = [idx['grass']] * (w * h)

    # 6. blocs par zone (saute les routes)
    fill_blocks(city.grid, z, r, seed, w, h, density, idx)

    # 7. routes par-dessus la grille
    for i in range(w * h):
        c = r[i]
        if c == 'h':
            city.grid[i] = idx['road_h']
        elif c == 'v':
            city.grid[i] = idx['road_v']
        elif c == 'x':
            city.grid[i] = idx['road_cross']

    # 8. trottoirs
    add_pavement(city.grid, w, h, idx)

    # 9. spawn
    city.spawn = pick_spawn(city.grid, w, h, solid_index, z, idx)
    return city.spawn
