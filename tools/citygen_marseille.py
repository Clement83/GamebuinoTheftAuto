"""Generateur de carte "Marseille" pilote par l'illustration marseille_simple.png.

Pipeline :
  1. tools.marseille_map.extract -> masques MER + ROUTE (vraie geographie).
  2. zones : mer -> eau, plage cotiere, collines (terre rurale loin des routes),
     centre/residentiel selon districts ancres aux vrais quartiers.
  3. routes : axes extraits de l'image + rues locales organiques + connexite.
  4. citygen.assemble : place TOUS les POI de campagne (chaine pois.py inchangee).

L'I/O image est isolee dans marseille_map ; ici tout est deterministe (image +
seed). Positions de districts ancrees en FRACTIONS de la carte.
"""
import math
import random

from tools import citygen, marseille_map
from tools.citygen import (Z_WATER, Z_PARK, Z_DOWNTOWN, Z_RESIDENTIAL, Z_SAND,
                           _value_noise)

# Quartiers ancres (fraction x,y de la carte, downtown?). Issus des libelles de
# l'illustration. Les centraux sont denses (downtown), la peripherie residentielle.
DISTRICT_SEEDS = [
    ("estaque",   0.11, 0.13, False),
    ("stantoine", 0.27, 0.08, False),
    ("larose",    0.55, 0.07, False),
    ("stjerome",  0.41, 0.16, False),
    ("gombert",   0.63, 0.17, False),
    ("allauch",   0.82, 0.18, False),
    ("stcharles", 0.36, 0.27, True),
    ("blancarde", 0.55, 0.28, True),
    ("sthenri",   0.13, 0.31, False),
    ("panier",    0.33, 0.36, True),
    ("vieuxport", 0.29, 0.41, True),
    ("plaine",    0.46, 0.38, True),
    ("stbarnabe", 0.63, 0.38, False),
    ("prado",     0.41, 0.48, True),
    ("lodi",      0.55, 0.47, True),
    ("roucas",    0.30, 0.55, False),
    ("stemarg",   0.51, 0.57, False),
    ("goudes",    0.11, 0.43, False),
    ("calanques", 0.64, 0.57, False),
]

HILL_DIST = 7          # terre a plus de N tuiles d'une route/mer -> colline (parc)
BEACH_W = 2            # largeur de plage (sable) le long de la mer


def _bfs_dist(src, w, h):
    """Distance Manhattan (BFS 4-connexe multi-source) ; -1 si inatteignable."""
    INF = -1
    dist = [INF] * (w * h)
    q = []
    for i, on in enumerate(src):
        if on:
            dist[i] = 0
            q.append(i)
    head = 0
    while head < len(q):
        i = q[head]; head += 1
        x, y = i % w, i // w
        d = dist[i] + 1
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h:
                j = ny * w + nx
                if dist[j] == INF:
                    dist[j] = d
                    q.append(j)
    return dist


def build_terrain(seed, w, h, sea, road):
    """(zones[w*h], sea[w*h], district_id[w*h], seed_type[ndist]) depuis les masques."""
    z = [Z_RESIDENTIAL] * (w * h)
    for i in range(w * h):
        if sea[i]:
            z[i] = Z_WATER

    # plage : terre a <= BEACH_W de la mer
    near = [False] * (w * h)
    for y in range(h):
        for x in range(w):
            if not sea[y * w + x]:
                continue
            for dy in range(-BEACH_W, BEACH_W + 1):
                for dx in range(-BEACH_W, BEACH_W + 1):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and not sea[ny * w + nx]:
                        near[ny * w + nx] = True
    for i in range(w * h):
        if near[i] and z[i] != Z_WATER:
            z[i] = Z_SAND

    # noyau urbain : disque autour des quartiers DENSES (downtown). Hors de ce
    # noyau, la terre loin des routes devient massif (parc) -> ville dense
    # entouree de collines (l'Etoile, Allauch, les Calanques).
    dt_seeds = [(int(fx * w), int(fy * h))
                for (_, fx, fy, dt) in DISTRICT_SEEDS if dt]
    r_core = 0.30 * w

    def in_core(x, y):
        return any((x - sx) ** 2 + (y - sy) ** 2 <= r_core ** 2
                   for sx, sy in dt_seeds)

    # collines (parcs) : terre loin des routes/mer, hors du noyau urbain.
    seed_src = [sea[i] or road[i] for i in range(w * h)]
    dist = _bfs_dist(seed_src, w, h)
    nf = citygen.noise_field(seed + 16, w, h, scale=max(6.0, w / 12.0))
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if z[i] in (Z_WATER, Z_SAND):
                continue
            d = dist[i]
            if d >= HILL_DIST and not in_core(x, y) and nf[y][x] < 0.55:
                z[i] = Z_PARK

    # legende/titre de l'image (coin haut-gauche) -> parc (cache le rectangle)
    lx1 = int(marseille_map.LEGEND_FRAC[2] * w)
    ly1 = int(marseille_map.LEGEND_FRAC[3] * h)
    for y in range(ly1):
        for x in range(lx1):
            i = y * w + x
            if z[i] != Z_WATER:
                z[i] = Z_PARK

    # districts (Voronoi sur graines ancrees ; eau ignoree au remplissage)
    seeds = [(int(fx * w), int(fy * h)) for (_, fx, fy, _) in DISTRICT_SEEDS]
    seed_type = [Z_DOWNTOWN if dt else Z_RESIDENTIAL
                 for (_, _, _, dt) in DISTRICT_SEEDS]
    district_id = [0] * (w * h)
    for y in range(h):
        for x in range(w):
            best_i, best_d = 0, None
            for i, (gx, gy) in enumerate(seeds):
                d = (gx - x) ** 2 + (gy - y) ** 2
                if best_d is None or d < best_d:
                    best_d, best_i = d, i
            district_id[y * w + x] = best_i

    return z, sea, district_id, seed_type


