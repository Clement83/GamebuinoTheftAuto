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


def _sea_adjacent_districts(sea, district_id, w, h):
    """Ensemble des districts dont une cellule de terre touche la mer cotiere."""
    adj = set()
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if sea[i]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and sea[ny * w + nx]:
                    adj.add(district_id[i])
                    break
    return adj


def assign_themes(seed, zones, district_id, seed_type, w, h, available, sea=None):
    """Affecte chaque theme disponible a UN district distinct -> (theme[w*h], assign).

    `available` : ensemble des theme_id activables (tuiles presentes). Chaque
    theme prend une cellule-district selon sa zone privilegiee ; le port exige un
    district borde par la mer cotiere (`sea`) -- repli : n'importe quelle eau
    adjacente, puis n'importe quel district libre. Le theme ne marque que les
    cellules terrestres (ni eau ni parc)."""
    theme = [THEME_NONE] * (w * h)
    rng = random.Random(seed + 8)
    by_zone = {
        Z_DOWNTOWN: [d for d, t in enumerate(seed_type) if t == Z_DOWNTOWN],
        Z_RESIDENTIAL: [d for d, t in enumerate(seed_type) if t == Z_RESIDENTIAL],
    }
    water_adj = _water_adjacent_districts(zones, district_id, w, h)
    # le port vise la mer cotiere ; sans masque sea on retombe sur l'eau quelconque
    port_adj = _sea_adjacent_districts(sea, district_id, w, h) if sea else set()
    port_adj = port_adj or water_adj
    used, assign = set(), {}
    for tid in THEME_ORDER:
        if tid not in available:
            continue
        d = THEME_DEFS[tid]
        pool = [k for k in by_zone[d["prefer"]] if k not in used]
        if d["water"]:
            pool = [k for k in pool if k in port_adj] or \
                   [k for k in range(len(seed_type)) if k not in used and k in port_adj]
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

# --- noms d'affichage (HUD) des POI ------------------------------------------
# Clefs : theme_id (quartiers) et nom de stamp (batiments-reperes). Servent a la
# fois pour le bandeau "tu es a ..." et pour cibler les missions (recherche par
# nom cote jeu). Courts : <=12 car. pour tenir dans 80 px.
THEME_NAMES = {
    THEME_CHINATOWN: "Chinatown",
    THEME_PORT: "Les Quais",
    THEME_CONSTRUCTION: "Chantier",
    THEME_JUNKYARD: "La Casse",
}
STAMP_NAMES = {
    "police": "Commissariat",
    "hospital": "Hopital",
    "fire": "Pompiers",
}


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


def place_docks(grid, sea, district_id, port_d, tile_index, w, h,
                jetties=3, max_len=4, gap=4):
    """Pose 2-3 jetees (tuile 'dock') le long de la facade maritime du port.

    Une jetee part d'une cellule de mer au bord de la terre du district port et
    avance perpendiculairement vers le large, sur au plus `max_len` cellules,
    en laissant toujours >= 1 cellule de mer au-dela (jamais "jusqu'au bout").
    Les amorces sont espacees d'au moins `gap`. Deterministe (aucun rng).
    Marchable (dock non solide). Retourne le nombre de tuiles de quai posees."""
    dock = tile_index["dock"]

    # amorces : cellule de mer dont le voisin "arriere" est la terre du port ;
    # (dx,dy) pointe alors vers le large.
    starts = []
    for y in range(h):
        for x in range(w):
            if not sea[y * w + x]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                bx, by = x - dx, y - dy        # cellule "derriere" (vers la terre)
                if 0 <= bx < w and 0 <= by < h:
                    b = by * w + bx
                    if not sea[b] and district_id[b] == port_d:
                        starts.append((x, y, dx, dy))
                        break

    # espacement : balayage deterministe, on garde une amorce loin des precedentes
    picks = []
    for s in starts:
        if all((s[0] - p[0]) ** 2 + (s[1] - p[1]) ** 2 >= gap * gap for p in picks):
            picks.append(s)
        if len(picks) >= jetties:
            break

    n = 0
    for x, y, dx, dy in picks:
        # longueur de mer disponible droit devant (avant la terre ou le bord)
        run = 0
        cx, cy = x, y
        while 0 <= cx < w and 0 <= cy < h and sea[cy * w + cx]:
            run += 1
            cx += dx
            cy += dy
        length = min(max_len, run - 1)        # laisse >= 1 cellule de mer au large
        cx, cy = x, y
        for _ in range(length):
            grid[cy * w + cx] = dock
            n += 1
            cx += dx
            cy += dy
    return n


