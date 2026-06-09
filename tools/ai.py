"""Couche IA pure : errance sur grille (voitures sur routes, piétons trottoirs).

Aucune dépendance (pas de pygame/Gamebuino). Source de vérité du comportement
IA, portée à l'identique en C++ (gta/ai.h). Toutes les fonctions sont pures.

Repère : tuiles 8 px ; directions cardinales dans l'ordre de l'enum Dir
(N E S W). y vers le bas (écran). « Droite » = rotation 90° horaire.
"""
import math

TILE = 8

DIR_N, DIR_E, DIR_S, DIR_W = 0, 1, 2, 3
DIRS = [(0, -1), (1, 0), (0, 1), (-1, 0)]   # N E S W
RIGHT = [DIR_E, DIR_S, DIR_W, DIR_N]         # rotation 90° horaire
LEFT = [DIR_W, DIR_N, DIR_E, DIR_S]
BACK = [DIR_S, DIR_W, DIR_N, DIR_E]

# Identifiants de tuiles (cf. enum TileId / assets.h).
GRASS, ROAD_H, ROAD_V, ROAD_CROSS, PAVEMENT = 0, 1, 2, 3, 4
ROAD_TILES = (ROAD_H, ROAD_V, ROAD_CROSS)
WALK_TILES = (PAVEMENT, GRASS)   # piétons : trottoirs + herbe (parcs/espaces)

LANE = 2            # décalage latéral (px) à droite du centre de tuile
STRAIGHT_WEIGHT = 4  # poids « tout droit » vs 1 pour tourner
STUCK = 0xFF         # aucune sortie valide (tuile isolée)


def _in_bounds(w, h, tx, ty):
    return 0 <= tx < w and 0 <= ty < h


def is_drivable(grid, w, h, tx, ty):
    """Tuile carrossable (route) et en bornes."""
    return _in_bounds(w, h, tx, ty) and grid[ty * w + tx] in ROAD_TILES


def is_walkable(grid, w, h, tx, ty):
    """Tuile marchable (trottoir ou herbe) et en bornes."""
    return _in_bounds(w, h, tx, ty) and grid[ty * w + tx] in WALK_TILES


def has_exit(grid, w, h, tx, ty, classify):
    """True si au moins une des 4 tuiles voisines est valide (sortie possible)."""
    for dx, dy in DIRS:
        if classify(grid, w, h, tx + dx, ty + dy):
            return True
    return False


def lane_point(tx, ty, d):
    """Point-cible (px monde) de la voie pour traverser la tuile dans le sens d.

    Centre de tuile décalé de LANE vers la droite du sens de marche -> roulage
    à droite (Est->moitié Sud, Sud->moitié Ouest, etc.).
    """
    cx = tx * TILE + TILE // 2
    cy = ty * TILE + TILE // 2
    rx, ry = DIRS[RIGHT[d]]
    return (cx + rx * LANE, cy + ry * LANE)


def rng_next(state):
    """xorshift32 déterministe : renvoie (valeur, nouvel_état). état != 0."""
    x = state & 0xFFFFFFFF
    if x == 0:
        x = 0x1234567
    x ^= (x << 13) & 0xFFFFFFFF
    x ^= (x >> 17)
    x ^= (x << 5) & 0xFFFFFFFF
    x &= 0xFFFFFFFF
    return (x, x)


def pick_exit(grid, w, h, tx, ty, d, classify, state):
    """Choisit une direction de sortie depuis la tuile (tx,ty) venant du sens d.

    Candidats {tout droit (poids STRAIGHT_WEIGHT), droite (1), gauche (1)} filtrés
    aux tuiles valides selon `classify`. Demi-tour seulement si aucune autre issue.
    Renvoie (direction, nouvel_état_rng).
    """
    options = []  # (direction, poids cumulé)
    total = 0
    for turn, wt in ((d, STRAIGHT_WEIGHT), (RIGHT[d], 1), (LEFT[d], 1)):
        nx, ny = tx + DIRS[turn][0], ty + DIRS[turn][1]
        if classify(grid, w, h, nx, ny):
            total += wt
            options.append((turn, total))
    if not options:
        # cul-de-sac : demi-tour si la case arrière est valide, sinon bloqué.
        bx, by = tx + DIRS[BACK[d]][0], ty + DIRS[BACK[d]][1]
        return (BACK[d] if classify(grid, w, h, bx, by) else STUCK, state)
    v, state = rng_next(state)
    r = v % total
    for turn, cum in options:
        if r < cum:
            return (turn, state)
    return (options[-1][0], state)  # garde-fou (inatteignable)


def place(grid, w, h, tx, ty, classify, state):
    """Place un agent sur (tx,ty). Renvoie (x, y, dir, tgtx, tgty, state)."""
    d, state = pick_exit(grid, w, h, tx, ty, DIR_N, classify, state)
    if d == STUCK:                            # tuile isolée : reste sur place
        px, py = lane_point(tx, ty, DIR_N)
        return (float(px), float(py), DIR_N, px, py, state)
    px, py = lane_point(tx, ty, d)
    ntx, nty = tx + DIRS[d][0], ty + DIRS[d][1]
    tgtx, tgty = lane_point(ntx, nty, d)
    return (float(px), float(py), d, tgtx, tgty, state)


def step(grid, w, h, x, y, d, tgtx, tgty, speed, classify, state):
    """Avance vers le point-cible ; au point, choisit la tuile suivante.

    Renvoie (x, y, dir, tgtx, tgty, state).
    """
    dx, dy = tgtx - x, tgty - y
    d2 = dx * dx + dy * dy
    if d2 <= speed * speed:
        x, y = float(tgtx), float(tgty)
        tx, ty = int(x) >> 3, int(y) >> 3
        nd, state = pick_exit(grid, w, h, tx, ty, d, classify, state)
        if nd != STUCK:                       # sinon : reste sur place (isolé)
            d = nd
            ntx, nty = tx + DIRS[d][0], ty + DIRS[d][1]
            tgtx, tgty = lane_point(ntx, nty, d)
    else:
        inv = speed / math.sqrt(d2)
        x += dx * inv
        y += dy * inv
    return (x, y, d, tgtx, tgty, state)
