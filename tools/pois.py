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
    # --- batiments-reperes de la trame principale (cf. campagne/) ---
    # prefer=None : leur dispersion est geree par STORY_STAMPS (placement le
    # plus eloigne possible des autres reperes) plutot que par une zone, sinon
    # ils s'agglutinent tous dans le meme quartier (downtown / Chinatown).
    "planque": dict(tiles={"F": "plan_facade", "S": "plan_sign", "D": "plan_door"},
                    prefer=None),
    "garage": dict(tiles={"F": "gar_facade", "S": "gar_sign", "D": "gar_door"},
                   prefer=None),
    "bar": dict(tiles={"F": "bar_facade", "S": "bar_sign", "D": "bar_door"},
                prefer=None),
    "bureaux": dict(tiles={"F": "bur_facade", "S": "bur_sign", "D": "bur_door"},
                    prefer=None),
    "casino": dict(tiles={"F": "cas_facade", "S": "cas_sign", "D": "cas_door"},
                   prefer=None),
    "commerces": dict(tiles={"F": "com_facade", "S": "com_sign", "D": "com_door"},
                      prefer=None),
}
STAMP_ORDER = ("police", "hospital", "fire", "planque", "garage", "bar",
               "bureaux", "casino", "commerces")
# Reperes de la trame : disperses (chacun le plus loin possible des deja poses)
# pour couvrir la carte. Les 3 premiers (services) gardent leur placement.
STORY_STAMPS = frozenset(("planque", "garage", "bar", "bureaux", "casino",
                          "commerces"))

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
    "planque": "Planque",
    "garage": "Le Garage",
    "bar": "Le Bar",
    "bureaux": "Les Bureaux",
    "casino": "Le Casino",
    "commerces": "Commerces",
}


# --- noms de quartiers GTA pour les districts SANS theme ---------------------
# Pioche deterministe sans doublon sur une carte. <=12 car. (tient dans le HUD
# 80 px). Inspires des quartiers de GTA III / Vice City / San Andreas.
DISTRICT_NAMES = (
    "Portland", "Staunton", "Shoreside", "Trenton", "Red Light",
    "Harwood", "Hepburn Hts", "St. Mark's", "Torrington", "Bedford Pt",
    "Cedar Grove", "Vice Point", "Ocean Beach", "Downtown", "Vinewood",
    "Idlewood", "Las Colinas", "Rockford", "Verona Bch", "Washington",
    "Pillbox", "Bayview", "Easton", "Belleville", "Westside",
    "Little Haiti", "Vespucci", "El Corona", "Hashbury", "Garcia",
)


def assign_district_names(seed, assign, num_districts):
    """Nom d'affichage pour CHAQUE district -> {district_id: nom}.

    District theme -> nom du theme (Chinatown, Les Quais...) ; les autres ->
    nom 'GTA' pioche dans DISTRICT_NAMES (deterministe, sans doublon). Garantit
    qu'on est toujours dans un quartier nomme."""
    rng = random.Random(seed + 60)
    pool = list(DISTRICT_NAMES)
    rng.shuffle(pool)
    names, pi = {}, 0
    for d in range(num_districts):
        if d in assign:
            names[d] = THEME_NAMES[assign[d]]
        else:
            names[d] = pool[pi % len(pool)]
            pi += 1
    return names


