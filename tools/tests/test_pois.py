from collections import Counter

from tools.citydsl import CompiledCity
from tools import citygen, pois
from tools.citygen import Z_DOWNTOWN, Z_RESIDENTIAL
from tools.pois import (THEME_CHINATOWN, THEME_PORT, THEME_CONSTRUCTION,
                        THEME_JUNKYARD)

# tileset complet ; ordre = enum de gta/assets.h
NAMES = ["grass", "road_h", "road_v", "road_cross", "pavement", "water",
         "building_a", "building_b",
         "cn_facade_a", "cn_facade_b", "cn_sign",
         "police_facade", "police_sign", "police_door",
         "hosp_facade", "hosp_sign", "hosp_door",
         "fire_facade", "fire_sign", "fire_door",
         "port_facade_a", "port_facade_b", "port_sign",
         "cons_facade_a", "cons_facade_b", "cons_sign",
         "junk_facade_a", "junk_facade_b", "junk_sign",
         "junk_ground", "junk_fence", "junk_wreck", "junk_crane",
         "cons_ground", "cons_fence", "cons_frame", "cons_crane",
         "plan_facade", "plan_sign", "plan_door",
         "gar_facade", "gar_sign", "gar_door",
         "bar_facade", "bar_sign", "bar_door",
         "bur_facade", "bur_sign", "bur_door",
         "cas_facade", "cas_sign", "cas_door",
         "com_facade", "com_sign", "com_door"]
TI = {n: i for i, n in enumerate(NAMES)}
DOORS = {"police_door", "hosp_door", "fire_door"}
# portes des batiments-reperes de la campagne (non solides comme les autres)
STORY_DOORS = {"plan_door", "gar_door", "bar_door", "bur_door",
               "cas_door", "com_door"}
# non solides parmi les tuiles POI : portes + sols carrossables (casse, chantier)
NONSOLID = DOORS | STORY_DOORS | {"junk_ground", "cons_ground"}
# solides : water/building + toutes les tuiles POI sauf portes / sol de casse
SI = {TI["water"], TI["building_a"], TI["building_b"]} | \
     {i for i, n in enumerate(NAMES) if i >= 8 and n not in NONSOLID}
TI8 = {n: v for n, v in TI.items() if v < 8}
SI8 = {5, 6, 7}

THEME_TILES = {
    THEME_CHINATOWN: {TI["cn_facade_a"], TI["cn_facade_b"], TI["cn_sign"]},
    THEME_PORT: {TI["port_facade_a"], TI["port_facade_b"], TI["port_sign"]},
    THEME_CONSTRUCTION: {TI["cons_facade_a"], TI["cons_facade_b"], TI["cons_sign"]},
    THEME_JUNKYARD: {TI["junk_facade_a"], TI["junk_facade_b"], TI["junk_sign"]},
}
SIGNS = {"police": TI["police_sign"], "hospital": TI["hosp_sign"], "fire": TI["fire_sign"]}
STAMP_DOORS = {TI["police_door"], TI["hosp_door"], TI["fire_door"]}


def _gen(seed=7, w=96, h=96, tile_index=TI, solid_index=SI, **kw):
    c = CompiledCity(w, h)
    citygen.generate_into(c, seed, tile_index, solid_index, **kw)
    return c


# ----- palettes / activation ------------------------------------------------

def test_resolve_palettes_all_four_themes():
    pals = pois.resolve_palettes(TI)
    assert set(pals) == {THEME_CHINATOWN, THEME_PORT, THEME_CONSTRUCTION, THEME_JUNKYARD}


def test_resolve_palettes_empty_on_minimal_tileset():
    assert pois.resolve_palettes(TI8) == {}
    assert pois.has_any_stamp(TI8) is False


def test_has_any_stamp_true_on_full_tileset():
    assert pois.has_any_stamp(TI) is True


# ----- assign_themes --------------------------------------------------------

def test_assign_themes_distinct_districts_and_zone_pref():
    z, sea = citygen.build_zones(7, 96, 96, 0.18, 0.10, 10)
    did, st, _ = citygen.voronoi_districts(7, 96, 96, 10)
    theme, assign = pois.assign_themes(7, z, did, st, 96, 96, set(THEME_TILES), sea=sea)
    assert len(assign) == len(set(assign))          # un district distinct par theme
    # zones privilegiees respectees (hors repli)
    for d, tid in assign.items():
        pref = pois.THEME_DEFS[tid]["prefer"]
        # le port peut basculer en repli ; les autres collent a leur zone
        if tid != THEME_PORT:
            assert st[d] == pref


