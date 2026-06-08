"""Helpers de conversion d'images pour le pipeline Gamebuino."""
from PIL import Image


def rgb565(r, g, b):
    """Pack un triplet RGB 8 bits en RGB565 (entier 16 bits)."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def load_rgb(path):
    """Ouvre une image (BMP/PNG, palettisée ou non) en mode RGB."""
    with Image.open(path) as im:
        return im.convert("RGB")


def load_rgba_keyed(path):
    """Ouvre en RGBA en rendant transparente la couleur du pixel (0,0)."""
    with Image.open(path) as im:
        rgba = im.convert("RGBA")
    bg = rgba.getpixel((0, 0))
    px = rgba.load()
    w, h = rgba.size
    for y in range(h):
        for x in range(w):
            if px[x, y][:3] == bg[:3]:
                px[x, y] = (0, 0, 0, 0)
    return rgba


def downscale(img, w, h):
    """Réduit en (w,h) par moyenne de zone (BOX) — propre pour du /8."""
    return img.resize((w, h), Image.Resampling.BOX)
