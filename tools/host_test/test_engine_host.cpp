// Test de parite host : engine.h (C++) vs engine.py (Python).
// Valeurs de reference generees par engine.py sur la VRAIE ville compilee
// (city/city.txt). Compile en natif, sans dependance Gamebuino :
//   g++ -std=c++11 -I gta tools/host_test/test_engine_host.cpp \
//       gta/citymap_data.cpp gta/assets_data.cpp -o /tmp/engine_host && /tmp/engine_host
#include <cstdio>
#include <cstdlib>
#include "engine.h"

static int failures = 0;

static void check_int(const char *what, long got, long want) {
  if (got != want) {
    printf("FAIL %s : got %ld, want %ld\n", what, got, want);
    failures++;
  }
}

static void check_move(int x, int y, int dx, int dy, int wantx, int wanty) {
  int nx = x, ny = y;
  tryMove(nx, ny, dx, dy);
  if (nx != wantx || ny != wanty) {
    printf("FAIL move(%d,%d,%d,%d) : got (%d,%d), want (%d,%d)\n",
           x, y, dx, dy, nx, ny, wantx, wanty);
    failures++;
  }
}

int main() {
  // --- clampCamera (parite test_clamp_camera_bounds + cas monde large) ---
  check_int("clamp(-5,40,24)", clampCamera(-5, 40, 24), 0);
  check_int("clamp(100,40,24)", clampCamera(100, 40, 24), 16);
  check_int("clamp(10,40,24)", clampCamera(10, 40, 24), 10);
  check_int("clamp(10,16,24)", clampCamera(10, 16, 24), 0);
  check_int("clamp(500,768,80)", clampCamera(500, 768, 80), 500);

  // --- isSolidAt sur la vraie cityMap (water/building solides, herbe/route non) ---
  check_int("solid(-1,0)", isSolidAt(-1, 0), 1);   // hors-bornes
  check_int("solid(0,-1)", isSolidAt(0, -1), 1);
  check_int("solid(96,0)", isSolidAt(96, 0), 1);
  check_int("solid(0,0)", isSolidAt(0, 0), 1);     // water (id 5)
  check_int("solid(2,0)", isSolidAt(2, 0), 1);
  check_int("solid(47,46)", isSolidAt(47, 46), 0); // spawn, non-solide
  check_int("solid(0,3)", isSolidAt(0, 3), 0);     // route

  // --- tryMove (parite engine.py sur la vraie ville) ---
  check_move(8, 0, 4, 0, 8, 0);        // bloque par water colonne 0..
  check_move(8, 0, 4, 4, 8, 0);
  check_move(0, 0, -4, -4, 0, 0);      // hors-bornes -> bloque
  check_move(376, 368, 1, 0, 377, 368);// spawn px : libre en x
  check_move(376, 368, 0, 1, 376, 368);// bloque en y
  check_move(376, 368, 1, 1, 377, 368);// glisse : x bouge, y bloque

  // --- voiture : invariants physique arcade ---
  // Place la voiture sur le spawn (non-solide), cap est, plein gaz : avance en +x.
  {
    CarState c = { 47.0f * 8 + 4, 46.0f * 8 + 4, 0.0f, 0.0f, 0.0f };
    float x0 = c.x;
    for (int i = 0; i < 20; i++) carUpdate(c, 1.0f, 0.0f, false);
    if (!(c.x > x0 + 5.0f)) { printf("FAIL car accelere : x %.1f -> %.1f\n", x0, c.x); failures++; }
    if (!(c.vx > 0.5f)) { printf("FAIL car vitesse avant trop faible vx=%.2f\n", c.vx); failures++; }
  }
  // Drift : a vitesse, frein a main + braquage -> vitesse laterale non nulle.
  {
    CarState c = { 47.0f * 8 + 4, 46.0f * 8 + 4, 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 15; i++) carUpdate(c, 1.0f, 0.0f, false);   // lance tout droit
    for (int i = 0; i < 8; i++) carUpdate(c, 1.0f, 1.0f, true);     // braque + handbrake
    float cs = cosf(c.angle), sn = sinf(c.angle);
    float lat = -c.vx * sn + c.vy * cs;
    if (!(fabsf(lat) > 0.1f)) { printf("FAIL car ne drifte pas, lat=%.3f\n", lat); failures++; }
  }
  // Collision : depuis le spawn (non-solide), cap ouest plein gaz vers l'eau
  // -> jamais dans une tuile solide (collision bloque/glisse).
  {
    CarState c = { 47.0f * 8 + 4, 46.0f * 8 + 4, (float)M_PI, 0.0f, 0.0f };
    for (int i = 0; i < 80; i++) {
      carUpdate(c, 1.0f, 0.0f, false);
      if (carBoxHitsSolid(c.x, c.y, CAR_HALF)) {
        printf("FAIL car dans le solide a la frame %d x=%.1f y=%.1f\n", i, c.x, c.y);
        failures++; break;
      }
    }
  }

  if (failures == 0) {
    printf("OK : parite engine.h <-> engine.py + physique voiture verifiees\n");
    return 0;
  }
  printf("%d echec(s)\n", failures);
  return 1;
}