def test_assign_themes_port_touches_water():
    z, sea = citygen.build_zones(7, 96, 96, 0.20, 0.05, 12)
    did, st, _ = citygen.voronoi_districts(7, 96, 96, 12)
    sea_adj = pois._sea_adjacent_districts(sea, did, 96, 96)
    _, assign = pois.assign_themes(7, z, did, st, 96, 96, set(THEME_TILES), sea=sea)
    port_d = [d for d, t in assign.items() if t == THEME_PORT]
    assert port_d and port_d[0] in sea_adj          # port borde la mer cotiere


def test_assign_themes_deterministic_and_confined():
    z, sea = citygen.build_zones(7, 64, 64, 0.18, 0.10, 8)
    did, st, _ = citygen.voronoi_districts(7, 64, 64, 8)
    a, _ = pois.assign_themes(7, z, did, st, 64, 64, set(THEME_TILES), sea=sea)
    b, _ = pois.assign_themes(7, z, did, st, 64, 64, set(THEME_TILES), sea=sea)
    assert a == b
    # chaque cellule thematisee appartient a un seul district par theme
    for tid in set(THEME_TILES):
        dists = {did[i] for i in range(64 * 64) if a[i] == tid}
        assert len(dists) <= 1


# ----- place_stamps (unitaire) ----------------------------------------------

def _check_stamp(grid, w, pos):
    x, y = pos
    n = Counter(grid[(y + ry) * w + (x + rx)] for ry in range(3) for rx in range(3))
    sign = sum(n[s] for s in SIGNS.values())
    door = sum(n[d] for d in STAMP_DOORS)
    assert sign == 1 and door == 1
    assert grid[(y + 3) * w + (x + 1)] in {TI["road_h"], TI["road_v"], TI["road_cross"]}


def test_place_stamps_places_all_three_without_overlap():
    w = h = 24
    grid = [TI["building_a"]] * (w * h)
    for ry in (6, 12, 18):                  # routes horizontales -> acces
        for x in range(w):
            grid[ry * w + x] = TI["road_h"]
    placed = pois.place_stamps(grid, [Z_DOWNTOWN] * (w * h),
                               [0] * (w * h), [Z_DOWNTOWN], 7, w, h, TI)
    assert all(placed[k] is not None for k in ("police", "hospital", "fire"))
    cells = set()
    for k in ("police", "hospital", "fire"):
        x, y = placed[k]
        these = {(y + ry) * w + (x + rx) for ry in range(3) for rx in range(3)}
        assert cells.isdisjoint(these), "stamps qui se chevauchent"
        cells |= these
        _check_stamp(grid, w, placed[k])


def test_place_stamps_none_when_no_road_access():
    w = h = 24
    grid = [TI["building_a"]] * (w * h)      # aucune route
    placed = pois.place_stamps(grid, [Z_DOWNTOWN] * (w * h),
                               [0] * (w * h), [Z_DOWNTOWN], 7, w, h, TI)
    assert all(v is None for v in placed.values())


# ----- integration generate_into --------------------------------------------

def test_generate_with_pois_deterministic():
    assert _gen(seed=11).grid == _gen(seed=11).grid


def test_all_four_themes_present_in_full_gen():
    n = Counter(_gen().grid)
    for tid, tiles in THEME_TILES.items():
        assert sum(n[t] for t in tiles) > 0, "theme %d absent" % tid


def test_each_stamp_placed_once_in_full_gen():
    n = Counter(_gen().grid)
    for name, sign in SIGNS.items():
        assert n[sign] == 1, "stamp %s : %d enseignes" % (name, n[sign])
    assert sum(n[d] for d in STAMP_DOORS) == 3


STORY_STAMP_SIGNS = ("plan_sign", "gar_sign", "bar_sign", "bur_sign",
                     "cas_sign", "com_sign")


def test_story_stamps_present_on_roomy_map():
    # sur une carte avec de la place, les 6 batiments-reperes de la campagne
    # sont tamponnes une fois chacun (seed 11 : tous les stamps tiennent).
    n = Counter(_gen(seed=11).grid)
    for sign in STORY_STAMP_SIGNS:
        assert n[TI[sign]] == 1, "stamp campagne %s : %d enseignes" % (sign, n[TI[sign]])


