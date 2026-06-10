"""Compilateur pur du DSL de ville : texte -> CompiledCity.

Aucune I/O de fichier ici (lecture city.txt + écriture headers/PNG = build_city.py).
"""
import random

from tools import citygen

MAX_DIM = 256
DIRS = {"north": 0, "east": 1, "south": 2, "west": 3}


class CityError(Exception):
    """Erreur de compilation avec numéro de ligne (1-based)."""

    def __init__(self, line_no, msg):
        super().__init__("ligne %d: %s" % (line_no, msg))
        self.line_no = line_no
        self.msg = msg


class CompiledCity:
    def __init__(self, w, h):
        self.w = w
        self.h = h
        self.grid = [0] * (w * h)
        self.spawn = None  # (x, y, dir)
        self.pois = []     # [dict(name,x0,y0,x1,y1,tx,ty)] -- rempli par citygen

    def _i(self, x, y):
        return y * self.w + x

    def get(self, x, y):
        return self.grid[self._i(x, y)]

    def set(self, x, y, t):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.grid[self._i(x, y)] = t


def _kwargs(tokens):
    """Paire les tokens restants en dict {cle: valeur} (ex. ['spacing','4','width','1'])."""
    return {tokens[i]: tokens[i + 1] for i in range(0, len(tokens) - 1, 2)}


def _roadgrid(city, spacing, width, margin, tile_index, line_no):
    rv = _tile("road_v", tile_index, line_no)
    rh = _tile("road_h", tile_index, line_no)
    rc = _tile("road_cross", tile_index, line_no)
    pv = _tile("pavement", tile_index, line_no)
    if spacing < 1 or width < 1:
        raise CityError(line_no, "roadgrid: spacing et width doivent etre >= 1")

    cols, rows = set(), set()
    x0 = margin
    while x0 < city.w:
        for x in range(x0, x0 + width):
            if 0 <= x < city.w:
                cols.add(x)
        x0 += spacing
    y0 = margin
    while y0 < city.h:
        for y in range(y0, y0 + width):
            if 0 <= y < city.h:
                rows.add(y)
        y0 += spacing

    road_ids = {rv, rh, rc}
    for y in range(city.h):
        for x in range(city.w):
            on_col, on_row = x in cols, y in rows
            if on_col and on_row:
                city.set(x, y, rc)
            elif on_col:
                city.set(x, y, rv)
            elif on_row:
                city.set(x, y, rh)
    # trottoirs : toute case non-route adjacente (4-voisinage) a une route
    for y in range(city.h):
        for x in range(city.w):
            if city.get(x, y) in road_ids:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < city.w and 0 <= ny < city.h and city.get(nx, ny) in road_ids:
                    city.set(x, y, pv)
                    break


def _int(tok, line_no, what):
    try:
        return int(tok)
    except (ValueError, TypeError):
        raise CityError(line_no, "%s: entier attendu, recu '%s'" % (what, tok))


def _tile(name, tile_index, line_no):
    if name not in tile_index:
        raise CityError(line_no, "tuile inconnue: '%s'" % name)
    return tile_index[name]


