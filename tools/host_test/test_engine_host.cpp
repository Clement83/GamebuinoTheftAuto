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

  // --- isSolidAt : hors-bornes toujours solide ---
  check_int("solid(-1,0)", isSolidAt(-1, 0), 1);   // hors-bornes
  check_int("solid(0,-1)", isSolidAt(0, -1), 1);
  check_int("solid(CITY_W,0)", isSolidAt(CITY_W, 0), 1);
  check_int("solid(0,CITY_H)", isSolidAt(0, CITY_H), 1);

  // --- ancre carrossable trouvee au runtime (agnostique a la map) ---
  // Tuile non-solide avec clearance N/S et le plus long degagement vers l'est :
  // point de depart des invariants de physique voiture, quelle que soit la
  // ville generee (Liberty City Perlin OU Marseille).
  int ATX = -1, ATY = -1, bestRun = -1;
  for (int ty = 2; ty < CITY_H - 2; ty++) {
    int tx = 2;
    while (tx < CITY_W - 2) {
      if (isSolidAt(tx, ty) || isSolidAt(tx, ty - 1) || isSolidAt(tx, ty + 1)) { tx++; continue; }
      int k = tx, run = 0;
      while (k < CITY_W - 2 && !isSolidAt(k, ty)) { run++; k++; }
      if (run > bestRun) { bestRun = run; ATX = tx; ATY = ty; }
      tx = k + 1;
    }
  }
  if (ATX < 0 || bestRun < 26) {
    printf("FAIL ancre carrossable introuvable (run=%d)\n", bestRun); failures++;
    printf("%d echec(s)\n", failures); return 1;
  }
  const float AX = ATX * 8.0f + 4.0f, AY = ATY * 8.0f + 4.0f;
  check_int("ancre non-solide", isSolidAt(ATX, ATY), 0);

  // tuile solide (eau/batiment) ayant une voisine OUEST non-solide : sert aux
  // tests tryMove "bloque" et a la collision laterale.
  int STX = -1, STY = -1;
  for (int ty = 1; ty < CITY_H - 1 && STX < 0; ty++)
    for (int tx = 2; tx < CITY_W - 1; tx++)
      if (isSolidAt(tx, ty) && !isSolidAt(tx - 1, ty)) { STX = tx; STY = ty; break; }
  if (STX < 0) { printf("FAIL aucune tuile solide a voisine libre\n"); failures++; }

  // --- tryMove (parite engine.py) ---
  check_move(0, 0, -4, -4, 0, 0);                          // hors-bornes -> bloque
  check_move((int)AX, (int)AY, 1, 0, (int)AX + 1, (int)AY); // degage : libre en x
  if (STX >= 0) {
    int nx = (STX - 1) * 8 + 4, ny = STY * 8 + 4;          // case libre ouest du mur
    tryMove(nx, ny, 8, 0);                                 // pousse vers le mur (est)
    if (nx >= STX * 8) { printf("FAIL tryMove non bloque par le mur (nx=%d, mur=%d)\n", nx, STX * 8); failures++; }
  }

  // --- voiture : invariants physique arcade ---
  // Place la voiture sur le spawn (non-solide), cap est, plein gaz : avance en +x.
  {
    CarState c = { AX, AY, 0.0f, 0.0f, 0.0f };
    float x0 = c.x;
    for (int i = 0; i < 20; i++) carUpdate(c, 1.0f, 0.0f, false, false);
    if (!(c.x > x0 + 5.0f)) { printf("FAIL car accelere : x %.1f -> %.1f\n", x0, c.x); failures++; }
    if (!(c.vx > 0.5f)) { printf("FAIL car vitesse avant trop faible vx=%.2f\n", c.vx); failures++; }
  }
  // Pointe : plein gaz vers l'est (route degagee ~20 tuiles) -> proche CAR_MAX_FWD.
  {
    CarState c = { AX, AY, 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 45; i++) carUpdate(c, 1.0f, 0.0f, false, false);
    if (carBoxHitsSolid(c.x, c.y, CAR_HALF)) { printf("FAIL pointe : touche un mur\n"); failures++; }
    if (!(carForwardSpeed(c) > CAR_MAX_FWD - 0.3f)) {
      printf("FAIL car pointe trop basse fwd=%.2f (max %.2f)\n", carForwardSpeed(c), CAR_MAX_FWD); failures++; }
  }
  // Drift : a vitesse, drift + braquage -> vitesse laterale non nulle.
  {
    CarState c = { AX, AY, 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 15; i++) carUpdate(c, 1.0f, 0.0f, false, false); // lance tout droit
    for (int i = 0; i < 8; i++) carUpdate(c, 0.0f, 1.0f, true, true);    // braque + drift(frein)
    float cs = cosf(c.angle), sn = sinf(c.angle);
    float lat = -c.vx * sn + c.vy * cs;
    if (!(fabsf(lat) > 0.1f)) { printf("FAIL car ne drifte pas, lat=%.3f\n", lat); failures++; }
  }
  // Frein : a vitesse, freiner reduit la vitesse avant plus vite que la trainee seule.
  {
    CarState a = { AX, AY, 0.0f, 0.0f, 0.0f };
    CarState b = a;
    for (int i = 0; i < 15; i++) { carUpdate(a, 1.0f, 0.0f, false, false); carUpdate(b, 1.0f, 0.0f, false, false); }
    carUpdate(a, 0.0f, 0.0f, false, false);   // roue libre (drag seul)
    carUpdate(b, 0.0f, 0.0f, false, true);    // frein
    if (!(carForwardSpeed(b) < carForwardSpeed(a))) {
      printf("FAIL frein n'aide pas : libre=%.2f frein=%.2f\n", carForwardSpeed(a), carForwardSpeed(b)); failures++; }
  }
  // Marche arriere : cap ouest, throttle -1 -> recule vers l'est (route degagee),
  // vitesse avant negative et bornee a -CAR_MAX_REV.
  {
    CarState c = { AX, AY, (float)M_PI, 0.0f, 0.0f };
    for (int i = 0; i < 30; i++) carUpdate(c, -1.0f, 0.0f, false, false);
    float fwd = carForwardSpeed(c);
    if (!(fwd < -0.3f)) { printf("FAIL pas de marche arriere fwd=%.2f\n", fwd); failures++; }
    if (!(fwd >= -CAR_MAX_REV - 0.05f)) { printf("FAIL marche arriere non bornee fwd=%.2f\n", fwd); failures++; }
  }
  // Pivot contre un mur : nez bloque (cap ouest, mur a tx45-46), gaz + braquage
  // -> le cap doit changer (sinon on reste coince sans pouvoir se degager).
  {
    CarState c = { AX, AY, (float)M_PI, 0.0f, 0.0f };
    for (int i = 0; i < 10; i++) carUpdate(c, 1.0f, 1.0f, false, false); // se plaque au mur
    float a0 = c.angle;
    for (int i = 0; i < 10; i++) carUpdate(c, 1.0f, 1.0f, false, false); // gaz + braque
    if (!(fabsf(c.angle - a0) > 0.1f)) {
      printf("FAIL pivot mur : cap fige a=%.3f (delta=%.3f)\n", c.angle, c.angle - a0); failures++; }
  }
  // Collision : depuis le spawn (non-solide), cap ouest plein gaz vers l'eau
  // -> jamais dans une tuile solide (collision bloque/glisse), et a l'impact la
  // composante bloquee est annulee (jamais de rebond vers l'arriere : vx >= 0).
  {
    CarState c = { AX, AY, (float)M_PI, 0.0f, 0.0f };
    for (int i = 0; i < 80; i++) {
      carUpdate(c, 1.0f, 0.0f, false, false);
      if (carBoxHitsSolid(c.x, c.y, CAR_HALF)) {
        printf("FAIL car dans le solide a la frame %d x=%.1f y=%.1f\n", i, c.x, c.y);
        failures++; break;
      }
      if (c.vx > 0.01f) {   // cap ouest : la poussee est en -x ; un vx>0 = rebond
        printf("FAIL collision rebondit (vx=%.2f) au lieu de glisser\n", c.vx);
        failures++; break;
      }
    }
  }

  // carUnstick : une voiture encastree dans le solide se degage en quelques
  // frames (sinon, coincee au bord d'un mur, on ne pourrait que pivoter).
  {
    int stx = -1, sty = -1;                 // tuile solide ayant une voisine libre
    for (int ty = 1; ty < CITY_H - 1 && stx < 0; ty++)
      for (int tx = 1; tx < CITY_W - 1; tx++) {
        if (!isSolidAt(tx, ty)) continue;
        if (!isSolidAt(tx + 1, ty) || !isSolidAt(tx - 1, ty) ||
            !isSolidAt(tx, ty + 1) || !isSolidAt(tx, ty - 1)) { stx = tx; sty = ty; break; }
      }
    if (stx < 0) { printf("FAIL carUnstick : aucune tuile solide de bord\n"); failures++; }
    else {
      CarState c = { (float)(stx * 8 + 4), (float)(sty * 8 + 4), 0.0f, 0.0f, 0.0f };
      if (!carBoxHitsSolid(c.x, c.y, CAR_HALF)) { printf("FAIL carUnstick setup : pas encastree\n"); failures++; }
      int n = 0;
      for (; n < 80 && carBoxHitsSolid(c.x, c.y, CAR_HALF); n++) carUpdate(c, 0.0f, 0.0f, false, false);
      if (carBoxHitsSolid(c.x, c.y, CAR_HALF)) {
        printf("FAIL carUnstick : encore encastree apres %d frames (%.1f,%.1f)\n", n, c.x, c.y); failures++; }
    }
  }

  if (failures == 0) {
    printf("OK : parite engine.h <-> engine.py + physique voiture verifiees\n");
    return 0;
  }
  printf("%d echec(s)\n", failures);
  return 1;
}