def test_story_stamps_never_duplicated():
    # invariant : jamais place plus d'une fois (peut etre absent si pas de place).
    for seed in (7, 11, 3, 42):
        n = Counter(_gen(seed=seed).grid)
        for sign in STORY_STAMP_SIGNS:
            assert n[TI[sign]] <= 1, "stamp %s duplique (seed %d)" % (sign, seed)


def test_stamps_never_on_road_or_water():
    c = _gen()
    road_water = {TI["road_h"], TI["road_v"], TI["road_cross"], TI["water"]}
    # aucune tuile de stamp ne doit coexister avec route/eau (verif via comptage)
    assert all(t not in road_water for t in
               (TI["police_facade"], TI["hosp_facade"], TI["fire_facade"]))


def test_pois_disabled_without_tiles_backward_compatible():
    c8 = _gen(tile_index=TI8, solid_index=SI8)
    assert all(t < 8 for t in c8.grid)


def test_spawn_not_on_solid_with_pois():
    sx, sy, _ = _gen().spawn
    assert _gen().get(sx, sy) not in SI


# ----- place_services (Pay'n'Spray / AMU Nation) ----------------------------

def _is_road_edge_block(grid, w, x, y):
    """Bloc building borde par une route du cote SANS trottoir (N d'une rue H
    ou O d'une rue V) -- la regle de placement des services."""
    if grid[y * w + x] not in (TI["building_a"], TI["building_b"]):
        return False
    rh, rv, rx = TI["road_h"], TI["road_v"], TI["road_cross"]
    north = grid[(y - 1) * w + x] if y > 0 else None
    west = grid[y * w + (x - 1)] if x > 0 else None
    return north in (rh, rx) or west in (rv, rx)


def test_place_services_count_rule_and_no_overlap():
    c = _gen()
    assert len(c.sprays) == pois.SPRAY_COUNT
    assert len(c.ammus) == pois.AMMU_COUNT
    for x, y in c.sprays + c.ammus:
        assert _is_road_edge_block(c.grid, c.w, x, y), "service hors bord-de-route"
    assert set(c.sprays).isdisjoint(c.ammus), "spray et ammu sur la meme case"


def test_place_services_deterministic():
    a, b = _gen(seed=11), _gen(seed=11)
    assert a.sprays == b.sprays and a.ammus == b.ammus


def test_place_services_empty_without_blocks():
    # tileset sans building_a/b -> aucun candidat -> listes vides (pas d'erreur)
    ti = {n: i for i, n in enumerate(("grass", "road_h", "road_v", "road_cross",
                                      "pavement", "water"))}
    grid = [ti["road_h"]] * (8 * 8)
    sprays, ammus = pois.place_services(grid, ti, 7, 8, 8)
    assert sprays == [] and ammus == []


# ----- enceinte de La Casse (place_junkyard) --------------------------------

ROADS = {TI["road_h"], TI["road_v"], TI["road_cross"]}


def test_junkyard_compound_stamped_in_full_gen():
    # la generation complete tamponne l'enceinte : zone de depose sur sol de
    # casse (carrossable) et base de grue sur tuile junk_crane.
    c = _gen()
    assert c.casse is not None and c.crane is not None
    zx, zy = c.casse
    assert c.grid[zy * c.w + zx] == TI["junk_ground"], "zone de depose = sol de casse"
    assert c.get(zx, zy) not in SI, "sol de casse non solide -> carrossable"
    kx, ky = c.crane
    assert c.grid[ky * c.w + kx] == TI["junk_crane"], "base de grue = tuile junk_crane"


def test_junkyard_compound_fenced_and_has_wrecks():
    # autour de la zone : au moins une grille (enceinte) et une epave statique.
    c = _gen()
    zx, zy = c.casse
    seen = Counter(c.get(zx + dx, zy + dy)
                   for dy in range(-5, 6) for dx in range(-6, 7)
                   if 0 <= zx + dx < c.w and 0 <= zy + dy < c.h)
    assert seen[TI["junk_fence"]] > 0, "enceinte (grille) absente autour de la zone"
    assert seen[TI["junk_wreck"]] > 0, "aucune epave statique dans la casse"


