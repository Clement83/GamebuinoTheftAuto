"""Interactive pygame city viewer (consumes the tested pure engine).

Not unit-tested. Renders the compiled city, supports scroll/zoom and
collision movement, and overlays the 80x64 Gamebuino device viewport.

NOTE: no pygame.init() at module top-level so importing this module
under pytest collection never crashes.
"""

import sys

import pygame

from tools import build_city, citydsl
from tools.engine import TILE, PLAYER, clamp_camera, try_move

WIN_W, WIN_H = 800, 640

# Gamebuino device screen, in world pixels.
DEV_W, DEV_H = 80, 64

_DIR_LETTERS = "nesw"  # 0=N, 1=E, 2=S, 3=W


class ViewerState:
    def __init__(self, px, py, direction, zoom=4):
        self.px = px
        self.py = py
        self.direction = direction
        self.frame = 0
        self.anim_counter = 0
        self.zoom = zoom


def load_world():
    """Load tiles, city, tile surfaces and player sprites.

    Requires a display surface to already exist (convert/convert_alpha).
    Returns (city, tiles, sprites, names, solid_set).
    """
    tile_index, solid_set, names = build_city.load_tiles("assets/tileset.csv")
    text = open("city/city.txt").read()
    city = citydsl.compile_city(text, tile_index, solid_set)

    tiles = [
        pygame.image.load("assets/tiles8/%s.png" % name).convert()
        for name in names
    ]

    sprites = {}
    for d, letter in enumerate(_DIR_LETTERS):
        sprites[d] = [
            pygame.image.load(
                "assets/sprites8/player_%s_%d.png" % (letter, frame)
            ).convert_alpha()
            for frame in (0, 1)
        ]

    return city, tiles, sprites, names, solid_set


def render_frame(screen, city, tiles, sprites, names, state):
    zoom = state.zoom
    view_w = WIN_W // zoom
    view_h = WIN_H // zoom

    cam_x = clamp_camera(state.px + PLAYER // 2 - view_w // 2, city.w * TILE, view_w)
    cam_y = clamp_camera(state.py + PLAYER // 2 - view_h // 2, city.h * TILE, view_h)

    screen.fill((0, 0, 0))

    tx0 = max(0, cam_x // TILE)
    tx1 = min(city.w - 1, (cam_x + view_w) // TILE)
    ty0 = max(0, cam_y // TILE)
    ty1 = min(city.h - 1, (cam_y + view_h) // TILE)

    tile_size = (TILE * zoom, TILE * zoom)
    for ty in range(ty0, ty1 + 1):
        for tx in range(tx0, tx1 + 1):
            surf = pygame.transform.scale(tiles[city.get(tx, ty)], tile_size)
            screen.blit(surf, ((tx * TILE - cam_x) * zoom, (ty * TILE - cam_y) * zoom))

    player_surf = pygame.transform.scale(
        sprites[state.direction][state.frame], (PLAYER * zoom, PLAYER * zoom)
    )
    screen.blit(player_surf, ((state.px - cam_x) * zoom, (state.py - cam_y) * zoom))

    dev_x = clamp_camera(state.px + PLAYER // 2 - DEV_W // 2, city.w * TILE, DEV_W)
    dev_y = clamp_camera(state.py + PLAYER // 2 - DEV_H // 2, city.h * TILE, DEV_H)
    pygame.draw.rect(
        screen,
        (255, 0, 255),
        pygame.Rect(
            (dev_x - cam_x) * zoom,
            (dev_y - cam_y) * zoom,
            DEV_W * zoom,
            DEV_H * zoom,
        ),
        width=max(1, zoom),
    )


def main(smoke=False):
    pygame.init()
    screen = pygame.display.set_mode((WIN_W, WIN_H))
    pygame.display.set_caption("GTA demake — viewer")
    clock = pygame.time.Clock()

    city, tiles, sprites, names, solid_set = load_world()
    state = ViewerState(
        px=city.spawn[0] * TILE,
        py=city.spawn[1] * TILE,
        direction=city.spawn[2],
        zoom=4,
    )

    if smoke:
        render_frame(screen, city, tiles, sprites, names, state)
        pygame.quit()
        return

    running = True
    while running:
        clock.tick(60)
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key in (pygame.K_PLUS, pygame.K_EQUALS, pygame.K_KP_PLUS):
                    state.zoom = min(16, state.zoom + 1)
                elif event.key in (pygame.K_MINUS, pygame.K_KP_MINUS):
                    state.zoom = max(1, state.zoom - 1)

        keys = pygame.key.get_pressed()
        dx = 1 if keys[pygame.K_RIGHT] else -1 if keys[pygame.K_LEFT] else 0
        dy = 1 if keys[pygame.K_DOWN] else -1 if keys[pygame.K_UP] else 0

        if dx or dy:
            nx, ny = try_move(
                state.px, state.py, dx, dy, city.grid, city.w, city.h, solid_set
            )
            if dx != 0:
                state.direction = 1 if dx > 0 else 3
            elif dy != 0:
                state.direction = 2 if dy > 0 else 0
            state.anim_counter += 1
            if state.anim_counter >= 8:
                state.anim_counter = 0
                state.frame ^= 1
            state.px, state.py = nx, ny
        else:
            state.frame = 0
            state.anim_counter = 0

        render_frame(screen, city, tiles, sprites, names, state)
        pygame.display.flip()

    pygame.quit()


if __name__ == "__main__":
    main(smoke="--smoke" in sys.argv)