# --- collecte des POI pour l'export jeu --------------------------------------

def _nearest_walkable(grid, cells, cx, cy, solid_index, prefer=None, w=0):
    """Parmi `cells` (indices grille), la cellule NON solide la plus proche de
    (cx,cy). `prefer` (set d'ids de tuiles) gagne a egalite de priorite. Renvoie
    (tx, ty) ou None. Sert de point-cible atteignable d'un POI (route/quai...)."""
    best = None
    best_key = None
    for i in cells:
        t = grid[i]
        if t in solid_index:
            continue
        x, y = i % w, i // w
        pr = 0 if (prefer and t in prefer) else 1
        d = (x - cx) ** 2 + (y - cy) ** 2
        key = (pr, d)
        if best_key is None or key < best_key:
            best_key = key
            best = (x, y)
    return best


def collect_pois(grid, theme, assign, placed, sea, tile_index, solid_index, w, h):
    """Construit la liste des POI exportables -> [dict(name,x0,y0,x1,y1,tx,ty)].

    * quartiers (theme) : bbox des cellules du district + point-cible = la
      cellule marchable la plus proche du centroide (le port privilegie un quai);
    * stamps : bbox du blueprint 3x3 + point-cible = la porte (case marchable
      bordant la route). `placed` = {nom: (x,y) coin haut-gauche | None}.
    Tout en coordonnees de TUILES. Ordre stable : quartiers puis stamps."""
    out = []
    road_pav = {tile_index[n] for n in ("road_h", "road_v", "road_cross", "pavement")
                if n in tile_index}
    dock = {tile_index["dock"]} if "dock" in tile_index else set()

    # quartiers thematiques
    by_theme = {}
    for i, t in enumerate(theme or []):
        if t == THEME_NONE:
            continue
        by_theme.setdefault(t, []).append(i)
    for tid in THEME_ORDER:
        cells = by_theme.get(tid)
        if not cells:
            continue
        xs = [c % w for c in cells]
        ys = [c // w for c in cells]
        x0, y0, x1, y1 = min(xs), min(ys), max(xs), max(ys)
        cx, cy = sum(xs) // len(xs), sum(ys) // len(ys)
        prefer = (dock or road_pav) if tid == THEME_PORT else road_pav
        tgt = _nearest_walkable(grid, cells, cx, cy, solid_index, prefer, w) \
            or (cx, cy)
        out.append(dict(name=THEME_NAMES[tid], x0=x0, y0=y0, x1=x1, y1=y1,
                        tx=tgt[0], ty=tgt[1]))

    # stamps : la porte est en bas-centre du blueprint 3x3
    drow, dcol = _door_cell(STAMP_BLUEPRINT)
    sh, sw = len(STAMP_BLUEPRINT), len(STAMP_BLUEPRINT[0])
    for name in STAMP_ORDER:
        xy = (placed or {}).get(name)
        if not xy:
            continue
        x, y = xy
        out.append(dict(name=STAMP_NAMES[name], x0=x, y0=y,
                        x1=x + sw - 1, y1=y + sh - 1,
                        tx=x + dcol, ty=y + drow + 1))   # case route devant la porte
    return out
