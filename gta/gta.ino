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
#include <stdio.h>
#include "assets.h"
#include "citymap.h"
#include "engine.h"
#include "car.h"
#include "player.h"
#include "ai.h"
#include "combat.h"
#include "mission.h"
#include "weapons.h"
#include "weapons_gfx.h"

// L'enum WeaponId (weapons.h) et les sprites (weapons_gfx.h) doivent rester
// alignes : meme nombre, meme ordre (cf. tools/build_weapons.py).
static_assert(WEAPON_COUNT == WEAPON_GFX_COUNT, "weapons.h / weapons_gfx.h desynchronises");

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

// --- attaque a pied : petit effet d'impact (pixels clairs devant) ---
static const int PUNCH_FX_FRAMES = 3;   // duree de l'effet visuel
static uint8_t punchTimer = 0;          // >0 : effet en cours (decremente/frame)

// --- armes : arme courante, possession, munitions. Le poing (WEAPON_FIST) est
//     toujours possede et a munitions infinies ; les autres se ramassent au sol
//     (pickups) et se selectionnent en boucle au bouton MENU (a pied). ---
static uint8_t curWeapon = WEAPON_FIST;
static bool    weaponOwned[WEAPON_COUNT] = { true, false, false, false, false, false };
static int16_t weaponAmmo[WEAPON_COUNT]  = { 0, 0, 0, 0, 0, 0 };

// Pickups d'armes poses dans le monde (objet au sol a ramasser en marchant
// dessus). Position voulue en TUILES ; setup() la snappe sur une case libre.
struct WeaponPickupDef { uint8_t tx, ty, weapon; };
static const WeaponPickupDef WEAPON_SPAWNS[] = {
  { 20, 22, WEAPON_PISTOL  },
  { 46, 30, WEAPON_SMG     },
  { 64, 46, WEAPON_SHOTGUN },
  { 30, 62, WEAPON_BAZOOKA },
  { 72, 72, WEAPON_GRENADE },
};
static const int NUM_PICKUPS = sizeof(WEAPON_SPAWNS) / sizeof(WEAPON_SPAWNS[0]);
static int16_t pickupPx[NUM_PICKUPS], pickupPy[NUM_PICKUPS];
static bool    pickupActive[NUM_PICKUPS];
static const int PICKUP_REACH = 6;      // px : distance de ramassage (centre a centre)