# --- ROUTES ------------------------------------------------------------------

def _local_streets(mask, w, h, z, seed):
    """Eperons locaux organiques greffes sur les axes extraits (restent donc
    connectes au reseau principal). Plus denses en centre (Z_DOWNTOWN)."""
    rng = random.Random(seed + 90)
    nf = citygen.noise_field(seed + 91, w, h, scale=max(5.0, w / 16.0))
    base = list(mask)        # ne greffe que sur les axes d'origine
    for y in range(2, h - 2):
        for x in range(2, w - 2):
            i = y * w + x
            if not base[i] or z[i] == Z_WATER:
                continue
            dense = z[i] == Z_DOWNTOWN
            if rng.random() > (0.14 if dense else 0.05):
                continue
            ang = rng.uniform(0, math.pi)
            length = (4 if dense else 3) + int(nf[y][x] * (8 if dense else 5))
            px, py = x + 0.0, y + 0.0
            lx, ly = x, y
            for k in range(length):
                px += math.cos(ang) + _value_noise(seed + 92, k + x, y) - 0.5
                py += math.sin(ang) + _value_noise(seed + 93, k + y, x) - 0.5
                nx, ny = int(round(px)), int(round(py))
                # marche orthogonale (4-connexite)
                while (lx, ly) != (nx, ny):
                    if lx != nx:
                        lx += 1 if nx > lx else -1
                    elif ly != ny:
                        ly += 1 if ny > ly else -1
                    if not (0 <= lx < w and 0 <= ly < h) or z[ly * w + lx] == Z_WATER:
                        break
                    mask[ly * w + lx] = True


def _keep_main_component(mask, w, h):
    """Ne garde que la plus grande composante 4-connexe de routes."""
    seen = [False] * (w * h)
    best, best_n = [], 0
    for s in range(w * h):
        if not mask[s] or seen[s]:
            continue
        comp = [s]; seen[s] = True; k = 0
        while k < len(comp):
            i = comp[k]; k += 1
            x, y = i % w, i // w
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h:
                    j = ny * w + nx
                    if mask[j] and not seen[j]:
                        seen[j] = True; comp.append(j)
        if len(comp) > best_n:
            best_n, best = len(comp), comp
    keep = set(best)
    for i in range(w * h):
        if mask[i] and i not in keep:
            mask[i] = False


def _classify(mask, w, h):
    """Masque -> codes '.'/'h'/'v'/'x' selon la connexite des voisins."""
    r = ['.'] * (w * h)
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if not mask[i]:
                continue
            n = y > 0 and mask[i - w]
            s = y < h - 1 and mask[i + w]
            e = x < w - 1 and mask[i + 1]
            wst = x > 0 and mask[i - 1]
            vert, horiz = (n or s), (e or wst)
            if vert and horiz:
                r[i] = 'x'
            elif vert:
                r[i] = 'v'
            elif horiz:
                r[i] = 'h'
            else:
                r[i] = 'x'
    return r


def build_roads(seed, w, h, z, road):
    """Codes routes depuis le masque extrait + rues locales + connexite."""
    mask = [bool(road[i]) and z[i] != Z_WATER for i in range(w * h)]
    _local_streets(mask, w, h, z, seed)
    _keep_main_component(mask, w, h)
    return _classify(mask, w, h)


# --- API ---------------------------------------------------------------------

def generate_into(city, seed, tile_index, solid_index,
                  image_path=marseille_map.IMAGE_PATH, density=0.85):
    """Genere la carte Marseille depuis l'image puis delegue a citygen.assemble."""
    idx = citygen.resolve_tile_idx(tile_index)
    w, h = city.w, city.h
    sea, road = marseille_map.extract(image_path, w, h)
    z, sea, district_id, seed_type = build_terrain(seed, w, h, sea, road)
    r = build_roads(seed, w, h, z, road)
    return citygen.assemble(city, seed, tile_index, solid_index, idx, z, sea, r,
                            district_id, seed_type, density)
