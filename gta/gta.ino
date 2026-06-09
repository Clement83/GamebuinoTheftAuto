// GTA demake -- sketch Gamebuino META (M4 + voiture).
// Pieton qui se deplace a la croix dans la ville scrollante, collision +
// camera clampee. Une voiture conduisible (physique arcade + drift) est garee
// a cote du spawn : A pour monter, MENU pour descendre. Rendu pixel par pixel
// (drawPixel) depuis des tableaux RGB565 en flash ; la voiture est un rectangle
// oriente (angle continu) trace a la volee, sans sprite.
//
// A pied  : croix = deplacer, A (pres de la voiture) = monter.
// Au volant : A = accelerer, B = freiner (puis marche arriere a l'arret),
//             GAUCHE/DROITE = braquer, MENU = descendre. Drift auto : freiner
//             en virage a vitesse part en glisse.
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
#include "combat.h"
#include "mission.h"

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

// --- coup de poing a pied : petit effet d'impact (pixels clairs devant) ---
static const int PUNCH_FX_FRAMES = 3;   // duree de l'effet visuel
static uint8_t punchTimer = 0;          // >0 : effet en cours (decremente/frame)

// ---------------------------------------------------------------------------
//  Trafic IA : voitures (routes) + pietons (trottoirs) errants, recolores.
//  Logique de deplacement = couche pure ai.h (errance sur grille + voies).
//  Glue device : pool statique, spawn/recyclage autour de la camera (culling),
//  blit recolore (KEY -> couleur entite), collisions avec le joueur.
// ---------------------------------------------------------------------------
static const int NUM_AI_CARS = 3;
static const int NUM_AI_PEDS = 6;
static const float AI_CAR_SPEED = 0.8f;   // px/frame (plus lent que le joueur)
static const float AI_PED_SPEED = 0.35f;
static const int AI_PED_ANIM = 10;        // frames entre deux images de marche
static const int RECYCLE_DIST = 80;       // px du centre camera -> recyclage
static const int RING_MIN = 44;           // anneau de (re)spawn : juste hors ecran
static const int RING_MAX = 72;
static const int PED_DOWN_FRAMES = 70;     // duree au sol avant recyclage
static const int RUNOVER_SPEED2 = 1;       // vitesse^2 mini pour renverser
static const int STOP_AHEAD = 15;          // px : distance d'arret devant obstacle
static const int STOP_SIDE  = 6;           // px : tolerance laterale de l'obstacle
static const int ENTER_AI_DIST = 12;       // px : portee pour voler une voiture IA

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
static const uint16_t PLAYER_CAR_COLOR  = 0xC800;  // rouge (voiture de depart)
// Couleur de la voiture actuellement pilotee (change si on en vole une autre).
static uint16_t carColor = PLAYER_CAR_COLOR;

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

// ---------------------------------------------------------------------------
//  Systeme de missions modulaire. Deux telephones FIXES dans la ville ; chacun
//  declenche TOUJOURS la meme mission scenarisee (objectifs enchaines + petits
//  messages de narration qui defilent en bas). Le moteur pur (mission.h) teste
//  et avance les objectifs a partir d'un instantane d'etat ; ici, la glue place
//  les entites (voiture de mission, Marco, cible), dessine marqueurs et bandeau,
//  et joue la sonnerie. Missions rejouables (les telephones re-sonnent au repos).
// ---------------------------------------------------------------------------
static const int   MISSION_SEE_RANGE   = 64;    // px : portee de "il me voit" (Joe)
static const int   MISSION_LOSE_FRAMES = 40;    // frames sans vue -> re-flane (Joe)
static const float TARGET_FLEE_SPEED   = 0.9f;  // px/frame en fuite (~joueur)
static const float TARGET_CHASE_SPEED  = 0.7f;  // px/frame en poursuite (tueur)
static const float TARGET_WANDER_SPEED = AI_PED_SPEED; // flanerie (trottoirs)
static const int   PHONE_REACH         = 12;    // px : portee pour decrocher
static const int   PHONE_HEAR_RANGE    = 28;    // px : cercle audible (sonnerie)
static const int   TARGET_MIN_DIST     = 60;    // px : Joe pas trop pres au spawn
static const float TARGET_RUNOVER_DIST = 6.0f;  // px : ecrasement par la voiture
static const uint16_t TARGET_COLOR     = 0x07E0; // vert vif (cible)
static const uint16_t PHONE_COLOR      = 0x001F; // bleu (cabine telephonique)
static const uint16_t MARKER_COLOR     = 0xFFE0; // jaune (marqueur de destination)
static const uint16_t MISSION_CAR_COLOR= 0xFD20; // orange (voiture de mission)
static const uint16_t MARCO_COLOR      = 0x07FF; // cyan (Marco)

