// GTA demake -- sketch Gamebuino META (M4 + voiture).
// Pieton qui se deplace a la croix dans la ville scrollante, collision +
// camera clampee. Une voiture conduisible (physique arcade + drift) est garee
// a cote du spawn : A pour monter/descendre. Rendu pixel par pixel (drawPixel)
// depuis des tableaux RGB565 en flash ; la voiture est un rectangle oriente
// (angle continu) trace a la volee, sans sprite.
//
// A pied  : croix = deplacer, A (pres de la voiture) = monter.
// Au volant : HAUT/BAS = gaz/frein-AR, GAUCHE/DROITE = braquer, B = frein a main
//             (drift !), A = descendre.
//
// Compile-flag DEMO_AUTOWALK : chemin scripte pieton (validation sans boutons).
// FOLDER_NAME : la lib l'exige (nom de sketch < 4 chars) -> aussi en build flag.
#ifndef FOLDER_NAME
#define FOLDER_NAME "GTADEMAKE"
#endif
#include <Gamebuino-Meta.h>
#include "assets.h"
#include "citymap.h"
#include "engine.h"

static const int SCREEN_W = 80;
static const int SCREEN_H = 64;
static const int ANIM_PERIOD = 8;   // frames entre deux images de marche
static const int WORLD_W = CITY_W * TILE_W;
static const int WORLD_H = CITY_H * TILE_H;
static const int ENTER_DIST = 12;   // px : portee pour monter dans la voiture

// --- etat perso (position en pixels monde, coin haut-gauche) ---
static int playerX;
static int playerY;
static uint8_t playerDir;
static uint8_t playerFrame;
static uint8_t animTimer;

// --- etat voiture ---
static CarState car;
static bool driving = false;

// estimation RAM libre (debug serie)
extern "C" char *sbrk(int incr);
static int freeRam() {
  char top;
  return &top - reinterpret_cast<char *>(sbrk(0));
}

#ifdef DEMO_AUTOWALK
struct DemoStep { int dx; int dy; int steps; };
static const DemoStep demoPath[] = {
  { 0, -1, 60 }, { 1, 0, 60 }, { 0, 1, 60 }, { -1, 0, 60 },
  { 1, 1, 40 }, { -1, -1, 40 },
};
static const int DEMO_LEN = sizeof(demoPath) / sizeof(demoPath[0]);
static int demoIdx = 0;
static int demoLeft = demoPath[0].steps;

static void demoInput(int &dx, int &dy) {
  if (demoLeft <= 0) {
    demoIdx = (demoIdx + 1) % DEMO_LEN;
    demoLeft = demoPath[demoIdx].steps;
  }
  dx = demoPath[demoIdx].dx;
  dy = demoPath[demoIdx].dy;
  demoLeft--;
}
#endif

// Cherche une case 8x8 non-solide proche du centre (cx,cy) en spirale.
// Renvoie le coin haut-gauche dans ox,oy. true si trouve.
static bool findFootSpot(int cx, int cy, int &ox, int &oy) {
  for (int r = 0; r <= 6; r++) {
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (r > 0 && abs(dx) != r && abs(dy) != r) continue; // anneau
        int px = cx - PLAYER_W / 2 + dx * TILE_W;
        int py = cy - PLAYER_H / 2 + dy * TILE_H;
        if (!boxHitsSolid(px, py)) { ox = px; oy = py; return true; }
      }
    }
  }
  return false;
}

void setup() {
  gb.begin();
  SerialUSB.begin(9600);
  playerX = PLAYER_START_X * TILE_W;
  playerY = PLAYER_START_Y * TILE_H;
  playerDir = PLAYER_START_DIR;
  playerFrame = 0;
  animTimer = 0;

  // Voiture garee sur une case libre juste a cote du perso.
  int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
  int sx = pcx + 2 * TILE_W, sy = pcy;        // 2 cases a l'est par defaut
  if (carBoxHitsSolid(sx, sy, CAR_HALF)) {
    int ox, oy;
    if (findFootSpot(pcx + 2 * TILE_W, pcy, ox, oy)) { sx = ox + PLAYER_W / 2; sy = oy + PLAYER_H / 2; }
  }
  car.x = sx; car.y = sy; car.angle = 0.0f; car.vx = 0.0f; car.vy = 0.0f;
}

static void readFootInput(int &dx, int &dy) {
#ifdef DEMO_AUTOWALK
  demoInput(dx, dy);
#else
  dx = gb.buttons.repeat(BUTTON_RIGHT, 1) ? 1 : (gb.buttons.repeat(BUTTON_LEFT, 1) ? -1 : 0);
  dy = gb.buttons.repeat(BUTTON_DOWN, 1) ? 1 : (gb.buttons.repeat(BUTTON_UP, 1) ? -1 : 0);
#endif
}

static void drawTile(uint8_t id, int sx, int sy) {
  const uint16_t *px = tileset[id];
  for (int ry = 0; ry < TILE_H; ry++) {
    int y = sy + ry;
    if (y < 0 || y >= SCREEN_H) { px += TILE_W; continue; }
    for (int rx = 0; rx < TILE_W; rx++) {
      int x = sx + rx;
      if (x >= 0 && x < SCREEN_W) gb.display.drawPixel(x, y, (Color)px[rx]);
    }
    px += TILE_W;
  }
}

