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
#include "wanted.h"

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

// --- argent : le joueur ramasse des billets (pietons abattus, plus tard
//     missions/braquages) et depense en services/biens dans les POI interactifs
//     (cf. POI.md). Affiche dans le HUD sous les cœurs. ---
static int32_t playerMoney = 0;

// --- butin au sol : objets laches par un pieton abattu (cf. dropLoot), ramasses
//     a pied en marchant dessus. Pool tournant : un nouveau drop ecrase le plus
//     ancien si le pool est plein. ---
enum LootKind : uint8_t { LOOT_MONEY = 0, LOOT_AMMO = 1 };
struct Loot {
  int16_t x, y;      // centre px monde
  uint8_t kind;      // LootKind
  uint8_t weapon;    // LOOT_AMMO : arme creditee
  int16_t amount;    // MONEY : dollars ; AMMO : munitions
  bool    active;
};
static const int NUM_LOOT = 6;
static Loot loots[NUM_LOOT];
static uint8_t lootNext = 0;             // index d'ecrasement (pool plein)
static const int LOOT_REACH = 6;         // px : distance de ramassage

// --- projectiles : petits pixels qui filent dans la direction du regard quand
//     on tire (purement visuel ; la frappe reste instantanee par cone, cf.
//     tryAttack). Remplace l'ancien gros trait blanc. ---
struct Bullet {
  float   x, y, vx, vy;  // px monde
  uint8_t life;          // frames restantes
  bool    active;
};
static const int   NUM_BULLETS  = 10;
static const float BULLET_SPEED = 4.0f;  // px/frame
static Bullet bullets[NUM_BULLETS];

// --- vie du joueur : cœurs perdus sous les balles ennemies / explosions ;
//     restauree a la mort (hopital) ou a l'arrestation (commissariat). ---
static const int PLAYER_HEARTS_MAX = 3;
static int       playerHearts = PLAYER_HEARTS_MAX;
static const uint8_t PLAYER_HURT_COOLDOWN = 25;  // ~1 s d'invuln. apres un coup
static uint8_t   playerHurtTimer = 0;

// --- recherche police (etoiles). wanted.h = machine d'etat pure. ---
static WantedState wanted = { 0, 0, 0, 0 };

// --- vie de la voiture pilotee : accidents + tirs l'usent ; a 0 elle explose
//     (le joueur dedans meurt). CAR_MAX_HP fixe a l'embarquement. ---
static const int16_t CAR_MAX_HP   = 30;
static int16_t       carHp        = CAR_MAX_HP;
static const int16_t CAR_HIT_DMG  = 6;    // degats d'un tir d'arme sur une voiture
static const int16_t CAR_AREA_DMG = 12;   // degats d'une arme de zone (pompe/bazooka)
static const int16_t CAR_CRASH_DMG= 2;    // degats par accident (collision voiture)
static const uint8_t CAR_CRASH_COOLDOWN = 12;  // frames entre deux comptages d'accident
static uint8_t       carCrashTimer = 0;

// --- police : portees de poursuite/arrestation/tir (px), cadence de tir. ---
static const int COP_ARREST_DIST  = 7;    // px : contact = arrestation (a pied)
static const int COP_SHOOT_RANGE  = 60;   // px : portee de tir du policier
static const uint16_t COP_SHOOT_PERIOD = 35;  // frames entre deux balles
static const int COP_SPAWN_PCT    = 45;   // % de pietons remplaces par des flics si recherche

// --- explosion (voiture) : un seul slot, anime quelques frames (cercle de feu). ---
static int     boomX = 0, boomY = 0;
static uint8_t boomTimer = 0;
static const uint8_t BOOM_FRAMES = 12;

// --- bandeau plein ecran a la mort / l'arrestation (facon "WASTED"/"BUSTED"). ---
static const char *overlayMsg = nullptr;
static uint16_t    overlayTimer = 0;
static const uint16_t OVERLAY_FRAMES = 55;   // ~2.2 s

// --- sequences cinematiques : mort, arrestation, repeinture. Pendant une
//     sequence le joueur est FIGE (aucun input) mais le monde (IA, police,
//     fumee, explosion...) continue de tourner. Petite machine d'etats par
//     phases + minuterie. La teleportation / revie se fait DERRIERE l'ecran
//     noir, en fin de sequence (et non plus instantanement comme avant). ---
enum { SEQ_NONE, SEQ_WASTED, SEQ_BUSTED, SEQ_SPRAY, SEQ_HEAL, SEQ_CRUSH };
enum { PH_EXPLODE, PH_MSG, PH_FADE, PH_IN, PH_SPRAY, PH_OUT,
       PH_HEAL, PH_CRANE, PH_CRUSH, PH_EJECT };
static uint8_t  seqKind  = SEQ_NONE;
static uint8_t  seqPhase = 0;
static uint16_t seqTimer = 0;
static const char *seqPoi = nullptr;            // POI de reapparition (mort/arrestation)
static const uint16_t SEQ_MSG_FRAMES   = 55;    // ~2.2 s : bandeau MORT/ARRETE
static const uint16_t SEQ_FADE_FRAMES  = 22;    // ~0.9 s : ecran noir avant TP
static const uint16_t SEQ_IN_FRAMES    = 26;    // voiture qui rentre dans le garage
static const uint16_t SEQ_SPRAY_FRAMES = 48;    // bombe de peinture + attente
static const uint16_t SEQ_OUT_FRAMES   = 16;    // voiture qui ressort repeinte
static const uint16_t SEQ_HEAL_FRAMES  = 40;    // ~1.6 s : soin (croix verte + jingle)
static const uint16_t SEQ_CRANE_FRAMES = 34;    // grue qui descend + saisit la caisse
static const uint16_t SEQ_CRUSH_FRAMES = 30;    // broyage de l'epave
static const uint16_t SEQ_EJECT_FRAMES = 16;    // prime affichee + ejection
static const int32_t  HEAL_COST   = 40;         // $ d'un soin complet a l'hopital
static bool           hospInside  = false;      // dans la bbox hopital a la frame -1
static int32_t        crushReward = 0;          // prime de la casse (broyage), pour l'affichage

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
  int16_t hp;          // points de vie (tir/accidents) -> explosion a 0
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
  uint8_t hp;          // 3 coups de poing pour tomber ; 1 balle suffit
  bool isCop;          // policier (bleu) : poursuit/arrete/tire quand recherche
  uint16_t shootTimer; // cadence de tir du policier (recharge entre deux balles)
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

// --- Telephones de la TRAME PRINCIPALE (couleur rouge, distincts des cabines
//     bleues de missions secondaires). Disperses sur la carte. Pour l'instant
//     ILS NE SONNENT PAS : ce sont des points d'ancrage poses d'avance pour la
//     future mission scenarisee ; quand elle sera branchee, l'un d'eux sonnera
//     et fera avancer l'histoire. Position voulue en TUILES, snappee sur le
//     trottoir le plus proche dans setup() (cf. findSidewalkSpot). ---
struct StoryPhoneDef { uint8_t tx, ty; };
static const StoryPhoneDef STORY_PHONES[] = {
  { 26, 25 },
  { 70, 25 },
  { 26, 70 },
  { 70, 70 },
};
static const int NUM_STORY_PHONES = sizeof(STORY_PHONES) / sizeof(STORY_PHONES[0]);
static int16_t storyPx[NUM_STORY_PHONES], storyPy[NUM_STORY_PHONES];  // px monde

// Couleurs de corps de cabine : bleu = missions secondaires, rouge = trame.
static const uint16_t PHONE_BODY_MISSION = 0x019F;   // bleu
static const uint16_t PHONE_BODY_STORY   = 0xC800;   // rouge fonce

