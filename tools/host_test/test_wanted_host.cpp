// Test host de la machine d'etat du niveau de recherche (wanted.h, pur).
// Compile en natif :
//   g++ -std=c++11 -I gta tools/host_test/test_wanted_host.cpp -o /tmp/wanted_host && /tmp/wanted_host
#include <cstdio>
#include <stdint.h>
#include "wanted.h"

static int failures = 0;
static void check(const char *what, bool ok) {
  if (!ok) { printf("FAIL %s\n", what); failures++; }
}

static void tick(WantedState &w, int n) { for (int i = 0; i < n; i++) wantedTick(w); }

int main() {
  // --- 3 meurtres rapproches => 1 etoile ---
  {
    WantedState w; wantedReset(w);
    wantedOnKill(w); check("1 kill : pas encore d'etoile", w.level == 0);
    wantedOnKill(w); check("2 kills : pas encore d'etoile", w.level == 0);
    wantedOnKill(w); check("3 kills : 1 etoile", w.level == 1);
  }

  // --- meurtre trop espace (> 20 s) => le streak repart de zero ---
  {
    WantedState w; wantedReset(w);
    wantedOnKill(w);
    tick(w, WANTED_STREAK_WINDOW);          // fenetre expiree
    check("streak expire", w.streak == 0);
    wantedOnKill(w); wantedOnKill(w);        // seulement 2 d'affilee ensuite
    check("pas d'etoile sans 3 d'affilee", w.level == 0);
  }

  // --- une etoile retombe a zero apres 60 s sans crime ---
  {
    WantedState w; wantedReset(w);
    wantedOnKill(w); wantedOnKill(w); wantedOnKill(w);
    check("1 etoile gagnee", w.level == 1);
    tick(w, WANTED_DECAY_FRAMES - 1);
    check("encore recherche juste avant 60 s", w.level == 1);
    tick(w, 1);
    check("plus recherche apres 60 s", w.level == 0);
  }

  // --- montee jusqu'a 5, puis gel (pas de decroissance) ---
  {
    WantedState w; wantedReset(w);
    for (int s = 0; s < WANTED_MAX; s++) {   // 5 etoiles = 15 meurtres rapproches
      for (int k = 0; k < WANTED_STREAK_NEEDED; k++) wantedOnKill(w);
    }
    check("niveau plafonne a 5", w.level == WANTED_MAX);
    tick(w, WANTED_DECAY_FRAMES * 3);        // longue attente
    check("niveau 5 ne redescend pas seul", w.level == WANTED_MAX);
    wantedClear(w);
    check("repeinture remet a zero", w.level == 0);
  }

  // --- tuer un flic : +1 etoile immediate, sans passer par le streak ---
  {
    WantedState w; wantedReset(w);
    wantedOnCopKill(w);
    check("flic tue : 1 etoile direct", w.level == 1);
    check("flic tue : streak intact", w.streak == 0);
    wantedOnCopKill(w);
    check("2e flic : 2 etoiles", w.level == 2);
  }

  // --- tuer un flic au plafond : reste a 5, ne deborde pas ---
  {
    WantedState w; wantedReset(w);
    for (int s = 0; s < WANTED_MAX; s++) wantedOnCopKill(w);
    check("5 flics : plafonne a 5", w.level == WANTED_MAX);
    wantedOnCopKill(w);
    check("6e flic : toujours 5", w.level == WANTED_MAX);
  }

  if (failures == 0) printf("OK test_wanted_host\n");
  return failures ? 1 : 0;
}