// Sonnerie : warble deux tons (dring-dring) puis longue pause, en boucle.
// frames @ ~25 fps (~40 ms). freq 0 = silence (pause).
struct RingNote { uint16_t freq; uint8_t frames; };
static const RingNote PHONE_RING[] = {
  { 1175, 2 }, { 1568, 2 }, { 1175, 2 }, { 1568, 2 }, { 0, 4 },
  { 1175, 2 }, { 1568, 2 }, { 1175, 2 }, { 1568, 2 }, { 0, 40 },
};
static const uint8_t PHONE_RING_N = sizeof(PHONE_RING) / sizeof(PHONE_RING[0]);
static uint8_t ringStep = 0, ringTimer = 0;

// --- Definitions des missions (flash). Coords monde = centre px de tuiles
//     valides verifiees sur la map generee (cf. docs/.../specs). ---
#define MISSION_JOE  0
#define MISSION_DEAL 1
static const Objective OBJS_JOE[] = {
  { OBJ_KILL, 0, 0, 0, false, EV_NONE,
    "Joe m'a balance aux flics. Retrouve-le et fais-le taire.",
    "Beau boulot. Joe parlera plus." },
};
static const Objective OBJS_DEAL[] = {
  { OBJ_ENTER_CAR, 420, 380,  0, false, EV_NONE,
    "J'ai laisse une caisse au parking. Va la chercher.", nullptr },
  { OBJ_GOTO,      196, 420, 12, true,  EV_MARCO_JOIN,
    "Passe prendre Marco au coin de la rue.", "Marco : roule, au chantier !" },
  { OBJ_GOTO,      316, 500, 12, true,  EV_MARCO_DIE,
    "Conduis Marco au chantier.", "Un type surgit... Marco s'effondre !" },
  { OBJ_KILL,      316, 500,  0, false, EV_NONE,
    "Tu peux pas laisser passer ca. Bute le tueur.", "Justice est faite." },
};
static const MissionDef MISSIONS[] = {
  { "Joe",              OBJS_JOE,  1 },
  { "Mauvaise affaire", OBJS_DEAL, 4 },
};

// --- Telephones a position FIXE (trottoirs proches du spawn). ---
struct PhoneDef { int16_t x, y; uint8_t mission; };
static const PhoneDef PHONES[] = {
  { 284, 364, MISSION_JOE },    // trottoir ouest
  { 436, 364, MISSION_DEAL },   // trottoir est
};
static const int NUM_PHONES = sizeof(PHONES) / sizeof(PHONES[0]);

// --- Etat runtime de la mission en cours. ---
static MissionRun missionRun = { 0, 0, false };
static uint16_t missionAnim = 0;          // compteur d'animation (clignotements)

// Cible de mission (Joe : erre + fuit ; tueur : fonce sur le joueur).
enum { T_WANDER = 0, T_FLEE = 1 };
struct Target {
  float x, y; uint8_t dir; int tgtx, tgty;
  uint8_t frame, animTimer; uint8_t phase; uint16_t loseTimer;
  bool active; bool chase;
};
static Target target;
static uint8_t targetDownTimer = 0;       // >0 : splat (cible/Marco) feedback
static int targetDownX = 0, targetDownY = 0;

// Voiture de mission a recuperer au parking (objectif ENTER_CAR).
static CarState mCar;
static bool mCarActive = false;           // garee, en attente d'etre prise
static bool carIsMission = false;         // le joueur conduit la voiture de mission

// Marco (passager scenarise).
static bool marcoWaiting = false;         // debout au marqueur, attend la prise
static bool marcoAboard  = false;         // monte dans la voiture

