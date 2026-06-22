"""Calque de retouches manuelles applique par-dessus la ville generee.

La ville est generee de facon deterministe a partir de city/city.txt (graine,
taille, params organiques). Ce module stocke des retouches tuile-par-tuile dans
city/overlay.json et les reapplique sur la grille compilee a chaque build, tant
que les params de generation n'ont pas change.

Format de city/overlay.json :
    {
      "params": "<sha1 de city.txt>",   # empreinte des params de generation
      "size": [w, h],                    # taille de la map au moment de l'edit
      "edits": { "x,y": tile_index, ... }
    }

L'editeur web (tools/map_editor.py) lit/ecrit ce fichier ; build_city l'applique.
"""
import hashlib
import json
import os

OVERLAY_JSON = "city/overlay.json"


def params_fingerprint(city_txt_path):
    """Empreinte des params de generation (= contenu de city.txt)."""
    with open(city_txt_path, "rb") as f:
        return hashlib.sha1(f.read()).hexdigest()


def load(path=OVERLAY_JSON):
    """Charge l'overlay. Retourne un dict normalise (jamais None)."""
    if not os.path.exists(path):
        return {"params": "", "size": None, "edits": {}}
    with open(path) as f:
        data = json.load(f)
    data.setdefault("params", "")
    data.setdefault("size", None)
    data.setdefault("edits", {})
    return data


def save(edits, fingerprint, size, path=OVERLAY_JSON):
    """Ecrit l'overlay. edits: {"x,y": tile_index}."""
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    payload = {"params": fingerprint, "size": list(size), "edits": edits}
    with open(path, "w") as f:
        json.dump(payload, f, indent=1, sort_keys=True)
    return payload


def apply(city, fingerprint, path=OVERLAY_JSON, warn=print):
    """Applique les retouches sur la grille de `city`. Retourne le nb applique.

    Avertit (via `warn`) si l'empreinte des params a change ou si la taille
    differe : les coordonnees risquent alors de ne plus correspondre.
    """
    data = load(path)
    edits = data.get("edits") or {}
    if not edits:
        return 0
    if data.get("params") and data["params"] != fingerprint:
        warn("ATTENTION overlay: city.txt a change depuis l'edition manuelle ; "
             "les %d retouche(s) peuvent etre decalees." % len(edits))
    if data.get("size") and tuple(data["size"]) != (city.w, city.h):
        warn("ATTENTION overlay: taille edition %s != map %s ; retouches ignorees."
             % (tuple(data["size"]), (city.w, city.h)))
        return 0
    n = 0
    for key, tile in edits.items():
        x, y = (int(v) for v in key.split(","))
        if 0 <= x < city.w and 0 <= y < city.h:
            city.set(x, y, int(tile))
            n += 1
    return n
