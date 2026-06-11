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

// Marge de collision : la boite du joueur est retrecie de PLAYER_COL_INSET px
// par cote (boite 6x6 pour un sprite 8x8) -> passe plus facilement dans les
// passages etroits, sans toucher au sprite. Parite : engine.COL_INSET.
static const int PLAYER_COL_INSET = 1;

// L'AABB (retrecie) du joueur en (px,py) chevauche-t-elle une tuile solide ?
// Decalage arithmetique >>3 == floor-div par 8 (meme pour px<0), parite //TILE.
inline bool boxHitsSolid(int px, int py) {
  int txMin = (px + PLAYER_COL_INSET) >> 3;
  int txMax = (px + PLAYER_W - 1 - PLAYER_COL_INSET) >> 3;
  int tyMin = (py + PLAYER_COL_INSET) >> 3;
  int tyMax = (py + PLAYER_H - 1 - PLAYER_COL_INSET) >> 3;
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
static const float CAR_ACCEL        = 0.18f;  // poussee moteur (plus de peche)
static const float CAR_DRAG         = 0.96f;  // trainee sur l'avancee
static const float CAR_BRAKE        = 0.80f;  // freinage : decel forte sur l'avancee
static const float CAR_MAX_FWD      = 2.3f;   // vitesse avant max
static const float CAR_MAX_REV      = 1.0f;   // marche arriere max
static const float CAR_STEER        = 0.14f;  // taux de braquage max (rad)
static const float CAR_STEER_REF    = 1.2f;   // vitesse de braquage "plein"
static const float CAR_PIVOT_MIN    = 0.5f;   // autorite de braquage mini sous gaz (pivote contre un mur)
static const float CAR_GRIP         = 0.93f;  // amortissement lateral normal (moins savonneux)
static const float CAR_DRIFT_GRIP   = 0.975f; // drift : peu de grip lateral -> glisse
static const int   CAR_HALF         = 2;      // demi-boite de collision (px) -- plus petite que le sprite
// Seuils d'aide a la conduite (utilises par l'appelant : input -> throttle/brake).
static const float CAR_REVERSE_EPS  = 0.15f;  // sous cette vitesse avant, B = marche arriere
static const float CAR_DRIFT_MIN    = 1.0f;   // vitesse avant mini pour partir en drift

inline bool carBoxHitsSolid(float cx, float cy, int half) {
  int x0 = (int)cx - half, x1 = (int)cx + half;
  int y0 = (int)cy - half, y1 = (int)cy + half;
  for (int ty = (y0 >> 3); ty <= (y1 >> 3); ty++)
    for (int tx = (x0 >> 3); tx <= (x1 >> 3); tx++)
      if (isSolidAt(tx, ty)) return true;
  return false;
}

// Vitesse avant signee (projection de la velocite sur le cap). Permet a
// l'appelant de choisir frein vs marche arriere selon la vitesse reelle.
inline float carForwardSpeed(const CarState &c) {
  return c.vx * cosf(c.angle) + c.vy * sinf(c.angle);
}

// Degage la voiture si sa boite chevauche deja du solide (spawn au ras d'un mur,
// choc qui l'a enfoncee...). Pousse le centre de 1px vers la case libre cardinale
// la plus proche -> en quelques frames la caisse "sort" du decor au lieu de rester
// coincee (ne pouvant que pivoter). Vitesse annulee : pas de lutte avec la collision.
inline void carUnstick(CarState &c) {
  if (!carBoxHitsSolid(c.x, c.y, CAR_HALF)) return;
  for (int r = 1; r <= 16; r++) {
    if (!carBoxHitsSolid(c.x + r, c.y, CAR_HALF)) { c.x += 1.0f; c.vx = c.vy = 0.0f; return; }
    if (!carBoxHitsSolid(c.x - r, c.y, CAR_HALF)) { c.x -= 1.0f; c.vx = c.vy = 0.0f; return; }
    if (!carBoxHitsSolid(c.x, c.y + r, CAR_HALF)) { c.y += 1.0f; c.vx = c.vy = 0.0f; return; }
    if (!carBoxHitsSolid(c.x, c.y - r, CAR_HALF)) { c.y -= 1.0f; c.vx = c.vy = 0.0f; return; }
  }
}

// Avance la voiture d'un pas. throttle/steer dans [-1,1].
//   drift = grip lateral faible (glisse) ; brake = decel forte sur l'avancee.
inline void carUpdate(CarState &c, float throttle, float steer, bool drift, bool brake) {
  carUnstick(c);                               // encastree ? on l'ejecte avant tout
  float cs = cosf(c.angle), sn = sinf(c.angle);

  // Braquage proportionnel a la vitesse avant (et a son signe -> AR inverse).
  float fwd = c.vx * cs + c.vy * sn;
  float k = fwd / CAR_STEER_REF;
  if (k > 1.0f) k = 1.0f; else if (k < -1.0f) k = -1.0f;
  // Sous gaz, garder un minimum d'autorite de braquage : sinon, bloque nez
  // contre un mur (vitesse annulee par la collision), on ne pourrait plus
  // pivoter pour se degager. Le signe suit le sens du gaz (avant / arriere).
  if (throttle != 0.0f) {
    float kmin = throttle > 0.0f ? CAR_PIVOT_MIN : -CAR_PIVOT_MIN;
    if (fabsf(k) < CAR_PIVOT_MIN) k = kmin;
  }
  c.angle += steer * CAR_STEER * k;
  cs = cosf(c.angle); sn = sinf(c.angle);

  // Poussee moteur le long du cap.
  c.vx += throttle * CAR_ACCEL * cs;
  c.vy += throttle * CAR_ACCEL * sn;

  // Decomposition avant / lateral par rapport au nouveau cap.
  fwd = c.vx * cs + c.vy * sn;
  float lat = -c.vx * sn + c.vy * cs;
  fwd *= CAR_DRAG;
  if (brake) fwd *= CAR_BRAKE;                 // freinage : decel franche
  lat *= drift ? CAR_DRIFT_GRIP : CAR_GRIP;
  if (fwd > CAR_MAX_FWD) fwd = CAR_MAX_FWD;
  else if (fwd < -CAR_MAX_REV) fwd = -CAR_MAX_REV;
  c.vx = fwd * cs - lat * sn;
  c.vy = fwd * sn + lat * cs;

  // Deplacement collision separee par axe : on annule la composante bloquee
  // (glisse le long du mur, comme le perso a pied) plutot que de rebondir --
  // un rebond renvoyait dans le mur a chaque frame -> voiture coincee/vibrante.
  float nx = c.x + c.vx;
  if (carBoxHitsSolid(nx, c.y, CAR_HALF)) c.vx = 0.0f; else c.x = nx;
  float ny = c.y + c.vy;
  if (carBoxHitsSolid(c.x, ny, CAR_HALF)) c.vy = 0.0f; else c.y = ny;
}
