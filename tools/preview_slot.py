#!/usr/bin/env python3
"""Preview de la machine a sous du casino (ecran Gamebuino 80x64).

Rend l'ecran complet en PNG agrandi dans previews/ pour juger l'apparence
AVANT de flasher. Les motifs de symboles ici sont la SOURCE et sont portes
tels quels dans gta.ino (drawCasino). NE PAS diverger.
"""
from PIL import Image

W, H = 80, 64
SCALE = 8

# --- palette par lettre (RGB888 ; rendue depuis RGB565-friendly). ' ' = transparent.
PAL = {
    'k': (0, 0, 0),        # contour noir
    'r': (224, 32, 32),    # rouge vif (cerise / sept)
    'd': (140, 12, 12),    # rouge sombre (ombre)
    'w': (255, 255, 255),  # reflet blanc
    'y': (255, 224, 0),    # jaune (citron / or)
    'o': (255, 152, 0),    # orange (ombre citron / cloche)
    'g': (40, 192, 48),    # vert (tige)
    'G': (16, 112, 24),    # vert sombre
    's': (210, 210, 224),  # argent clair (cloche/bar)
    'S': (120, 120, 140),  # argent sombre
    'b': (96, 64, 24),     # brun (battant cloche / contour bar)
    'p': (40, 48, 96),     # bleu nuit (fond rouleau)
}

# Symboles 11x11 (centres dans une case rouleau de 13x13).
CHERRY = [
    "      gg   ",
    "     g G   ",
    "    g  G   ",
    "   g   G   ",
    "  g    GG  ",
    " dwd  d  G ",
    "drrwd dwd  ",
    "drrrd drrwd",
    "drrrd drrrd",
    " ddd  drrrd",
    "       ddd ",
]
LEMON = [
    "           ",
    "    ooo    ",
    "  ooyyyo   ",
    " oyyyyyyo  ",
    " oyyywyyo  ",
    "oyyywyyyyo ",
    "oyyyyyyyyo ",
    " oyyyyyyo  ",
    " oyyyyyyo  ",
    "  ooyyoo   ",
    "    ooo    ",
]
BELL = [
    "     k     ",
    "    kyk    ",
    "    yyy    ",
    "   yyyyk   ",
    "  yyyyyok  ",
    "  ysyyyok  ",
    " ysyyyyook ",
    " ysyyyyook ",
    "kyyyyyyyook",
    "kkkkbbkkkk ",
    "    kbbk   ",
]
BAR = [
    " sSSSSSSSs ",
    " swwwwwwws ",
    " sSSSSSSSs ",
    "           ",
    " sSSSSSSSs ",
    " swwwwwwws ",
    " sSSSSSSSs ",
    "           ",
    " sSSSSSSSs ",
    " swwwwwwws ",
    " sSSSSSSSs ",
]
SEVEN = [
    "           ",
    " rrrrrrrrr ",
    " rwwwwwddr ",
    " ddddddrrr ",
    "      drr  ",
    "     drr   ",
    "     rr    ",
    "    drr    ",
    "    rr     ",
    "   rr      ",
    "           ",
]
SYMS = [CHERRY, LEMON, BELL, BAR, SEVEN]
SYM_NAMES = ["CHERRY", "LEMON", "BELL", "BAR", "SEVEN"]


def putpx(img, x, y, rgb):
    if 0 <= x < W and 0 <= y < H:
        img.putpixel((x, y), rgb)


def blit(img, sx, sy, rows):
    # halo noir 1px puis remplissage couleur (comme blitIcon5 mais multicolore)
    for ry, row in enumerate(rows):
        for rx, ch in enumerate(row):
            if ch == ' ':
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                putpx(img, sx + rx + dx, sy + ry + dy, (0, 0, 0))
    for ry, row in enumerate(rows):
        for rx, ch in enumerate(row):
            if ch == ' ':
                continue
            putpx(img, sx + rx, sy + ry, PAL.get(ch, (255, 0, 255)))


def fillrect(img, x, y, w, h, rgb):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            putpx(img, xx, yy, rgb)


def rect(img, x, y, w, h, rgb):
    for xx in range(x, x + w):
        putpx(img, xx, y, rgb)
        putpx(img, xx, y + h - 1, rgb)
    for yy in range(y, y + h):
        putpx(img, x, yy, rgb)
        putpx(img, x + w - 1, yy, rgb)


def render(reels):
    """reels : liste de 3 index de symboles a afficher dans les 3 rouleaux."""
    img = Image.new("RGB", (W, H), (24, 0, 8))  # corps maroon tres sombre

    GOLD = (212, 168, 40)
    GOLD_D = (140, 104, 16)
    REELBG = (16, 16, 40)

    # marquee haut : bandeau dore
    fillrect(img, 0, 0, W, 7, GOLD_D)
    fillrect(img, 0, 0, W, 6, GOLD)
    fillrect(img, 0, 6, W, 1, (255, 224, 96))

    # fenetre des rouleaux : cadre dore en relief
    fx, fy, fw, fh = 4, 16, 72, 19
    fillrect(img, fx, fy, fw, fh, GOLD_D)
    rect(img, fx, fy, fw, fh, (255, 224, 96))
    # interieur des 3 rouleaux
    for r in range(3):
        rx = fx + 3 + r * 23
        fillrect(img, rx, fy + 2, 19, fh - 4, REELBG)
        rect(img, rx, fy + 2, 19, fh - 4, GOLD_D)
        blit(img, rx + 4, fy + 4, SYMS[reels[r]])
    # ligne de paiement (rouge translucide simule)
    fillrect(img, fx + 1, fy + fh // 2, fw - 2, 1, (200, 40, 40))

    # boutons lateraux deco (rivets dores)
    for yy in (20, 26, 31):
        putpx(img, 1, yy, GOLD); putpx(img, W - 2, yy, GOLD)

    return img


def main():
    import os
    os.makedirs("previews", exist_ok=True)
    # planche 1 : jackpot 777
    img = render([4, 4, 4])
    img.resize((W * SCALE, H * SCALE), Image.NEAREST).save("previews/slot_jackpot.png")
    # planche 2 : mix
    img = render([0, 2, 3])
    img.resize((W * SCALE, H * SCALE), Image.NEAREST).save("previews/slot_mix.png")
    # planche 3 : tous les symboles cote a cote (zoom)
    board = Image.new("RGB", (11 * 5 + 4 * 5 + 4, 18), (16, 16, 40))
    for i, s in enumerate(SYMS):
        blit(board, 2 + i * 15, 3, s)
    board.resize((board.width * 10, board.height * 10), Image.NEAREST).save("previews/slot_symbols.png")
    print("ecrit previews/slot_jackpot.png, slot_mix.png, slot_symbols.png")


if __name__ == "__main__":
    main()
