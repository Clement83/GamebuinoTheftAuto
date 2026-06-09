"""Placement deterministe des points d'interet (POI) sur la grille de ville.

Pur (aucune I/O). S'appuie sur le decoupage en districts de citygen. Deux
mecanismes :

  * quartier thematique  -> build_chinatown_theme : promeut UN district downtown
    en theme Chinatown (re-tuilage des blocs, gere dans citygen.fill_blocks) ;
  * stamp / batiment-repere -> place_police : tamponne un petit blueprint fixe
    sur une parcelle bati-libre accolee a une route.

Active seulement si les tuiles POI sont presentes (citygen verifie en amont).
Tout derive de la graine -> reproductible.
"""
import random

from tools import citygen
from tools.citygen import THEME_NONE, THEME_CHINATOWN, Z_DOWNTOWN, Z_WATER, Z_PARK


def build_chinatown_theme(seed, zones, district_id, seed_type, w, h):
    """Choisit UN district downtown comme Chinatown -> (theme[w*h], district|None).

    Le theme ne couvre que les cellules terrestres (ni eau ni parc) du district
    retenu. S'il n'existe aucun district downtown, renvoie un theme vide."""
    downtown = [i for i, t in enumerate(seed_type) if t == Z_DOWNTOWN]
    theme = [THEME_NONE] * (w * h)
    if not downtown:
        return theme, None
    pick = random.Random(seed + 8).choice(downtown)
    for i in range(w * h):
        if district_id[i] == pick and zones[i] not in (Z_WATER, Z_PARK):
            theme[i] = THEME_CHINATOWN
    return theme, pick


# Blueprint commissariat 3x3. F=facade, S=enseigne (haut-centre),
# D=porte non-solide (bas-centre -> entree orientee vers la route au sud).
POLICE_STAMP = (
    "FSF",
    "FFF",
    "FDF",
)
_STAMP_TILE = {"F": "police_facade", "S": "police_sign", "D": "police_door"}


def _door_cell():
    """(row, col) de la porte 'D' dans le blueprint."""
    for r, row in enumerate(POLICE_STAMP):
        c = row.find("D")
        if c >= 0:
            return r, c
    raise ValueError("POLICE_STAMP sans porte 'D'")


def place_police(grid, zones, district_id, seed_type, seed, w, h, idx, margin=3):
    """Tamponne le commissariat et renvoie (x,y) du coin haut-gauche, ou None.

    Contraintes d'un emplacement valide :
      * parcelle de la taille du blueprint entierement 'bati-libre'
        (batiments/grass/tuiles chinatown -- jamais route/eau) ;
      * une route juste au sud de la porte (acces + orientation) ;
      * dans la marge interieure.
    Priorite aux parcelles downtown, puis ordre derive de la graine. Aucune
    parcelle valide -> None (skip propre, signale par citygen via le retour)."""
    sh, sw = len(POLICE_STAMP), len(POLICE_STAMP[0])
    drow, dcol = _door_cell()
    road_ids = {idx["road_h"], idx["road_v"], idx["road_cross"]}
    # 'pavement' inclus : le stamp est pose apres la passe trottoir, donc une
    # parcelle valide peut comporter des bandes de trottoir le long des routes.
    buildable = {idx[k] for k in
                 ("building_a", "building_b", "grass", "pavement",
                  "cn_facade_a", "cn_facade_b", "cn_sign")
                 if k in idx}

    cands = []
    for y in range(margin, h - margin - sh):
        for x in range(margin, w - margin - sw):
            if any(grid[(y + ry) * w + (x + rx)] not in buildable
                   for ry in range(sh) for rx in range(sw)):
                continue
            sx, sy = x + dcol, y + drow + 1   # cellule au sud de la porte
            if sy >= h or grid[sy * w + sx] not in road_ids:
                continue
            cx, cy = x + sw // 2, y + sh // 2
            downtown = seed_type[district_id[cy * w + cx]] == Z_DOWNTOWN
            cands.append((0 if downtown else 1, x, y))

    if not cands:
        return None
    random.Random(seed + 9).shuffle(cands)
    cands.sort(key=lambda c: c[0])          # downtown d'abord (tri stable)
    _, x, y = cands[0]
    for ry in range(sh):
        for rx in range(sw):
            grid[(y + ry) * w + (x + rx)] = idx[_STAMP_TILE[POLICE_STAMP[ry][rx]]]
    return (x, y)
