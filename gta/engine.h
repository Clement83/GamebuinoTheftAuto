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

// ----------------------------------------------------------------------------
//  Voiture : physique arcade avec drift (float, pure, host-testable).
//  Modele velocite + cap : la vitesse est decomposee en composante avant (le
//  long du cap) et laterale ; la laterale s'amortit (grip). Frein a main =>
//  grip lateral fort => glisse/drift. Position en pixels monde (centre).
// ----------------------------------------------------------------------------
#include <math.h>

struct CarState {
  float x, y;     // centre, pixels monde
  float angle;    // cap, radians (0 = est, +y = sud comme l'ecran)
  float vx, vy;   // velocite, px/frame (monde)
};

// Reglages (px/frame @ ~25 fps). Ajustables pour le feeling.
static const float CAR_ACCEL        = 0.10f;  // poussee moteur
static const float CAR_DRAG         = 0.96f;  // trainee sur l'avancee
static const float CAR_MAX_FWD      = 2.6f;   // vitesse avant max
static const float CAR_MAX_REV      = 1.0f;   // marche arriere max
static const float CAR_STEER        = 0.14f;  // taux de braquage max (rad)
static const float CAR_STEER_REF    = 1.2f;   // vitesse de braquage "plein"
static const float CAR_GRIP         = 0.85f;  // amortissement lateral normal
static const float CAR_DRIFT_GRIP   = 0.975f; // frein a main : peu de grip -> drift
static const int   CAR_HALF         = 3;      // demi-boite de collision (px)

inline bool carBoxHitsSolid(float cx, float cy, int half) {
  int x0 = (int)cx - half, x1 = (int)cx + half;
  int y0 = (int)cy - half, y1 = (int)cy + half;
  for (int ty = (y0 >> 3); ty <= (y1 >> 3); ty++)
    for (int tx = (x0 >> 3); tx <= (x1 >> 3); tx++)
      if (isSolidAt(tx, ty)) return true;
  return false;
}

// Avance la voiture d'un pas. throttle/steer dans [-1,1], handbrake = frein a main.
inline void carUpdate(CarState &c, float throttle, float steer, bool handbrake) {
  float cs = cosf(c.angle), sn = sinf(c.angle);

  // Braquage proportionnel a la vitesse avant (et a son signe -> AR inverse).
  float fwd = c.vx * cs + c.vy * sn;
  float k = fwd / CAR_STEER_REF;
  if (k > 1.0f) k = 1.0f; else if (k < -1.0f) k = -1.0f;
  c.angle += steer * CAR_STEER * k;
  cs = cosf(c.angle); sn = sinf(c.angle);

  // Poussee moteur le long du cap.
  c.vx += throttle * CAR_ACCEL * cs;
  c.vy += throttle * CAR_ACCEL * sn;

  // Decomposition avant / lateral par rapport au nouveau cap.
  fwd = c.vx * cs + c.vy * sn;
  float lat = -c.vx * sn + c.vy * cs;
  fwd *= CAR_DRAG;
  lat *= handbrake ? CAR_DRIFT_GRIP : CAR_GRIP;
  if (fwd > CAR_MAX_FWD) fwd = CAR_MAX_FWD;
  else if (fwd < -CAR_MAX_REV) fwd = -CAR_MAX_REV;
  c.vx = fwd * cs - lat * sn;
  c.vy = fwd * sn + lat * cs;

  // Deplacement collision separee par axe (petit rebond amorti sur mur).
  float nx = c.x + c.vx;
  if (carBoxHitsSolid(nx, c.y, CAR_HALF)) c.vx *= -0.25f; else c.x = nx;
  float ny = c.y + c.vy;
  if (carBoxHitsSolid(c.x, ny, CAR_HALF)) c.vy *= -0.25f; else c.y = ny;
}
