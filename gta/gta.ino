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
#include <string.h>
#include "assets.h"
#include "citymap.h"
#include "engine.h"
#include "car.h"
#include "player.h"
#include "ai.h"

// Pointeur direct sur le framebuffer RGB565 80x64 (gb.display._buffer).
// On ecrit dedans sans passer par drawPixel (pas d'appel virtuel ni de
// bounds-check par pixel) -> rendu nettement plus rapide. fb[y*80+x].
static uint16_t *fb = nullptr;

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

// ---------------------------------------------------------------------------
//  Trafic IA : voitures (routes) + pietons (trottoirs) errants, recolores.
//  Logique de deplacement = couche pure ai.h (errance sur grille + voies).
//  Glue device : pool statique, spawn/recyclage autour de la camera (culling),
//  blit recolore (KEY -> couleur entite), collisions avec le joueur.
// ---------------------------------------------------------------------------
static const int NUM_AI_CARS = 6;
static const int NUM_AI_PEDS = 6;
static const float AI_CAR_SPEED = 0.8f;   // px/frame (plus lent que le joueur)
static const float AI_PED_SPEED = 0.35f;
static const int AI_PED_ANIM = 10;        // frames entre deux images de marche
static const int RECYCLE_DIST = 80;       // px du centre camera -> recyclage
static const int RING_MIN = 44;           // anneau de (re)spawn : juste hors ecran
static const int RING_MAX = 72;
static const int PED_DOWN_FRAMES = 70;     // duree au sol avant recyclage
static const int RUNOVER_SPEED2 = 1;       // vitesse^2 mini pour renverser

// Frame de rotation voiture par direction cardinale (N E S W).
// Angle 0 = est -> frame 0 ; +pi/2 = sud -> 6 ; pi = ouest -> 12 ; -pi/2 = nord -> 18.
static const uint8_t AI_CAR_FRAME[4] = { 18, 0, 6, 12 };

// Petite palette de teintes franches (RGB565) tiree au spawn.
// (eviter 0xF81F = couleur transparente des sprites.)
static const uint16_t AI_PALETTE[] = {
  0x001F,  // bleu
  0xFFE0,  // jaune
  0x07FF,  // cyan
  0xFD20,  // orange
  0xAFE5,  // vert clair
  0xFFFF,  // blanc
  0x8410,  // gris
  0x780F,  // violet
  0xC618,  // argent
  0x051F,  // bleu ciel
  0xFC18,  // rose
  0x2DE0,  // vert
};
static const int AI_PALETTE_N = sizeof(AI_PALETTE) / sizeof(AI_PALETTE[0]);

// Teintes fixes du joueur (sprites partages, recolores comme les IA).
static const uint16_t PLAYER_BODY_COLOR = 0xC800;  // rouge (perso a pied)
static const uint16_t PLAYER_CAR_COLOR  = 0xC800;  // rouge (voiture du joueur)

struct AiCar {
  float x, y;          // centre, px monde
  uint8_t dir;         // 0..3 (N E S W)
  int tgtx, tgty;      // point-cible (lane point de la tuile destination)
  uint16_t color;
  bool active;
};

struct AiPed {
  float x, y;
  uint8_t dir;
  int tgtx, tgty;
  uint16_t color;
  uint8_t frame, animTimer;
  uint8_t state;       // 0 = marche, 1 = au sol (renverse)
  uint16_t downTimer;
  bool active;
};

static AiCar aiCars[NUM_AI_CARS];
static AiPed aiPeds[NUM_AI_PEDS];
static uint32_t aiRng = 0xC0FFEEu;   // graine PRNG partagee (xorshift32)

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

  // Pools IA vides : remplis au premier tour autour du point de vue (aiUpdate).
  for (int i = 0; i < NUM_AI_CARS; i++) aiCars[i].active = false;
  for (int i = 0; i < NUM_AI_PEDS; i++) { aiPeds[i].active = false; aiPeds[i].state = 0; }
}

static void readFootInput(int &dx, int &dy) {
#ifdef DEMO_AUTOWALK
  demoInput(dx, dy);
#else
  dx = gb.buttons.repeat(BUTTON_RIGHT, 1) ? 1 : (gb.buttons.repeat(BUTTON_LEFT, 1) ? -1 : 0);
  dy = gb.buttons.repeat(BUTTON_DOWN, 1) ? 1 : (gb.buttons.repeat(BUTTON_UP, 1) ? -1 : 0);
#endif
}

// Tuile 8x8 : copie ligne par ligne (memcpy) directement dans le framebuffer,
// avec clipping aux bords de l'ecran. Source contigue de 8 px -> 16 octets/ligne.
static void drawTile(uint8_t id, int sx, int sy) {
  int rx0 = 0, rx1 = TILE_W;
  if (sx < 0) rx0 = -sx;
  if (sx + TILE_W > SCREEN_W) rx1 = SCREEN_W - sx;
  if (rx0 >= rx1) return;
  int ry0 = 0, ry1 = TILE_H;
  if (sy < 0) ry0 = -sy;
  if (sy + TILE_H > SCREEN_H) ry1 = SCREEN_H - sy;
  const uint16_t *src = tileset[id];
  int w2 = (rx1 - rx0) * 2;
  for (int ry = ry0; ry < ry1; ry++) {
    memcpy(fb + (sy + ry) * SCREEN_W + (sx + rx0),
           src + ry * TILE_W + rx0, w2);
  }
}

