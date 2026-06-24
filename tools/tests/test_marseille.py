"""Invariants de la carte Marseille (image-driven) critiques pour la campagne.

Verrouille : determinisme, presence de TOUS les POI de campagne, reseau de
routes connexe, spawn non-solide. L'image city/sources/marseille_hight_contrast.png
doit exister (sinon le test est ignore)."""
import os

import pytest

from tools.citydsl import CompiledCity
from tools import citygen_marseille

# tileset complet ; ordre = enum de gta/assets.h (cf. test_pois.py)
NAMES = ["grass", "road_h", "road_v", "road_cross", "pavement", "water",
         "building_a", "building_b",
         "cn_facade_a", "cn_facade_b", "cn_sign",
         "police_facade", "police_sign", "police_door",
         "hosp_facade", "hosp_sign", "hosp_door",
         "fire_facade", "fire_sign", "fire_door",
         "port_facade_a", "port_facade_b", "port_sign",
         "cons_facade_a", "cons_facade_b", "cons_sign",
         "junk_facade_a", "junk_facade_b", "junk_sign",
         "sand", "dock",
         "junk_ground", "junk_fence", "junk_wreck", "junk_crane",
         "cons_ground", "cons_fence", "cons_frame", "cons_crane",
         "plan_facade", "plan_sign", "plan_door",
         "gar_facade", "gar_sign", "gar_door",
         "bar_facade", "bar_sign", "bar_door",
         "bur_facade", "bur_sign", "bur_door",
         "cas_facade", "cas_sign", "cas_door",
         "com_facade", "com_sign", "com_door"]
TI = {n: i for i, n in enumerate(NAMES)}
NONSOLID = {"grass", "road_h", "road_v", "road_cross", "pavement", "sand",
            "dock", "police_door", "hosp_door", "fire_door", "plan_door",
            "gar_door", "bar_door", "bur_door", "cas_door", "com_door",
            "junk_ground", "cons_ground"}
SI = {i for i, n in enumerate(NAMES) if n not in NONSOLID}

IMG = "city/sources/marseille_hight_contrast.png"
REQUIRED_POIS = {"Planque", "Le Casino", "Commissariat", "Hopital", "Pompiers",
                 "Le Garage", "Le Bar", "Les Bureaux", "Commerces", "La Casse",
                 "Chantier", "Les Quais"}

pytestmark = pytest.mark.skipif(not os.path.exists(IMG),
                                reason="city/sources/marseille_hight_contrast.png absent")

W, H, SEED = 240, 160, 7


def _build():
    c = CompiledCity(W, H)
    citygen_marseille.generate_into(c, SEED, TI, SI, IMG, density=0.92)
    return c


def test_deterministe():
    a, b = _build(), _build()
    assert a.grid == b.grid
    assert a.spawn == b.spawn


def test_tous_les_pois_campagne_presents():
    c = _build()
    names = {p["name"] for p in c.pois}
    manquants = REQUIRED_POIS - names
    assert not manquants, "POI de campagne manquants: %s" % manquants


def test_reseau_routes_connexe():
    """La composante de routes principale couvre la quasi-totalite du reseau.
    (Quelques epis isoles par un stamp de POI 3x3 sont toleres : < 2 %.)"""
    c = _build()
    road_ids = {TI["road_h"], TI["road_v"], TI["road_cross"]}
    roads = {i for i, t in enumerate(c.grid) if t in road_ids}
    assert roads, "aucune route"
    seen = set()
    best = 0
    for start in roads:
        if start in seen:
            continue
        comp = {start}
        stack = [start]
        while stack:
            i = stack.pop()
            x, y = i % W, i // W
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H:
                    j = ny * W + nx
                    if j in roads and j not in comp:
                        comp.add(j)
                        stack.append(j)
        seen |= comp
        best = max(best, len(comp))
    frac = best / len(roads)
    assert frac >= 0.98, \
        "reseau trop fragmente: composante principale %d/%d (%.1f%%)" \
        % (best, len(roads), 100 * frac)


def test_spawn_non_solide():
    c = _build()
    sx, sy, _ = c.spawn
    assert c.grid[sy * W + sx] not in SI, "spawn sur tuile solide"
