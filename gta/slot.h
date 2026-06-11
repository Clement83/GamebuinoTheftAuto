#pragma once
#include <stdint.h>

// Logique PURE de la machine a sous du Casino (aucune dependance Gamebuino).
// Testee en natif par tools/host_test/test_slot_host.cpp.
//
// 5 symboles, tirage uniforme par rouleau. Table de gains (3 identiques) +
// "exactement deux cerises". Le multiplicateur s'applique a la mise et
// represente le TOTAL recredite (mise deja deduite a l'amorce du spin) :
// retour moyen ~0,90 (avantage maison ~10 %, pas de farm d'argent infini).

enum SlotSym {
  SYM_CHERRY = 0,
  SYM_LEMON,
  SYM_BELL,
  SYM_BAR,
  SYM_SEVEN,
  SYM_COUNT
};

// Gain BRUT (montant recredite) pour la combinaison (a,b,c) a la mise `bet`.
// 0 = perdu. Voir la table dans le design.
static inline int32_t slotEvaluate(uint8_t a, uint8_t b, uint8_t c, int32_t bet) {
  if (a == b && b == c) {
    switch (a) {
      case SYM_SEVEN:  return bet * 50;   // jackpot
      case SYM_BAR:    return bet * 20;
      case SYM_BELL:   return bet * 10;
      case SYM_LEMON:  return bet * 5;
      case SYM_CHERRY: return bet * 4;
      default:         return 0;
    }
  }
  // exactement deux cerises (pas trois : gere ci-dessus)
  int cherries = (a == SYM_CHERRY) + (b == SYM_CHERRY) + (c == SYM_CHERRY);
  if (cherries == 2) return bet * 2;
  return 0;
}

// RNG xorshift32 minuscule (seedable, deterministe) -> symbole 0..SYM_COUNT-1.
// Garde le tirage hors moteur Gamebuino pour rester testable/portable.
static inline uint8_t slotNext(uint32_t *state) {
  uint32_t x = *state ? *state : 0x1234567u;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  *state = x;
  return (uint8_t)(x % SYM_COUNT);
}