// --- Pay'n'Spray : garages eparpilles, accessibles EN VOITURE. Position voulue
//     en TUILES ; setup() la snappe sur la case ROUTE la plus proche (sinon case
//     libre) pour qu'on puisse y entrer en caisse. Rouler dessus repeint la
//     voiture (nouvelle couleur) et remet la recherche police a zero (debit $).
//     Positions provisoires sur une grille large -- a ajuster (cf. demande). ---
struct SprayDef { uint8_t tx, ty; };
static const SprayDef SPRAYS[] = {
  { PLAYER_START_X, PLAYER_START_Y },   // un garage tout pres du spawn (repere)
  { 24, 24 }, { 72, 24 }, { 48, 48 }, { 24, 72 }, { 72, 72 },
};
static const int NUM_SPRAYS = sizeof(SPRAYS) / sizeof(SPRAYS[0]);
static int16_t sprayPx[NUM_SPRAYS], sprayPy[NUM_SPRAYS];   // px monde (snappes)
static const int   SPRAY_REACH = 10;    // px : rayon de declenchement (centre voiture)
static const int32_t SPRAY_COST = 50;   // $ preleve si on en a (sinon gratuit)
static bool sprayInside = false;         // dans la zone a la frame precedente (detection d'entree)

// --- AMU Nation : armureries accessibles A PIED. Comme les Pay'n'Spray, la
//     position voulue est en TUILES et snappee sur le TROTTOIR le plus proche
//     dans setup() (findSidewalkSpot). S'en approcher et presser A ouvre le
//     magasin (UI modale). Positions provisoires (a ajuster). ---
struct AmmuDef { uint8_t tx, ty; };
static const AmmuDef AMMUS[] = {
  { PLAYER_START_X + 3, PLAYER_START_Y },   // une armurerie pres du spawn (repere)
  { 36, 36 }, { 60, 60 },
};
static const int NUM_AMMUS = sizeof(AMMUS) / sizeof(AMMUS[0]);
static int16_t ammuPx[NUM_AMMUS], ammuPy[NUM_AMMUS];   // px monde (snappes)
static const int AMMU_REACH = 12;        // px : portee d'ouverture (centre joueur)

// Tarifs : prix d'achat de l'arme (1re fois) et prix d'un rechargement (1 lot de
// munitions = WEAPONS[w].ammoPickup). Index par WeaponId ; FIST a 0 (jamais vendu).
static const int32_t WEAPON_PRICE[WEAPON_COUNT] = { 0, 100, 250, 200, 600, 300 };
static const int32_t AMMO_PRICE[WEAPON_COUNT]   = { 0,  40, 100,  80, 250, 120 };

// Etat du magasin (UI modale, monde gele pendant l'achat).
static bool    shopOpen = false;
static uint8_t shopSel  = WEAPON_PISTOL;   // arme surlignee (WEAPON_PISTOL..COUNT-1)

// La Casse (broyeur) : amener sa caisse sur la zone marquee la broie contre une
// prime (selon les PV). carGone = la voiture du joueur n'existe plus (broyee) :
// plus dessinee, plus re-montable tant qu'on n'en vole pas une autre.
static const int CASSE_REACH = 14;         // px : rayon de la zone de depose
static bool      casseInside = false;      // sur la zone a la frame -1 (detection d'entree)
static bool      carGone     = false;      // voiture du joueur broyee / inexistante

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
static int  findPoi(const char *name);
static void hurtPlayer(int dmg);
static void explodeCarAt(int wx, int wy);

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

// Cherche le centre (px monde) d'une tuile trottoir (PAVEMENT) proche de
// (cx,cy), en spirale. Les cabines doivent etre POSEES sur un trottoir pour
// rester accessibles a pied. Renvoie false si aucun trottoir dans le rayon.
static bool findSidewalkSpot(int cx, int cy, int &ox, int &oy) {
  int ctx = cx >> 3, cty = cy >> 3;
  for (int r = 0; r <= 14; r++) {
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (r > 0 && abs(dx) != r && abs(dy) != r) continue;   // anneau
        int tx = ctx + dx, ty = cty + dy;
        if (tx < 0 || tx >= CITY_W || ty < 0 || ty >= CITY_H) continue;
        if (cityMap[ty * CITY_W + tx] == TILE_PAVEMENT) {
          ox = tx * TILE_W + TILE_W / 2;
          oy = ty * TILE_H + TILE_H / 2;
          return true;
        }
      }
    }
  }
  return false;
}