// Toast d'arme : petit bandeau central (nom de l'arme) qui apparait au
// changement/ramassage et disparait tout seul. >0 : frames restantes.
static uint16_t weaponToast = 0;
static const int WEAPON_TOAST_FRAMES = 55;   // ~2.2 s @ ~25 fps

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
// MISSION_DEAL est le seul dont le KILL est un TUEUR (poursuite) ; partout
// ailleurs le KILL est un PNJ qui erre/fuit pres du POI (cf. enterObjective).
#define MISSION_JOE  0
#define MISSION_DEAL 1
// Champs : { type, x, y, radius, requireCar, event, poi, text, doneText,
//            count, limit }. x,y = repli si POI absent (resolus au lancement).
// limit en frames (~25/s) : SURVIVE = duree a tenir ; sinon delai max (fail).
static const Objective OBJS_JOE[] = {
  { OBJ_KILL, 0, 0, 0, false, EV_NONE, "Les Quais",
    "Joe se planque aux Quais. Retrouve-le et fais-le taire.",
    "Beau boulot. Joe parlera plus." },
};
static const Objective OBJS_DEAL[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "La Casse",
    "Une caisse t'attend a la Casse. Va la prendre.", nullptr },
  { OBJ_GOTO,      0, 0, 14, true,  EV_MARCO_JOIN, "Chantier",
    "Passe prendre Marco au Chantier.", "Marco : roule, a Chinatown !" },
  { OBJ_GOTO,      0, 0, 16, true,  EV_MARCO_DIE, "Chinatown",
    "Conduis Marco a Chinatown.", "Un type surgit... Marco s'effondre !" },
  { OBJ_KILL,      0, 0,  0, false, EV_NONE, "Chinatown",
    "Tu peux pas laisser passer ca. Bute le tueur.", "Justice est faite." },
};
// --- Combat ---
static const Objective OBJS_FIGHT[] = {
  { OBJ_BEAT, 0, 0, 0, false, EV_NONE, nullptr,
    "Bagarre ! Mets 3 gars au tapis.", "Personne te cherche plus.", 3, 0 },
};
static const Objective OBJS_VENGEANCE[] = {
  { OBJ_KILL, 0, 0, 0, false, EV_NONE, "Chinatown",
    "Il se cache a Chinatown. Fais-lui la peau.", "Vengeance accomplie." },
};
static const Objective OBJS_CLEAN[] = {
  { OBJ_GOTO, 0, 0, 24, false, EV_NONE, "Chantier",
    "Le Chantier grouille de fouineurs. Fonce.", "Maintenant, nettoie." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Chantier",
    "Vire-en 5 du quartier.", "Le coin est calme.", 5, 0 },
};
static const Objective OBJS_WITNESS[] = {
  { OBJ_KILL, 0, 0, 0, false, EV_NONE, "Hopital",
    "Un temoin tourne pres de l'Hopital. Reduis-le au silence.",
    "Plus de temoin." },
};
// --- Conduite ---
static const Objective OBJS_TAXI[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "La Casse",
    "Taxi clandestin : prends la caisse a la Casse.", nullptr },
  { OBJ_GOTO,      0, 0, 16, true,  EV_NONE, "Hopital",
    "Depose le client a l'Hopital, et vite !", "Course payee !", 0, 1100 },
};
static const Objective OBJS_RACE[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "La Casse",
    "Au volant ! Le chrono tourne.", nullptr },
  { OBJ_GOTO,      0, 0, 16, true,  EV_NONE, "Chantier",
    "Checkpoint 1 : le Chantier.", "1/3 !", 0, 800 },
  { OBJ_GOTO,      0, 0, 16, true,  EV_NONE, "Chinatown",
    "Checkpoint 2 : Chinatown.", "2/3 !", 0, 800 },
  { OBJ_GOTO,      0, 0, 16, true,  EV_NONE, "Les Quais",
    "Checkpoint 3 : les Quais.", "Record battu !", 0, 800 },
};
static const Objective OBJS_DELIVERY[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "La Casse",
    "Un colis a livrer. Prends le vehicule.", nullptr },
  { OBJ_GOTO,      0, 0, 16, true,  EV_NONE, "Les Quais",
    "Livre le colis aux Quais avant la fin.", "Colis livre !", 0, 1100 },
};
static const Objective OBJS_RUN[] = {
  { OBJ_SURVIVE, 0, 0, 0, false, EV_NONE, nullptr,
    "Alerte ! Sème-le et tiens 30 secondes.", "Tu l'as seme. Tranquille.",
    0, 750 },
};
// --- Criminel ---
static const Objective OBJS_DEBT[] = {
  { OBJ_GOTO, 0, 0, 20, false, EV_NONE, "Chinatown",
    "Un mauvais payeur traine a Chinatown. Va le voir.", "Le voila." },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Chinatown",
    "Fais-lui cracher la dette : tabasse-le.", "Il paiera, crois-moi." },
};
static const Objective OBJS_RACKET[] = {
  { OBJ_GOTO, 0, 0, 20, false, EV_NONE, "Chinatown",
    "Tournee de racket. Commerce 1 : Chinatown.", "1 encaisse." },
  { OBJ_GOTO, 0, 0, 20, false, EV_NONE, "Chantier",
    "Commerce 2 : le Chantier.", "2 encaisses." },
  { OBJ_GOTO, 0, 0, 20, false, EV_NONE, "Les Quais",
    "Commerce 3 : les Quais.", "Caisse pleine !" },
};
static const Objective OBJS_STEAL[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "La Casse",
    "Vol de caisse : repère-la a la Casse et embarque.", nullptr },
  { OBJ_GOTO,      0, 0, 18, true,  EV_NONE, "Commissariat",
    "Ramène-la planquee derriere le Commissariat.", "Beau brelan." },
};
// --- Humour ---
static const Objective OBJS_ROADHOG[] = {
  { OBJ_BEAT, 0, 0, 0, true, EV_NONE, nullptr,
    "Le chauffard : ecrase 10 pietons !", "Carnage total. Bravo (?).", 10, 0 },
};
static const Objective OBJS_LOST[] = {
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Pompiers",
    "Livreur perdu : la planque est pres des Pompiers.", "Trouve ! Enfin." },
};
static const MissionDef MISSIONS[] = {
  { "Joe",              OBJS_JOE,      1 },
  { "Mauvaise affaire", OBJS_DEAL,     4 },
  { "Bagarre de rue",   OBJS_FIGHT,    1 },
  { "Vengeance",        OBJS_VENGEANCE,1 },
  { "Nettoyage",        OBJS_CLEAN,    2 },
  { "Temoin genant",    OBJS_WITNESS,  1 },
  { "Taxi clandestin",  OBJS_TAXI,     2 },
  { "Course folle",     OBJS_RACE,     4 },
  { "Livraison",        OBJS_DELIVERY, 2 },
  { "Cavale",           OBJS_RUN,      1 },
  { "Recouvrement",     OBJS_DEBT,     2 },
  { "Racket",           OBJS_RACKET,   3 },
  { "Vol de caisse",    OBJS_STEAL,    2 },
  { "Le chauffard",     OBJS_ROADHOG,  1 },
  { "Le livreur perdu", OBJS_LOST,     1 },
};
static const int NUM_MISSIONS = sizeof(MISSIONS) / sizeof(MISSIONS[0]);

