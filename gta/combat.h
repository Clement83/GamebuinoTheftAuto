// Couche combat pure : ciblage du coup de poing a pied.
// Aucune dependance Gamebuino : compilable et testable en natif (host).
// Reutilise les tables de direction de ai.h (ordre N E S W == enum Dir).
#pragma once
#include <stdint.h>
#include <math.h>
#include "ai.h"

static const float PUNCH_REACH = 9.0f;   // px : portee devant le joueur
static const float PUNCH_SIDE  = 5.0f;   // px : tolerance laterale

// (px,py) est-il dans le cone d'attaque devant (playerCx,playerCy) regardant dir ?
// Cone = demi-plan avant, projection avant dans ]0, reach] et |lateral| < side.
// Brique commune au poing et aux armes (portee/largeur propres a chaque arme).
inline bool combatInCone(float px, float py, int playerCx, int playerCy,
                         uint8_t dir, float reach, float side) {
  float fdx = AI_DX[dir], fdy = AI_DY[dir];                 // avant
  float rdx = AI_DX[AI_RIGHT[dir]], rdy = AI_DY[AI_RIGHT[dir]]; // lateral (droite)
  float relx = px - playerCx, rely = py - playerCy;
  float fwd = relx * fdx + rely * fdy;
  float lat = relx * rdx + rely * rdy;
  return fwd > 0.0f && fwd <= reach && fabsf(lat) < side;
}

// Indice du pieton le plus proche devant le joueur dans le cone (reach,side), ou
// -1. Tableaux paralleles px/py (centre px monde) et active (frappable), taille n.
inline int combatConeTarget(const float *px, const float *py, const bool *active,
                            int n, int playerCx, int playerCy, uint8_t dir,
                            float reach, float side) {
  int best = -1;
  float bestFwd = reach + 1.0f;
  float fdx = AI_DX[dir], fdy = AI_DY[dir];
  for (int i = 0; i < n; i++) {
    if (!active[i]) continue;
    if (!combatInCone(px[i], py[i], playerCx, playerCy, dir, reach, side)) continue;
    float fwd = (px[i] - playerCx) * fdx + (py[i] - playerCy) * fdy;
    if (fwd < bestFwd) { best = i; bestFwd = fwd; }
  }
  return best;
}

// Coup de poing : cone court (PUNCH_REACH x PUNCH_SIDE). Conserve pour la parite
// avec l'appelant a pied historique et les tests existants.
inline int combatPunchTarget(const float *px, const float *py, const bool *active,
                             int n, int playerCx, int playerCy, uint8_t dir) {
  return combatConeTarget(px, py, active, n, playerCx, playerCy, dir,
                          PUNCH_REACH, PUNCH_SIDE);
}
