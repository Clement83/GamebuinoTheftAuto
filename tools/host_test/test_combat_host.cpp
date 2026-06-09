// Test host de la logique de coup de poing (combat.h, pur, sans Gamebuino).
// Compile en natif :
//   g++ -std=c++11 -I gta tools/host_test/test_combat_host.cpp -o /tmp/combat_host && /tmp/combat_host
#include <cstdio>
#include <stdint.h>
#include "combat.h"

static int failures = 0;
static void check_int(const char *what, long got, long want) {
  if (got != want) { printf("FAIL %s : got %ld, want %ld\n", what, got, want); failures++; }
}

int main() {
  // Joueur au centre (50,50). Directions : NORTH=0 E=1 S=2 W=3 (ordre Dir).
  const int PCX = 50, PCY = 50;

  // --- pieton droit devant (est), a portee -> touche ---
  {
    float px[1] = { 50 + 6.0f }, py[1] = { 50.0f };
    bool act[1] = { true };
    check_int("front east hit", combatPunchTarget(px, py, act, 1, PCX, PCY, 1), 0);
  }
  // --- pieton derriere (joueur regarde est, pieton a l'ouest) -> rien ---
  {
    float px[1] = { 50 - 6.0f }, py[1] = { 50.0f };
    bool act[1] = { true };
    check_int("behind miss", combatPunchTarget(px, py, act, 1, PCX, PCY, 1), -1);
  }
  // --- pieton trop loin devant (> PUNCH_REACH) -> rien ---
  {
    float px[1] = { 50 + (float)PUNCH_REACH + 3.0f }, py[1] = { 50.0f };
    bool act[1] = { true };
    check_int("too far miss", combatPunchTarget(px, py, act, 1, PCX, PCY, 1), -1);
  }
  // --- pieton devant mais trop decale lateralement -> rien ---
  {
    float px[1] = { 50 + 6.0f }, py[1] = { 50 + (float)PUNCH_SIDE + 3.0f };
    bool act[1] = { true };
    check_int("lateral miss", combatPunchTarget(px, py, act, 1, PCX, PCY, 1), -1);
  }
  // --- pieton inactif ignore ---
  {
    float px[1] = { 50 + 6.0f }, py[1] = { 50.0f };
    bool act[1] = { false };
    check_int("inactive ignored", combatPunchTarget(px, py, act, 1, PCX, PCY, 1), -1);
  }
  // --- deux pietons devant : le plus proche est renvoye ---
  {
    float px[2] = { 50 + 8.0f, 50 + 4.0f };
    float py[2] = { 50.0f, 50.0f };
    bool act[2] = { true, true };
    check_int("nearest in front", combatPunchTarget(px, py, act, 2, PCX, PCY, 1), 1);
  }
  // --- direction sud : pieton au sud touche ---
  {
    float px[1] = { 50.0f }, py[1] = { 50 + 6.0f };
    bool act[1] = { true };
    check_int("front south hit", combatPunchTarget(px, py, act, 1, PCX, PCY, 2), 0);
  }

  if (failures == 0) { printf("OK : combat.h verifie\n"); return 0; }
  printf("%d echec(s)\n", failures);
  return 1;
}
