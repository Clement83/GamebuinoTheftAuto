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
    ("estaque",   0.22, 0.12, False),
    ("stantoine", 0.40, 0.10, False),
    ("larose",    0.65, 0.09, False),
    ("laviste",   0.33, 0.20, False),
    ("stjerome",  0.47, 0.21, False),
    ("standre",   0.62, 0.25, False),
    ("gombert",   0.80, 0.28, False),
    ("allauch",   0.90, 0.42, False),
    ("stcharles", 0.42, 0.33, True),
    ("belledmai", 0.55, 0.38, True),
    ("blancarde", 0.66, 0.37, True),
    ("sthenri",   0.33, 0.40, False),
    ("panier",    0.35, 0.46, True),
    ("plaine",    0.55, 0.46, True),
    ("stbarnabe", 0.66, 0.46, False),
    ("timone",    0.63, 0.52, True),
    ("vieuxport", 0.38, 0.54, True),
    ("corniche",  0.25, 0.58, False),
    ("prado",     0.52, 0.60, True),
    ("stemarg",   0.62, 0.62, False),
    ("vivaux",    0.78, 0.55, False),
    ("roucas",    0.45, 0.68, False),
    ("mazargues", 0.55, 0.72, False),
    ("sttronc",   0.78, 0.70, False),
    ("goudes",    0.12, 0.65, False),
    ("calanques", 0.62, 0.82, False),
]

BEACH_W = 2            # largeur de plage (sable) le long de la mer


def build_terrain(seed, w, h, sea, road, park):
    """(zones[w*h], sea[w*h], district_id[w*h], seed_type[ndist]) depuis les masques.

    La terre urbaine (beige de l'image) est UNIFORMEMENT dense (Z_DOWNTOWN) : le
    vert vient des massifs extraits, pas des trous du remplissage residentiel.
    seed_type reste varie pour la distribution des THEMES (port/casse/chantier)."""
    z = [Z_DOWNTOWN] * (w * h)
    for i in range(w * h):
        if sea[i]:
            z[i] = Z_WATER
        elif park[i]:                             # massifs verts extraits de l'image
            z[i] = Z_PARK

    # plage : terre a <= BEACH_W de la mer (override le parc cotier)
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


def _thin(mask, w, h):
    """Squelettisation Zhang-Suen : amincit les axes epais a 1 tuile de large
    (preserve la connexite) -> classement h/v propre, libere de la place."""
    M = list(mask)

    def nb(x, y):
        return [M[(y - 1) * w + x], M[(y - 1) * w + x + 1], M[y * w + x + 1],
                M[(y + 1) * w + x + 1], M[(y + 1) * w + x], M[(y + 1) * w + x - 1],
                M[y * w + x - 1], M[(y - 1) * w + x - 1]]   # P2..P9 horaire

    changed = True
    while changed:
        changed = False
        for step in (0, 1):
            todel = []
            for y in range(1, h - 1):
                for x in range(1, w - 1):
                    if not M[y * w + x]:
                        continue
                    p = nb(x, y)
                    b = sum(p)
                    if b < 2 or b > 6:
                        continue
                    a = sum(1 for k in range(8) if not p[k] and p[(k + 1) % 8])
                    if a != 1:
                        continue
                    if step == 0:
                        if p[0] and p[2] and p[4]:
                            continue
                        if p[2] and p[4] and p[6]:
                            continue
                    else:
                        if p[0] and p[2] and p[6]:
                            continue
                        if p[0] and p[4] and p[6]:
                            continue
                    todel.append(y * w + x)
            if todel:
                changed = True
                for i in todel:
                    M[i] = False
    for i in range(w * h):
        mask[i] = M[i]


def _orthogonalize(mask, w, h):
    """Apres amincissement (squelette 8-connexe), ponte les liens diagonaux pour
    garantir la 4-connexite (escalier orthogonal) requise par l'IA trafic."""
    add = []
    for y in range(h - 1):
        for x in range(w - 1):
            a = mask[y * w + x]; b = mask[y * w + x + 1]
            c = mask[(y + 1) * w + x]; d = mask[(y + 1) * w + x + 1]
            if a and d and not b and not c:        # diagonale \\ -> ajoute droite-haut
                add.append((x + 1, y))
            elif b and c and not a and not d:       # diagonale / -> ajoute gauche-haut
                add.append((x, y))
    for x, y in add:
        mask[y * w + x] = True


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
    _thin(mask, w, h)                       # axes epais de l'image -> 1 tuile
    _orthogonalize(mask, w, h)              # squelette 8-connexe -> 4-connexe
    _local_streets(mask, w, h, z, seed)     # eperons (deja fins) greffes dessus
    _keep_main_component(mask, w, h)
    return _classify(mask, w, h)


# --- API ---------------------------------------------------------------------

def generate_into(city, seed, tile_index, solid_index,
                  image_path=marseille_map.IMAGE_PATH, density=0.85):
    """Genere la carte Marseille depuis l'image puis delegue a citygen.assemble."""
    idx = citygen.resolve_tile_idx(tile_index)
    w, h = city.w, city.h
    sea, road, park = marseille_map.extract(image_path, w, h)
    z, sea, district_id, seed_type = build_terrain(seed, w, h, sea, road, park)
    r = build_roads(seed, w, h, z, road)
    return citygen.assemble(city, seed, tile_index, solid_index, idx, z, sea, r,
                            district_id, seed_type, density)