// --- Telephones : UN par mission, repartis sur toute la carte (grille ~4x4).
//     Position voulue en TUILES ; setup() la snappe sur la case libre la plus
//     proche (la carte change a chaque regeneration). `mission` = index dans
//     MISSIONS (meme ordre que ci-dessus). ---
struct PhoneDef { uint8_t tx, ty; uint8_t mission; };
static const PhoneDef PHONES[] = {
  { 14, 14,  0 },   // Joe
  { 38, 14,  1 },   // Mauvaise affaire
  { 60, 14,  2 },   // Bagarre de rue
  { 82, 14,  3 },   // Vengeance
  { 14, 36,  4 },   // Nettoyage
  { 38, 36,  5 },   // Temoin genant
  { 60, 36,  6 },   // Taxi clandestin
  { 82, 36,  7 },   // Course folle
  { 14, 58,  8 },   // Livraison
  { 38, 58,  9 },   // Cavale
  { 60, 58, 10 },   // Recouvrement
  { 82, 58, 11 },   // Racket
  { 14, 80, 12 },   // Vol de caisse
  { 38, 80, 13 },   // Le chauffard
  { 60, 80, 14 },   // Le livreur perdu
};
static const int NUM_PHONES = sizeof(PHONES) / sizeof(PHONES[0]);
static int16_t phonePx[NUM_PHONES], phonePy[NUM_PHONES];   // positions monde (px)

// --- Etat runtime de la mission en cours. ---
static MissionRun missionRun = { 0, 0, false };
static uint16_t missionAnim = 0;          // compteur d'animation (clignotements)

// Copie runtime des objectifs de la mission active : les coords des objectifs
// references par un POI (champ `poi`) sont resolues ici au lancement, car la
// carte (donc les POI) change a chaque regeneration. curDef pointe sur curObjs.
static const int MAX_OBJS = 4;
static Objective curObjs[MAX_OBJS];
static MissionDef curDef = { "", curObjs, 0 };

// Compteurs de l'objectif courant : pietons mis KO et frames ecoulees.
static int      objBeat = 0;
static uint16_t objElapsed = 0;

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

  // Telephones : position resolue sur une case libre proche du decalage voulu.
  for (int i = 0; i < NUM_PHONES; i++) {
    int wx = PHONES[i].tx * TILE_W + TILE_W / 2;
    int wy = PHONES[i].ty * TILE_H + TILE_H / 2;
    int ox, oy;
    if (findFootSpot(wx, wy, ox, oy)) { wx = ox + PLAYER_W / 2; wy = oy + PLAYER_H / 2; }
    phonePx[i] = wx; phonePy[i] = wy;
  }

  // Armes : seul le poing au depart ; pickups poses sur une case libre proche
  // du decalage voulu (la carte change a chaque regeneration).
  curWeapon = WEAPON_FIST;
  for (int i = 0; i < WEAPON_COUNT; i++) { weaponOwned[i] = (i == WEAPON_FIST); weaponAmmo[i] = 0; }
  for (int i = 0; i < NUM_PICKUPS; i++) {
    int wx = WEAPON_SPAWNS[i].tx * TILE_W + TILE_W / 2;
    int wy = WEAPON_SPAWNS[i].ty * TILE_H + TILE_H / 2;
    int ox, oy;
    if (findFootSpot(wx, wy, ox, oy)) { wx = ox + PLAYER_W / 2; wy = oy + PLAYER_H / 2; }
    pickupPx[i] = wx; pickupPy[i] = wy; pickupActive[i] = true;
  }

  // Missions au repos : les telephones sonnent quand on s'en approche.
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

