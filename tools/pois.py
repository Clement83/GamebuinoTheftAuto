"""Placement deterministe des points d'interet (POI) sur la grille de ville.

Pur (aucune I/O). S'appuie sur le decoupage en districts de citygen. Deux
mecanismes data-driven :

  * quartiers thematiques (THEME_DEFS) -> assign_themes / resolve_palettes :
    chaque theme s'empare d'UN district (contrainte de zone, voire d'adjacence a
    l'eau pour le port) et y re-tuile les blocs (palette facade A/B + enseigne) ;
  * stamps / batiments-reperes (STAMP_DEFS) -> place_stamps : tamponne un petit
    blueprint fixe sur une parcelle libre accolee a une route (porte orientee).

Chaque mecanisme s'active independamment selon que ses tuiles existent dans le
tileset. Tout derive de la graine -> reproductible.
"""
import random

from tools.citygen import THEME_NONE, Z_DOWNTOWN, Z_RESIDENTIAL, Z_WATER, Z_PARK

# --- quartiers thematiques ---------------------------------------------------

THEME_CHINATOWN = 1
THEME_PORT = 2
THEME_CONSTRUCTION = 3
THEME_JUNKYARD = 4

# a/b = facades (choisies par bruit), sign = enseigne eparse (proba sign_p),
# build_p = densite de bati du quartier, prefer = zone privilegiee,
# water = le district doit toucher l'eau.
THEME_DEFS = {
    THEME_CHINATOWN: dict(a="cn_facade_a", b="cn_facade_b", sign="cn_sign",
                          sign_p=0.12, build_p=0.90, prefer=Z_DOWNTOWN, water=False),
    THEME_PORT: dict(a="port_facade_a", b="port_facade_b", sign="port_sign",
                     sign_p=0.15, build_p=0.70, prefer=Z_RESIDENTIAL, water=True),
    THEME_CONSTRUCTION: dict(a="cons_facade_a", b="cons_facade_b", sign="cons_sign",
                             sign_p=0.22, build_p=0.45, prefer=Z_DOWNTOWN, water=False),
    THEME_JUNKYARD: dict(a="junk_facade_a", b="junk_facade_b", sign="junk_sign",
                         sign_p=0.15, build_p=0.60, prefer=Z_RESIDENTIAL, water=False),
}
# Ordre d'affectation : le plus contraint d'abord (port = adjacence eau).
THEME_ORDER = (THEME_PORT, THEME_CHINATOWN, THEME_CONSTRUCTION, THEME_JUNKYARD)


def resolve_palettes(tile_index):
    """Palettes resolues (index de tuiles) des themes dont les 3 tuiles existent."""
    pals = {}
    for tid, d in THEME_DEFS.items():
        if all(d[k] in tile_index for k in ("a", "b", "sign")):
            pals[tid] = dict(a=tile_index[d["a"]], b=tile_index[d["b"]],
                             sign=tile_index[d["sign"]],
                             sign_p=d["sign_p"], build_p=d["build_p"])
    return pals


def _water_adjacent_districts(zones, district_id, w, h):
    """Ensemble des districts dont au moins une cellule terrestre touche l'eau."""
    adj = set()
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if zones[i] == Z_WATER:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and zones[ny * w + nx] == Z_WATER:
                    adj.add(district_id[i])
                    break
    return adj


def assign_themes(seed, zones, district_id, seed_type, w, h, available):
    """Affecte chaque theme disponible a UN district distinct -> (theme[w*h], assign).

    `available` : ensemble des theme_id activables (tuiles presentes). Chaque
    theme prend une cellule-district selon sa zone privilegiee ; le port exige un
    district touchant l'eau (repli : n'importe quel district eau-adjacent libre).
    Le theme ne marque que les cellules terrestres (ni eau ni parc)."""
    theme = [THEME_NONE] * (w * h)
    rng = random.Random(seed + 8)
    by_zone = {
        Z_DOWNTOWN: [d for d, t in enumerate(seed_type) if t == Z_DOWNTOWN],
        Z_RESIDENTIAL: [d for d, t in enumerate(seed_type) if t == Z_RESIDENTIAL],
    }
    water_adj = _water_adjacent_districts(zones, district_id, w, h)
    used, assign = set(), {}
    for tid in THEME_ORDER:
        if tid not in available:
            continue
        d = THEME_DEFS[tid]
        pool = [k for k in by_zone[d["prefer"]] if k not in used]
        if d["water"]:
            pool = [k for k in pool if k in water_adj] or \
                   [k for k in range(len(seed_type)) if k not in used and k in water_adj]
        if not pool:                       # repli : n'importe quel district libre
            pool = [k for k in range(len(seed_type)) if k not in used]
        if not pool:
            continue
        pick = rng.choice(pool)
        used.add(pick)
        assign[pick] = tid
    for i in range(w * h):
        d = district_id[i]
        if d in assign and zones[i] not in (Z_WATER, Z_PARK):
            theme[i] = assign[d]
    return theme, assign