def collect_district_labels(district_id, names, grid, tile_index, w, h):
    """Etiquettes de quartiers pour le rendu PNG -> [(nom, cx, cy)] en TUILES.

    Un libelle par district ayant des cellules terrestres ; (cx,cy) = centroide
    des cellules de terre (l'eau est ignoree). Ordre stable par district_id."""
    water = tile_index.get("water")
    by_dist = {}
    for i in range(w * h):
        if grid[i] == water:
            continue
        by_dist.setdefault(district_id[i], []).append(i)
    labels = []
    for d in sorted(by_dist):
        cells = by_dist[d]
        cx = sum(c % w for c in cells) // len(cells)
        cy = sum(c // w for c in cells) // len(cells)
        labels.append((names.get(d, ""), cx, cy))
    return labels


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
    centers = []        # centres (tx,ty) des stamps deja poses (pour dispersion)

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
        if name in STORY_STAMPS and centers:
            # repere de trame : on le pose le plus LOIN possible de tous les
            # reperes deja places (services + reperes precedents) -> dispersion
            # sur la carte. Deterministe (distance, puis zone, puis x,y).
            def _spread_key(c):
                _, cxx, cyy, _cells = c
                mx, my = cxx + sw // 2, cyy + sh // 2
                dmin = min((mx - px) ** 2 + (my - py) ** 2 for px, py in centers)
                return (-dmin, c[0], cxx, cyy)
            cands.sort(key=_spread_key)
        else:
            rng = random.Random(seed + 20 + k)
            rng.shuffle(cands)
            cands.sort(key=lambda c: c[0])     # zone privilegiee d'abord (tri stable)
        _, x, y, cells = cands[0]
        for ci, c in enumerate(cells):
            ry, rx = divmod(ci, sw)
            grid[c] = tile_index[sdef["tiles"][blueprint[ry][rx]]]
            occupied.add(c)
        placed[name] = (x, y)
        centers.append((x + sw // 2, y + sh // 2))
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


# --- services en bord de route : Pay'n'Spray & AMU Nation --------------------
# Marqueurs poses par le generateur (et plus aleatoirement par le jeu). Rendus
# par des sprites procuduraux cote jeu -> aucune tuile dediee, la grille n'est
# PAS modifiee : on n'exporte que des coordonnees (tuiles). Regle de placement
# commune : sur une cellule de bati bordant directement une route DU COTE SANS
# TROTTOIR. add_pavement ne pose le trottoir qu'au nord des rues H et a l'ouest
# des rues V ; le cote oppose (sud d'une rue H, est d'une rue V) borde donc la
# chaussee sans trottoir -- c'est la qu'on accroche les garages/armureries.

SPRAY_COUNT = 6        # Pay'n'Spray (acces en voiture)
AMMU_COUNT = 8         # AMU Nation (acces a pied), disseminees sur la carte


def _road_edge_blocks(grid, tile_index, w, h):
    """Cellules building_a/b bordant une route du cote SANS trottoir -> [(x,y)].

    Cote sud d'une rue horizontale ou cote est d'une rue verticale (cf.
    add_pavement, qui ne trottoirise que le nord/l'ouest). La cellule reste
    solide ; le sprite du service est dessine par-dessus cote jeu, et le
    vehicule/joueur la declenche depuis la chaussee adjacente."""
    if "building_a" not in tile_index or "building_b" not in tile_index:
        return []
    blocks = {tile_index["building_a"], tile_index["building_b"]}
    rh, rv, rx = (tile_index["road_h"], tile_index["road_v"],
                  tile_index["road_cross"])
    out = []
    for y in range(h):
        for x in range(w):
            if grid[y * w + x] not in blocks:
                continue
            north = grid[(y - 1) * w + x] if y > 0 else None
            west = grid[y * w + (x - 1)] if x > 0 else None
            if north in (rh, rx) or west in (rv, rx):   # route au N (rue H) ou a l'O (rue V)
                out.append((x, y))
    return out


def _spread_pick(cands, seed, count, gap, occupied):
    """Jusqu'a `count` cellules de `cands` bien dispersees (>= `gap` entre elles
    et hors `occupied`), tirage deterministe. Repli : complete sans contrainte
    d'espacement si l'espacement ne suffit pas. Met a jour `occupied`."""
    rng = random.Random(seed)
    pool = [c for c in cands if c not in occupied]
    rng.shuffle(pool)
    g2 = gap * gap
    picks = []
    for (x, y) in pool:
        if (x, y) in occupied:
            continue
        if all((x - px) ** 2 + (y - py) ** 2 >= g2 for px, py in picks):
            picks.append((x, y))
            occupied.add((x, y))
        if len(picks) >= count:
            return picks
    for (x, y) in pool:                  # repli : remplir le quota, gap relache
        if (x, y) in occupied:
            continue
        picks.append((x, y))
        occupied.add((x, y))
        if len(picks) >= count:
            break
    return picks


def place_casse(grid, district_id, assign, tile_index, solid_index, w, h):
    """Point de depose / grue de La Casse -> (tx, ty) en TUILES, ou None.

    Une tuile ROUTE (carrossable -> accessible en voiture) du district junkyard,
    la plus proche du centroide des cellules du quartier (a defaut, une route
    bordant le district). Deterministe (depart : plus proche du centroide, puis
    indice). None si le theme junkyard n'est pas place (tuiles ou district
    absents) -> la feature est simplement absente cote jeu."""
    junk = [d for d, t in assign.items() if t == THEME_JUNKYARD]
    if not junk:
        return None
    jd = junk[0]
    roads = {tile_index[n] for n in ("road_h", "road_v", "road_cross")
             if n in tile_index}
    if not roads:
        return None
    cells = [i for i in range(w * h) if district_id[i] == jd]
    if not cells:
        return None
    cx = sum(i % w for i in cells) // len(cells)
    cy = sum(i // w for i in cells) // len(cells)
    # candidates : routes DANS le district ; a defaut routes bordant le district.
    cands = [i for i in cells if grid[i] in roads]
    if not cands:
        jset = set(cells)
        for i in range(w * h):
            if grid[i] not in roads:
                continue
            x, y = i % w, i // w
            if any(0 <= x + dx < w and 0 <= y + dy < h
                   and (y + dy) * w + (x + dx) in jset
                   for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                cands.append(i)
    if not cands:
        return None
    best = min(cands, key=lambda i: ((i % w - cx) ** 2 + (i // w - cy) ** 2, i))
    return (best % w, best // w)


def place_crane(grid, casse, tile_index, w, h):
    """Cabine de la grue de La Casse -> (tx, ty), tuile de FACADE junkyard la
    plus proche de la zone de depose `casse`. La grue est plantee sur un
    batiment du quartier ; sa fleche pivote vers la voiture garee puis vers le
    broyeur. Deterministe. None si pas de zone ou pas de facade junkyard."""
    if not casse:
        return None
    facades = {tile_index[n] for n in ("junk_facade_a", "junk_facade_b", "junk_sign")
               if n in tile_index}
    if not facades:
        return None
    cx, cy = casse
    best, best_key = None, None
    for i in range(w * h):
        if grid[i] not in facades:
            continue
        x, y = i % w, i // w
        key = ((x - cx) ** 2 + (y - cy) ** 2, i)
        if best_key is None or key < best_key:
            best_key, best = key, (x, y)
    return best


# --- enceinte de La Casse (casse automobile clôturée) -----------------------
# Plutot qu'un simple point sur la route, on TAMPONNE une vraie cour de casse
# dans le district junkyard : grille tout autour (junk_fence, solide), sol de
# cambouis carrossable (junk_ground), epaves statiques (junk_wreck, solides,
# rendues comme des batiments -> zero cout CPU), base de grue (junk_crane) et
# zone de depose centrale. Une seule ouverture (entree) bordee par une route.
#
# Legende : G=grille  .=sol  w=epave  K=base de grue  Z=zone de depose
#           (l'unique '.' de la derniere ligne = entree, route exigee au sud).
JUNK_BLUEPRINT = (
    "GGGGGGGGG",
    "G.w...w.G",
    "G...K...G",
    "Gw.....wG",
    "G...Z...G",
    "G.w...w.G",
    "GGGG.GGGG",
)
JUNK_TILE_OF = {"G": "junk_fence", ".": "junk_ground", "Z": "junk_ground",
                "w": "junk_wreck", "K": "junk_crane"}


def junkyard_tiles_available(tile_index):
    """Vrai si les 4 tuiles de l'enceinte sont dans le tileset."""
    return all(t in tile_index for t in JUNK_TILE_OF.values())


def _blueprint_find(ch):
    for ry, row in enumerate(JUNK_BLUEPRINT):
        c = row.find(ch)
        if c >= 0:
            return ry, c
    return None


def place_junkyard(grid, district_id, assign, tile_index, w, h, margin=2):
    """Tamponne l'enceinte de La Casse dans le district junkyard.

    Cherche une fenetre de la taille du blueprint, libre de route/eau, dont
    l'entree (bas-centre) est bordee par une route au sud (acces voiture) ;
    priorite aux fenetres centrees dans le district junkyard puis proches de son
    centroide (deterministe). Ecrit les tuiles sur `grid` et renvoie
    {zone:(tx,ty), crane:(tx,ty), entrance:(tx,ty)} en TUILES, ou None si pas de
    theme junkyard, tuiles manquantes, ou aucune fenetre valide (repli : l'appelant
    retombe sur place_casse/place_crane)."""
    if not junkyard_tiles_available(tile_index):
        return None
    junk = [d for d, t in assign.items() if t == THEME_JUNKYARD]
    if not junk:
        return None
    jd = junk[0]
    bh, bw = len(JUNK_BLUEPRINT), len(JUNK_BLUEPRINT[0])
    roads = {tile_index[n] for n in ("road_h", "road_v", "road_cross")
             if n in tile_index}
    if not roads:
        return None
    water = tile_index.get("water")
    erow, ecol = (bh - 1), JUNK_BLUEPRINT[bh - 1].index(".")  # entree (gap du bas)

    cells = [i for i in range(w * h) if district_id[i] == jd]
    if not cells:
        return None
    cx = sum(i % w for i in cells) // len(cells)
    cy = sum(i // w for i in cells) // len(cells)

    cands = []
    for y in range(margin, h - margin - bh):
        for x in range(margin, w - margin - bw):
            block = [(y + ry) * w + (x + rx)
                     for ry in range(bh) for rx in range(bw)]
            if any(grid[c] in roads or grid[c] == water for c in block):
                continue
            ey, ex = y + erow, x + ecol         # case d'entree
            if ey + 1 >= h or grid[(ey + 1) * w + ex] not in roads:
                continue                        # route exigee juste au sud
            ccx, ccy = x + bw // 2, y + bh // 2
            inj = 0 if district_id[ccy * w + ccx] == jd else 1
            d = (ccx - cx) ** 2 + (ccy - cy) ** 2
            cands.append((inj, d, y, x, block))
    if not cands:
        return None
    cands.sort()                                # district d'abord, puis centroide
    _, _, y, x, block = cands[0]
    for ci, c in enumerate(block):
        ry, rx = divmod(ci, bw)
        grid[c] = tile_index[JUNK_TILE_OF[JUNK_BLUEPRINT[ry][rx]]]
    zr, zc = _blueprint_find("Z")
    kr, kc = _blueprint_find("K")
    return dict(zone=(x + zc, y + zr), crane=(x + kc, y + kr),
                entrance=(x + ecol, y + erow))


# --- enceinte du Chantier (chantier de construction clôturé) ----------------
# Même principe que La Casse : on TAMPONNE un vrai chantier dans le district
# construction -> palissade de danger tout autour (cons_fence, solide), sol de
# terre damee carrossable (cons_ground), ossatures beton (cons_frame, solides,
# rendues comme des batiments -> zero cout CPU) et base de grue a tour
# (cons_crane). Une seule ouverture (entree) bordee par une route au sud.
#
# Legende : G=palissade  .=terre  F=ossature  K=base de grue
#           (l'unique '.' de la derniere ligne = entree, route exigee au sud).
# 9x7 : grand chantier. Les blocs du quartier construction sont petits, donc
# on tolere de RECOUVRIR quelques tuiles de route (le placement minimise ce
# nombre) pour garder une grande enceinte ENTIEREMENT dans le quartier.
CONS_BLUEPRINT = (
    "GGGGGGGGG",
    "G.F...F.G",
    "G.......G",
    "GF..K..FG",
    "G.......G",
    "G.F...F.G",
    "GGGG.GGGG",
)
CONS_TILE_OF = {"G": "cons_fence", ".": "cons_ground",
                "F": "cons_frame", "K": "cons_crane"}


def construction_tiles_available(tile_index):
    """Vrai si les 4 tuiles du chantier sont dans le tileset."""
    return all(t in tile_index for t in CONS_TILE_OF.values())


def place_construction(grid, district_id, assign, tile_index, w, h, margin=2):
    """Tamponne le GRAND chantier (9x7) DANS le quartier construction.

    Les blocs du quartier sont petits : aucune fenetre 9x7 sans route n'y tient.
    On choisit donc la fenetre qui (1) recouvre le PLUS de cellules du quartier,
    (2) recouvre le MOINS de route possible (on tolere d'en ecraser quelques
    tuiles), (3) est la plus centrale. On n'ecrase jamais l'eau ni un autre POI
    (Casse, stamps). Deterministe. Renvoie {crane,entrance} en TUILES ou None."""
    if not construction_tiles_available(tile_index):
        return None
    cons = [d for d, t in assign.items() if t == THEME_CONSTRUCTION]
    if not cons:
        return None
    cd = cons[0]
    bh, bw = len(CONS_BLUEPRINT), len(CONS_BLUEPRINT[0])
    roads = {tile_index[n] for n in ("road_h", "road_v", "road_cross")
             if n in tile_index}
    # Tuiles INTERDITES (jamais ecrasees) : eau + tous les autres POI (enceinte
    # de La Casse, stamps services + reperes de trame, quais). Les routes, elles,
    # sont TOLEREES (recouvertes) mais minimisees -- cf. cle de tri.
    hard = set()
    if "water" in tile_index:
        hard.add(tile_index["water"])
    for n in ("junk_ground", "junk_fence", "junk_wreck", "junk_crane",
              "dock", "sand"):
        if n in tile_index:
            hard.add(tile_index[n])
    for sdef in STAMP_DEFS.values():
        for t in sdef["tiles"].values():
            if t in tile_index:
                hard.add(tile_index[t])
    erow = bh - 1
    ecol = CONS_BLUEPRINT[erow].index(".")          # entree (gap du bas)

    cells = [i for i in range(w * h) if district_id[i] == cd]
    if not cells:
        return None
    cx = sum(i % w for i in cells) // len(cells)
    cy = sum(i // w for i in cells) // len(cells)

    cands = []
    for y in range(margin, h - margin - bh):
        for x in range(margin, w - margin - bw):
            block = [(y + ry) * w + (x + rx)
                     for ry in range(bh) for rx in range(bw)]
            if any(grid[c] in hard for c in block):
                continue
            overlap = sum(1 for c in block if district_id[c] == cd)
            if overlap == 0:                        # doit toucher le quartier
                continue
            roadn = sum(1 for c in block if grid[c] in roads)
            ccx, ccy = x + bw // 2, y + bh // 2
            d = (ccx - cx) ** 2 + (ccy - cy) ** 2
            # max recouvrement quartier, puis min route ecrasee, puis central.
            cands.append((-overlap, roadn, d, y, x, block))
    if not cands:
        return None
    cands.sort()
    _, _, _, y, x, block = cands[0]
    # decalage esthetique d'une case vers la gauche (demande), si la fenetre
    # decalee reste valide (dans les bornes, sans tuile interdite).
    if x - 1 >= margin:
        shifted = [(y + ry) * w + (x - 1 + rx)
                   for ry in range(bh) for rx in range(bw)]
        if not any(grid[c] in hard for c in shifted):
            x -= 1
            block = shifted
    for ci, c in enumerate(block):
        ry, rx = divmod(ci, bw)
        grid[c] = tile_index[CONS_TILE_OF[CONS_BLUEPRINT[ry][rx]]]
    kr = kc = None
    for ry, row in enumerate(CONS_BLUEPRINT):
        cpos = row.find("K")
        if cpos >= 0:
            kr, kc = ry, cpos
            break
    return dict(crane=(x + kc, y + kr), entrance=(x + ecol, y + erow))


def place_services(grid, tile_index, seed, w, h, occupied=None):
    """Positions des Pay'n'Spray et AMU Nation -> (sprays, ammus) en TUILES.

    Bord de route sans trottoir, dispersees, deterministes. Ne modifie pas la
    grille. Les deux services partagent `occupied` -> jamais sur la meme case.
    Les sprays (voiture) sont plus espaces que les armureries (a pied)."""
    occupied = occupied if occupied is not None else set()
    cands = _road_edge_blocks(grid, tile_index, w, h)
    sprays = _spread_pick(cands, seed + 40, SPRAY_COUNT, 18, occupied)
    ammus = _spread_pick(cands, seed + 41, AMMU_COUNT, 12, occupied)
    return sprays, ammus


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


def collect_pois(grid, theme, assign, placed, sea, tile_index, solid_index, w, h,
                 district_id=None, names=None):
    """Construit la liste des POI exportables -> [dict(name,x0,y0,x1,y1,tx,ty)].

    * quartiers thematiques (theme) : bbox des cellules du district + point-cible
      = la cellule marchable la plus proche du centroide (le port privilegie un
      quai);
    * quartiers generiques (tout district non theme, si `district_id`+`names`
      fournis) : meme principe sur les cellules de terre du district, nom 'GTA'
      -> on est toujours dans un quartier nomme cote HUD ;
    * stamps : bbox du blueprint 3x3 + point-cible = la porte (case marchable
      bordant la route). `placed` = {nom: (x,y) coin haut-gauche | None}.
    Tout en coordonnees de TUILES. Ordre stable : quartiers themes, quartiers
    generiques, puis stamps (les stamps ont la plus petite bbox -> priorite HUD)."""
    out = []
    road_pav = {tile_index[n] for n in ("road_h", "road_v", "road_cross", "pavement")
                if n in tile_index}
    dock = {tile_index["dock"]} if "dock" in tile_index else set()
    water = tile_index.get("water")

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

    # quartiers generiques : tout district non theme avec des cellules de terre.
    if district_id is not None and names:
        by_dist = {}
        for i in range(w * h):
            d = district_id[i]
            if d in assign or grid[i] == water:   # themes deja emis ; ignore l'eau
                continue
            by_dist.setdefault(d, []).append(i)
        for d in sorted(by_dist):
            cells = by_dist[d]
            xs = [c % w for c in cells]
            ys = [c // w for c in cells]
            x0, y0, x1, y1 = min(xs), min(ys), max(xs), max(ys)
            cx, cy = sum(xs) // len(xs), sum(ys) // len(ys)
            tgt = _nearest_walkable(grid, cells, cx, cy, solid_index, road_pav, w) \
                or (cx, cy)
            out.append(dict(name=names[d], x0=x0, y0=y0, x1=x1, y1=y1,
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
