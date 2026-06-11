// Test host de la logique de la machine a sous (slot.h, pur, sans Gamebuino).
// Compile en natif :
//   g++ -std=c++11 -I gta tools/host_test/test_slot_host.cpp -o /tmp/slot_host && /tmp/slot_host
#include <cstdio>
#include <stdint.h>
#include "slot.h"

static int failures = 0;
static void check(const char *what, bool ok) {
  if (!ok) { printf("FAIL %s\n", what); failures++; }
}

int main() {
  // --- Combinaisons gagnantes (3 identiques) ---
  check("777 = x50",        slotEvaluate(SYM_SEVEN, SYM_SEVEN, SYM_SEVEN, 10) == 500);
  check("BAR x3 = x20",     slotEvaluate(SYM_BAR,   SYM_BAR,   SYM_BAR,   10) == 200);
  check("Bell x3 = x10",    slotEvaluate(SYM_BELL,  SYM_BELL,  SYM_BELL,  10) == 100);
  check("Lemon x3 = x5",    slotEvaluate(SYM_LEMON, SYM_LEMON, SYM_LEMON, 10) == 50);
  check("Cherry x3 = x4",   slotEvaluate(SYM_CHERRY,SYM_CHERRY,SYM_CHERRY,10) == 40);

  // --- Exactement deux cerises = x2 (peu importe la position) ---
  check("2 cerises (0,1)",  slotEvaluate(SYM_CHERRY,SYM_CHERRY,SYM_BAR,   10) == 20);
  check("2 cerises (0,2)",  slotEvaluate(SYM_CHERRY,SYM_BAR,   SYM_CHERRY,10) == 20);
  check("2 cerises (1,2)",  slotEvaluate(SYM_BAR,   SYM_CHERRY,SYM_CHERRY,10) == 20);

  // --- Une seule cerise / aucune = perdu ---
  check("1 cerise = 0",     slotEvaluate(SYM_CHERRY,SYM_BAR,   SYM_BELL,  10) == 0);
  check("0 cerise = 0",     slotEvaluate(SYM_BAR,   SYM_BELL,  SYM_SEVEN, 10) == 0);

  // --- 3 cerises prime sur la regle "2 cerises" (40, pas 20) ---
  check("3 cerises != 2c",  slotEvaluate(SYM_CHERRY,SYM_CHERRY,SYM_CHERRY,10) == 40);

  // --- La mise multiplie lineairement ---
  check("mise x1 -> x50",   slotEvaluate(SYM_SEVEN, SYM_SEVEN, SYM_SEVEN, 1) == 50);
  check("mise x7 -> 350",   slotEvaluate(SYM_SEVEN, SYM_SEVEN, SYM_SEVEN, 7) == 350);

  // --- RNG : deterministe, borne, et avance l'etat ---
  {
    uint32_t s = 42;
    uint8_t a = slotNext(&s);
    check("slotNext borne",  a < SYM_COUNT);
    check("slotNext avance",  s != 42);
    // reproductible avec la meme graine
    uint32_t s1 = 99, s2 = 99;
    check("slotNext repro",  slotNext(&s1) == slotNext(&s2));
    // distribution : couvre tous les symboles sur un grand echantillon
    int seen[SYM_COUNT] = {0};
    uint32_t r = 7;
    for (int i = 0; i < 5000; i++) seen[slotNext(&r)]++;
    bool all = true;
    for (int i = 0; i < SYM_COUNT; i++) if (seen[i] == 0) all = false;
    check("slotNext couvre tout", all);
  }

  if (failures == 0) printf("OK slot host (tous les tests passent)\n");
  else               printf("%d test(s) en echec\n", failures);
  return failures ? 1 : 0;
}