// Narration : file de messages flash ; bandeau bas auto-time + scroll horizontal.
static const char *narrQueue[4];
static uint8_t narrHead = 0, narrCount = 0;
static uint16_t narrTimer = 0;       // frames restantes du message courant
static uint16_t narrAge = 0;         // frames ecoulees sur le message courant
static int narrScroll = 0;           // decalage horizontal (px)
static const int NARR_HOLD    = 35;  // frames cale a gauche (lecture du debut)
static const int NARR_SPEED10 = 7;   // px de scroll par 10 frames (plus grand = rapide)
static const int NARR_ENDPAD  = 35;  // frames cale a la fin

// Declarations anticipees (utilisees avant leur definition).
static void killTarget(int px, int py);
static void startMission(uint8_t m);
static void narrate(const char *s);

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

  // Missions au repos : les telephones fixes sonnent quand on s'en approche.
  missionRun.active = false;
  target.active = false;
  targetDownTimer = 0;
  mCarActive = false; carIsMission = false;
  marcoWaiting = false; marcoAboard = false;
  narrHead = 0; narrCount = 0; narrTimer = 0;
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

// Effet d'impact du coup de poing : quelques pixels clairs devant le joueur,
// dans la direction du regard (playerDir). Dessine pendant punchTimer frames.
static void blitPunchFx(int camX, int camY) {
  int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
  int fx = pcx + AI_DX[playerDir] * 5 - camX;   // ~5 px devant
  int fy = pcy + AI_DY[playerDir] * 5 - camY;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      if (dx != 0 && dy != 0) continue;          // petite croix
      int x = fx + dx, y = fy + dy;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        fb[y * SCREEN_W + x] = 0xFFFF;           // blanc
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
  blitCar(camX, camY, (int)car.x, (int)car.y, idx, carColor);
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

// Cherche une tuile marchable avec sortie proche de (cx,cy) px (spirale <=4).
static bool aiFindWalkTileNear(int cx, int cy, int &otx, int &oty) {
  int btx = cx >> 3, bty = cy >> 3;
  for (int r = 0; r <= 4; r++)
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        if (r > 0 && abs(dx) != r && abs(dy) != r) continue;   // anneau
        int tx = btx + dx, ty = bty + dy;
        if (aiIsWalkable(cityMap, CITY_W, CITY_H, tx, ty) &&
            aiHasExit(cityMap, CITY_W, CITY_H, tx, ty, aiIsWalkable)) {
          otx = tx; oty = ty; return true;
        }
      }
  return false;
}

// Le conducteur d'une voiture volee descend : un pieton apparait a cote.
static void aiEjectDriver(int atx_px, int aty_px) {
  int slot = -1;
  for (int i = 0; i < NUM_AI_PEDS; i++) if (!aiPeds[i].active) { slot = i; break; }
  if (slot < 0) slot = 0;                  // pool plein : ecrase le premier
  int tx, ty;
  if (!aiFindWalkTileNear(atx_px, aty_px, tx, ty)) return;   // pas de place
  AiPed &p = aiPeds[slot];
  aiPlace(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
          tx, ty, aiIsWalkable, aiRng);
  p.color = AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N];
  p.frame = 0; p.animTimer = 0; p.state = 0; p.downTimer = 0;
  p.active = true;
}

