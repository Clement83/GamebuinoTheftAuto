"""Tests de la couche IA pure (errance sur grille, voies, choix d'intersection)."""
from tools.ai import (
    DIRS, RIGHT, LEFT, BACK, LANE, STUCK,
    is_drivable, is_walkable, is_open, has_exit, lane_point, tile_center,
    rng_next, pick_exit, place, step, panic_dir, panic_step,
    DIR_N, DIR_E, DIR_S, DIR_W,
)

# Petite ville test 5x5 : croix de routes centrale + un trottoir.
#  . = grass(0)  H = road_h(1)  V = road_v(2)  X = cross(3)  P = pavement(4)  # = building(6)
ROAD_H, ROAD_V, ROAD_CROSS, PAVE, BLD = 1, 2, 3, 4, 6
W = H = 5


def _grid(rows):
    sym = {".": 0, "H": ROAD_H, "V": ROAD_V, "X": ROAD_CROSS, "P": PAVE, "#": BLD}
    g = []
    for r in rows:
        g.extend(sym[c] for c in r)
    return g


# croix : ligne 2 horizontale, colonne 2 verticale, centre = cross
CITY = _grid([
    "..V..",
    "..V..",
    "HHXHH",
    "..V..",
    "..V..",
])


def test_direction_tables_are_consistent():
    # RIGHT = rotation 90 horaire (ecran y bas) ; LEFT = inverse ; BACK = oppose
    for d in range(4):
        assert RIGHT[LEFT[d]] == d
        assert BACK[BACK[d]] == d
        rx, ry = DIRS[RIGHT[d]]
        dx, dy = DIRS[d]
        # produit vectoriel dir x right > 0 (right est bien a droite, y vers le bas)
        assert dx * ry - dy * rx == 1


def test_is_drivable_walkable():
    assert is_drivable(CITY, W, H, 2, 2) is True   # cross
    assert is_drivable(CITY, W, H, 0, 2) is True    # road_h
    assert is_drivable(CITY, W, H, 0, 0) is False   # grass
    assert is_drivable(CITY, W, H, -1, 2) is False  # hors-bornes
    # trottoir + herbe marchables, route/immeuble non
    mixed = _grid(["PP#..", "HHV..", ".....", ".....", "....."])
    assert is_walkable(mixed, W, H, 0, 0) is True    # pavement
    assert is_walkable(mixed, W, H, 3, 0) is True    # grass
    assert is_walkable(mixed, W, H, 2, 0) is False   # building
    assert is_walkable(mixed, W, H, 0, 1) is False   # route


def test_lane_point_offsets_to_the_right():
    # Tuile (2,2), centre px (20,20). LANE de decalage a droite selon le sens.
    cx, cy = 2 * 8 + 4, 2 * 8 + 4
    assert lane_point(2, 2, DIR_E) == (cx, cy + LANE)   # est -> moitie sud
    assert lane_point(2, 2, DIR_W) == (cx, cy - LANE)   # ouest -> moitie nord
    assert lane_point(2, 2, DIR_S) == (cx - LANE, cy)   # sud -> moitie ouest
    assert lane_point(2, 2, DIR_N) == (cx + LANE, cy)   # nord -> moitie est


def test_rng_is_deterministic_and_nonzero_cycle():
    s = 12345
    vals = []
    for _ in range(5):
        v, s = rng_next(s)
        vals.append(v)
    # reproductible
    s2 = 12345
    for v in vals:
        v2, s2 = rng_next(s2)
        assert v2 == v
    # pas coince a zero
    assert len(set(vals)) > 1


def test_pick_exit_never_returns_invalid_tile():
    # depuis le centre de la croix, dans toutes les directions, sur 200 tirages
    s = 1
    for d in range(4):
        for _ in range(50):
            nd, s = pick_exit(CITY, W, H, 2, 2, d, is_drivable, s)
            nx, ny = 2 + DIRS[nd][0], 2 + DIRS[nd][1]
            assert is_drivable(CITY, W, H, nx, ny), f"d={d} -> nd={nd} invalide"


def test_pick_exit_prefers_straight():
    # sur la croix, venant de l'ouest (dir E), tout droit (E) doit dominer
    s = 7
    counts = {0: 0, 1: 0, 2: 0, 3: 0}
    for _ in range(400):
        nd, s = pick_exit(CITY, W, H, 2, 2, DIR_E, is_drivable, s)
        counts[nd] += 1
    assert counts[DIR_E] > counts[DIR_N] + counts[DIR_S]  # poids 4 vs 1+1
    assert counts[DIR_W] == 0                              # jamais demi-tour ici


def test_pick_exit_isolated_tile_is_stuck():
    # tuile route isolee : aucun voisin valide -> STUCK (pas de demi-tour fantome)
    iso = _grid(["....." , ".....", "..H..", ".....", "....."])
    # (2,2) road_h entouree de grass ; venant de l'est, back=(3,2) grass invalide
    nd, _ = pick_exit(iso, W, H, 2, 2, DIR_W, is_drivable, 1)
    assert nd == STUCK
    assert has_exit(iso, W, H, 2, 2, is_drivable) is False


