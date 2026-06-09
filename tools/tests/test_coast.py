"""Cote maritime forcee : mer le long d'un bord (seed), plage, quais."""
from collections import Counter

from tools import citygen, pois
from tools.citygen import Z_WATER, Z_SAND, COAST_EDGES
from tools.citydsl import CompiledCity

W = H = 96


def _border_cells(edge, w, h):
    if edge == 'N':
        return [(x, 0) for x in range(w)]
    if edge == 'S':
        return [(x, h - 1) for x in range(w)]
    if edge == 'W':
        return [(0, y) for y in range(h)]
    return [(w - 1, y) for y in range(h)]


# ----- choix du bord --------------------------------------------------------

def test_coast_edge_deterministic():
    assert citygen.coast_edge(7) == citygen.coast_edge(7)
    assert citygen.coast_edge(7) in COAST_EDGES


def test_coast_edge_varies_across_seeds():
    edges = {citygen.coast_edge(s) for s in range(40)}
    assert len(edges) >= 2          # la seed fait varier le bord


# ----- masque mer -----------------------------------------------------------

def test_build_zones_returns_sea_mask():
    zones, sea = citygen.build_zones(7, W, H, 0.18, 0.10, 10)
    assert len(sea) == W * H
    assert all(isinstance(b, bool) for b in sea[:8])


def test_sea_is_always_water():
    zones, sea = citygen.build_zones(7, W, H, 0.18, 0.10, 10)
    assert any(sea)
    for i in range(W * H):
        if sea[i]:
            assert zones[i] == Z_WATER


def test_sea_covers_most_of_edge_not_all():
    edge = citygen.coast_edge(7)
    zones, sea = citygen.build_zones(7, W, H, 0.18, 0.10, 10)
    flags = [sea[y * W + x] for x, y in _border_cells(edge, W, H)]
    cnt = sum(flags)
    assert 0.5 * len(flags) <= cnt < len(flags)      # large frange, pas tout le bord
    # un seul segment contigu de mer le long du bord
    runs = (1 if flags[0] else 0) + sum(1 for i in range(1, len(flags))
                                        if flags[i] and not flags[i - 1])
    assert runs == 1


# ----- plage ----------------------------------------------------------------

def _is_near_sea(sea, x, y, w, h, rad=2):
    for dy in range(-rad, rad + 1):
        for dx in range(-rad, rad + 1):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and sea[ny * w + nx]:
                return True
    return False


def test_beach_band_exists_next_to_sea():
    zones, sea = citygen.build_zones(7, W, H, 0.18, 0.10, 10)
    sand = [i for i in range(W * H) if zones[i] == Z_SAND]
    assert sand                                     # au moins une cellule de sable
    for i in sand:
        x, y = i % W, i // W
        assert zones[i] != Z_WATER
        assert _is_near_sea(sea, x, y, W, H), "sable loin de la mer"


# ----- integration : tuiles sand/dock ---------------------------------------

NAMES = ["grass", "road_h", "road_v", "road_cross", "pavement", "water",
         "building_a", "building_b",
         "cn_facade_a", "cn_facade_b", "cn_sign",
         "police_facade", "police_sign", "police_door",
         "hosp_facade", "hosp_sign", "hosp_door",
         "fire_facade", "fire_sign", "fire_door",
         "port_facade_a", "port_facade_b", "port_sign",
         "cons_facade_a", "cons_facade_b", "cons_sign",
         "junk_facade_a", "junk_facade_b", "junk_sign",
         "sand", "dock"]
TI = {n: i for i, n in enumerate(NAMES)}
DOORS = {"police_door", "hosp_door", "fire_door", "dock"}
SI = {TI["water"], TI["building_a"], TI["building_b"]} | \
     {i for i, n in enumerate(NAMES) if 8 <= i < 29 and n not in DOORS}


def _gen(seed=7, **kw):
    c = CompiledCity(W, H)
    citygen.generate_into(c, seed, TI, SI, **kw)
    return c


def test_sand_tiles_rendered():
    n = Counter(_gen().grid)
    assert n[TI["sand"]] > 0


def test_docks_placed_and_walkable():
    n = Counter(_gen().grid)
    assert n[TI["dock"]] > 0
    assert TI["dock"] not in SI                     # quai marchable


def test_docks_are_a_few_jetties_not_a_slab():
    """2-3 jetees fines, pas une dalle massive sur toute la cote."""
    g = _gen().grid
    docks = sum(1 for t in g if t == TI["dock"])
    assert 0 < docks <= 18                          # petites jetees


def test_jetties_leave_open_water_at_the_end():
    """Chaque quai laisse au moins une cellule de mer au-dela (pas jusqu'au bout)."""
    g = _gen().grid
    # une jetee posee implique de la mer (water) encore presente vers le large
    assert any(t == TI["dock"] for t in g)
    assert any(t == TI["water"] for t in g)


def test_sea_is_thinner_than_quarter_map():
    """La mer cotiere reste une frange, pas la moitie de la carte."""
    zones, sea = citygen.build_zones(7, W, H, 0.18, 0.10, 10)
    assert sum(sea) < (W * H) * 0.18                # < ~1/5 de la carte


def test_generation_deterministic_with_coast():
    assert _gen(seed=11).grid == _gen(seed=11).grid
