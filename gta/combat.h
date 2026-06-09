// Couche combat pure : ciblage du coup de poing a pied.
// Aucune dependance Gamebuino : compilable et testable en natif (host).
// Reutilise les tables de direction de ai.h (ordre N E S W == enum Dir).
#pragma once
#include <stdint.h>
#include <math.h>
#include "ai.h"

static const float PUNCH_REACH = 9.0f;   // px : portee devant le joueur
static const float PUNCH_SIDE  = 5.0f;   // px : tolerance laterale

// Renvoie l'indice du pieton frappe par un coup de poing devant le joueur, ou -1.
// Tableaux paralleles px/py (centre px monde) et active (frappable) de taille n.
// Cible = pieton actif dont la projection avant est dans ]0, PUNCH_REACH] et
// l'ecart lateral < PUNCH_SIDE ; le plus proche devant (plus petit fwd) gagne.
inline int combatPunchTarget(const float *px, const float *py, const bool *active,
                             int n, int playerCx, int playerCy, uint8_t dir) {
  int best = -1;
  float bestFwd = PUNCH_REACH + 1.0f;
  float fdx = AI_DX[dir], fdy = AI_DY[dir];                 // avant
  float rdx = AI_DX[AI_RIGHT[dir]], rdy = AI_DY[AI_RIGHT[dir]]; // lateral (droite)
  for (int i = 0; i < n; i++) {
    if (!active[i]) continue;
    float relx = px[i] - playerCx, rely = py[i] - playerCy;
    float fwd = relx * fdx + rely * fdy;
    float lat = relx * rdx + rely * rdy;
    if (fwd > 0.0f && fwd <= PUNCH_REACH && fabsf(lat) < PUNCH_SIDE && fwd < bestFwd) {
      best = i; bestFwd = fwd;
    }
  }
  return best;
}