// Blit recolore d'un sprite voiture : frame de rotation frameIdx, centre en
// (worldCx,worldCy) px monde. KEY de carrosserie -> color. Framebuffer direct.
static void blitCar(int camX, int camY, int worldCx, int worldCy,
                    int frameIdx, uint16_t color) {
  const uint16_t *src = carFrames[frameIdx];
  int ox = worldCx - camX - CAR_BOX / 2;
  int oy = worldCy - camY - CAR_BOX / 2;
  for (int ry = 0; ry < CAR_BOX; ry++) {
    int y = oy + ry;
    if (y < 0 || y >= SCREEN_H) continue;
    uint16_t *row = fb + y * SCREEN_W;
    const uint16_t *srow = src + ry * CAR_BOX;
    for (int rx = 0; rx < CAR_BOX; rx++) {
      uint16_t c = srow[rx];
      if (c == CAR_TRANSPARENT) continue;
      if (c == CAR_BODY_KEY) c = color;
      int x = ox + rx;
      if (x >= 0 && x < SCREEN_W) row[x] = c;
    }
  }
}

// Blit recolore d'un piéton : direction dir, image de marche frame, centre en
// (worldCx,worldCy). KEY de t-shirt -> color. Framebuffer direct.
static void blitPed(int camX, int camY, int worldCx, int worldCy,
                    uint8_t dir, uint8_t frame, uint16_t color) {
  const uint16_t *src = playerFrames[dir][frame];
  int ox = worldCx - camX - PLAYER_BOX / 2;
  int oy = worldCy - camY - PLAYER_BOX / 2;
  for (int ry = 0; ry < PLAYER_BOX; ry++) {
    int y = oy + ry;
    if (y < 0 || y >= SCREEN_H) continue;
    uint16_t *row = fb + y * SCREEN_W;
    const uint16_t *srow = src + ry * PLAYER_BOX;
    for (int rx = 0; rx < PLAYER_BOX; rx++) {
      uint16_t c = srow[rx];
      if (c == PLAYER_TRANSP) continue;
      if (c == PLAYER_SHIRT_KEY) c = color;
      int x = ox + rx;
      if (x >= 0 && x < SCREEN_W) row[x] = c;
    }
  }
}

// Petit "splat" (piéton renverse) : quelques pixels sombres au sol.
static void blitSplat(int camX, int camY, int worldCx, int worldCy) {
  int sx = worldCx - camX, sy = worldCy - camY;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      int x = sx + dx, y = sy + dy;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        fb[y * SCREEN_W + x] = 0x6000;   // rouge tres sombre
    }
}

// Piéton joueur : sprite recolore en teinte fixe (centre sur la boite PLAYER).
static void drawPlayer(int camX, int camY) {
  blitPed(camX, camY, playerX + PLAYER_W / 2, playerY + PLAYER_H / 2,
          playerDir, playerFrame, PLAYER_BODY_COLOR);
}

// Voiture joueur : frame de rotation la plus proche de l'angle continu.
static void drawCar(int camX, int camY) {
  float a = car.angle;
  int idx = (int)(a / TWO_PI * CAR_FRAMES + 0.5f);
  idx %= CAR_FRAMES;
  if (idx < 0) idx += CAR_FRAMES;
  blitCar(camX, camY, (int)car.x, (int)car.y, idx, PLAYER_CAR_COLOR);
}

// Cherche une tuile valide dans l'anneau [minR,maxR] px autour de (ccx,ccy).
static bool aiFindTileInRing(int ccx, int ccy, int minR, int maxR,
                             AiClassify classify, int &otx, int &oty) {
  for (int i = 0; i < 60; i++) {
    uint32_t r = aiRngNext(aiRng);
    int dx = (int)(r % (2 * maxR + 1)) - maxR;
    int dy = (int)((r >> 16) % (2 * maxR + 1)) - maxR;
    int d2 = dx * dx + dy * dy;
    if (d2 < minR * minR || d2 > maxR * maxR) continue;
    int tx = (ccx + dx) >> 3, ty = (ccy + dy) >> 3;
    if (classify(cityMap, CITY_W, CITY_H, tx, ty) &&
        aiHasExit(cityMap, CITY_W, CITY_H, tx, ty, classify)) {
      otx = tx; oty = ty; return true;
    }
  }
  return false;
}

static void aiRespawnCar(AiCar &c, int ccx, int ccy) {
  int tx, ty;
  if (aiFindTileInRing(ccx, ccy, RING_MIN, RING_MAX, aiIsDrivable, tx, ty)) {
    aiPlace(cityMap, CITY_W, CITY_H, c.x, c.y, c.dir, c.tgtx, c.tgty,
            tx, ty, aiIsDrivable, aiRng);
    c.color = AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N];
    c.active = true;
  } else {
    c.active = false;
  }
}