// Coup de poing a pied : frappe le pieton le plus proche devant le joueur.
// Construit des tableaux paralleles (pietons frappables = actifs et debout),
// delegue le ciblage a combat.h, puis met la cible au sol + son + effet visuel.
static void tryPunch() {
  float px[NUM_AI_PEDS], py[NUM_AI_PEDS];
  bool act[NUM_AI_PEDS];
  for (int i = 0; i < NUM_AI_PEDS; i++) {
    px[i] = aiPeds[i].x; py[i] = aiPeds[i].y;
    act[i] = aiPeds[i].active && aiPeds[i].state == 0;
  }
  int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
  int hit = combatPunchTarget(px, py, act, NUM_AI_PEDS, pcx, pcy, playerDir);
  punchTimer = PUNCH_FX_FRAMES;
  gb.sound.playTick();                       // bip court (lib, aucun asset)
  if (hit >= 0) { aiPeds[hit].state = 1; aiPeds[hit].downTimer = PED_DOWN_FRAMES; }
  // La cible de mission est frappable comme un pieton (un seul element).
  if (missionRun.active && target.active) {
    float tpx[1] = { target.x }, tpy[1] = { target.y }; bool ta[1] = { true };
    if (combatPunchTarget(tpx, tpy, ta, 1, pcx, pcy, playerDir) == 0)
      killTarget(pcx, pcy);
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

  // Obstacle = entite joueur active (voiture pilotee, ou perso a pied).
  int obx = driving ? (int)car.x : playerX + PLAYER_W / 2;
  int oby = driving ? (int)car.y : playerY + PLAYER_H / 2;

  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (c.active) {
      int ddx = (int)c.x - fcx, ddy = (int)c.y - fcy;
      if (ddx * ddx + ddy * ddy > rec2) c.active = false;
    }
    if (!c.active) { aiRespawnCar(c, fcx, fcy); continue; }
    // Arret si le joueur est juste devant (dans la voie, a moins de STOP_AHEAD).
    float relx = obx - c.x, rely = oby - c.y;
    float fwd = relx * AI_DX[c.dir] + rely * AI_DY[c.dir];
    float lat = relx * AI_DX[AI_RIGHT[c.dir]] + rely * AI_DY[AI_RIGHT[c.dir]];
    bool blocked = (fwd > 0.0f && fwd < STOP_AHEAD && fabsf(lat) < STOP_SIDE);
    if (!blocked)
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

// Empile un message de narration (pointeur flash, pas de copie). File de 4.
static void narrate(const char *s) {
  if (narrCount < 4) { narrQueue[(narrHead + narrCount) % 4] = s; narrCount++; }
}

// Fait apparaitre Joe (errance + fuite) sur une tuile marchable au hasard,
// assez loin de (pcx,pcy). true si place.
static bool spawnTargetWander(int pcx, int pcy) {
  for (int i = 0; i < 400; i++) {
    uint32_t r = aiRngNext(aiRng);
    int tx = (int)(r % CITY_W), ty = (int)((r >> 16) % CITY_H);
    if (!aiIsWalkable(cityMap, CITY_W, CITY_H, tx, ty)) continue;
    if (!aiHasExit(cityMap, CITY_W, CITY_H, tx, ty, aiIsWalkable)) continue;
    int cx = tx * 8 + 4, cy = ty * 8 + 4;
    long d2 = (long)(cx - pcx) * (cx - pcx) + (long)(cy - pcy) * (cy - pcy);
    if (d2 < (long)TARGET_MIN_DIST * TARGET_MIN_DIST) continue;
    aiPlace(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
            target.tgtx, target.tgty, tx, ty, aiIsWalkable, aiRng);
    target.frame = 0; target.animTimer = 0; target.phase = T_WANDER;
    target.loseTimer = 0; target.active = true; target.chase = false;
    return true;
  }
  return false;
}

// Fait apparaitre le tueur (poursuite) sur une tuile marchable proche de (wx,wy).
static void spawnTargetAt(int wx, int wy) {
  int tx, ty;
  if (!aiFindWalkTileNear(wx, wy, tx, ty)) { tx = wx >> 3; ty = wy >> 3; }
  aiPlace(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
          target.tgtx, target.tgty, tx, ty, aiIsWalkable, aiRng);
  target.frame = 0; target.animTimer = 0; target.phase = T_WANDER;
  target.loseTimer = 0; target.active = true; target.chase = true;
}

// Active l'objectif courant : narration + spawn des entites necessaires.
static void enterObjective() {
  const MissionDef &def = MISSIONS[missionRun.def];
  const Objective &o = def.objectives[missionRun.step];
  narrate(o.text);
  if (o.type == OBJ_ENTER_CAR) {
    mCar.x = o.x; mCar.y = o.y; mCar.angle = 0.0f; mCar.vx = 0.0f; mCar.vy = 0.0f;
    mCarActive = true;
  } else if (o.type == OBJ_KILL && !target.active) {
    int pcx = driving ? (int)car.x : playerX + PLAYER_W / 2;
    int pcy = driving ? (int)car.y : playerY + PLAYER_H / 2;
    if (missionRun.def == MISSION_DEAL) spawnTargetAt(o.x, o.y);
    else                                spawnTargetWander(pcx, pcy);
  } else if (o.type == OBJ_GOTO && missionRun.def == MISSION_DEAL && missionRun.step == 1) {
    marcoWaiting = true;                 // Marco attend au coin de la rue
  }
}

// Demarre une mission (telephone decroche).
static void startMission(uint8_t m) {
  missionRun.def = m; missionRun.step = 0; missionRun.active = true;
  target.active = false; marcoWaiting = false; marcoAboard = false;
  mCarActive = false;
  enterObjective();
  gb.sound.playOK();
}

// Cible tuee (poing/voiture) : splat de feedback. L'avancement de mission est
// detecte par missionProgress (target.active == false -> objectif KILL rempli).
static void killTarget(int px, int py) {
  (void)px; (void)py;
  target.active = false;
  targetDownX = (int)target.x; targetDownY = (int)target.y;
  targetDownTimer = PED_DOWN_FRAMES;
  gb.sound.playOK();
}

// Teste l'objectif courant ; s'il est rempli, applique l'evenement de
// transition (Marco monte / meurt) puis active l'objectif suivant, ou termine.
static void missionProgress() {
  if (!missionRun.active) return;
  const MissionDef &def = MISSIONS[missionRun.def];
  MissionState s;
  s.actorCx = driving ? (int)car.x : playerX + PLAYER_W / 2;
  s.actorCy = driving ? (int)car.y : playerY + PLAYER_H / 2;
  s.driving = driving;
  s.inMissionCar = driving && carIsMission;
  s.targetAlive = target.active;
  if (!missionObjectiveDone(def.objectives[missionRun.step], s)) return;

  const Objective &done = def.objectives[missionRun.step];  // objectif accompli
  uint8_t ev = missionAdvance(missionRun, def);   // step++ (active=false si fini)
  if (done.doneText) narrate(done.doneText);      // message "objectif atteint"
  if (ev == EV_MARCO_JOIN) {
    marcoWaiting = false; marcoAboard = true;
  } else if (ev == EV_MARCO_DIE) {
    marcoAboard = false;
    const Objective &k = def.objectives[missionRun.step];  // KILL : coords chantier
    targetDownX = k.x; targetDownY = k.y; targetDownTimer = PED_DOWN_FRAMES;
  }
  if (missionRun.active) enterObjective();
  else gb.sound.playOK();                          // mission terminee
}

// Deplace la cible chaque frame : tueur -> poursuite ; Joe -> vue/fuite/flanerie.
// Plus animation et ecrasement par la voiture lancee. (fcx,fcy) = repere joueur.
static void missionUpdate(int fcx, int fcy) {
  if (!missionRun.active || !target.active) return;

  if (target.chase) {
    missionChaseStep(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
                     target.tgtx, target.tgty, TARGET_CHASE_SPEED, fcx, fcy, aiRng);
  } else {
    bool sees = missionLineOfSight(cityMap, CITY_W, CITY_H,
                                   (int)target.x, (int)target.y, fcx, fcy,
                                   MISSION_SEE_RANGE);
    if (sees) {
      if (target.phase == T_WANDER) {
        target.phase = T_FLEE;
        missionFleeRetarget(cityMap, CITY_W, CITY_H, target.x, target.y,
                            target.dir, target.tgtx, target.tgty, fcx, fcy, aiRng);
      }
      target.loseTimer = 0;
    } else if (target.phase == T_FLEE) {
      if (++target.loseTimer >= MISSION_LOSE_FRAMES) {
        target.phase = T_WANDER;                 // recale sur un trottoir proche
        int tx, ty;
        if (aiFindWalkTileNear((int)target.x, (int)target.y, tx, ty))
          aiPlace(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
                  target.tgtx, target.tgty, tx, ty, aiIsWalkable, aiRng);
      }
    }
    if (target.phase == T_FLEE)
      missionFleeStep(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
                      target.tgtx, target.tgty, TARGET_FLEE_SPEED, fcx, fcy, aiRng);
    else
      aiStep(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
             target.tgtx, target.tgty, TARGET_WANDER_SPEED, aiIsWalkable, aiRng);
  }

  if (++target.animTimer >= AI_PED_ANIM) { target.animTimer = 0; target.frame ^= 1; }

  if (driving) {
    float spd2 = car.vx * car.vx + car.vy * car.vy;
    if (spd2 > RUNOVER_SPEED2 &&
        fabsf(car.x - target.x) < TARGET_RUNOVER_DIST &&
        fabsf(car.y - target.y) < TARGET_RUNOVER_DIST)
      killTarget((int)car.x, (int)car.y);
  }
}

// Cabine bleue (procedural), toit clignotant + ondes, en (px,py) monde.
static void drawPhone1(int camX, int camY, int px, int py) {
  int sx = px - camX, sy = py - camY;
  bool blink = ((missionAnim >> 2) & 1);
  for (int dy = -3; dy <= 3; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      int x = sx + dx, y = sy + dy;
      if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
      uint16_t c = (dy == -3) ? (blink ? 0xFFFF : 0xFFE0) : PHONE_COLOR;
      fb[y * SCREEN_W + x] = c;
    }
  if (blink) {                                   // ondes de sonnerie
    int ox[4] = { -4, 4, -4, 4 }, oy[4] = { -4, -4, 0, 0 };
    for (int k = 0; k < 4; k++) {
      int x = sx + ox[k], y = sy + oy[k];
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        fb[y * SCREEN_W + x] = 0xFFE0;
    }
  }
}

// Dessine les deux telephones fixes quand la mission est au repos (decrochables).
static void drawPhones(int camX, int camY) {
  if (missionRun.active) return;
  for (int i = 0; i < NUM_PHONES; i++)
    drawPhone1(camX, camY, PHONES[i].x, PHONES[i].y);
}

// Voiture de mission garee (objectif ENTER_CAR) : orange, orientee est.
static void drawMissionCar(int camX, int camY) {
  if (!mCarActive) return;
  blitCar(camX, camY, (int)mCar.x, (int)mCar.y, AI_CAR_FRAME[DIR_EAST], MISSION_CAR_COLOR);
}

// Marco : pieton cyan debout au marqueur tant qu'il n'est pas pris.
static void drawMarco(int camX, int camY) {
  if (!marcoWaiting || !missionRun.active) return;
  const Objective &o = MISSIONS[MISSION_DEAL].objectives[1];
  blitPed(camX, camY, o.x, o.y, DIR_SOUTH, 0, MARCO_COLOR);
}

// Marqueur de destination clignotant (objectif GOTO / ENTER_CAR).
static void drawMarker(int camX, int camY) {
  if (!missionRun.active) return;
  const Objective &o = MISSIONS[missionRun.def].objectives[missionRun.step];
  if (o.type != OBJ_GOTO && o.type != OBJ_ENTER_CAR) return;
  int sx = o.x - camX, sy = o.y - camY;
  bool blink = ((missionAnim >> 2) & 1);
  for (int dy = -5; dy <= 0; dy++) {             // pilier vertical
    int x = sx, y = sy + dy;
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
      fb[y * SCREEN_W + x] = blink ? 0xFFFF : MARKER_COLOR;
  }
  for (int dx = -1; dx <= 1; dx++) {             // base
    int x = sx + dx, y = sy;
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
      fb[y * SCREEN_W + x] = MARKER_COLOR;
  }
}

// Cible : splat (cible tuee ou Marco abattu) ET/OU sprite vivant. Les deux
// coexistent quand le tueur vient d'apparaitre sur le splat de Marco.
static void drawTarget(int camX, int camY) {
  if (targetDownTimer > 0) blitSplat(camX, camY, targetDownX, targetDownY);
  if (missionRun.active && target.active)
    blitPed(camX, camY, (int)target.x, (int)target.y, target.dir, target.frame, TARGET_COLOR);
}

// Fleche HUD autour du joueur, pointant la destination ou la cible courante.
static void drawMissionArrow(int camX, int camY, int fcx, int fcy) {
  if (!missionRun.active) return;
  const Objective &o = MISSIONS[missionRun.def].objectives[missionRun.step];
  int tx, ty; uint16_t col;
  if (o.type == OBJ_KILL) {
    if (!target.active) return;
    tx = (int)target.x; ty = (int)target.y; col = TARGET_COLOR;
  } else {
    tx = o.x; ty = o.y; col = MARKER_COLOR;
  }
  float nx, ny;
  missionArrowVec(tx, ty, fcx, fcy, nx, ny);
  int cx = fcx - camX, cy = fcy - camY;          // joueur a l'ecran
  int tipx = cx + (int)(nx * 12.0f), tipy = cy + (int)(ny * 12.0f);
  int bx = cx + (int)(nx * 7.0f), by = cy + (int)(ny * 7.0f);
  int perpx = (int)(-ny * 2.0f), perpy = (int)(nx * 2.0f);
  int pts[3][2] = { { tipx, tipy }, { bx + perpx, by + perpy }, { bx - perpx, by - perpy } };
  for (int k = 0; k < 3; k++) {
    int x = pts[k][0], y = pts[k][1];
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
      fb[y * SCREEN_W + x] = col;
  }
  if (bx >= 0 && bx < SCREEN_W && by >= 0 && by < SCREEN_H)
    fb[by * SCREEN_W + bx] = col;
}

// Narration : avance la file. ~4 px/caractere (font3x5). Le message commence
// CALE A GAUCHE et reste fixe NARR_HOLD frames (lecture du debut), puis defile
// lentement, puis reste cale a la fin NARR_ENDPAD frames avant de passer au
// suivant. Un message court (tient a l'ecran) reste simplement affiche.
static void narrUpdate() {
  if (narrCount == 0) return;
  int w = (int)strlen(narrQueue[narrHead]) * 4;
  int maxScroll = (w > SCREEN_W) ? (w - SCREEN_W) : 0;
  if (narrTimer == 0) {                          // demarre le message courant
    int scrollFrames = (maxScroll * 10 + NARR_SPEED10 - 1) / NARR_SPEED10;  // ceil
    narrTimer = (uint16_t)(NARR_HOLD + scrollFrames + NARR_ENDPAD);
    narrScroll = 0; narrAge = 0;
    return;                                      // 1re frame : debut cale a gauche
  }
  narrAge++;
  if (maxScroll > 0 && narrAge > (uint16_t)NARR_HOLD) {
    int prog = (int)(narrAge - NARR_HOLD) * NARR_SPEED10 / 10;
    narrScroll = (prog < maxScroll) ? prog : maxScroll;
  }
  if (--narrTimer == 0) { narrHead = (narrHead + 1) % 4; narrCount--; }
}

// Narration : bandeau bas + texte (scroll horizontal si trop long).
static void narrDraw() {
  if (narrCount == 0) return;
  for (int y = SCREEN_H - 7; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) fb[y * SCREEN_W + x] = 0x0000;
  gb.display.setColor(WHITE);
  gb.display.setCursor(1 - narrScroll, SCREEN_H - 6);
  gb.display.print(narrQueue[narrHead]);
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
    // A : decrocher un telephone fixe qui sonne (demarre sa mission), sinon
    // monter dans la voiture la plus proche a portee (voiture de mission garee,
    // voiture du joueur, ou voiture IA volee), sinon coup de poing.
    // best : -3 voiture de mission, -2 aucune, -1 voiture joueur, >=0 IA.
    if (gb.buttons.pressed(BUTTON_A)) {
      int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
      bool answered = false;
      if (!missionRun.active) {
        for (int i = 0; i < NUM_PHONES; i++) {
          long dP = (long)(pcx - PHONES[i].x) * (pcx - PHONES[i].x)
                  + (long)(pcy - PHONES[i].y) * (pcy - PHONES[i].y);
          if (dP <= (long)PHONE_REACH * PHONE_REACH) {
            startMission(PHONES[i].mission); answered = true; break;
          }
        }
      }
      if (answered) {
        // telephone decroche : on ignore voiture/poing pour ce A.
      } else {
        int best = -2;
        long bestd = (long)ENTER_DIST * ENTER_DIST;
        if (mCarActive) {
          long dM = (long)(pcx - (int)mCar.x) * (pcx - (int)mCar.x)
                  + (long)(pcy - (int)mCar.y) * (pcy - (int)mCar.y);
          if (dM <= bestd) { best = -3; bestd = dM; }
        }
        long dC = (long)(pcx - (int)car.x) * (pcx - (int)car.x)
                + (long)(pcy - (int)car.y) * (pcy - (int)car.y);
        if (dC <= bestd) { best = -1; bestd = dC; }
        long aiThr = (long)ENTER_AI_DIST * ENTER_AI_DIST;
        for (int i = 0; i < NUM_AI_CARS; i++) {
          if (!aiCars[i].active) continue;
          long d = (long)(pcx - (int)aiCars[i].x) * (pcx - (int)aiCars[i].x)
                 + (long)(pcy - (int)aiCars[i].y) * (pcy - (int)aiCars[i].y);
          if (d <= aiThr && d < bestd) { best = i; bestd = d; }
        }
        if (best == -3) {                   // voiture de mission au parking
          car = mCar; car.vx = 0.0f; car.vy = 0.0f;
          carColor = MISSION_CAR_COLOR; carIsMission = true;
          mCarActive = false; driving = true;
        } else if (best == -1) {
          driving = true; carIsMission = false;   // remonter dans sa voiture
        } else if (best >= 0) {
          AiCar &c = aiCars[best];           // vol : le conducteur descend
          aiEjectDriver((int)c.x, (int)c.y);
          car.x = c.x; car.y = c.y; car.vx = 0.0f; car.vy = 0.0f;
          car.angle = AI_CAR_FRAME[c.dir] * (TWO_PI / CAR_FRAMES);
          carColor = c.color; carIsMission = false;
          c.active = false;                  // la voiture quitte le pool IA
          driving = true;
        } else {
          tryPunch();                        // aucune voiture a portee -> poing
        }
      }
    }
  } else {
    // --- AU VOLANT ---
    // A = accelerer, B = freiner (puis marche arriere une fois ~arrete),
    // GAUCHE/DROITE = braquer, MENU = sortir. Drift auto : B + braquage a vitesse.
    bool accel = gb.buttons.repeat(BUTTON_A, 1);
    bool brakeBtn = gb.buttons.repeat(BUTTON_B, 1);
    float steer = gb.buttons.repeat(BUTTON_RIGHT, 1) ? 1.0f
                : (gb.buttons.repeat(BUTTON_LEFT, 1) ? -1.0f : 0.0f);
    float fwd = carForwardSpeed(car);
    float throttle = 0.0f;
    bool brake = false;
    if (accel) {
      throttle = 1.0f;
    } else if (brakeBtn) {
      if (fwd > CAR_REVERSE_EPS) brake = true;   // roule encore -> on freine
      else throttle = -1.0f;                     // ~arrete -> marche arriere
    }
    bool drift = brakeBtn && steer != 0.0f && fwd > CAR_DRIFT_MIN;
    carUpdate(car, throttle, steer, drift, brake);
    // Descendre (MENU) : poser le perso sur une case libre a cote.
    if (gb.buttons.pressed(BUTTON_MENU)) {
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

  // Mission : deplacement de la cible, avancement des objectifs, narration.
  missionUpdate(focusX, focusY);
  missionProgress();
  if (targetDownTimer > 0) targetDownTimer--;
  missionAnim++;                               // clignotement marqueurs/telephones
  narrUpdate();
  // Sonnerie : au repos, le telephone fixe le plus proche sonne s'il est dans
  // le cercle audible. Melodie deux tons (sinon muet).
  if (!missionRun.active) {
    int near = -1; long bestd = (long)PHONE_HEAR_RANGE * PHONE_HEAR_RANGE;
    for (int i = 0; i < NUM_PHONES; i++) {
      long dP = (long)(focusX - PHONES[i].x) * (focusX - PHONES[i].x)
              + (long)(focusY - PHONES[i].y) * (focusY - PHONES[i].y);
      if (dP <= bestd) { bestd = dP; near = i; }
    }
    if (near >= 0) {
      if (ringTimer == 0) {
        const RingNote &n = PHONE_RING[ringStep];
        if (n.freq) gb.sound.tone(n.freq, (int32_t)n.frames * 45);
        ringTimer = n.frames;
        ringStep = (ringStep + 1) % PHONE_RING_N;
      }
      ringTimer--;
    } else {
      ringStep = 0; ringTimer = 0;             // hors cercle : sonnerie au repos
    }
  }

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

  // Trafic IA (sous le joueur), entites de mission, puis voiture joueur, perso.
  aiDraw(camX, camY);
  drawMissionCar(camX, camY);
  drawMarco(camX, camY);
  drawTarget(camX, camY);
  drawPhones(camX, camY);
  drawMarker(camX, camY);
  drawCar(camX, camY);
  if (!driving) drawPlayer(camX, camY);
  if (!driving && punchTimer > 0) { blitPunchFx(camX, camY); punchTimer--; }

  // HUD : fleche vers l'objectif/cible, puis bandeau de narration.
  drawMissionArrow(camX, camY, focusX, focusY);
  narrDraw();

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