# --- stamps / batiments-reperes ----------------------------------------------

# Blueprint 3x3 commun. F=facade, S=enseigne (haut-centre),
# D=porte non-solide (bas-centre -> entree orientee vers la route au sud).
STAMP_BLUEPRINT = (
    "FSF",
    "FFF",
    "FDF",
)
STAMP_DEFS = {
    "police": dict(tiles={"F": "police_facade", "S": "police_sign", "D": "police_door"},
                   prefer=Z_DOWNTOWN),
    "hospital": dict(tiles={"F": "hosp_facade", "S": "hosp_sign", "D": "hosp_door"},
                     prefer=None),
    "fire": dict(tiles={"F": "fire_facade", "S": "fire_sign", "D": "fire_door"},
                 prefer=None),
}
STAMP_ORDER = ("police", "hospital", "fire")


def has_any_stamp(tile_index):
    """Vrai si au moins un stamp a toutes ses tuiles dans le tileset."""
    return any(all(t in tile_index for t in s["tiles"].values())
               for s in STAMP_DEFS.values())


def _door_cell(blueprint):
    for r, row in enumerate(blueprint):
        c = row.find("D")
        if c >= 0:
            return r, c
    raise ValueError("blueprint sans porte 'D'")


def place_stamps(grid, zones, district_id, seed_type, seed, w, h, tile_index,
                 margin=3):
    """Tamponne tous les stamps disponibles -> {nom: (x,y) | None}.

    Un emplacement valide : parcelle de la taille du blueprint sans route/eau ni
    chevauchement d'un stamp deja pose, avec une route juste au sud de la porte
    (acces + orientation). Priorite a la zone privilegiee, puis ordre derive de
    la graine. Aucune parcelle valide -> None pour ce stamp (skip propre)."""
    blueprint = STAMP_BLUEPRINT
    sh, sw = len(blueprint), len(blueprint[0])
    drow, dcol = _door_cell(blueprint)
    road_ids = {tile_index["road_h"], tile_index["road_v"], tile_index["road_cross"]}
    blocked = road_ids | {tile_index["water"]}
    occupied, placed = set(), {}

    for k, name in enumerate(STAMP_ORDER):
        sdef = STAMP_DEFS[name]
        if not all(t in tile_index for t in sdef["tiles"].values()):
            continue
        cands = []
        for y in range(margin, h - margin - sh):
            for x in range(margin, w - margin - sw):
                cells = [(y + ry) * w + (x + rx)
                         for ry in range(sh) for rx in range(sw)]
                if any(grid[c] in blocked or c in occupied for c in cells):
                    continue
                if y + drow + 1 >= h or \
                        grid[(y + drow + 1) * w + (x + dcol)] not in road_ids:
                    continue
                cx, cy = x + sw // 2, y + sh // 2
                pref = sdef["prefer"]
                prio = 0 if (pref is None or seed_type[district_id[cy * w + cx]] == pref) else 1
                cands.append((prio, x, y, cells))
        if not cands:
            placed[name] = None
            continue
        rng = random.Random(seed + 20 + k)
        rng.shuffle(cands)
        cands.sort(key=lambda c: c[0])         # zone privilegiee d'abord (tri stable)
        _, x, y, cells = cands[0]
        for ci, c in enumerate(cells):
            ry, rx = divmod(ci, sw)
            grid[c] = tile_index[sdef["tiles"][blueprint[ry][rx]]]
            occupied.add(c)
        placed[name] = (x, y)
    return placed
