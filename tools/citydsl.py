"""Compilateur pur du DSL de ville : texte -> CompiledCity.

Aucune I/O de fichier ici (lecture city.txt + écriture headers/PNG = build_city.py).
"""
import random

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

    def _i(self, x, y):
        return y * self.w + x

    def get(self, x, y):
        return self.grid[self._i(x, y)]

    def set(self, x, y, t):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.grid[self._i(x, y)] = t


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
            rng = random.Random(_int(tok[1], line_no, "seed"))

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

        else:
            raise CityError(line_no, "commande inconnue: '%s'" % cmd)

    return _finalize(city, solid_index, tile_index)


def _finalize(city, solid_index, tile_index):
    if city is None:
        raise CityError(0, "fichier vide: 'size' manquant")
    return city
