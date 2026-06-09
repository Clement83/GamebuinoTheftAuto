// Test host de l'IA sur la VRAIE ville (gta/citymap_data.cpp) : un agent place
// sur une route/trottoir doit y rester indefiniment (invariant cle de l'errance).
//   g++ -std=c++11 -I gta tools/host_test/test_ai_city_host.cpp \
//       gta/citymap_data.cpp gta/assets_data.cpp -o /tmp/ai_city && /tmp/ai_city
#include <cstdio>
#include <stdint.h>
#include "ai.h"
#include "citymap.h"

static int failures = 0;

// Fait errer un agent depuis chaque tuile valide trouvee (jusqu'a NB), nb_pas
// chacune, avec classifieur donne. Verifie qu'il reste toujours sur tuile valide
// et qu'il parcourt effectivement du chemin.
static void wander(const char *label, AiClassify classify, float speed, uint8_t tile) {
  int tested = 0;
  uint32_t rng = 0xBADC0DE;
  for (int t = 0; t < CITY_W * CITY_H && tested < 8; t++) {
    int tx = t % CITY_W, ty = t / CITY_W;
    if (!classify(cityMap, CITY_W, CITY_H, tx, ty)) continue;
    tested++;
    float x, y; uint8_t dir; int gx, gy;
    aiPlace(cityMap, CITY_W, CITY_H, x, y, dir, gx, gy, tx, ty, classify, rng);
    float moved = 0.0f, px = x, py = y;
    for (int i = 0; i < 3000; i++) {
      aiStep(cityMap, CITY_W, CITY_H, x, y, dir, gx, gy, speed, classify, rng);
      int cx = (int)x >> 3, cy = (int)y >> 3;
      if (!classify(cityMap, CITY_W, CITY_H, cx, cy)) {
        printf("FAIL %s : hors zone a (%.1f,%.1f) tuile (%d,%d) depart (%d,%d)\n",
               label, x, y, cx, cy, tx, ty);
        failures++; break;
      }
      moved += (x > px ? x - px : px - x) + (y > py ? y - py : py - y);
      px = x; py = y;
    }
    if (moved < 50.0f) {
      printf("FAIL %s : agent quasi immobile (moved=%.1f) depart (%d,%d)\n",
             label, moved, tx, ty);
      failures++;
    }
  }
  if (tested == 0) { printf("FAIL %s : aucune tuile %d trouvee\n", label, tile); failures++; }
}

int main() {
  wander("voiture/routes", aiIsDrivable, 0.8f, 1);
  wander("pieton/trottoirs", aiIsWalkable, 0.35f, 4);

  if (failures == 0) { printf("OK : errance IA reste sur routes/trottoirs (vraie ville)\n"); return 0; }
  printf("%d echec(s)\n", failures);
  return 1;
}
