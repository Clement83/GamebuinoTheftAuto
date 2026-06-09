"""Pure Python movement/camera engine (no pygame, no I/O).

Designed for direct C++ port parity (engine.h, M4).
All functions are pure: no side effects, explicit params only.
"""

TILE = 8
PLAYER = 8


def clamp_camera(player_px: int, world_px: int, screen_px: int) -> int:
    """Clamp desired camera top-left coordinate to valid range.

    If world fits in screen, always returns 0.
    Otherwise clamps to [0, world_px - screen_px].
    """
    if world_px > screen_px:
        return max(0, min(player_px, world_px - screen_px))
    return 0


def is_solid(grid, w: int, h: int, solid_set: set, tx: int, ty: int) -> bool:
    """Return True if tile (tx, ty) is solid or out-of-bounds."""
    if tx < 0 or tx >= w or ty < 0 or ty >= h:
        return True
    return grid[ty * w + tx] in solid_set


def _box_hits_solid(px: int, py: int, grid, w: int, h: int, solid_set: set) -> bool:
    """Return True if the PLAYER×PLAYER AABB at (px, py) overlaps any solid tile."""
    tx_min = px // TILE
    tx_max = (px + PLAYER - 1) // TILE
    ty_min = py // TILE
    ty_max = (py + PLAYER - 1) // TILE
    for ty in range(ty_min, ty_max + 1):
        for tx in range(tx_min, tx_max + 1):
            if is_solid(grid, w, h, solid_set, tx, ty):
                return True
    return False


def try_move(x: int, y: int, dx: int, dy: int,
             grid, w: int, h: int, solid_set: set) -> tuple:
    """Axis-separated collision move. Returns (nx, ny).

    Tries x+dx first; if blocked keeps x. Then tries y+dy from resolved x;
    if blocked keeps y. Wall-sliding emerges from axis separation.
    """
    # Try X axis
    nx = x + dx
    if _box_hits_solid(nx, y, grid, w, h, solid_set):
        nx = x

    # Try Y axis from resolved X
    ny = y + dy
    if _box_hits_solid(nx, ny, grid, w, h, solid_set):
        ny = y

    return (nx, ny)