def test_place_and_step_dont_leave_walkable_on_real_shapes():
    # herbe = marchable : un pieton sur herbe erre sans sortir des cases marchables
    g = _grid(["PP...", ".....", "..#..", ".....", "...PP"])
    s = 5
    x, y, d, tx, ty, s = place(g, W, H, 0, 0, is_walkable, s)
    for _ in range(300):
        x, y, d, tx, ty, s = step(g, W, H, x, y, d, tx, ty, 0.35, is_walkable, s)
        assert is_walkable(g, W, H, int(x) >> 3, int(y) >> 3)


def _center_tile_drivable(g, w, h, x, y):
    return is_drivable(g, w, h, int(x) >> 3, int(y) >> 3)


def test_agent_stays_on_road_over_many_steps():
    # Place une voiture sur la croix et la fait errer 500 pas : jamais hors route.
    s = 42
    x, y, d, tx, ty, s = place(CITY, W, H, 2, 2, is_drivable, s)
    for _ in range(500):
        x, y, d, tx, ty, s = step(CITY, W, H, x, y, d, tx, ty, 0.8, is_drivable, s)
        assert _center_tile_drivable(CITY, W, H, x, y), f"hors route en ({x:.1f},{y:.1f})"


def test_agent_actually_moves():
    s = 9
    x0, y0, d, tx, ty, s = place(CITY, W, H, 2, 2, is_drivable, s)
    x, y = x0, y0
    moved = 0.0
    for _ in range(60):
        nx, ny, d, tx, ty, s = step(CITY, W, H, x, y, d, tx, ty, 0.8, is_drivable, s)
        moved += abs(nx - x) + abs(ny - y)
        x, y = nx, ny
    assert moved > 10.0   # a parcouru du chemin


def test_is_open_includes_roads_and_pavement_not_buildings():
    # panique : herbe, trottoir ET route franchissables ; bâtiment non.
    mixed = _grid(["PH#..", ".....", ".....", ".....", "....."])
    assert is_open(mixed, W, H, 0, 0) is True    # pavement
    assert is_open(mixed, W, H, 1, 0) is True     # route (franchie en panique)
    assert is_open(mixed, W, H, 2, 0) is False    # bâtiment
    assert is_open(mixed, W, H, -1, 0) is False   # hors-bornes


def test_panic_dir_flees_away_from_threat():
    # plein ouvert : menace à l'OUEST (away pointe vers l'est) -> fuir vers l'est.
    g = _grid([".....", ".....", ".....", ".....", "....."])
    assert panic_dir(g, W, H, 2, 2, away_dx=1, away_dy=0) == DIR_E
    assert panic_dir(g, W, H, 2, 2, away_dx=-1, away_dy=0) == DIR_W
    assert panic_dir(g, W, H, 2, 2, away_dx=0, away_dy=1) == DIR_S
    assert panic_dir(g, W, H, 2, 2, away_dx=0, away_dy=-1) == DIR_N


def test_panic_dir_routes_around_building():
    # fuite vers l'est bloquée par un bâtiment -> contourne par une case ouverte.
    g = _grid([".....", ".....", "..#..", ".....", "....."])
    # depuis (1,2) menace à l'ouest (away est) mais (2,2) est un bâtiment :
    nd = panic_dir(g, W, H, 1, 2, away_dx=1, away_dy=0)
    assert nd != DIR_E
    nx, ny = 1 + DIRS[nd][0], 2 + DIRS[nd][1]
    assert is_open(g, W, H, nx, ny)


def test_panic_dir_cornered_returns_stuck():
    g = _grid([".#...", "#H#..", ".#...", ".....", "....."])
    # (1,1) route cernée de bâtiments des 4 côtés -> aucune fuite
    assert panic_dir(g, W, H, 1, 1, away_dx=1, away_dy=0) == STUCK


def test_panic_step_increases_distance_and_stays_open():
    g = _grid([".....", ".....", ".....", ".....", "....."])
    sx, sy = tile_center(0, 0)            # menace en haut-gauche
    x, y = float(tile_center(2, 2)[0]), float(tile_center(2, 2)[1])
    d = DIR_E
    tx, ty = tile_center(3, 2)
    d0 = (x - sx) ** 2 + (y - sy) ** 2
    for _ in range(40):
        x, y, d, tx, ty = panic_step(g, W, H, x, y, d, tx, ty, 0.5, sx, sy)
        assert is_open(g, W, H, int(x) >> 3, int(y) >> 3)
    assert (x - sx) ** 2 + (y - sy) ** 2 > d0   # s'est éloigné de la menace


def test_pick_exit_dead_end_does_uturn():
    # cul-de-sac : stub horizontal (1,2)-(2,2) ; on roule vers l'est et on bute.
    dead = _grid([".....", ".....", ".HH..", ".....", "....."])
    s = 3
    nd, s = pick_exit(dead, W, H, 2, 2, DIR_E, is_drivable, s)
    assert nd == BACK[DIR_E]  # seule sortie valide : demi-tour vers l'ouest
    nx, ny = 2 + DIRS[nd][0], 2 + DIRS[nd][1]
    assert is_drivable(dead, W, H, nx, ny)
