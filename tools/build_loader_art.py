#!/usr/bin/env python3
"""Regenere les visuels du loader Gamebuino META a partir d'une grande image :

  GTADEMAKE/ICON.BMP        32x32
  GTADEMAKE/TITLESCREEN.BMP 80x64

Les deux fichiers doivent rester des BMP 32 bits a en-tete BITMAPV5HEADER (le
loader META lit ce format precis). Plutot que de reconstruire l'en-tete, on
REUTILISE tel quel l'en-tete des fichiers existants (138 octets : 14 + 124) et
on ne remplace que le tableau de pixels -> type/format/taille garantis
identiques. Les dimensions cibles sont lues dans l'en-tete (biWidth/biHeight).

L'image source est recadree facon "cover" (centre) au ratio cible puis
redimensionnee, pour remplir sans deformer.

Usage :  .venv/bin/python tools/build_loader_art.py [source.png]
         (defaut : assets/marseille_BB.png)

TITLESCREEN = cover plein de l'image. ICON = zoom carre sur la tete du heros
(boite ICON_CROP, calee pour assets/marseille_BB.png ; a reajuster si on change
de source).
"""
import struct
import sys
from PIL import Image

SRC_DEFAULT = "assets/marseille_BB.png"
HDR_LEN = 138   # BITMAPFILEHEADER(14) + BITMAPV5HEADER(124)

# (chemin, boite de crop source ou None=cover plein)
#   ICON : carre sur la tete du heros dans assets/marseille_BB.png (1264x842).
ICON_CROP = (812, 125, 1022, 335)
TARGETS = [
    ("GTADEMAKE/ICON.BMP", ICON_CROP),
    ("GTADEMAKE/TITLESCREEN.BMP", None),
]


def cover(im, w, h):
    """Recadre au ratio (w/h) en centrant, puis redimensionne en (w, h)."""
    sw, sh = im.size
    if sw * h > sh * w:                 # source trop large -> rogne les cotes
        nw = sh * w // h
        x = (sw - nw) // 2
        im = im.crop((x, 0, x + nw, sh))
    else:                               # source trop haute -> rogne haut/bas
        nh = sw * h // w
        y = (sh - nh) // 2
        im = im.crop((0, y, sw, y + nh))
    return im.resize((w, h), Image.LANCZOS)


def regen(src_im, path, crop):
    with open(path, "rb") as f:
        data = bytearray(f.read())
    header = bytes(data[:HDR_LEN])
    # biWidth @14+4=18, biHeight @14+8=22 (int32 LE). Hauteur >0 = bottom-up.
    w = struct.unpack_from("<i", header, 18)[0]
    h = struct.unpack_from("<i", header, 22)[0]
    bottom_up = h > 0
    h = abs(h)

    base = src_im.crop(crop) if crop else src_im
    im = cover(base, w, h).convert("RGBA")
    px = im.load()

    out = bytearray(header)
    rows = range(h - 1, -1, -1) if bottom_up else range(h)
    for y in rows:
        for x in range(w):
            r, g, b, a = px[x, y]
            out += bytes((b, g, r, 255))   # BGRA, opaque
    with open(path, "wb") as f:
        f.write(out)
    print("genere: %s (%dx%d, %d octets)" % (path, w, h, len(out)))


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAULT
    src_im = Image.open(src).convert("RGBA")
    for path, crop in TARGETS:
        regen(src_im, path, crop)


if __name__ == "__main__":
    main()
