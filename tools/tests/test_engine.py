import pytest
from tools.engine import clamp_camera, is_solid, try_move, TILE, PLAYER

W, H = 5, 5
SOLID = {1}
def _grid():
    g = [0] * (W * H)
    for y in range(H):
        g[y * W + 2] = 1   # colonne 2 solide
    return g


def test_clamp_camera_bounds():
    world, screen = 40, 24
    assert clamp_camera(-5, world, screen) == 0
    assert clamp_camera(100, world, screen) == world - screen
    assert clamp_camera(10, world, screen) == 10
    assert clamp_camera(10, 16, 24) == 0


def test_is_solid_oob_and_tiles():
    g = _grid()
    assert is_solid(g, W, H, SOLID, -1, 0) is True
    assert is_solid(g, W, H, SOLID, 0, 10) is True
    assert is_solid(g, W, H, SOLID, 2, 0) is True
    assert is_solid(g, W, H, SOLID, 0, 0) is False


def test_try_move_blocks_into_solid():
    g = _grid()
    nx, ny = try_move(8, 0, 4, 0, g, W, H, SOLID)
    assert nx == 8 and ny == 0


def test_try_move_slides_along_wall():
    g = _grid()
    nx, ny = try_move(8, 0, 4, 4, g, W, H, SOLID)
    assert nx == 8
    assert ny == 4


def test_try_move_stays_in_bounds():
    g = _grid()
    nx, ny = try_move(0, 0, -4, -4, g, W, H, SOLID)
    assert nx == 0 and ny == 0