// Cherche le centre (px monde) d'une tuile ROUTE (carrossable) proche de (cx,cy),
// en spirale. Les Pay'n'Spray doivent etre accessibles EN VOITURE. false si
// aucune route dans le rayon.
static bool findRoadSpot(int cx, int cy, int &ox, int &oy) {
  int ctx = cx >> 3, cty = cy >> 3;
  for (int r = 0; r <= 24; r++) {
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (r > 0 && abs(dx) != r && abs(dy) != r) continue;   // anneau
        int tx = ctx + dx, ty = cty + dy;
        if (aiIsDrivable(cityMap, CITY_W, CITY_H, tx, ty)) {
          ox = tx * TILE_W + TILE_W / 2;
          oy = ty * TILE_H + TILE_H / 2;
          return true;
        }
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

  // Telephones : cabine POSEE sur le trottoir le plus proche du decalage voulu
  // (acces a pied garanti). Repli sur une case libre si aucun trottoir trouve.
  for (int i = 0; i < NUM_PHONES; i++) {
    int wx = PHONES[i].tx * TILE_W + TILE_W / 2;
    int wy = PHONES[i].ty * TILE_H + TILE_H / 2;
    int ox, oy;
    if (findSidewalkSpot(wx, wy, ox, oy)) { wx = ox; wy = oy; }
    else if (findFootSpot(wx, wy, ox, oy)) { wx = ox + PLAYER_W / 2; wy = oy + PLAYER_H / 2; }
    phonePx[i] = wx; phonePy[i] = wy;
  }

  // Cabines de la trame principale : meme snap trottoir (muettes pour l'instant).
  for (int i = 0; i < NUM_STORY_PHONES; i++) {
    int wx = STORY_PHONES[i].tx * TILE_W + TILE_W / 2;
    int wy = STORY_PHONES[i].ty * TILE_H + TILE_H / 2;
    int ox, oy;
    if (findSidewalkSpot(wx, wy, ox, oy)) { wx = ox; wy = oy; }
    else if (findFootSpot(wx, wy, ox, oy)) { wx = ox + PLAYER_W / 2; wy = oy + PLAYER_H / 2; }
    storyPx[i] = wx; storyPy[i] = wy;
  }

  // Pay'n'Spray : pose sur la ROUTE la plus proche (accessible en voiture). Repli
  // sur une case libre si aucune route dans le rayon.
  for (int i = 0; i < NUM_SPRAYS; i++) {
    int wx = SPRAYS[i].tx * TILE_W + TILE_W / 2;
    int wy = SPRAYS[i].ty * TILE_H + TILE_H / 2;
    int ox, oy;
    if (findRoadSpot(wx, wy, ox, oy)) { wx = ox; wy = oy; }
    else if (findFootSpot(wx, wy, ox, oy)) { wx = ox + PLAYER_W / 2; wy = oy + PLAYER_H / 2; }
    sprayPx[i] = wx; sprayPy[i] = wy;
  }
  sprayInside = false;

  // AMU Nation : posee sur le TROTTOIR le plus proche (acces a pied), comme les
  // cabines. Repli case libre sinon.
  for (int i = 0; i < NUM_AMMUS; i++) {
    int wx = AMMUS[i].tx * TILE_W + TILE_W / 2;
    int wy = AMMUS[i].ty * TILE_H + TILE_H / 2;
    int ox, oy;
    if (findSidewalkSpot(wx, wy, ox, oy)) { wx = ox; wy = oy; }
    else if (findFootSpot(wx, wy, ox, oy)) { wx = ox + PLAYER_W / 2; wy = oy + PLAYER_H / 2; }
    ammuPx[i] = wx; ammuPy[i] = wy;
  }
  shopOpen = false; shopSel = WEAPON_PISTOL;
  casseInside = false; carGone = false;

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

  // Vie, recherche, etat voiture : valeurs de depart.
  playerHearts = PLAYER_HEARTS_MAX;
  playerHurtTimer = 0;
  wantedReset(wanted);
  carHp = CAR_MAX_HP;
  boomTimer = 0;
  overlayMsg = nullptr; overlayTimer = 0;
  seqKind = SEQ_NONE; seqPhase = 0; seqTimer = 0; seqPoi = nullptr;
  hospInside = false; crushReward = 0;

  // Argent, butin au sol et projectiles : tout vide au demarrage.
  playerMoney = 0;
  for (int i = 0; i < NUM_LOOT; i++) loots[i].active = false;
  for (int i = 0; i < NUM_BULLETS; i++) bullets[i].active = false;
  lootNext = 0;

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

// Effet du coup de poing : une petite croix blanche ~5 px devant le joueur,
// dessinee pendant punchTimer frames. Les armes a feu n'utilisent plus de trait :
// elles crachent des pixels-projectiles (cf. fireBullets / drawBullets).
static void blitAttackFx(int camX, int camY) {
  if (curWeapon != WEAPON_FIST) return;
  int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
  int fx = pcx + AI_DX[playerDir] * 5 - camX;
  int fy = pcy + AI_DY[playerDir] * 5 - camY;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      if (dx != 0 && dy != 0) continue;          // petite croix
      int x = fx + dx, y = fy + dy;
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
  // Pendant la repeinture, la caisse est "dans" le garage : on la masque jusqu'a
  // ce qu'elle ressorte (PH_OUT). Au broyage, elle disparait des PH_CRUSH. Et si
  // elle a ete broyee, plus de voiture du tout.
  if (seqKind == SEQ_SPRAY && (seqPhase == PH_IN || seqPhase == PH_SPRAY)) return;
  if (seqKind == SEQ_CRUSH && (seqPhase == PH_CRUSH || seqPhase == PH_EJECT)) return;
  if (carGone) return;
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
    c.hp = CAR_MAX_HP;
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
    p.frame = 0; p.animTimer = 0; p.state = 0; p.downTimer = 0;
    p.hp = 3; p.shootTimer = COP_SHOOT_PERIOD;
    // Si la police nous recherche, une partie des pietons spawn en flics (bleus)
    // qui foncent sur le joueur (cf. aiUpdate). Sinon, pieton civil recolore.
    p.isCop = (wanted.level > 0) && ((aiRngNext(aiRng) % 100) < COP_SPAWN_PCT);
    p.color = p.isCop ? 0x001F : AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N];
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
  p.hp = 3; p.isCop = false; p.shootTimer = COP_SHOOT_PERIOD;
  p.active = true;
}

// Pose un butin (argent/munitions) sur une case libre du pool. Ecrase le plus
// ancien si tout est actif (pool tournant).
static void spawnLoot(int wx, int wy, uint8_t kind, uint8_t weapon, int amount) {
  int slot = -1;
  for (int i = 0; i < NUM_LOOT; i++) if (!loots[i].active) { slot = i; break; }
  if (slot < 0) { slot = lootNext; lootNext = (lootNext + 1) % NUM_LOOT; }
  Loot &l = loots[slot];
  l.x = wx; l.y = wy; l.kind = kind; l.weapon = weapon;
  l.amount = (int16_t)amount; l.active = true;
}

// A la mort d'un pieton (wx,wy px monde) : chance de lacher un billet et/ou une
// arme de poing (pistolet + munitions). Tire sur le PRNG IA partage.
static void dropLoot(int wx, int wy) {
  uint32_t r = aiRngNext(aiRng);
  if ((r % 100) < 40)                      // 40% : argent (5..50 $)
    spawnLoot(wx, wy, LOOT_MONEY, 0, 5 + (int)((r >> 16) % 46));
  if (((r >> 8) % 100) < 12)               // 12% : pistolet (decale d'un poil)
    spawnLoot(wx + 4, wy, LOOT_AMMO, WEAPON_PISTOL, WEAPONS[WEAPON_PISTOL].ammoPickup);
}

// Met un pieton au sol (KO/mort) + comptabilise l'objectif BEAT + fait monter la
// recherche police (un mort = un crime). Mutualise poing/arme/ecrasement.
static void knockDownPed(AiPed &p) {
  p.state = 1; p.downTimer = PED_DOWN_FRAMES;
  if (missionRun.active) objBeat++;
  wantedOnKill(wanted);                    // crime : streak -> etoiles
  dropLoot((int)p.x, (int)p.y);
}

// Inflige un coup au pieton. lethal = arme a feu (1 balle suffit) ; sinon poing
// (3 coups : on entame les PV, KO quand ils tombent a 0).
static void hitPed(AiPed &p, bool lethal) {
  if (lethal || p.hp <= 1) knockDownPed(p);
  else p.hp--;
}

// Place le joueur a pied sur une case libre proche du POI nomme (repli : coords
// brutes). Utilise pour reapparaitre a l'hopital / au commissariat.
static void respawnAtPoi(const char *poiName) {
  int wx = playerX + PLAYER_W / 2, wy = playerY + PLAYER_H / 2;
  int pi = findPoi(poiName);
  if (pi >= 0) { wx = cityPois[pi].tx; wy = cityPois[pi].ty; }
  int ox, oy;
  if (findFootSpot(wx, wy, ox, oy)) { playerX = ox; playerY = oy; }
  else { playerX = wx - PLAYER_W / 2; playerY = wy - PLAYER_H / 2; }
  playerDir = DIR_SOUTH; playerFrame = 0; animTimer = 0;
  driving = false; carIsMission = false;
}

// Remet le joueur d'aplomb apres mort/arrestation : vie pleine, recherche a zero,
// invulnerabilite breve, et on coupe l'arme courante au poing.
static void reviveCommon() {
  playerHearts = PLAYER_HEARTS_MAX;
  playerHurtTimer = PLAYER_HURT_COOLDOWN * 2;
  wantedClear(wanted);
  if (missionRun.active) {                  // une mission en cours echoue
    missionRun.active = false; target.active = false;
    marcoWaiting = false; marcoAboard = false;
    mCarActive = false;
  }
}

// Demarre une sequence de mort/arrestation : on fige le joueur, on prend
// l'argent tout de suite, et on laisse la cinematique (message -> ecran noir ->
// TP) se derouler dans updateSequence(). fromCar => on attend d'abord que
// l'explosion de la caisse finisse de jouer.
static void startEndSeq(uint8_t kind, const char *msg, const char *poi, bool fromCar) {
  if (seqKind != SEQ_NONE) return;             // deja en cinematique : on ignore
  seqKind = kind; overlayMsg = msg; seqPoi = poi;
  playerMoney -= playerMoney / 2;              // on lache la moitie du fric
  if (fromCar) { seqPhase = PH_EXPLODE; seqTimer = BOOM_FRAMES + 6; }
  else { seqPhase = PH_MSG; seqTimer = SEQ_MSG_FRAMES; gb.sound.playCancel(); }
}

// Soin a l'hopital (entree a pied, blesse) : on prend le fric tout de suite et
// on lance une courte anim (croix verte + jingle) ; la vie remonte a la fin.
static void startHealSeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_HEAL; seqPhase = PH_HEAL; seqTimer = SEQ_HEAL_FRAMES;
  int32_t pay = playerMoney < HEAL_COST ? playerMoney : HEAL_COST;
  playerMoney -= pay;
  narrate("Soigne. -$40");
}

// Mort du joueur a pied (plus de cœurs) : reapparition devant l'hopital.
static void wastedPlayer() { startEndSeq(SEQ_WASTED, "MORT", "Hopital", false); }

// Arrestation (un flic te touche) : retour au commissariat.
static void bustedPlayer() { startEndSeq(SEQ_BUSTED, "ARRETE", "Commissariat", false); }

