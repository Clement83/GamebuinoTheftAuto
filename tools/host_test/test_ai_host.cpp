// Test de parite host : ai.h (C++) vs ai.py (Python).
// Valeurs de reference produites par ai.py (rng seed 12345, pick_exit seed 1).
// Compile en natif, sans dependance Gamebuino :
//   g++ -std=c++11 -I gta tools/host_test/test_ai_host.cpp -o /tmp/ai_host && /tmp/ai_host
#include <cstdio>
#include <stdint.h>
#include "ai.h"

static int failures = 0;

static void check_u32(const char *what, uint32_t got, uint32_t want) {
  if (got != want) { printf("FAIL %s : got 0x%08X, want 0x%08X\n", what, got, want); failures++; }
}
static void check_int(const char *what, long got, long want) {
  if (got != want) { printf("FAIL %s : got %ld, want %ld\n", what, got, want); failures++; }
}

// croix 5x5 : ligne 2 horizontale, colonne 2 verticale, centre = cross.
static const uint8_t CROSS[25] = {
  0,0,2,0,0,
  0,0,2,0,0,
  1,1,3,1,1,
  0,0,2,0,0,
  0,0,2,0,0,
};

int main() {
  // --- tables de direction ---
  for (int d = 0; d < 4; d++) {
    check_int("right(left)", AI_RIGHT[AI_LEFT[d]], d);
    check_int("back(back)", AI_BACK[AI_BACK[d]], d);
    // dir x right == +1 (right bien a droite, y vers le bas)
    int cr = AI_DX[d] * AI_DY[AI_RIGHT[d]] - AI_DY[d] * AI_DX[AI_RIGHT[d]];
    check_int("cross_prod", cr, 1);
  }

  // --- classification ---
  check_int("drivable cross", aiIsDrivable(CROSS, 5, 5, 2, 2), 1);
  check_int("drivable grass", aiIsDrivable(CROSS, 5, 5, 0, 0), 0);
  check_int("drivable oob", aiIsDrivable(CROSS, 5, 5, -1, 2), 0);

  // --- lane_point : decalage a droite selon le sens (parite test_lane_point) ---
  int px, py;
  aiLanePoint(2, 2, DIR_E, px, py); check_int("lane E x", px, 20); check_int("lane E y", py, 20 + AI_LANE);
  aiLanePoint(2, 2, DIR_W, px, py); check_int("lane W y", py, 20 - AI_LANE);
  aiLanePoint(2, 2, DIR_S, px, py); check_int("lane S x", px, 20 - AI_LANE);
  aiLanePoint(2, 2, DIR_N, px, py); check_int("lane N x", px, 20 + AI_LANE);

  // --- rng : parite exacte avec ai.py (seed 12345) ---
  {
    uint32_t s = 12345;
    check_u32("rng[0]", aiRngNext(s), 0xC6E5747Au);
    check_u32("rng[1]", aiRngNext(s), 0x652A09AFu);
    check_u32("rng[2]", aiRngNext(s), 0xA7E08FA0u);
    check_u32("rng[3]", aiRngNext(s), 0x748E41EAu);
  }

  // --- pick_exit : parite exacte (seed 1, venant Est sur la croix) ---
  {
    uint32_t s = 1;
    const uint8_t want[8] = { 1, 1, 1, 0, 0, 2, 2, 1 };
    for (int i = 0; i < 8; i++) {
      uint8_t nd = aiPickExit(CROSS, 5, 5, 2, 2, DIR_E, aiIsDrivable, s);
      if (nd != want[i]) { printf("FAIL pick_exit[%d] got %d want %d\n", i, nd, want[i]); failures++; }
    }
  }

  // --- invariant : pick_exit ne renvoie jamais une tuile non carrossable ---
  {
    uint32_t s = 1;
    for (int d = 0; d < 4; d++)
      for (int k = 0; k < 50; k++) {
        uint8_t nd = aiPickExit(CROSS, 5, 5, 2, 2, (uint8_t)d, aiIsDrivable, s);
        int nx = 2 + AI_DX[nd], ny = 2 + AI_DY[nd];
        if (!aiIsDrivable(CROSS, 5, 5, nx, ny)) {
          printf("FAIL exit invalide d=%d nd=%d\n", d, nd); failures++; break;
        }
      }
  }

  // --- panique : is_open + panic_dir (parite ai.py) ---
  check_int("open road", aiIsOpen(CROSS, 5, 5, 0, 2), 1);   // route franchie en panique
  check_int("open grass", aiIsOpen(CROSS, 5, 5, 0, 0), 1);
  {
    // plein ouvert 5x5 : fuite alignee sur le vecteur "away".
    static const uint8_t OPEN[25] = {0};
    check_int("panic E", aiPanicDir(OPEN, 5, 5, 2, 2,  1,  0), DIR_E);
    check_int("panic W", aiPanicDir(OPEN, 5, 5, 2, 2, -1,  0), DIR_W);
    check_int("panic S", aiPanicDir(OPEN, 5, 5, 2, 2,  0,  1), DIR_S);
    check_int("panic N", aiPanicDir(OPEN, 5, 5, 2, 2,  0, -1), DIR_N);
  }
  {
    // batiment au centre : fuite est bloquee -> contourne (pas DIR_E).
    static const uint8_t BLK[25] = {
      0,0,0,0,0, 0,0,0,0,0, 0,0,6,0,0, 0,0,0,0,0, 0,0,0,0,0,
    };
    uint8_t nd = aiPanicDir(BLK, 5, 5, 1, 2, 1.0f, 0.0f);
    if (nd == DIR_E) { printf("FAIL panic detour : a traverse le batiment\n"); failures++; }
  }

  if (failures == 0) { printf("OK : parite ai.h <-> ai.py verifiee\n"); return 0; }
  printf("%d echec(s)\n", failures);
  return 1;
}