static void aiRespawnPed(AiPed &p, int ccx, int ccy) {
  int tx, ty;
  if (aiFindTileInRing(ccx, ccy, RING_MIN, RING_MAX, aiIsWalkable, tx, ty)) {
    aiPlace(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
            tx, ty, aiIsWalkable, aiRng);
    p.color = AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N];
    p.frame = 0; p.animTimer = 0; p.state = 0; p.downTimer = 0;
    p.active = true;
  } else {
    p.active = false;
  }
}

// Met a jour le trafic autour du point de vue (fcx,fcy = centre suivi, px monde).
// Recyclage des entites trop loin (culling), pas IA, collisions avec le joueur.
static void aiUpdate(int fcx, int fcy) {
  const int rec2 = RECYCLE_DIST * RECYCLE_DIST;
  // Collision voiture joueur : seuils en px (sommes de demi-boites).
  const float COL_CC = 9.0f;   // voiture-voiture
  const float COL_CP = 6.0f;   // voiture-pieton
  float spd2 = car.vx * car.vx + car.vy * car.vy;

  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (c.active) {
      int ddx = (int)c.x - fcx, ddy = (int)c.y - fcy;
      if (ddx * ddx + ddy * ddy > rec2) c.active = false;
    }
    if (!c.active) { aiRespawnCar(c, fcx, fcy); continue; }
    aiStep(cityMap, CITY_W, CITY_H, c.x, c.y, c.dir, c.tgtx, c.tgty,
           AI_CAR_SPEED, aiIsDrivable, aiRng);
    // Solide vis-a-vis de la voiture joueur : on repousse le joueur.
    if (driving) {
      float dx = car.x - c.x, dy = car.y - c.y;
      if (fabsf(dx) < COL_CC && fabsf(dy) < COL_CC) {
        float px = COL_CC - fabsf(dx), py = COL_CC - fabsf(dy);
        if (px < py) { car.x += (dx < 0 ? -px : px); car.vx *= -0.3f; }
        else { car.y += (dy < 0 ? -py : py); car.vy *= -0.3f; }
      }
    }
  }

  for (int i = 0; i < NUM_AI_PEDS; i++) {
    AiPed &p = aiPeds[i];
    if (p.state == 1) {                       // au sol : compte a rebours
      if (p.downTimer == 0 || --p.downTimer == 0) p.active = false;
    } else if (p.active) {
      int ddx = (int)p.x - fcx, ddy = (int)p.y - fcy;
      if (ddx * ddx + ddy * ddy > rec2) p.active = false;
    }
    if (!p.active) { aiRespawnPed(p, fcx, fcy); continue; }
    if (p.state == 1) continue;               // renverse : immobile
    aiStep(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
           AI_PED_SPEED, aiIsWalkable, aiRng);
    if (++p.animTimer >= AI_PED_ANIM) { p.animTimer = 0; p.frame ^= 1; }
    // Renversement par la voiture joueur lancee.
    if (driving && spd2 > RUNOVER_SPEED2) {
      if (fabsf(car.x - p.x) < COL_CP && fabsf(car.y - p.y) < COL_CP) {
        p.state = 1; p.downTimer = PED_DOWN_FRAMES;
      }
    }
  }
}

static void aiDraw(int camX, int camY) {
  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (c.active) blitCar(camX, camY, (int)c.x, (int)c.y, AI_CAR_FRAME[c.dir], c.color);
  }
  for (int i = 0; i < NUM_AI_PEDS; i++) {
    AiPed &p = aiPeds[i];
    if (!p.active) continue;
    if (p.state == 1) blitSplat(camX, camY, (int)p.x, (int)p.y);
    else blitPed(camX, camY, (int)p.x, (int)p.y, p.dir, p.frame, p.color);
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

  // Centre suivi (px monde) puis mise a jour du trafic IA autour (culling +
  // collisions avec le joueur). Fait avant le clamp camera : 1 frame de retard
  // negligeable sur le repere de recyclage.
  int focusX = driving ? (int)car.x : playerX + PLAYER_W / 2;
  int focusY = driving ? (int)car.y : playerY + PLAYER_H / 2;
  aiUpdate(focusX, focusY);

  // La collision a pu deplacer la voiture : recalculer le focus.
  focusX = driving ? (int)car.x : playerX + PLAYER_W / 2;
  focusY = driving ? (int)car.y : playerY + PLAYER_H / 2;

  // Camera centree sur l'entite active.
  int camX = clampCamera(focusX - SCREEN_W / 2, WORLD_W, SCREEN_W);
  int camY = clampCamera(focusY - SCREEN_H / 2, WORLD_H, SCREEN_H);

  // Rendu : ecriture directe dans le framebuffer (recupere chaque frame).
  fb = gb.display._buffer;

  // Tuiles : 11x9 (10x8 visibles + 1 marge / axe pour scroll sub-tuile).
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

  // Trafic IA (sous le joueur), puis voiture joueur, puis perso a pied.
  aiDraw(camX, camY);
  drawCar(camX, camY);
  if (!driving) drawPlayer(camX, camY);

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