// Le joueur encaisse dmg cœur(s) (balle de flic, etc.), avec invulnerabilite
// breve pour eviter de tout perdre d'un coup. Mort si plus de cœurs.
static void hurtPlayer(int dmg) {
  if (seqKind != SEQ_NONE) return;          // fige pendant une cinematique
  if (playerHurtTimer > 0) return;
  playerHearts -= dmg;
  playerHurtTimer = PLAYER_HURT_COOLDOWN;
  gb.sound.tone(180, 60);
  if (playerHearts <= 0) { playerHearts = 0; wastedPlayer(); }
}

// Declenche une explosion de voiture en (wx,wy) : effet visuel + son. Si le
// joueur conduit cette voiture, il meurt.
static void explodeCarAt(int wx, int wy) {
  boomX = wx; boomY = wy; boomTimer = BOOM_FRAMES;
  gb.sound.tone(70, 200);
}

// Tir d'arme a feu : engendre un ou plusieurs pixels-projectiles partant du
// joueur dans la direction du regard. La pompe crache une gerbe (3 pixels avec
// un leger ecart lateral) ; les autres armes, un seul. Portee = celle de l'arme.
static void fireBullets(int pcx, int pcy) {
  float fdx = AI_DX[playerDir], fdy = AI_DY[playerDir];
  float rdx = AI_DX[AI_RIGHT[playerDir]], rdy = AI_DY[AI_RIGHT[playerDir]];
  int pellets = (curWeapon == WEAPON_SHOTGUN) ? 3 : 1;
  uint8_t life = (uint8_t)(WEAPONS[curWeapon].reach / BULLET_SPEED + 1);
  for (int k = 0; k < pellets; k++) {
    int slot = -1;
    for (int i = 0; i < NUM_BULLETS; i++) if (!bullets[i].active) { slot = i; break; }
    if (slot < 0) slot = k % NUM_BULLETS;
    float spread = (pellets > 1) ? (float)(k - 1) * 0.35f : 0.0f;   // -1,0,+1 -> ecart
    Bullet &b = bullets[slot];
    b.x = (float)pcx; b.y = (float)pcy;
    b.vx = (fdx + rdx * spread) * BULLET_SPEED;
    b.vy = (fdy + rdy * spread) * BULLET_SPEED;
    b.life = life; b.active = true;
  }
}

// Avance les projectiles (purement visuel) et eteint ceux a bout de course.
static void updateBullets() {
  for (int i = 0; i < NUM_BULLETS; i++) {
    Bullet &b = bullets[i];
    if (!b.active) continue;
    b.x += b.vx; b.y += b.vy;
    if (b.life == 0 || --b.life == 0) b.active = false;
  }
}