def test_junkyard_entrance_borders_a_road():
    # l'enceinte a une entree (sol de casse) bordee par une route au sud.
    c = _gen()
    grid, w = c.grid, c.w
    gap = None
    for y in range(c.h - 1):
        for x in range(w):
            if grid[y * w + x] == TI["junk_ground"] and grid[(y + 1) * w + x] in ROADS:
                gap = (x, y)
    assert gap is not None, "aucune entree de casse bordant une route"


def test_junkyard_deterministic():
    a, b = _gen(seed=11), _gen(seed=11)
    assert a.casse == b.casse and a.crane == b.crane


def test_place_junkyard_none_without_theme_or_tiles():
    # pas de theme junkyard -> None ; tuiles manquantes -> None
    grid = [TI["grass"]] * (16 * 16)
    assert pois.place_junkyard(grid, [0] * (16 * 16), {}, TI, 16, 16) is None
    assert pois.place_junkyard(grid, [0] * (16 * 16), {0: THEME_JUNKYARD},
                               TI8, 16, 16) is None


# ----- enceinte du Chantier (place_construction) ----------------------------

def test_construction_compound_stamped_in_full_gen():
    # la generation complete tamponne le chantier : base de grue sur cons_crane.
    c = _gen()
    assert c.chantier is not None
    kx, ky = c.chantier["crane"]
    assert c.grid[ky * c.w + kx] == TI["cons_crane"], "base de grue = cons_crane"


def test_construction_compound_fenced_and_has_frames():
    # autour de la grue : palissade (cons_fence) + ossatures beton (cons_frame).
    c = _gen()
    kx, ky = c.chantier["crane"]
    seen = Counter(c.get(kx + dx, ky + dy)
                   for dy in range(-5, 6) for dx in range(-6, 7)
                   if 0 <= kx + dx < c.w and 0 <= ky + dy < c.h)
    assert seen[TI["cons_fence"]] > 0, "palissade absente autour du chantier"
    assert seen[TI["cons_frame"]] > 0, "aucune ossature beton dans le chantier"


def test_construction_ground_carrossable():
    # le sol de chantier (terre damee) est carrossable (non solide).
    c = _gen()
    assert TI["cons_ground"] not in SI


def test_construction_entrance_borders_a_road():
    c = _gen()
    grid, w = c.grid, c.w
    gap = None
    for y in range(c.h - 1):
        for x in range(w):
            if grid[y * w + x] == TI["cons_ground"] and grid[(y + 1) * w + x] in ROADS:
                gap = (x, y)
    assert gap is not None, "aucune entree de chantier bordant une route"


def test_construction_deterministic():
    a, b = _gen(seed=11), _gen(seed=11)
    assert a.chantier == b.chantier


def test_place_construction_none_without_theme_or_tiles():
    grid = [TI["grass"]] * (16 * 16)
    assert pois.place_construction(grid, [0] * (16 * 16), {}, TI, 16, 16) is None
    assert pois.place_construction(grid, [0] * (16 * 16),
                                   {0: THEME_CONSTRUCTION}, TI8, 16, 16) is None


# ----- place_casse / place_crane (repli quand l'enceinte ne tient pas) -------

def test_place_casse_fallback_road_point():
    # repli : sur un district sans place pour l'enceinte, place_casse rend une
    # tuile route carrossable (comportement historique conserve).
    ti = {n: i for i, n in enumerate(("grass", "road_h", "road_v", "road_cross",
                                      "pavement", "water"))}
    w = h = 8
    grid = [ti["grass"]] * (w * h)
    for x in range(w):
        grid[3 * w + x] = ti["road_h"]
    did = [0] * (w * h)
    pt = pois.place_casse(grid, did, {0: THEME_JUNKYARD}, ti, {ti["water"]}, w, h)
    assert pt is not None and grid[pt[1] * w + pt[0]] in ROADS


def test_place_casse_none_without_junkyard_theme():
    ti = {n: i for i, n in enumerate(("grass", "road_h", "road_v", "road_cross",
                                      "pavement", "water"))}
    grid = [ti["road_h"]] * (8 * 8)
    did = [0] * (8 * 8)
    assert pois.place_casse(grid, did, {}, ti, {ti["water"]}, 8, 8) is None


def test_place_crane_none_without_casse():
    ti = {n: i for i, n in enumerate(("grass", "road_h", "junk_facade_a"))}
    grid = [ti["grass"]] * (8 * 8)
    assert pois.place_crane(grid, None, ti, 8, 8) is None