static void drawPlayer(int sx, int sy) {
  const uint16_t *px = playerSprite[playerDir][playerFrame];
  for (int ry = 0; ry < PLAYER_H; ry++) {
    int y = sy + ry;
    for (int rx = 0; rx < PLAYER_W; rx++) {
      uint16_t c = px[ry * PLAYER_W + rx];
      if (c == PLAYER_TRANSPARENT) continue;
      int x = sx + rx;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) gb.display.drawPixel(x, y, (Color)c);
    }
  }
}

// Voiture : rectangle oriente rempli (avance en jaune = phares), camera soustraite.
static void drawCar(int camX, int camY) {
  float cs = cosf(car.angle), sn = sinf(car.angle);
  const float L = 5.0f, W = 3.0f;   // demi-longueur / demi-largeur
  for (float t = -L; t <= L + 0.01f; t += 1.0f) {
    Color body = (t > L - 1.5f) ? Color::yellow : Color::red;   // museau jaune
    for (float u = -W; u <= W + 0.01f; u += 1.0f) {
      int x = (int)(car.x + t * cs - u * sn) - camX;
      int y = (int)(car.y + t * sn + u * cs) - camY;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) gb.display.drawPixel(x, y, body);
    }
  }
}

void loop() {
  while (!gb.update());

  if (!driving) {
    // --- A PIED ---
    int dx, dy;
    readFootInput(dx, dy);
    if (dx != 0 || dy != 0) {
      int ox = playerX, oy = playerY;
      tryMove(playerX, playerY, dx, dy);
      int mx = playerX - ox, my = playerY - oy;
      if (mx != 0) playerDir = (mx > 0) ? DIR_EAST : DIR_WEST;
      else if (my != 0) playerDir = (my > 0) ? DIR_SOUTH : DIR_NORTH;
      if (++animTimer >= ANIM_PERIOD) { animTimer = 0; playerFrame ^= 1; }
    } else {
      playerFrame = 0; animTimer = 0;
    }
    // Monter dans la voiture si on est a portee.
    if (gb.buttons.pressed(BUTTON_A)) {
      int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
      int ddx = pcx - (int)car.x, ddy = pcy - (int)car.y;
      if (ddx * ddx + ddy * ddy <= ENTER_DIST * ENTER_DIST) driving = true;
    }
  } else {
    // --- AU VOLANT ---
    float throttle = gb.buttons.repeat(BUTTON_UP, 1) ? 1.0f
                   : (gb.buttons.repeat(BUTTON_DOWN, 1) ? -1.0f : 0.0f);
    float steer = gb.buttons.repeat(BUTTON_RIGHT, 1) ? 1.0f
                : (gb.buttons.repeat(BUTTON_LEFT, 1) ? -1.0f : 0.0f);
    bool handbrake = gb.buttons.repeat(BUTTON_B, 1);
    carUpdate(car, throttle, steer, handbrake);
    // Descendre : poser le perso sur une case libre a cote.
    if (gb.buttons.pressed(BUTTON_A)) {
      int ox, oy;
      if (findFootSpot((int)car.x, (int)car.y, ox, oy)) {
        playerX = ox; playerY = oy; driving = false;
        playerFrame = 0; animTimer = 0;
      }
    }
  }

  // Camera centree sur l'entite active.
  int focusX = driving ? (int)car.x : playerX + PLAYER_W / 2;
  int focusY = driving ? (int)car.y : playerY + PLAYER_H / 2;
  int camX = clampCamera(focusX - SCREEN_W / 2, WORLD_W, SCREEN_W);
  int camY = clampCamera(focusY - SCREEN_H / 2, WORLD_H, SCREEN_H);

  // Rendu tuiles : 11x9 (10x8 visibles + 1 marge / axe pour scroll sub-tuile).
  int col0 = camX >> 3, offX = camX & 7;
  int row0 = camY >> 3, offY = camY & 7;
  for (int ty = 0; ty < 9; ty++) {
    int wtY = row0 + ty;
    if (wtY < 0 || wtY >= CITY_H) continue;
    for (int tx = 0; tx < 11; tx++) {
      int wtX = col0 + tx;
      if (wtX < 0 || wtX >= CITY_W) continue;
      drawTile(cityMap[wtY * CITY_W + wtX], tx * TILE_W - offX, ty * TILE_H - offY);
    }
  }

  // Voiture toujours visible ; perso seulement a pied (sinon il est dedans).
  drawCar(camX, camY);
  if (!driving) drawPlayer(playerX - camX, playerY - camY);

  // Debug serie periodique (~1/s).
  static uint32_t frame = 0;
  if ((frame++ & 31) == 0) {
    SerialUSB.print(driving ? "DRIVE" : "FOOT ");
    SerialUSB.print(" car=("); SerialUSB.print((int)car.x);
    SerialUSB.print(","); SerialUSB.print((int)car.y);
    SerialUSB.print(") spd="); SerialUSB.print(sqrtf(car.vx * car.vx + car.vy * car.vy), 2);
    SerialUSB.print(" cpu="); SerialUSB.print(gb.getCpuLoad());
    SerialUSB.print("% ram="); SerialUSB.print(freeRam());
    SerialUSB.println();
  }
}