// Dessine les projectiles : un pixel vif + une tracee attenuee derriere.
static void drawBullets(int camX, int camY) {
  for (int i = 0; i < NUM_BULLETS; i++) {
    Bullet &b = bullets[i];
    if (!b.active) continue;
    int x = (int)b.x - camX, y = (int)b.y - camY;
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0xFFE0;
    int tx = (int)(b.x - b.vx * 0.5f) - camX, ty = (int)(b.y - b.vy * 0.5f) - camY;
    if (tx >= 0 && tx < SCREEN_W && ty >= 0 && ty < SCREEN_H) fb[ty * SCREEN_W + tx] = 0xC600;
  }
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
  int pcx = playerX + PLAYER_W / 2, pcy = playerY + PLAYER_H / 2;
  if (curWeapon == WEAPON_FIST) gb.sound.playTick();
  else { gb.sound.tone(110, 50); fireBullets(pcx, pcy); }  // bang + pixels de tir
  bool firearm = (curWeapon != WEAPON_FIST);   // arme a feu : 1 touche = mort
  if (wd.area) {
    // Zone : tous les pietons debout dans le cone tombent (armes a feu).
    for (int i = 0; i < NUM_AI_PEDS; i++) {
      if (!(aiPeds[i].active && aiPeds[i].state == 0)) continue;
      if (combatInCone(aiPeds[i].x, aiPeds[i].y, pcx, pcy, playerDir, wd.reach, wd.side))
        hitPed(aiPeds[i], true);
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
    if (hit >= 0) hitPed(aiPeds[hit], firearm);   // poing : 3 coups ; arme : 1
  }
  // Les armes a feu abiment aussi les voitures IA dans le cone : a 0 PV, boom.
  if (firearm) {
    int16_t dmg = wd.area ? CAR_AREA_DMG : CAR_HIT_DMG;
    for (int i = 0; i < NUM_AI_CARS; i++) {
      if (!aiCars[i].active) continue;
      if (!combatInCone(aiCars[i].x, aiCars[i].y, pcx, pcy, playerDir, wd.reach, wd.side)) continue;
      aiCars[i].hp -= dmg;
      if (aiCars[i].hp <= 0) { explodeCarAt((int)aiCars[i].x, (int)aiCars[i].y); aiCars[i].active = false; }
    }
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
    // Solide vis-a-vis de la voiture joueur : on repousse le joueur, et un
    // accident a vitesse use la caisse (PV). Trop d'accidents -> elle fume puis
    // explose (cf. updateDrivenCar).
    if (driving) {
      float dx = car.x - c.x, dy = car.y - c.y;
      if (fabsf(dx) < COL_CC && fabsf(dy) < COL_CC) {
        float px = COL_CC - fabsf(dx), py = COL_CC - fabsf(dy);
        if (px < py) { car.x += (dx < 0 ? -px : px); car.vx *= -0.3f; }
        else { car.y += (dy < 0 ? -py : py); car.vy *= -0.3f; }
        if (spd2 > RUNOVER_SPEED2 && carCrashTimer == 0) {
          carHp -= CAR_CRASH_DMG; carCrashTimer = CAR_CRASH_COOLDOWN;
        }
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
    // Plus de recherche : les flics debout redeviennent de simples passants.
    if (p.isCop && wanted.level == 0 && p.state == 0) { p.active = false; aiRespawnPed(p, fcx, fcy); continue; }
    if (!p.active) { aiRespawnPed(p, fcx, fcy); continue; }
    if (p.state == 1) continue;               // renverse : immobile
    if (p.isCop)                              // flic : fonce sur le joueur
      missionChaseStep(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
                       AI_PED_SPEED + 0.15f, fcx, fcy, aiRng);
    else                                      // civil : errance trottoirs
      aiStep(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
             AI_PED_SPEED, aiIsWalkable, aiRng);
    if (++p.animTimer >= AI_PED_ANIM) { p.animTimer = 0; p.frame ^= 1; }
    // Renversement par la voiture joueur lancee (vaut aussi pour les flics).
    if (driving && spd2 > RUNOVER_SPEED2 &&
        fabsf(car.x - p.x) < COL_CP && fabsf(car.y - p.y) < COL_CP) {
      knockDownPed(p);                        // ecrasement : KO + butin eventuel
      continue;
    }
    // Police : arrestation au contact (a pied) ; tir au-dela d'une etoile.
    if (p.isCop) {
      float ddx = (float)fcx - p.x, ddy = (float)fcy - p.y;
      float d2 = ddx * ddx + ddy * ddy;
      if (!driving && d2 < (float)(COP_ARREST_DIST * COP_ARREST_DIST)) {
        bustedPlayer(); return;               // touche a pied -> arrete
      }
      if (wanted.level >= 2) {
        if (p.shootTimer > 0) p.shootTimer--;
        else if (d2 < (float)(COP_SHOOT_RANGE * COP_SHOOT_RANGE)) {
          p.shootTimer = COP_SHOOT_PERIOD;    // tir : son + chance de toucher
          gb.sound.tone(140, 40);
          if ((aiRngNext(aiRng) % 100) < 60) hurtPlayer(1);
        }
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

// Cabine telephonique 8x8 (cadre sombre, vitre, combine visible), centree sur
// (px,py) monde. `body` = couleur du corps (bleu missions / rouge trame).
// Si `canRing`, le toit clignote (jaune) + ondes de sonnerie ; sinon muette.
// Gabarit code par index de palette pour recolorer le corps a la volee :
// 0=transparent 1=cadre 2=corps(body) 3=vitre 4=socle 5=combine.
static void drawPhoneBooth(int camX, int camY, int px, int py,
                           uint16_t body, bool canRing) {
  static const uint8_t MAP[64] = {
    0,1,1,1,1,1,1,0,
    1,2,2,2,2,2,2,1,
    1,2,3,3,3,3,2,1,
    1,2,3,4,4,3,2,1,
    1,2,3,4,5,3,2,1,
    1,2,3,3,3,3,2,1,
    1,2,2,2,2,2,2,1,
    0,1,1,2,2,1,1,0
  };
  const uint16_t pal[6] = { 0xF81F, 0x10A2, body, 0xAEFB, 0xCE59, 0x0000 };
  int sx = px - camX - 4, sy = py - camY - 4;    // coin haut-gauche (8x8 centre)
  bool blink = canRing && ((missionAnim >> 2) & 1);
  for (int dy = 0; dy < 8; dy++)
    for (int dx = 0; dx < 8; dx++) {
      uint8_t idx = MAP[dy * 8 + dx];
      if (idx == 0) continue;                    // transparent
      uint16_t c = pal[idx];
      if (blink && dy == 0) c = 0xFFE0;          // toit clignotant (sonnerie)
      int x = sx + dx, y = sy + dy;
      if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
      fb[y * SCREEN_W + x] = c;
    }
  if (blink) {                                   // ondes de sonnerie
    int ox[4] = { -5, 5, -5, 5 }, oy[4] = { -5, -5, 1, 1 };
    for (int k = 0; k < 4; k++) {
      int x = px - camX + ox[k], y = py - camY + oy[k];
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

// Butin au sol : billet vert (argent) ou sprite d'arme (munitions de poing).
static void drawLoot(int camX, int camY) {
  for (int i = 0; i < NUM_LOOT; i++) {
    if (!loots[i].active) continue;
    if (loots[i].kind == LOOT_AMMO) {
      blitWeapon(camX, camY, loots[i].x, loots[i].y, loots[i].weapon);
      continue;
    }
    int sx = loots[i].x - camX - 2, sy = loots[i].y - camY - 1;   // billet 5x3
    for (int dy = 0; dy < 3; dy++)
      for (int dx = 0; dx < 5; dx++) {
        int x = sx + dx, y = sy + dy;
        if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
        bool edge = (dx == 0 || dx == 4 || dy == 0 || dy == 2);
        fb[y * SCREEN_W + x] = edge ? 0x05A0 : 0x07E0;   // bord vert sombre / vert vif
      }
    if (sy + 1 >= 0 && sy + 1 < SCREEN_H && sx + 2 >= 0 && sx + 2 < SCREEN_W)
      fb[(sy + 1) * SCREEN_W + (sx + 2)] = 0xFFE0;        // centre jaune ($)
  }
}

// Ramassage du butin au sol (a pied) : argent credite, ou arme de poing + ses
// munitions (equipee si on ne possedait pas mieux). (pcx,pcy) px monde.
static void tryPickupLoot(int pcx, int pcy) {
  for (int i = 0; i < NUM_LOOT; i++) {
    if (!loots[i].active) continue;
    long d = (long)(pcx - loots[i].x) * (pcx - loots[i].x)
           + (long)(pcy - loots[i].y) * (pcy - loots[i].y);
    if (d > (long)LOOT_REACH * LOOT_REACH) continue;
    if (loots[i].kind == LOOT_MONEY) {
      playerMoney += loots[i].amount;
      gb.sound.playOK();
    } else {
      uint8_t w = loots[i].weapon;
      bool had = weaponOwned[w];
      weaponOwned[w] = true;
      weaponAmmo[w] += loots[i].amount;
      if (!had) { curWeapon = w; weaponToast = WEAPON_TOAST_FRAMES; }
      gb.sound.playOK();
    }
    loots[i].active = false;
  }
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

// Dessine les cabines. Les cabines "trame principale" (rouges, muettes) sont
// des reperes permanents -> toujours visibles. Les cabines de missions
// secondaires (bleues) ne s'affichent qu'au repos et sonnent (decrochables).
// Garage Pay'n'Spray 8x8 centre en (px,py) monde : auvent raye jaune/bleu sur un
// box gris a porte sombre. Repere visuel d'un point de repeinture (sur la route).
static void drawSprayShop(int camX, int camY, int px, int py) {
  static const uint8_t MAP[64] = {
    1,1,1,1,1,1,1,1,
    2,3,2,3,2,3,2,3,
    4,4,4,4,4,4,4,4,
    4,5,5,5,5,5,5,4,
    4,5,5,5,5,5,5,4,
    4,5,5,5,5,5,5,4,
    4,5,5,5,5,5,5,4,
    4,4,4,4,4,4,4,4,
  };
  const uint16_t pal[6] = { 0xF81F, 0x4208, 0xFFE0, 0x001F, 0xAD55, 0x10A2 };
  int sx = px - camX - 4, sy = py - camY - 4;
  for (int dy = 0; dy < 8; dy++)
    for (int dx = 0; dx < 8; dx++) {
      uint8_t idx = MAP[dy * 8 + dx];
      uint16_t c = pal[idx];
      int x = sx + dx, y = sy + dy;
      if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
      fb[y * SCREEN_W + x] = c;
    }
}

static void drawSprayShops(int camX, int camY) {
  for (int i = 0; i < NUM_SPRAYS; i++)
    drawSprayShop(camX, camY, sprayPx[i], sprayPy[i]);
}

// Devanture AMU Nation 8x8 : facade gris-vert, auvent rouge, enseigne jaune
// (croix "+ d'armes"), porte sombre. Repere d'armurerie (sur trottoir).
static void drawAmmuShop(int camX, int camY, int px, int py) {
  static const uint8_t MAP[64] = {
    2,2,2,2,2,2,2,2,   // auvent rouge
    1,3,1,3,1,3,1,3,   // enseigne jaune rayee
    4,4,4,4,4,4,4,4,
    4,3,4,4,4,4,3,4,   // points jaunes (vitrine)
    4,4,4,5,5,4,4,4,
    4,4,4,5,5,4,4,4,
    4,4,4,5,5,4,4,4,
    4,4,4,5,5,4,4,4,
  };
  const uint16_t pal[6] = { 0xF81F, 0xFFE0, 0xC800, 0xFFE0, 0x7BCC, 0x18E3 };
  int sx = px - camX - 4, sy = py - camY - 4;
  for (int dy = 0; dy < 8; dy++)
    for (int dx = 0; dx < 8; dx++) {
      uint16_t c = pal[MAP[dy * 8 + dx]];
      int x = sx + dx, y = sy + dy;
      if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
      fb[y * SCREEN_W + x] = c;
    }
}

static void drawAmmuShops(int camX, int camY) {
  for (int i = 0; i < NUM_AMMUS; i++)
    drawAmmuShop(camX, camY, ammuPx[i], ammuPy[i]);
}

// Zone de depose de La Casse : carre raye jaune/noir (hazard) sur le point-cible
// du POI. Repere ou amener sa caisse pour la broyer.
static void drawCasseZone(int camX, int camY) {
  int ci = findPoi("La Casse");
  if (ci < 0) return;
  int sx = cityPois[ci].tx - camX, sy = cityPois[ci].ty - camY;
  for (int dy = -6; dy <= 6; dy++)
    for (int dx = -6; dx <= 6; dx++) {
      bool edge = (dx <= -5 || dx >= 5 || dy <= -5 || dy >= 5);
      if (!edge) continue;                         // contour seulement
      int x = sx + dx, y = sy + dy;
      if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
      fb[y * SCREEN_W + x] = (((dx + dy) >> 1) & 1) ? 0xFFE0 : 0x0000;  // raye jaune/noir
    }
}

// Entree dans un Pay'n'Spray : demarre la cinematique (la caisse rentre, on
// entend la bombe, elle ressort repeinte). Le joueur attend, fige.
static void startSpraySeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_SPRAY; seqPhase = PH_IN; seqTimer = SEQ_IN_FRAMES;
  car.vx = 0.0f; car.vy = 0.0f;                // on coupe l'elan en entrant
}

// Entree sur la zone de La Casse en voiture : demarre le broyage. Prime selon
// les PV restants (epave plus chere si la caisse est saine). On fige la caisse.
static void startCrushSeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_CRUSH; seqPhase = PH_CRANE; seqTimer = SEQ_CRANE_FRAMES;
  crushReward = 40 + (carHp > 0 ? carHp : 0) * 4;   // ~40..160 $
  car.vx = 0.0f; car.vy = 0.0f;
  gb.sound.tone(200, 120);                           // demarrage de la grue
}

// Repeinture (rouler dans un Pay'n'Spray) : nouvelle couleur de caisse, recherche
// police a zero, debit $ (gratuit si fauche pour ne pas bloquer a 5 etoiles).
static void repaintCar() {
  uint16_t nc;
  do { nc = AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N]; } while (nc == carColor);
  carColor = nc;
  wantedClear(wanted);
  int32_t pay = playerMoney < SPRAY_COST ? playerMoney : SPRAY_COST;
  playerMoney -= pay;
  narrate("Repeinte. Plus recherche.");
  gb.sound.playOK();
}

static void drawPhones(int camX, int camY) {
  for (int i = 0; i < NUM_STORY_PHONES; i++)
    drawPhoneBooth(camX, camY, storyPx[i], storyPy[i], PHONE_BODY_STORY, false);
  if (missionRun.active) return;
  for (int i = 0; i < NUM_PHONES; i++)
    drawPhoneBooth(camX, camY, phonePx[i], phonePy[i], PHONE_BODY_MISSION, true);
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
  const int top = SCREEN_H - 13;                 // en bas a gauche, juste au-dessus
  for (int y = top; y < top + 6; y++)            // du bandeau de narration
    for (int x = 0; x < w && x < SCREEN_W; x++) fb[y * SCREEN_W + x] = 0x0000;
  gb.display.setColor(WHITE);
  gb.display.setCursor(1, top + 1);
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

// Bornes d'affichage (la valeur courante vient de playerHearts / wanted.level).
static const int HUD_HEARTS_MAX = PLAYER_HEARTS_MAX;   // vie : 3 cœurs
static const int HUD_STARS_MAX  = WANTED_MAX;          // recherche police : 5 etoiles
static const uint16_t HEART_FULL = 0xF800, HEART_EMPTY = 0x4208;  // rouge / gris sombre
static const uint16_t STAR_FULL  = 0xFFE0, STAR_EMPTY  = 0x4208;  // jaune / gris sombre

// Barre de stats du haut (GTA2-like) sur fond transparent : cœurs a gauche,
// etoiles a droite, arme + munitions au centre (a pied), toast central au
// changement d'arme. Dessinee au ras du haut (y=0).
static void drawTopHud() {
  for (int i = 0; i < HUD_HEARTS_MAX; i++)      // cœurs (haut-gauche)
    blitIcon5(1 + i * 6, 1, ICON_HEART, i < playerHearts ? HEART_FULL : HEART_EMPTY);
  const int starsX = (SCREEN_W - HUD_STARS_MAX * 6) / 2;
  for (int i = 0; i < HUD_STARS_MAX; i++)       // etoiles (centre)
    blitIcon5(starsX + i * 6, 1, ICON_STAR, i < wanted.level ? STAR_FULL : STAR_EMPTY);
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
  // Argent (sous les cœurs) : "$" + montant en vert, avec ombre portee noire.
  char money[12];
  snprintf(money, sizeof(money), "$%ld", (long)playerMoney);
  gb.display.setColor(BLACK); gb.display.setCursor(2, 9); gb.display.print(money);
  gb.display.setColor((Color)0x07E0); gb.display.setCursor(1, 8); gb.display.print(money);
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

// Etat de la voiture pilotee : a 0 PV elle explose, le joueur meurt (hopital).
// Appelee chaque frame au volant (apres les collisions/tirs de la frame).
static void updateDrivenCar() {
  if (!driving) return;
  if (seqKind != SEQ_NONE) return;             // cinematique en cours : caisse figee
  if (carCrashTimer > 0) carCrashTimer--;
  if (carHp <= 0) {
    explodeCarAt((int)car.x, (int)car.y);
    startEndSeq(SEQ_WASTED, "MORT", "Hopital", true);  // dedans = mort, on voit le boom
  }
}

// Fumee de la voiture endommagee : 1 panache leger sous ~60 % de PV, plus dense
// sous ~30 %. Pixels gris/sombres au-dessus de la caisse, scintillants.
static void drawCarSmoke(int camX, int camY) {
  if (!driving || carHp > (CAR_MAX_HP * 3) / 5) return;
  bool heavy = carHp <= (CAR_MAX_HP * 3) / 10;
  int n = heavy ? 4 : 2;
  int cx = (int)car.x - camX, cy = (int)car.y - camY;
  for (int k = 0; k < n; k++) {
    int ph = (missionAnim + k * 5) % 12;         // montee du panache
    int x = cx + ((k & 1) ? 1 : -1) * (k / 2) + ((ph >> 2) & 1);
    int y = cy - 3 - ph;
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
    fb[y * SCREEN_W + x] = heavy ? 0x4208 : 0x8410;  // gris sombre / gris
  }
}

// Explosion : cercle de feu en expansion (orange -> jaune) sur quelques frames.
static void drawBoom(int camX, int camY) {
  if (boomTimer == 0) return;
  int cx = boomX - camX, cy = boomY - camY;
  int age = BOOM_FRAMES - boomTimer;             // 0..BOOM_FRAMES
  int r = 1 + age;                               // rayon croissant
  uint16_t col = (boomTimer > BOOM_FRAMES / 2) ? 0xFFE0 : 0xFC00;  // jaune puis orange
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 > r * r || d2 < (r - 1) * (r - 1)) continue;   // anneau
      int x = cx + dx, y = cy + dy;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = col;
    }
}

// Fait avancer la cinematique en cours (mort / arrestation / repeinture). La
// teleportation et la repeinture se font sur les transitions de phase, pas a
// l'entree : on laisse le temps de voir l'explosion, le message, l'ecran noir,
// la bombe de peinture. Le monde, lui, continue de tourner autour.
static void updateSequence() {
  if (seqKind == SEQ_NONE) return;
  if (seqTimer > 0) {
    if (seqKind == SEQ_SPRAY && seqPhase == PH_SPRAY && (seqTimer % 8) == 0)
      gb.sound.tone(2200, 70);                   // pschitt de la bombe de peinture
    if (seqKind == SEQ_HEAL) {                    // jingle de soin ascendant (do-mi-sol)
      if (seqTimer == SEQ_HEAL_FRAMES - 1) gb.sound.tone(523, 90);
      else if (seqTimer == SEQ_HEAL_FRAMES - 10) gb.sound.tone(659, 90);
      else if (seqTimer == SEQ_HEAL_FRAMES - 19) gb.sound.tone(784, 140);
    }
    if (seqKind == SEQ_CRUSH) {
      if (seqPhase == PH_CRANE && (seqTimer % 6) == 0)
        gb.sound.tone(150 + seqTimer * 4, 50);    // vrombissement descendant de la grue
      else if (seqPhase == PH_CRUSH && (seqTimer % 5) == 0)
        gb.sound.tone(70, 90);                     // grincement du broyeur
    }
    seqTimer--; return;
  }
  switch (seqPhase) {
    case PH_EXPLODE:                             // l'explosion a fini de jouer
      seqPhase = PH_MSG; seqTimer = SEQ_MSG_FRAMES; gb.sound.playCancel(); break;
    case PH_MSG:                                 // le bandeau a tenu sa duree
      seqPhase = PH_FADE; seqTimer = SEQ_FADE_FRAMES; break;
    case PH_FADE:                                // derriere l'ecran noir : revie + TP
      reviveCommon(); respawnAtPoi(seqPoi);
      seqKind = SEQ_NONE; seqPhase = 0; overlayMsg = nullptr; break;
    case PH_IN:                                  // la caisse a disparu dans le garage
      seqPhase = PH_SPRAY; seqTimer = SEQ_SPRAY_FRAMES; break;
    case PH_SPRAY:                               // bombe finie : repeinte, elle ressort
      repaintCar(); seqPhase = PH_OUT; seqTimer = SEQ_OUT_FRAMES; break;
    case PH_OUT:                                 // ressortie : on rend la main au joueur
      seqKind = SEQ_NONE; seqPhase = 0; break;
    case PH_HEAL:                                // soin termine : vie au max
      playerHearts = PLAYER_HEARTS_MAX;
      seqKind = SEQ_NONE; seqPhase = 0; break;
    case PH_CRANE:                               // caisse saisie : on la broie
      seqPhase = PH_CRUSH; seqTimer = SEQ_CRUSH_FRAMES;
      gb.sound.tone(60, 200); break;             // gros craquement
    case PH_CRUSH: {                             // broyee : prime + ejection du joueur
      playerMoney += crushReward;
      carGone = true; driving = false;
      int ox, oy;                                // poser le perso a cote de l'epave
      if (findFootSpot((int)car.x, (int)car.y, ox, oy)) { playerX = ox; playerY = oy; }
      playerFrame = 0; animTimer = 0; playerDir = DIR_SOUTH;
      narrate("Epave vendue !");
      gb.sound.tone(988, 60); gb.sound.playOK();  // cha-ching
      seqPhase = PH_EJECT; seqTimer = SEQ_EJECT_FRAMES; break;
    }
    case PH_EJECT:                               // prime affichee : on rend la main
      seqKind = SEQ_NONE; seqPhase = 0; break;
  }
}

// Calque cinematique dessine par-dessus la scene selon la phase courante.
static void drawSequence(int camX, int camY) {
  if (seqKind == SEQ_NONE) return;
  if (seqKind == SEQ_SPRAY) {                    // bombe : nuage de peinture sur le garage
    if (seqPhase != PH_SPRAY) return;
    int cx = (int)car.x - camX, cy = (int)car.y - camY;
    static const uint16_t puff[3] = { 0x07FF, 0xFFE0, 0xFD20 };
    for (int k = 0; k < 12; k++) {
      int a = missionAnim * 3 + k * 37;
      int x = cx + (a % 13) - 6, y = cy + ((a >> 2) % 13) - 6;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = puff[k % 3];
    }
    return;
  }
  if (seqKind == SEQ_HEAL) {                       // croix verte pulsante au-dessus du joueur
    int px = playerX + PLAYER_W / 2 - camX, py = playerY - 5 - camY;
    int s = 2 + ((missionAnim >> 2) & 1);          // taille qui pulse (2/3 px)
    for (int d = -s; d <= s; d++) {
      int hx = px + d, hy = py + d;
      if (px >= 0 && px < SCREEN_W && hy >= 0 && hy < SCREEN_H) fb[hy * SCREEN_W + px] = 0x07E0;
      if (hx >= 0 && hx < SCREEN_W && py >= 0 && py < SCREEN_H) fb[py * SCREEN_W + hx] = 0x07E0;
    }
    return;
  }
  if (seqKind == SEQ_CRUSH) {                      // grue + broyeur a La Casse
    int cx = (int)car.x - camX, cy = (int)car.y - camY;
    if (seqPhase == PH_CRANE) {                    // pince qui descend sur la caisse
      int p = SEQ_CRANE_FRAMES - seqTimer;         // 0..CRANE
      int clawY = cy - 22 + (p * 22) / SEQ_CRANE_FRAMES;
      for (int y = 0; y < clawY; y++)              // cable
        if (cx >= 0 && cx < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + cx] = 0x4208;
      for (int dx = -3; dx <= 3; dx++) {           // pince (barre + crochets)
        int x = cx + dx, y = clawY;
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0xFD20;
        if ((dx == -3 || dx == 3) && x >= 0 && x < SCREEN_W && y + 1 >= 0 && y + 1 < SCREEN_H)
          fb[(y + 1) * SCREEN_W + x] = 0xFD20;
      }
    } else if (seqPhase == PH_CRUSH) {             // caisse ecrasee : bloc qui s'aplatit
      int p = SEQ_CRUSH_FRAMES - seqTimer;
      int h = 1 + (7 * seqTimer) / SEQ_CRUSH_FRAMES;
      for (int dy = -h; dy <= h; dy++)
        for (int dx = -5; dx <= 5; dx++) {
          int x = cx + dx, y = cy + dy;
          if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0x632C;
        }
      for (int k = 0; k < 6; k++) {                // eclats de metal
        int a = p * 5 + k * 41;
        int x = cx + (a % 15) - 7, y = cy + ((a >> 2) % 9) - 4;
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0xC618;
      }
    } else {                                       // PH_EJECT : prime qui monte
      char r[10]; snprintf(r, sizeof(r), "+$%ld", (long)crushReward);
      int len = (int)strlen(r), yy = 28 - (SEQ_EJECT_FRAMES - seqTimer);
      gb.display.setColor(BLACK); gb.display.setCursor((SCREEN_W - len * 4) / 2 + 1, yy + 1); gb.display.print(r);
      gb.display.setColor((Color)0x07E0); gb.display.setCursor((SCREEN_W - len * 4) / 2, yy); gb.display.print(r);
    }
    return;
  }
  if (seqPhase == PH_FADE) {                      // ecran noir plein avant la TP
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = 0x0000;
    return;
  }
  if (seqPhase == PH_MSG && overlayMsg) {         // bandeau central "MORT"/"ARRETE"
    for (int y = 24; y < 38; y++)
      for (int x = 0; x < SCREEN_W; x++) fb[y * SCREEN_W + x] = 0x0000;
    int len = (int)strlen(overlayMsg);
    printShadow((SCREEN_W - len * 4) / 2, 28, overlayMsg);
  }
  // PH_EXPLODE : rien a ajouter, drawBoom fait le spectacle.
}

// Achat de l'arme surlignee : 1re fois = prix d'arme (+ 1 lot de munitions),
// ensuite = prix de rechargement. Refuse (son grave) si solde insuffisant.
static void shopBuy() {
  uint8_t w = shopSel;
  bool owned = weaponOwned[w];
  int32_t cost = owned ? AMMO_PRICE[w] : WEAPON_PRICE[w];
  if (playerMoney < cost) { gb.sound.tone(120, 120); return; }   // trop cher
  playerMoney -= cost;
  weaponOwned[w] = true;
  weaponAmmo[w] += WEAPONS[w].ammoPickup;
  curWeapon = w; weaponToast = WEAPON_TOAST_FRAMES;              // equipee a l'achat
  gb.sound.tone(880, 40); gb.sound.playOK();                     // cha-ching
}

// Navigation du magasin (UI modale) : HAUT/BAS choisit, A achete, B/MENU ferme.
static void updateShop() {
  if (gb.buttons.pressed(BUTTON_UP)   && shopSel > WEAPON_PISTOL)    { shopSel--; gb.sound.playTick(); }
  if (gb.buttons.pressed(BUTTON_DOWN) && shopSel < WEAPON_COUNT - 1) { shopSel++; gb.sound.playTick(); }
  if (gb.buttons.pressed(BUTTON_A)) shopBuy();
  if (gb.buttons.pressed(BUTTON_B) || gb.buttons.pressed(BUTTON_MENU)) {
    shopOpen = false; gb.sound.playCancel();
  }
}

// Rendu plein ecran du magasin : titre, liste d'armes (icone + nom + prix), solde.
static void drawShop() {
  fb = gb.display._buffer;
  for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = 0x0008;   // fond bleu nuit
  printShadow((SCREEN_W - 10 * 4) / 2, 1, "AMU NATION");
  for (uint8_t w = WEAPON_PISTOL; w < WEAPON_COUNT; w++) {
    int r = w - WEAPON_PISTOL, y = 11 + r * 9;
    if (w == shopSel)                              // surlignage de la ligne choisie
      for (int yy = y - 1; yy < y + 7 && yy < SCREEN_H; yy++)
        for (int xx = 0; xx < SCREEN_W; xx++) fb[yy * SCREEN_W + xx] = 0x2945;
    blitWeaponHudIcon(2, y, w);                    // icone 6x6
    printShadow(10, y, WEAPONS[w].name);
    char st[8];
    if (weaponOwned[w]) snprintf(st, sizeof(st), "+%ld", (long)AMMO_PRICE[w]);
    else                snprintf(st, sizeof(st), "%ld",  (long)WEAPON_PRICE[w]);
    printShadow(SCREEN_W - (int)strlen(st) * 4 - 1, y, st);
  }
  char money[12];                                  // solde en bas, en vert
  snprintf(money, sizeof(money), "$%ld", (long)playerMoney);
  gb.display.setColor(BLACK); gb.display.setCursor(2, SCREEN_H - 6); gb.display.print(money);
  gb.display.setColor((Color)0x07E0); gb.display.setCursor(1, SCREEN_H - 7); gb.display.print(money);
}

void loop() {
  while (!gb.update());

  // Magasin ouvert (AMU Nation) : UI modale, monde gele. On traite la nav et on
  // dessine le menu, puis on sort de la frame.
  if (shopOpen) { updateShop(); drawShop(); return; }

  // Cinematique en cours (mort / arrestation / repeinture) : le joueur est fige,
  // aucun input ne passe. Le monde (IA, police, fumee...) tourne quand meme,
  // plus bas. Sinon, on lit les commandes normalement.
  if (seqKind != SEQ_NONE) {
    // joueur fige
  } else if (!driving) {
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
    tryPickupLoot(playerX + PLAYER_W / 2, playerY + PLAYER_H / 2);
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
      // Armurerie a portee ? Ouvre le magasin (prioritaire sur le coup de poing).
      for (int i = 0; i < NUM_AMMUS; i++) {
        long dA = (long)(pcx - ammuPx[i]) * (pcx - ammuPx[i])
                + (long)(pcy - ammuPy[i]) * (pcy - ammuPy[i]);
        if (dA <= (long)AMMU_REACH * AMMU_REACH) {
          shopOpen = true; shopSel = WEAPON_PISTOL; gb.sound.playOK();
          answered = true; break;
        }
      }
      if (!answered && !missionRun.active) {
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
        if (!carGone) {                     // pas de voiture si elle a ete broyee
          long dC = (long)(pcx - (int)car.x) * (pcx - (int)car.x)
                  + (long)(pcy - (int)car.y) * (pcy - (int)car.y);
          if (dC <= bestd) { best = -1; bestd = dC; }
        }
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
          mCarActive = false; driving = true; carHp = CAR_MAX_HP; carGone = false;
        } else if (best == -1) {
          driving = true; carIsMission = false;   // remonter dans sa voiture
          carHp = CAR_MAX_HP;
        } else if (best >= 0) {
          AiCar &c = aiCars[best];           // vol : le conducteur descend
          aiEjectDriver((int)c.x, (int)c.y);
          car.x = c.x; car.y = c.y; car.vx = 0.0f; car.vy = 0.0f;
          car.angle = AI_CAR_FRAME[c.dir] * (TWO_PI / CAR_FRAMES);
          carColor = c.color; carIsMission = false;
          carHp = c.hp > 0 ? c.hp : CAR_MAX_HP;   // herite de l'usure de la caisse volee
          c.active = false;                  // la voiture quitte le pool IA
          driving = true; carGone = false;
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
  updateBullets();                             // projectiles de tir (visuel)
  narrUpdate();

  // Recherche police + minuteries de degats/effets ; explosion eventuelle de la
  // voiture pilotee (peut tuer le joueur -> reapparition hopital).
  wantedTick(wanted);
  if (playerHurtTimer > 0) playerHurtTimer--;
  if (boomTimer > 0) boomTimer--;
  if (overlayTimer > 0) overlayTimer--;
  updateDrivenCar();
  updateSequence();                            // avance la cinematique mort/arret/spray

  // Pay'n'Spray : entrer en voiture dans un garage repeint la caisse et efface
  // les etoiles. Detection d'entree (declenche une fois par passage, pas en
  // boucle si on reste sur place).
  bool onSpray = false;
  if (driving) {
    for (int i = 0; i < NUM_SPRAYS; i++) {
      long ddx = (int)car.x - sprayPx[i], ddy = (int)car.y - sprayPy[i];
      if (ddx * ddx + ddy * ddy <= (long)SPRAY_REACH * SPRAY_REACH) { onSpray = true; break; }
    }
  }
  if (onSpray && !sprayInside && seqKind == SEQ_NONE) startSpraySeq();  // on entre : cinematique
  sprayInside = onSpray;

  // La Casse : amener sa caisse (pas une caisse de mission) sur la zone marquee
  // la broie contre une prime. Detection d'entree (une fois par passage).
  bool onCasse = false;
  if (driving && !carIsMission && seqKind == SEQ_NONE) {
    int ci = findPoi("La Casse");
    if (ci >= 0) {
      long dcx = (int)car.x - cityPois[ci].tx, dcy = (int)car.y - cityPois[ci].ty;
      onCasse = (dcx * dcx + dcy * dcy <= (long)CASSE_REACH * CASSE_REACH);
    }
  }
  if (onCasse && !casseInside) startCrushSeq();
  casseInside = onCasse;

  // Hopital : entrer A PIED en etant blesse -> soin paye (anim croix verte). Une
  // fois par entree dans la bbox ; narre si trop fauche pour payer.
  bool onHosp = false;
  if (!driving && seqKind == SEQ_NONE) {
    int pt = poiAtTile((playerX + PLAYER_W / 2) >> 3, (playerY + PLAYER_H / 2) >> 3);
    onHosp = (pt >= 0 && strcmp(cityPois[pt].name, "Hopital") == 0);
  }
  if (onHosp && !hospInside && playerHearts < PLAYER_HEARTS_MAX) {
    if (playerMoney >= HEAL_COST) startHealSeq();
    else narrate("Pas assez ($40)");
  }
  hospInside = onHosp;
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
  drawSprayShops(camX, camY);                  // garages (marquage au sol, sur route)
  drawAmmuShops(camX, camY);                    // armureries (devanture, sur trottoir)
  drawCasseZone(camX, camY);                    // zone de broyage (La Casse)
  aiDraw(camX, camY);
  drawWeaponPickups(camX, camY);
  drawLoot(camX, camY);
  drawMissionCar(camX, camY);
  drawMarco(camX, camY);
  drawTarget(camX, camY);
  drawPhones(camX, camY);
  drawMarker(camX, camY);
  drawCar(camX, camY);
  if (!driving) drawPlayer(camX, camY);
  if (!driving && punchTimer > 0) { blitAttackFx(camX, camY); punchTimer--; }
  drawBullets(camX, camY);                     // pixels de tir par-dessus la scene
  drawCarSmoke(camX, camY);                    // fumee si la caisse est amochee
  drawBoom(camX, camY);                        // explosion de voiture

  // HUD : nom du POI courant (haut), fleche vers l'objectif/cible, narration.
  drawPoiHud(focusX, focusY);
  drawMissionArrow(camX, camY, focusX, focusY);
  drawMissionStatus();
  drawTopHud();                                // barre stats : cœurs, etoiles, arme
  narrDraw();
  drawSequence(camX, camY);                    // cinematique : message / ecran noir / bombe

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