def compile_city(text, tile_index, solid_index):
    """Compile le DSL. tile_index: {nom: index}. solid_index: set d'index solides."""
    city = None
    rng = random.Random(1)  # seed par defaut ; 'seed' la reinitialise
    seed_int = 1  # seed entier par defaut ; 'seed' le met a jour

    for line_no, raw in enumerate(text.splitlines(), start=1):
        # retirer commentaire (; ou #) puis trim
        line = raw.split(";", 1)[0].split("#", 1)[0].strip()
        if not line:
            continue
        tok = line.split()
        cmd = tok[0]

        if city is None and cmd != "size":
            raise CityError(line_no, "la premiere commande doit etre 'size'")

        if cmd == "size":
            if len(tok) != 3:
                raise CityError(line_no, "usage: size <W> <H>")
            if city is not None:
                raise CityError(line_no, "'size' deja defini")
            w = _int(tok[1], line_no, "W")
            h = _int(tok[2], line_no, "H")
            if not (1 <= w <= MAX_DIM and 1 <= h <= MAX_DIM):
                raise CityError(line_no, "dimensions hors bornes 1..%d" % MAX_DIM)
            city = CompiledCity(w, h)

        elif cmd == "fill":
            if len(tok) != 2:
                raise CityError(line_no, "usage: fill <tile>")
            t = _tile(tok[1], tile_index, line_no)
            city.grid = [t] * (city.w * city.h)

        elif cmd == "rect":
            if len(tok) != 6:
                raise CityError(line_no, "usage: rect <tile> <x0> <y0> <x1> <y1>")
            t = _tile(tok[1], tile_index, line_no)
            x0 = _int(tok[2], line_no, "x0"); y0 = _int(tok[3], line_no, "y0")
            x1 = _int(tok[4], line_no, "x1"); y1 = _int(tok[5], line_no, "y1")
            for y in range(min(y0, y1), max(y0, y1) + 1):
                for x in range(min(x0, x1), max(x0, x1) + 1):
                    city.set(x, y, t)

        elif cmd == "seed":
            if len(tok) != 2:
                raise CityError(line_no, "usage: seed <N>")
            seed_int = _int(tok[1], line_no, "seed")
            rng = random.Random(seed_int)

        elif cmd == "player":
            if len(tok) != 4:
                raise CityError(line_no, "usage: player <x> <y> <north|south|east|west>")
            x = _int(tok[1], line_no, "x"); y = _int(tok[2], line_no, "y")
            d = tok[3]
            if d not in DIRS:
                raise CityError(line_no, "direction inconnue: '%s'" % d)
            if not (0 <= x < city.w and 0 <= y < city.h):
                raise CityError(line_no, "spawn hors bornes: (%d,%d)" % (x, y))
            if city.get(x, y) in solid_index:
                raise CityError(line_no, "spawn sur tuile solide: (%d,%d)" % (x, y))
            city.spawn = (x, y, DIRS[d])

        elif cmd == "hline":
            if len(tok) != 5:
                raise CityError(line_no, "usage: hline <tile> <x0> <x1> <y>")
            t = _tile(tok[1], tile_index, line_no)
            x0 = _int(tok[2], line_no, "x0"); x1 = _int(tok[3], line_no, "x1")
            y = _int(tok[4], line_no, "y")
            for x in range(min(x0, x1), max(x0, x1) + 1):
                city.set(x, y, t)

        elif cmd == "vline":
            if len(tok) != 5:
                raise CityError(line_no, "usage: vline <tile> <y0> <y1> <x>")
            t = _tile(tok[1], tile_index, line_no)
            y0 = _int(tok[2], line_no, "y0"); y1 = _int(tok[3], line_no, "y1")
            x = _int(tok[4], line_no, "x")
            for y in range(min(y0, y1), max(y0, y1) + 1):
                city.set(x, y, t)

        elif cmd == "river":
            water = _tile("water", tile_index, line_no)
            if len(tok) != 5 or tok[3] != "width":
                raise CityError(line_no, "usage: river <vertical|horizontal> <pos> width <W>")
            orient = tok[1]
            pos = _int(tok[2], line_no, "pos")
            wd = _int(tok[4], line_no, "width")
            if orient == "vertical":
                for x in range(pos, pos + wd):
                    for y in range(city.h):
                        city.set(x, y, water)
            elif orient == "horizontal":
                for y in range(pos, pos + wd):
                    for x in range(city.w):
                        city.set(x, y, water)
            else:
                raise CityError(line_no, "orientation inconnue: '%s'" % orient)

        elif cmd == "roadgrid":
            kv = _kwargs(tok[1:])
            if "spacing" not in kv or "width" not in kv:
                raise CityError(line_no, "usage: roadgrid spacing <S> width <W> [margin <M>]")
            spacing = _int(kv["spacing"], line_no, "spacing")
            width = _int(kv["width"], line_no, "width")
            margin = _int(kv.get("margin", "0"), line_no, "margin")
            _roadgrid(city, spacing, width, margin, tile_index, line_no)

        elif cmd == "blocks":
            if len(tok) < 4:
                raise CityError(line_no, "usage: blocks <tile> density <D> [on <baseTile>]")
            t = _tile(tok[1], tile_index, line_no)
            kv = _kwargs(tok[2:])
            if "density" not in kv:
                raise CityError(line_no, "usage: blocks <tile> density <D> [on <baseTile>]")
            try:
                density = float(kv["density"])
            except ValueError:
                raise CityError(line_no, "density: nombre attendu, recu '%s'" % kv["density"])
            base_name = kv.get("on", "grass")
            base = _tile(base_name, tile_index, line_no)
            for y in range(city.h):
                for x in range(city.w):
                    if city.get(x, y) == base and rng.random() < density:
                        city.set(x, y, t)

        elif cmd == "organic":
            kv = _kwargs(tok[1:])
            water, parks, density = 0.18, 0.10, 0.85
            if "water" in kv:
                try:
                    water = float(kv["water"])
                except ValueError:
                    raise CityError(line_no, "water: nombre attendu, recu '%s'" % kv["water"])
            if "parks" in kv:
                try:
                    parks = float(kv["parks"])
                except ValueError:
                    raise CityError(line_no, "parks: nombre attendu, recu '%s'" % kv["parks"])
            if "density" in kv:
                try:
                    density = float(kv["density"])
                except ValueError:
                    raise CityError(line_no, "density: nombre attendu, recu '%s'" % kv["density"])
            districts = _int(kv["districts"], line_no, "districts") if "districts" in kv else 8
            citygen.generate_into(city, seed_int, tile_index, solid_index,
                                  water, parks, density, districts)

        else:
            raise CityError(line_no, "commande inconnue: '%s'" % cmd)

    return _finalize(city, solid_index, tile_index)


def _finalize(city, solid_index, tile_index):
    if city is None:
        raise CityError(0, "fichier vide: 'size' manquant")
    if city.spawn is None:
        raise CityError(0, "commande 'player' manquante")
    return city
