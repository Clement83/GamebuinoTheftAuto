// Moteur pur (camera/collision) -- portage C++ de tools/engine.py.
// Aucune dependance Gamebuino : compilable et testable en natif (host).
// Source de verite du comportement : doit rester en parite avec engine.py.
#pragma once
#include <stdint.h>
#include "assets.h"
#include "citymap.h"

// Clamp du coin haut-gauche desire de la camera vers [0, world - screen].
// Si le monde tient dans l'ecran, retourne 0. Parite : engine.clamp_camera.
inline int clampCamera(int playerPx, int worldPx, int screenPx) {
  if (worldPx > screenPx) {
    int v = playerPx;
    if (v < 0) v = 0;
    int maxv = worldPx - screenPx;
    if (v > maxv) v = maxv;
    return v;
  }
  return 0;
}

// Tuile solide ou hors-bornes. Parite : engine.is_solid.
inline bool isSolidAt(int tx, int ty) {
  if (tx < 0 || tx >= CITY_W || ty < 0 || ty >= CITY_H) return true;
  uint8_t id = cityMap[ty * CITY_W + tx];
  return (tileFlags[id] & TILE_SOLID) != 0;
}

// L'AABB PLAYER x PLAYER en (px,py) chevauche-t-elle une tuile solide ?
// Decalage arithmetique >>3 == floor-div par 8 (meme pour px<0), parite //TILE.
inline bool boxHitsSolid(int px, int py) {
  int txMin = px >> 3;
  int txMax = (px + PLAYER_W - 1) >> 3;
  int tyMin = py >> 3;
  int tyMax = (py + PLAYER_H - 1) >> 3;
  for (int ty = tyMin; ty <= tyMax; ty++) {
    for (int tx = txMin; tx <= txMax; tx++) {
      if (isSolidAt(tx, ty)) return true;
    }
  }
  return false;
}

// Deplacement avec collision separee par axe (glissement le long des murs).
// Modifie x,y sur place. Parite : engine.try_move.
inline void tryMove(int &x, int &y, int dx, int dy) {
  int nx = x + dx;
  if (!boxHitsSolid(nx, y)) x = nx;
  int ny = y + dy;
  if (!boxHitsSolid(x, ny)) y = ny;
}
