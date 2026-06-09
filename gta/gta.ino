// GTA demake -- sketch Gamebuino META (M4).
// Pieton qui se deplace a la croix dans la ville scrollante, collision +
// camera clampee. Rendu pixel par pixel (drawPixel) depuis des tableaux
// RGB565 en flash : aucune dependance au format binaire Image de la lib.
//
// Compile-flag DEMO_AUTOWALK : chemin scripte (pour validation sans boutons).
//   arduino-cli compile --fqbn gamebuino:samd:gamebuino_meta_native \
//     --build-property "build.extra_flags=-DDEMO_AUTOWALK" gta/
// Nom de sketch < 4 chars ("gta") : la lib exige FOLDER_NAME (dossier de save SD).
// Doit aussi etre vu par les .cpp de la lib -> passe en build flag (voir ci-dessous).
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

// --- etat perso (position en pixels monde) ---
static int playerX;
static int playerY;
static uint8_t playerDir;
static uint8_t playerFrame;
static uint8_t animTimer;

// estimation RAM libre (debug serie)
extern "C" char *sbrk(int incr);
static int freeRam() {
  char top;
  return &top - reinterpret_cast<char *>(sbrk(0));
}

#ifdef DEMO_AUTOWALK
// Chemin scripte : suite de segments {dx, dy, nb de pas}.
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

void setup() {
  gb.begin();
  SerialUSB.begin(9600);
  playerX = PLAYER_START_X * TILE_W;
  playerY = PLAYER_START_Y * TILE_H;
  playerDir = PLAYER_START_DIR;
  playerFrame = 0;
  animTimer = 0;
}

static void readInput(int &dx, int &dy) {
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
      if (x >= 0 && x < SCREEN_W) {
        gb.display.drawPixel(x, y, (Color)px[rx]);
      }
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
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        gb.display.drawPixel(x, y, (Color)c);
      }
    }
  }
}

void loop() {
  while (!gb.update());

  // 1. Input -> (dx,dy)
  int dx, dy;
  readInput(dx, dy);

  // 2. Collision + orientation + frame de marche
  if (dx != 0 || dy != 0) {
    int ox = playerX, oy = playerY;
    tryMove(playerX, playerY, dx, dy);
    int mx = playerX - ox, my = playerY - oy;
    if (mx != 0) playerDir = (mx > 0) ? DIR_EAST : DIR_WEST;
    else if (my != 0) playerDir = (my > 0) ? DIR_SOUTH : DIR_NORTH;
    if (++animTimer >= ANIM_PERIOD) { animTimer = 0; playerFrame ^= 1; }
  } else {
    playerFrame = 0;
    animTimer = 0;
  }

  // 3. Camera (coin haut-gauche en pixels monde), clampee aux bords
  int camX = clampCamera(playerX + PLAYER_W / 2 - SCREEN_W / 2, WORLD_W, SCREEN_W);
  int camY = clampCamera(playerY + PLAYER_H / 2 - SCREEN_H / 2, WORLD_H, SCREEN_H);

  // 4. Rendu tuiles : 11x9 (10x8 visibles + 1 marge / axe pour scroll sub-tuile)
  int col0 = camX >> 3, offX = camX & 7;
  int row0 = camY >> 3, offY = camY & 7;
  for (int ty = 0; ty < 9; ty++) {
    int wtY = row0 + ty;
    if (wtY < 0 || wtY >= CITY_H) continue;
    for (int tx = 0; tx < 11; tx++) {
      int wtX = col0 + tx;
      if (wtX < 0 || wtX >= CITY_W) continue;
      uint8_t id = cityMap[wtY * CITY_W + wtX];
      drawTile(id, tx * TILE_W - offX, ty * TILE_H - offY);
    }
  }

  // 5. Perso (position ecran)
  drawPlayer(playerX - camX, playerY - camY);

  // 6. Debug serie periodique (~1/s)
  static uint32_t frame = 0;
  if ((frame++ & 31) == 0) {
    SerialUSB.print("tile=("); SerialUSB.print(playerX / TILE_W);
    SerialUSB.print(","); SerialUSB.print(playerY / TILE_H);
    SerialUSB.print(") cam=("); SerialUSB.print(camX);
    SerialUSB.print(","); SerialUSB.print(camY);
    SerialUSB.print(") cpu="); SerialUSB.print(gb.getCpuLoad());
    SerialUSB.print("% ram="); SerialUSB.print(freeRam());
    SerialUSB.println();
  }
}
