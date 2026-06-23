"""Extraction de masques mer/route depuis l'illustration marseille_simple.png.

Carte routière stylisée (mer pastel bleue, terre beige, autoroutes jaunes,
grandes routes orange). On en derive, a la resolution de la grille de jeu :
  - un masque MER (la vraie cote de Marseille),
  - un masque ROUTE (les axes reels), ferme morphologiquement pour ponter les
    pointilles (tunnels) et garantir un reseau carrossable.

I/O image (PIL) -> isole ici ; le generateur (citygen_marseille) reste sinon
pur. Deterministe : meme image + meme taille => memes masques.
"""
import colorsys

IMAGE_PATH = "marseille_simple.png"

# Coin haut-gauche : titre "MARSEILLE" + encart de legende (traits jaunes/orange
# parasites + pastille mer). On neutralise la detection de ROUTE dans ce cadre.
LEGEND_FRAC = (0.0, 0.0, 0.165, 0.205)   # (x0,y0,x1,y1) en fractions de l'image


def _is_road(r, g, b):
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    return 18 <= h * 360 <= 55 and s > 0.5 and v > 0.55


def _is_sea(r, g, b):
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    return b > r + 8 and b > g - 2 and v > 0.6 and h * 360 > 180


def _dilate(mask, w, h):
    out = [False] * (w * h)
    for y in range(h):
        for x in range(w):
            if not mask[y * w + x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        out[ny * w + nx] = True
    return out


def _erode(mask, w, h):
    out = [False] * (w * h)
    for y in range(h):
        for x in range(w):
            ok = True
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nx, ny = x + dx, y + dy
                    if not (0 <= nx < w and 0 <= ny < h and mask[ny * w + nx]):
                        ok = False
                        break
                if not ok:
                    break
            out[y * w + x] = ok
    return out


def extract(image_path, w, h):
    """Retourne (sea[w*h] bool, road[w*h] bool) a la resolution (w,h)."""
    from PIL import Image
    im = Image.open(image_path).convert("RGB")
    IW, IH = im.size
    px = im.load()
    sx, sy = IW / w, IH / h
    lx0 = int(LEGEND_FRAC[0] * w); ly0 = int(LEGEND_FRAC[1] * h)
    lx1 = int(LEGEND_FRAC[2] * w); ly1 = int(LEGEND_FRAC[3] * h)

    sea = [False] * (w * h)
    road = [False] * (w * h)
    for gy in range(h):
        in_leg_y = ly0 <= gy < ly1
        for gx in range(w):
            x0, y0 = int(gx * sx), int(gy * sy)
            x1, y1 = int((gx + 1) * sx), int((gy + 1) * sy)
            nroad = nsea = nland = 0
            for yy in range(y0, max(y0 + 1, y1), 2):
                for xx in range(x0, max(x0 + 1, x1), 2):
                    r, g, b = px[xx, yy]
                    if _is_road(r, g, b):
                        nroad += 1
                    elif _is_sea(r, g, b):
                        nsea += 1
                    else:
                        nland += 1
            i = gy * w + gx
            in_legend = in_leg_y and lx0 <= gx < lx1
            if nroad >= 2 and not in_legend:
                road[i] = True
            elif nsea > nland:
                sea[i] = True
    # fermeture morphologique : ponte les pointilles (tunnels) sans trop epaissir
    road = _erode(_dilate(road, w, h), w, h)
    # une route ne traverse pas la mer (les rares cas image -> on garde terre)
    for i in range(w * h):
        if road[i] and sea[i]:
            sea[i] = False
    return sea, road


def debug_render(sea, road, w, h, out_path, scale=3):
    """PNG de controle : mer bleue, route noire, terre beige."""
    from PIL import Image
    img = Image.new("RGB", (w, h))
    o = img.load()
    for y in range(h):
        for x in range(w):
            i = y * w + x
            o[x, y] = (30, 30, 30) if road[i] else (
                (110, 180, 235) if sea[i] else (225, 210, 180))
    img.resize((w * scale, h * scale), Image.NEAREST).save(out_path)