// Blit d'un sprite d'arme 6x6 centre en (worldCx,worldCy) px monde. Pas de
// recolor : couleurs telles quelles ; WEAPON_TRANSP saute. Sert au sol ET au HUD
// (passer camX=camY=0 et des coords ecran pour le HUD).
static void blitWeapon(int camX, int camY, int worldCx, int worldCy, uint8_t weapon) {
  const uint16_t *src = weaponSprites[weapon];
  int ox = worldCx - camX - WEAPON_BOX / 2;
  int oy = worldCy - camY - WEAPON_BOX / 2;
  for (int ry = 0; ry < WEAPON_BOX; ry++) {
    int y = oy + ry;
    if (y < 0 || y >= SCREEN_H) continue;
    uint16_t *row = fb + y * SCREEN_W;
    const uint16_t *srow = src + ry * WEAPON_BOX;
    for (int rx = 0; rx < WEAPON_BOX; rx++) {
      uint16_t c = srow[rx];
      if (c == WEAPON_TRANSP) continue;
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

// Effet d'attaque : pour le poing, une petite croix blanche ~5 px devant ; pour
// une arme a feu, un trait blanc partant du joueur jusqu'a la portee de l'arme
// (dans la direction du regard). Dessine pendant punchTimer frames.
static void blitAttackFx(int camX, int camY) {
  int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
  if (curWeapon == WEAPON_FIST) {
    int fx = pcx + AI_DX[playerDir] * 5 - camX;
    int fy = pcy + AI_DY[playerDir] * 5 - camY;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        if (dx != 0 && dy != 0) continue;        // petite croix
        int x = fx + dx, y = fy + dy;
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
          fb[y * SCREEN_W + x] = 0xFFFF;
      }
    return;
  }
  int reach = (int)WEAPONS[curWeapon].reach;     // trait de tir le long du regard
  for (int t = 4; t <= reach; t++) {
    int x = pcx + AI_DX[playerDir] * t - camX;
    int y = pcy + AI_DY[playerDir] * t - camY;
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
      fb[y * SCREEN_W + x] = 0xFFFF;
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

// Met un pieton au sol (KO) + comptabilise l'objectif BEAT. Mutualise poing/arme.
static void knockDownPed(AiPed &p) {
  p.state = 1; p.downTimer = PED_DOWN_FRAMES;
  if (missionRun.active) objBeat++;
}

// Attaque a pied avec l'arme courante. Cone (portee/largeur) propre a l'arme
// (cf. weapons.h) : le poing frappe le pieton le plus proche devant ; les armes
// a feu portent plus loin, et les armes de zone (pompe/bazooka/grenade) mettent
// au sol TOUS les pietons du cone. Consomme 1 munition (sauf poing) ; arme videe
// => retiree de l'arsenal et retour au poing. Son + effet visuel.
static void tryAttack() {
  const WeaponDef &wd = WEAPONS[curWeapon];
  // Arme a feu vide : declic, aucune frappe (le poing reste toujours utilisable).
  if (!weaponInfinite(curWeapon) && weaponAmmo[curWeapon] <= 0) {
    gb.sound.playTick();
    return;
  }
  punchTimer = PUNCH_FX_FRAMES;
  if (curWeapon == WEAPON_FIST) gb.sound.playTick();
  else gb.sound.tone(110, 50);               // bang court (grave) pour les armes

  int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
  if (wd.area) {
    // Zone : tous les pietons debout dans le cone tombent.
    for (int i = 0; i < NUM_AI_PEDS; i++) {
      if (!(aiPeds[i].active && aiPeds[i].state == 0)) continue;
      if (combatInCone(aiPeds[i].x, aiPeds[i].y, pcx, pcy, playerDir, wd.reach, wd.side))
        knockDownPed(aiPeds[i]);
    }
  } else {
    // Mono-cible : le pieton le plus proche devant dans le cone.
    float px[NUM_AI_PEDS], py[NUM_AI_PEDS];
    bool act[NUM_AI_PEDS];
    for (int i = 0; i < NUM_AI_PEDS; i++) {
      px[i] = aiPeds[i].x; py[i] = aiPeds[i].y;
      act[i] = aiPeds[i].active && aiPeds[i].state == 0;
    }
    int hit = combatConeTarget(px, py, act, NUM_AI_PEDS, pcx, pcy, playerDir, wd.reach, wd.side);
    if (hit >= 0) knockDownPed(aiPeds[hit]);
  }
  // La cible de mission est frappable comme un pieton (toujours dans le cone).
  if (missionRun.active && target.active &&
      combatInCone(target.x, target.y, pcx, pcy, playerDir, wd.reach, wd.side))
    killTarget(pcx, pcy);

  // Decompte des munitions ; arme videe -> retiree, retour au poing.
  if (!weaponInfinite(curWeapon) && --weaponAmmo[curWeapon] <= 0) {
    weaponOwned[curWeapon] = false;
    curWeapon = WEAPON_FIST;
  }
}

// Met a jour le trafic autour du point de vue (fcx,fcy = centre suivi, px monde).
// Recyclage des entites trop loin (culling), pas IA, collisions avec le joueur.
static void aiUpdate(int fcx, int fcy) {
  const int rec2 = RECYCLE_DIST * RECYCLE_DIST;
  // Collision voiture joueur : seuils en px (sommes de demi-boites).
  const float COL_CC = 6.0f;   // voiture-voiture (hitbox < sprite)
  const float COL_CP = 4.0f;   // voiture-pieton
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
        if (missionRun.active) objBeat++;    // ecrasement comptabilise (BEAT)
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

// Fait apparaitre Joe (errance + fuite) sur une tuile marchable proche du POI
// (wx,wy). Comme spawnTargetWander mais ancre autour du quartier cible.
static bool spawnTargetWanderNear(int wx, int wy) {
  int tx, ty;
  if (!aiFindWalkTileNear(wx, wy, tx, ty)) return spawnTargetWander(wx, wy);
  aiPlace(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
          target.tgtx, target.tgty, tx, ty, aiIsWalkable, aiRng);
  target.frame = 0; target.animTimer = 0; target.phase = T_WANDER;
  target.loseTimer = 0; target.active = true; target.chase = false;
  return true;
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

// Index du POI nomme `name` dans la table exportee, ou -1 si absent.
static int findPoi(const char *name) {
#if CITY_NUM_POIS > 0
  if (name) for (int i = 0; i < CITY_NUM_POIS; i++)
    if (strcmp(cityPois[i].name, name) == 0) return i;
#else
  (void)name;
#endif
  return -1;
}

// Plus petit POI (bbox) contenant la tuile (tx,ty), ou -1. Les stamps (3x3)
// l'emportent ainsi sur les grands quartiers en cas de chevauchement.
static int poiAtTile(int tx, int ty) {
#if CITY_NUM_POIS > 0
  int best = -1; long bestArea = 0;
  for (int i = 0; i < CITY_NUM_POIS; i++) {
    const CityPoi &p = cityPois[i];
    if (tx < p.x0 || tx > p.x1 || ty < p.y0 || ty > p.y1) continue;
    long area = (long)(p.x1 - p.x0 + 1) * (p.y1 - p.y0 + 1);
    if (best < 0 || area < bestArea) { best = i; bestArea = area; }
  }
  return best;
#else
  (void)tx; (void)ty; return -1;
#endif
}

// Construit curDef/curObjs : copie les objectifs de la mission m et resout les
// coords des objectifs references par un POI (champ poi). Repli sur les coords
// litterales si le POI est absent de la carte courante.
static void buildMissionRuntime(uint8_t m) {
  const MissionDef &src = MISSIONS[m];
  uint8_t n = src.count <= MAX_OBJS ? src.count : MAX_OBJS;
  for (uint8_t i = 0; i < n; i++) {
    curObjs[i] = src.objectives[i];
    int pi = findPoi(curObjs[i].poi);
    if (pi >= 0) { curObjs[i].x = cityPois[pi].tx; curObjs[i].y = cityPois[pi].ty; }
  }
  curDef.title = src.title;
  curDef.count = n;
}

// Active l'objectif courant : narration + spawn des entites necessaires.
static void enterObjective() {
  const MissionDef &def = curDef;
  const Objective &o = def.objectives[missionRun.step];
  narrate(o.text);
  objBeat = 0; objElapsed = 0;             // compteurs propres a cet objectif
  if (o.type == OBJ_ENTER_CAR) {
    mCar.x = o.x; mCar.y = o.y; mCar.angle = 0.0f; mCar.vx = 0.0f; mCar.vy = 0.0f;
    mCarActive = true;
  } else if (o.type == OBJ_KILL && !target.active) {
    if (missionRun.def == MISSION_DEAL) spawnTargetAt(o.x, o.y);
    else                                spawnTargetWanderNear(o.x, o.y);
  } else if (o.type == OBJ_SURVIVE) {
    int pcx = driving ? (int)car.x : playerX + PLAYER_W / 2;
    int pcy = driving ? (int)car.y : playerY + PLAYER_H / 2;
    spawnTargetAt(pcx, pcy);               // un poursuivant te colle (pression)
  } else if (o.type == OBJ_GOTO && missionRun.def == MISSION_DEAL && missionRun.step == 1) {
    marcoWaiting = true;                 // Marco attend au coin de la rue
  }
}

// Demarre une mission (telephone decroche).
static void startMission(uint8_t m) {
  missionRun.def = m; missionRun.step = 0; missionRun.active = true;
  buildMissionRuntime(m);                 // resout les coords POI de la mission
  target.active = false; marcoWaiting = false; marcoAboard = false;
  mCarActive = false;
  narrate(curDef.title);                  // annonce le nom de la mission
  enterObjective();
  gb.sound.playOK();
}

// Mission echouee (limite de temps depassee) : message + retour au repos.
static void failMission(const char *msg) {
  narrate(msg);
  missionRun.active = false;
  target.active = false; marcoWaiting = false; marcoAboard = false;
  mCarActive = false; carIsMission = false;
  gb.sound.playCancel();
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
  const MissionDef &def = curDef;
  MissionState s;
  s.actorCx = driving ? (int)car.x : playerX + PLAYER_W / 2;
  s.actorCy = driving ? (int)car.y : playerY + PLAYER_H / 2;
  s.driving = driving;
  s.inMissionCar = driving && carIsMission;
  s.targetAlive = target.active;
  s.beatCount = objBeat;
  s.elapsed = objElapsed;
  const Objective &cur = def.objectives[missionRun.step];
  if (missionTimedOut(cur, objElapsed)) { failMission("Trop tard ! Mission ratee."); return; }
  if (!missionObjectiveDone(cur, s)) return;

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

// Armes au sol : blit du sprite 6x6 a leur position monde (clipping ecran).
static void drawWeaponPickups(int camX, int camY) {
  for (int i = 0; i < NUM_PICKUPS; i++)
    if (pickupActive[i])
      blitWeapon(camX, camY, pickupPx[i], pickupPy[i], WEAPON_SPAWNS[i].weapon);
}

// Ramassage : le joueur a pied passe sur un pickup actif -> arme possedee,
// munitions creditees, arme equipee automatiquement, narration. (pcx,pcy) px.
static void tryPickupWeapons(int pcx, int pcy) {
  for (int i = 0; i < NUM_PICKUPS; i++) {
    if (!pickupActive[i]) continue;
    long d = (long)(pcx - pickupPx[i]) * (pcx - pickupPx[i])
           + (long)(pcy - pickupPy[i]) * (pcy - pickupPy[i]);
    if (d > (long)PICKUP_REACH * PICKUP_REACH) continue;
    uint8_t w = WEAPON_SPAWNS[i].weapon;
    weaponOwned[w] = true;
    weaponAmmo[w] += WEAPONS[w].ammoPickup;
    curWeapon = w;                              // equipee a la prise
    weaponToast = WEAPON_TOAST_FRAMES;          // toast central "Pistolet", ...
    pickupActive[i] = false;
    gb.sound.playOK();
  }
}

// Dessine les deux telephones fixes quand la mission est au repos (decrochables).
static void drawPhones(int camX, int camY) {
  if (missionRun.active) return;
  for (int i = 0; i < NUM_PHONES; i++)
    drawPhone1(camX, camY, phonePx[i], phonePy[i]);
}

// Voiture de mission garee (objectif ENTER_CAR) : orange, orientee est.
static void drawMissionCar(int camX, int camY) {
  if (!mCarActive) return;
  blitCar(camX, camY, (int)mCar.x, (int)mCar.y, AI_CAR_FRAME[DIR_EAST], MISSION_CAR_COLOR);
}

// Marco : pieton cyan debout au marqueur tant qu'il n'est pas pris.
static void drawMarco(int camX, int camY) {
  if (!marcoWaiting || !missionRun.active) return;
  const Objective &o = curObjs[1];        // Marco attend au marqueur de l'objectif 1
  blitPed(camX, camY, o.x, o.y, DIR_SOUTH, 0, MARCO_COLOR);
}

// Marqueur de destination clignotant (objectif GOTO / ENTER_CAR).
static void drawMarker(int camX, int camY) {
  if (!missionRun.active) return;
  const Objective &o = curObjs[missionRun.step];
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
  const Objective &o = curObjs[missionRun.step];
  int tx, ty; uint16_t col;
  if (o.type == OBJ_BEAT || o.type == OBJ_SURVIVE) return;   // pas de destination
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

// HUD : nom du POI ou se trouve l'entite suivie (bandeau haut-gauche, 2e ligne
// sous la barre de stats). Affiche tant qu'on est dans la bbox d'un POI. px.
static void drawPoiHud(int fcx, int fcy) {
#if CITY_NUM_POIS > 0
  int pi = poiAtTile(fcx >> 3, fcy >> 3);
  if (pi < 0) return;
  const char *name = cityPois[pi].name;
  int w = (int)strlen(name) * 4 + 1;
  for (int y = 8; y < 15; y++)
    for (int x = 0; x < w && x < SCREEN_W; x++) fb[y * SCREEN_W + x] = 0x0000;
  gb.display.setColor(WHITE);
  gb.display.setCursor(1, 9);
  gb.display.print(name);
#else
  (void)fcx; (void)fcy;
#endif
}

// Indicateur de mission (haut-droite) : compteur de KO "n/N" pour OBJ_BEAT, ou
// secondes restantes "Ns" pour un objectif chronometre (SURVIVE / limite).
static void drawMissionStatus() {
  if (!missionRun.active) return;
  const Objective &o = curObjs[missionRun.step];
  char buf[8];
  if (o.type == OBJ_BEAT) {
    int n = objBeat > o.count ? o.count : objBeat;
    snprintf(buf, sizeof(buf), "%d/%d", n, (int)o.count);
  } else if (o.limit > 0) {
    int left = (int)o.limit - (int)objElapsed; if (left < 0) left = 0;
    snprintf(buf, sizeof(buf), "%ds", (left + 24) / 25);   // ~25 fps -> secondes
  } else {
    return;
  }
  int w = (int)strlen(buf) * 4 + 1;
  for (int y = 8; y < 15; y++)
    for (int x = SCREEN_W - w; x < SCREEN_W; x++)
      if (x >= 0) fb[y * SCREEN_W + x] = 0x0000;
  gb.display.setColor(WHITE);
  gb.display.setCursor(SCREEN_W - w + 1, 9);
  gb.display.print(buf);
}

// Texte avec ombre portee (noir decale +1,+1 puis blanc) : lisible sur n'importe
// quel decor sans fond opaque. Police 3x5 (~4 px/caractere).
static void printShadow(int x, int y, const char *s) {
  gb.display.setColor(BLACK); gb.display.setCursor(x + 1, y + 1); gb.display.print(s);
  gb.display.setColor(WHITE); gb.display.setCursor(x, y);         gb.display.print(s);
}

// Icone d'arme 6x6 en coords ecran avec contour noir 1 px (halo) : se detache du
// fond sans rectangle opaque. (sx,sy) = coin haut-gauche.
static void blitWeaponHudIcon(int sx, int sy, uint8_t weapon) {
  const uint16_t *src = weaponSprites[weapon];
  static const int8_t nb[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
  for (int ry = 0; ry < WEAPON_BOX; ry++)      // passe 1 : halo noir autour
    for (int rx = 0; rx < WEAPON_BOX; rx++) {
      if (src[ry * WEAPON_BOX + rx] == WEAPON_TRANSP) continue;
      for (int k = 0; k < 4; k++) {
        int x = sx + rx + nb[k][0], y = sy + ry + nb[k][1];
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0x0000;
      }
    }
  for (int ry = 0; ry < WEAPON_BOX; ry++)      // passe 2 : sprite par-dessus
    for (int rx = 0; rx < WEAPON_BOX; rx++) {
      uint16_t c = src[ry * WEAPON_BOX + rx];
      if (c == WEAPON_TRANSP) continue;
      int x = sx + rx, y = sy + ry;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = c;
    }
}

// Petite icone 5x5 a partir d'un motif ('x' = plein, autre = vide) en coords
// ecran, avec halo noir 1 px (lisible sur le decor sans fond opaque).
static void blitIcon5(int sx, int sy, const char *const rows[5], uint16_t color) {
  static const int8_t nb[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
  for (int ry = 0; ry < 5; ry++)               // passe 1 : halo
    for (int rx = 0; rx < 5; rx++) {
      if (rows[ry][rx] != 'x') continue;
      for (int k = 0; k < 4; k++) {
        int x = sx + rx + nb[k][0], y = sy + ry + nb[k][1];
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0x0000;
      }
    }
  for (int ry = 0; ry < 5; ry++)               // passe 2 : plein
    for (int rx = 0; rx < 5; rx++) {
      if (rows[ry][rx] != 'x') continue;
      int x = sx + rx, y = sy + ry;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = color;
    }
}

// Motifs 5x5 (cœur, etoile).
static const char *const ICON_HEART[5] = { "x x x", "xxxxx", "xxxxx", " xxx ", "  x  " };
static const char *const ICON_STAR[5]  = { "  x  ", " xxx ", "xxxxx", " xxx ", " x x " };

// Stats en dur pour l'instant (a brancher plus tard sur de vrais systemes).
static const int HUD_HEARTS_MAX = 3, HUD_HEARTS = 3;   // vie : 3 cœurs (pleins)
static const int HUD_STARS_MAX  = 5, HUD_STARS  = 5;   // recherche police : 5 etoiles
static const uint16_t HEART_FULL = 0xF800, HEART_EMPTY = 0x4208;  // rouge / gris sombre
static const uint16_t STAR_FULL  = 0xFFE0, STAR_EMPTY  = 0x4208;  // jaune / gris sombre

// Barre de stats du haut (GTA2-like) sur fond transparent : cœurs a gauche,
// etoiles a droite, arme + munitions au centre (a pied), toast central au
// changement d'arme. Dessinee au ras du haut (y=0).
static void drawTopHud() {
  for (int i = 0; i < HUD_HEARTS_MAX; i++)      // cœurs (haut-gauche)
    blitIcon5(1 + i * 6, 1, ICON_HEART, i < HUD_HEARTS ? HEART_FULL : HEART_EMPTY);
  const int starsX = (SCREEN_W - HUD_STARS_MAX * 6) / 2;
  for (int i = 0; i < HUD_STARS_MAX; i++)       // etoiles (centre)
    blitIcon5(starsX + i * 6, 1, ICON_STAR, i < HUD_STARS ? STAR_FULL : STAR_EMPTY);
  if (!driving) {                               // arme courante (haut-droite)
    const int sx = SCREEN_W - WEAPON_BOX - 1, sy = 0;
    blitWeaponHudIcon(sx, sy, curWeapon);
    if (!weaponInfinite(curWeapon)) {
      char buf[6];
      snprintf(buf, sizeof(buf), "%d", (int)weaponAmmo[curWeapon]);
      printShadow(sx - (int)strlen(buf) * 4 - 1, 1, buf);
    }
  }
  if (weaponToast > 0) {                         // toast central (nom de l'arme)
    const char *nm = WEAPONS[curWeapon].name;
    printShadow((SCREEN_W - (int)strlen(nm) * 4) / 2, 26, nm);
  }
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
    // Ramassage d'une arme au sol (en marchant dessus) et changement d'arme en
    // boucle (MENU). MENU ne sert a sortir de la voiture qu'au volant : libre ici.
    tryPickupWeapons(playerX + PLAYER_W / 2, playerY + PLAYER_H / 2);
    if (gb.buttons.pressed(BUTTON_MENU)) {
      uint8_t w = weaponCycleNext(weaponOwned, curWeapon);
      if (w != curWeapon) { curWeapon = w; weaponToast = WEAPON_TOAST_FRAMES; }
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
          long dP = (long)(pcx - phonePx[i]) * (pcx - phonePx[i])
                  + (long)(pcy - phonePy[i]) * (pcy - phonePy[i]);
          if (dP <= (long)PHONE_REACH * PHONE_REACH) {
            startMission(PHONES[i].mission);           // mission dediee a ce telephone
            answered = true; break;
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
          tryAttack();                       // aucune voiture a portee -> arme courante
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

  // Mission : chrono de l'objectif, deplacement de la cible, avancement.
  if (missionRun.active && objElapsed < 0xFFFF) objElapsed++;
  missionUpdate(focusX, focusY);
  missionProgress();
  if (targetDownTimer > 0) targetDownTimer--;
  missionAnim++;                               // clignotement marqueurs/telephones
  if (weaponToast > 0) weaponToast--;          // toast d'arme : disparait tout seul
  narrUpdate();
  // Sonnerie : au repos, le telephone fixe le plus proche sonne s'il est dans
  // le cercle audible. Melodie deux tons (sinon muet).
  if (!missionRun.active) {
    int near = -1; long bestd = (long)PHONE_HEAR_RANGE * PHONE_HEAR_RANGE;
    for (int i = 0; i < NUM_PHONES; i++) {
      long dP = (long)(focusX - phonePx[i]) * (focusX - phonePx[i])
              + (long)(focusY - phonePy[i]) * (focusY - phonePy[i]);
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
  drawWeaponPickups(camX, camY);
  drawMissionCar(camX, camY);
  drawMarco(camX, camY);
  drawTarget(camX, camY);
  drawPhones(camX, camY);
  drawMarker(camX, camY);
  drawCar(camX, camY);
  if (!driving) drawPlayer(camX, camY);
  if (!driving && punchTimer > 0) { blitAttackFx(camX, camY); punchTimer--; }

  // HUD : nom du POI courant (haut), fleche vers l'objectif/cible, narration.
  drawPoiHud(focusX, focusY);
  drawMissionArrow(camX, camY, focusX, focusY);
  drawMissionStatus();
  drawTopHud();                                // barre stats : cœurs, etoiles, arme
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
