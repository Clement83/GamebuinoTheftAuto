"""Extraction de masques mer/route/parc depuis l'illustration de Marseille.

Carte routiere stylisee haut-contraste (mer bleu sature, massifs verts, urbain
beige, autoroutes rouges + grandes routes orange). On en derive, a la resolution
de la grille de jeu :
  - MER   : la vraie cote de Marseille (baies, Vieux-Port, calanques, iles),
  - ROUTE : les axes reels (rouge/orange),
  - PARC  : les massifs/espaces verts (l'Etoile, Allauch, Calanques),
  - le reste = terre urbaine (batie).

Les pastilles vertes des libelles de quartiers sont supprimees (filtre de
petites composantes) pour ne garder que les vrais massifs. I/O image (PIL)
isolee ici ; le generateur reste sinon pur. Deterministe.
"""
import colorsys

IMAGE_PATH = "city/sources/marseille_hight_contrast.png"

MIN_PARK_BLOB = 16        # composantes vertes plus petites = pastilles de label


def _hsv(r, g, b):
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    return h * 360.0, s, v


def _is_road(r, g, b):
    h, s, v = _hsv(r, g, b)
    return s > 0.55 and v > 0.65 and (h <= 48 or h >= 345)


def _is_sea(r, g, b):
    h, s, v = _hsv(r, g, b)
    return s > 0.5 and 185 <= h <= 235


def _is_park(r, g, b):
    h, s, v = _hsv(r, g, b)
    return s > 0.28 and 55 <= h <= 165


def _drop_small_blobs(mask, w, h, min_size):
    """Met a False les composantes 4-connexes plus petites que min_size."""
    seen = [False] * (w * h)
    for s in range(w * h):
        if not mask[s] or seen[s]:
            continue
        comp = [s]; seen[s] = True; k = 0
        while k < len(comp):
            i = comp[k]; k += 1
            x, y = i % w, i // w
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h:
                    j = ny * w + nx
                    if mask[j] and not seen[j]:
                        seen[j] = True; comp.append(j)
        if len(comp) < min_size:
            for i in comp:
                mask[i] = False


def extract(image_path, w, h):
    """Retourne (sea[w*h], road[w*h], park[w*h]) bool a la resolution (w,h)."""
    from PIL import Image
    im = Image.open(image_path).convert("RGB")
    IW, IH = im.size
    px = im.load()
    sx, sy = IW / w, IH / h

    sea = [False] * (w * h)
    road = [False] * (w * h)
    park = [False] * (w * h)
    for gy in range(h):
        for gx in range(w):
            x0, y0 = int(gx * sx), int(gy * sy)
            x1, y1 = int((gx + 1) * sx), int((gy + 1) * sy)
            nroad = nsea = npark = nland = 0
            for yy in range(y0, max(y0 + 1, y1), 2):
                for xx in range(x0, max(x0 + 1, x1), 2):
                    r, g, b = px[xx, yy]
                    if _is_road(r, g, b):
                        nroad += 1
                    elif _is_sea(r, g, b):
                        nsea += 1
                    elif _is_park(r, g, b):
                        npark += 1
                    else:
                        nland += 1
            i = gy * w + gx
            if nroad >= 2:                       # route prioritaire (lignes fines)
                road[i] = True
            elif nsea >= nland + npark and nsea > 0:
                sea[i] = True
            elif npark > nland:
                park[i] = True
    _drop_small_blobs(park, w, h, MIN_PARK_BLOB)
    for i in range(w * h):                        # une route ne traverse pas la mer
        if road[i] and sea[i]:
            sea[i] = False
    return sea, road, park


def debug_render(sea, road, park, w, h, out_path, scale=3):
    """PNG de controle : mer bleue, route rouge, parc vert, urbain beige."""
    from PIL import Image
    img = Image.new("RGB", (w, h))
    o = img.load()
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if road[i]:
                o[x, y] = (210, 60, 20)
            elif sea[i]:
                o[x, y] = (40, 90, 180)
            elif park[i]:
                o[x, y] = (70, 150, 60)
            else:
                o[x, y] = (225, 210, 180)
    img.resize((w * scale, h * scale), Image.NEAREST).save(out_path)
