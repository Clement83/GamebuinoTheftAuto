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
#include <math.h>
#include "assets.h"
#include "citymap.h"
#include "engine.h"
#include "car.h"
#include "smoke.h"
#include "player.h"
#include "ai.h"
#include "combat.h"
#include "mission.h"
#include "weapons.h"
#include "weapons_gfx.h"
#include "wanted.h"
#include "slot.h"

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
// --- coup recu d'un flic : petit eclat jaune sur le joueur (feedback visuel) ---
static const int COP_HIT_FX_FRAMES = 5; // duree de l'eclat (un peu plus long, bien visible)
static uint8_t copHitTimer = 0;         // >0 : eclat en cours (decremente/frame)

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

// --- animation de la cagnotte : un gain (mission, butin au sol, broyage...) ne
//     saute pas d'un coup. moneyShown rattrape playerMoney en "roulant" dollar
//     par dollar (~2 s), et un "+$X" flottant remonte depuis sous la cagnotte
//     vers celle-ci. Generique : tout passe par addMoney(). Les depenses, elles,
//     sont rattrapees instantanement (pas d'anim a la baisse). ---
static int32_t moneyShown    = 0;     // $ affiche dans le HUD (suit playerMoney)
static int32_t moneyRollStep = 1;     // $/frame du compteur qui tourne
static int32_t moneyGainAmt  = 0;     // montant du dernier "+$X" flottant
static uint16_t moneyGainTimer = 0;   // >0 : "+$X" en cours de montee
static const uint16_t MONEY_GAIN_FRAMES = 24;   // ~1 s : duree du "+$X" flottant
static const int32_t  MONEY_ROLL_FRAMES = 50;   // ~2 s : duree cible du compteur

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
  bool    hostile;       // true : balle de flic (touche le joueur) ; false : balle joueur
  int16_t carDmg;        // balle joueur : degats infliges a une voiture touchee (0 pour flic)
  bool    active;
};
static const int   NUM_BULLETS  = 10;
static const float BULLET_SPEED = 1.0f;  // px/frame (assez lent pour voir/esquiver)
static Bullet bullets[NUM_BULLETS];

// --- projectiles explosifs : roquette (file tout droit, explose a l'impact) et
//     grenade (lancee en cloche, explose au bout de ~3 s ; les PNJ qui la voient
//     paniquent avant). La detonation appelle explodeCarAt (degats radiaux +
//     chaine), pas le tir lui-meme. ---
struct Projectile {
  float    x, y, vx, vy;  // px monde
  uint8_t  kind;          // 0 = roquette, 1 = grenade
  uint8_t  z;             // hauteur visuelle de la grenade (cloche), px
  uint16_t flight;        // frames de vol restants (roquette: portee ; grenade: arc)
  uint16_t fuse;          // grenade: frames avant explosion ; roquette: 0
  bool     active;
};
static const int      NUM_PROJ           = 3;
static const float    ROCKET_SPEED       = 3.0f;  // px/frame
static const int      ROCKET_RANGE       = 90;    // px : portee max avant auto-explosion
static const int      GRENADE_THROW_DIST = 26;    // px : distance de lancer (cloche)
static const uint16_t GRENADE_FLIGHT     = 16;    // frames de vol (arc)
static const uint8_t  GRENADE_MAX_Z      = 7;     // px : hauteur du sommet de la cloche
static const uint16_t GRENADE_FUSE       = 75;    // ~3 s a ~25 fps avant explosion
static const int      GRENADE_FEAR_RADIUS = 30;   // px : les PNJ voyant la grenade paniquent
static Projectile projs[NUM_PROJ];

// --- vie + gilet pare-balles du joueur : UN SEUL compteur de 0 a 6. 1..3 = vie
//     (cœurs rouges), 4..6 = gilet (cœurs bleus, encaisses en premier puisque ce
//     sont les points du dessus). Le gilet (+3) s'achete a AMU Nation ou se met
//     en montant dans une voiture de police. La mort/arrestation le perd ; le
//     soin a l'hopital ne rend QUE la vie (il ne se declenche qu'a hp < 3, donc
//     ne touche jamais le gilet). ---
static const int PLAYER_HEARTS_MAX = 3;          // vie pleine (sans gilet)
static const int PLAYER_HP_MAX     = 6;          // vie pleine + gilet plein
static int       playerHearts = PLAYER_HEARTS_MAX;
static const uint8_t PLAYER_HURT_COOLDOWN = 25;  // ~1 s d'invuln. apres un coup
static uint8_t   playerHurtTimer = 0;
static const uint8_t HIT_FLASH_FRAMES = 6;       // frames de flash rouge des LEDs a chaque coup encaisse
static uint8_t   hitFlashTimer = 0;              // >0 : LEDs rouges (coup recu)

// --- joueur au sol : coup de poing d'un flic (-> arrestation) ou percute par une
//     voiture (-> se releve). Fige les commandes le temps de l'anim couchee. ---
static bool      playerDown       = false;
static uint16_t  playerDownTimer  = 0;
static bool      playerDownArrest = false;   // au relevage : true = arrestation, false = on se releve

// --- saut (bouton B a pied) : bond en avant dans la direction du regard. En
//     l'air, le joueur survole les voitures (ni repousse ni renverse). Apres le
//     bond, court cooldown anti-spam. ---
static uint8_t   playerJump      = 0;        // >0 : en l'air (frames de vol restantes)
static uint8_t   playerJumpCool  = 0;        // >0 : recharge avant un nouveau saut
static float     playerJumpAcc   = 0.0f;     // reste fractionnaire de deplacement
static const uint8_t JUMP_FRAMES   = 11;     // ~0.44 s de vol
static const uint8_t JUMP_COOLDOWN = 7;      // frames de recharge apres l'atterrissage
static const float   JUMP_SPEED    = 1.3f;   // px/frame -> bond d'environ 1.5 case
static const int     JUMP_MAX_Z    = 5;      // px : hauteur visuelle au sommet du bond

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

// --- mort lente de la caisse : a 0 PV elle prend feu et laisse ~10 s (grosse
//     flamme + fumee) avant d'exploser ; conduisible pendant ce temps. Si on
//     saute en marche elle continue sur sa lancee (runaway) et explose plus
//     loin. L'explosion fait un petit saut (oppose au dernier choc) + des
//     degats de zone (PNJ tues, joueur -1 coeur). ---
static const uint16_t CAR_FUSE_FRAMES = 250;   // ~10 s a ~25 fps
static uint16_t       carFuse        = 0;       // >0 : compte a rebours avant boom
static bool           carRunaway     = false;   // caisse sans conducteur, sur sa lancee
static float          carImpactX = 0.0f, carImpactY = 0.0f;  // sens du dernier choc
// Sortie de voiture, 3 paliers selon la vitesse^2 (max ~5.3 = CAR_MAX_FWD^2) :
//  - <= PARK   : a l'arret/presque, on se gare proprement (aucune chute).
//  - <= HURT   : un peu de vitesse, on saute et on FINIT AU SOL, sans degat.
//  -  > HURT   : tres vite, on finit au sol ET on se fait mal (-1 cœur).
static const float    CAR_BAIL_PARK_SPEED2 = 0.25f;  // ~0.5 px/f : sortie propre
static const float    CAR_BAIL_HURT_SPEED2 = 2.0f;   // ~1.4 px/f : au-dela, ca fait mal
static const float    CAR_RUNAWAY_FRICTION = 0.94f;  // decel de la caisse lancee
// Klaxon des voitures IA : un cooldown GLOBAL (pas par voiture, pour la RAM)
// evite le spam quand plusieurs caisses sont genees en meme temps.
static uint8_t        carHonkCooldown = 0;
static uint8_t        carHonkBeep2    = 0;       // frames avant le 2e "tuut" (klaxon = 2 bips)
static const uint8_t  CAR_HONK_COOLDOWN  = 30;   // frames de silence entre 2 klaxons
static const uint16_t CAR_HONK_FREQ      = 320;  // Hz : tonalite du klaxon
static const uint16_t CAR_HONK_BEEP_MS   = 70;   // duree d'un bip
static const uint8_t  CAR_HONK_GAP       = 5;    // frames entre les 2 bips ("tuut tuut")
static const int      SMOKE_HOOD_DIST = 4;      // px : avance du panache sur le capot
static const int      BOOM_HURT_RADIUS  = 16;   // px : zone letale (PNJ) / -1 coeur (joueur a pied)
static const int      BOOM_PANIC_RADIUS = 40;   // px : PNJ alentour pris de panique
static const int      BOOM_VEHICLE_RADIUS = 24; // px : portee des degats de zone aux vehicules
static const int16_t  BOOM_CENTER_DMG     = 40; // degats au centre (> CAR_MAX_HP : detruit a l'impact)

// --- police : portees de poursuite/arrestation/tir (px), cadence de tir. ---
static const int COP_ARREST_DIST  = 7;    // px : contact a pied (portee de matraque)
static const float COP_DRAG_MAX_SPEED = 0.35f; // px/frame : au-dela, le flic ne m'extrait plus de la caisse (il faut etre quasi a l'arret)
static const int COP_MELEE_DIST   = 9;    // px : portee du coup de matraque (a pied)
static const uint16_t COP_MELEE_PERIOD = 22;  // frames entre deux coups de matraque (~1 s)
static const int COP_MELEE_DMG    = 1;    // coeurs perdus par coup de matraque
static const int COP_SHOOT_RANGE  = 60;   // px : portee de tir du policier
static const uint16_t COP_SHOOT_PERIOD = 35;  // frames entre deux balles A 5 ETOILES (le plus rapide)
static const uint16_t COP_SHOOT_STEP   = 38;  // +frames de cooldown par etoile manquante
// Spawn police LINEAIRE selon les etoiles (1* = peu, 5* = beaucoup). Les flics a
// pied n'apparaissent qu'en recherche ; les voitures de police gardent une petite
// patrouille ambiante a 0* (elles ne poursuivent qu'avec recherche).
static const int COP_SPAWN_PER_STAR = 10;  // % de pietons-flics par etoile (1*=10 ... 5*=50)
static const int COP_SPAWN_MAX      = 50;  // plafond du % de pietons-flics
static const int POLICE_CAR_BASE    = 8;   // % de voitures de police a 0* (patrouille)
static const int POLICE_CAR_PER_STAR = 5;  // +% de voitures de police par etoile (5*=33)
static const int POLICE_EJECT_DIST = 22;  // px : voiture de police assez pres -> un flic descend
static const uint16_t POLICE_BLUE = 0x001F;  // bleu des voitures de police
static const uint16_t COP_BUST_DELAY = 28;   // frames au sol apres le coup de poing du flic -> arrestation

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
enum { SEQ_NONE, SEQ_WASTED, SEQ_BUSTED, SEQ_SPRAY, SEQ_HEAL, SEQ_CRUSH, SEQ_SLEEP, SEQ_CUT };
enum { PH_EXPLODE, PH_MSG, PH_FADE, PH_IN, PH_SPRAY, PH_OUT,
       PH_HEAL, PH_SWING, PH_CARRY, PH_CRUSH, PH_EJECT };
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
static const uint16_t SEQ_SWING_FRAMES = 30;    // fleche pivote vers la voiture garee
static const uint16_t SEQ_CARRY_FRAMES = 32;    // fleche repivote avec la voiture vers le broyeur
static const uint16_t SEQ_CRUSH_FRAMES = 30;    // broyage de l'epave
static const uint16_t SEQ_EJECT_FRAMES = 16;    // prime affichee + ejection
static const uint16_t SEQ_SLEEP_FRAMES = 45;    // ~1.8 s : ecran noir du dodo (Planque)
static const int32_t  HEAL_COST   = 40;         // $ d'un soin complet a l'hopital
static bool           hospInside  = false;      // dans la bbox hopital a la frame -1
static bool           commInside  = false;      // dans la bbox commissariat a la frame -1
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
static const int PED_DOWN_FRAMES = 35;     // duree au sol avant recyclage (mort) -- /2 : les PNJ restent moins longtemps a terre
static const int GETUP_MIN_FRAMES = 22;    // au sol RECUPERABLE : ~0.9 s + alea avant de se relever (auto, filet de securite)
static const int GETUP_RAND_FRAMES = 18;   // alea ajoute (pour que ce ne soit pas mecanique)
static const int RUNOVER_SPEED2 = 1;       // vitesse^2 mini pour renverser
static const float COL_CP_HIT = 5.0f;      // px : demi-boite voiture<->personne pour l'ecrasement
static const int STOP_AHEAD = 15;          // px : distance d'arret devant obstacle
static const int STOP_SIDE  = 6;           // px : tolerance laterale de l'obstacle
static const uint8_t BRAKE_REACT_FRAMES = 9;   // frames avant qu'une caisse freine pour le joueur (~0.36 s : laisse une chance de me faucher)
// --- effroi trottoir : caisse qui monte sur le trottoir vers un pieton -> panique ---
static const int SIDEWALK_SCARE_RANGE = 22;   // px : portee de la menace (caisse sur trottoir)
static const int ENTER_AI_DIST = 12;       // px : portee pour voler une voiture IA
static const int DRIVER_DOOR_OFFSET = 7;   // px : conducteur ejecte pose a gauche de la caisse (marge anti-ecrasement)
static const uint16_t PED_PANIC_FRAMES   = 70;   // duree de la fuite affolee
static const float    AI_PED_PANIC_SPEED = 0.6f; // plus rapide qu'en flanerie
// --- voitures affolees par un crime : roulent vite, n'evitent plus personne ---
static const uint16_t CAR_FLEE_FRAMES    = 90;   // duree de la fuite d'une voiture
static const float    CAR_FLEE_SPEED     = 1.3f; // px/frame en fuite (> AI_CAR_SPEED)
static const float    CAR_RUNOVER_SPEED2 = 0.55f;// vitesse^2 mini d'une voiture IA pour ecraser (sinon: solide) -- relevee : une caisse qui ralentit/tourne pousse au lieu d'ecraser
static const float    CAR_EJECT_PUSH     = 10.0f;// px : projection du joueur renverse par une voiture
static const uint8_t  CAR_BACKOFF_FRAMES = 22;   // frames de recul d'une caisse IA apres un choc (s'eloigne au lieu de forcer)
static const int      GUNSHOT_PANIC_RANGE = 36;  // px : PNJ qui entendent un tir
static const int      KILL_PANIC_RANGE    = 22;  // px : PNJ temoin d'un meurtre proche
static const uint8_t  WRECK_HOP_FRAMES   = 8;    // frames du petit saut a l'explosion

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
  bool driver;         // un conducteur au volant : descend en panique si on tire dessus
  bool isPolice;       // voiture de police (bleue) : poursuit le joueur recherche
  bool fleeing;        // affolee par un crime : roule vite, n'evite plus personne, ecrase
  uint16_t fleeTimer;  // >0 : frames de fuite restantes (decremente -> retour normal)
  uint8_t brakeReact;  // temps de reaction : compte les frames ou le joueur barre la
                       // route avant que la caisse ne freine vraiment (sinon : ecrase)
  uint8_t backoff;     // >0 : frames ou la caisse RECULE (s'eloigne du joueur) au lieu
                       // de poursuivre/forcer -- arme apres un choc ou en panique
  bool active;
};

struct AiPed {
  float x, y;
  uint8_t dir;
  int tgtx, tgty;
  uint16_t color;
  uint8_t frame, animTimer;
  uint8_t state;       // 0 = marche, 1 = mort/renverse (despawn), 2 = panique (fuite),
                       // 3 = au sol RECUPERABLE (se releve puis fuit ; ex. vol de voiture)
  uint16_t downTimer;
  uint8_t hp;          // 3 coups de poing pour tomber ; 1 balle suffit
  bool isCop;          // policier (bleu) : poursuit/matraque/tire quand recherche
  uint16_t shootTimer; // cadence de tir du policier (recharge entre deux balles)
  uint16_t meleeTimer; // cadence de matraquage du policier (recharge entre deux coups)
  uint16_t panicTimer; // >0 (state==2) : frames de fuite affolee restantes
  int16_t panicX, panicY; // point a fuir (px monde) : explosion/tireur/agresseur
  bool active;
};

// Epave : carcasse laissee par une voiture explosee. Obstacle statique (trafic
// + voiture joueur), fume (palier leger), petit saut a la naissance puis
// immobile. Recyclee quand on s'eloigne (pas de timer). Non conduisible.
struct Wreck { float x, y, vx, vy; uint8_t frame; uint8_t hop; bool active; };
static const int NUM_WRECKS = 2;
static Wreck wrecks[NUM_WRECKS];

// Panache de fumee d'explosion (roquette/grenade) : purement visuel, dure ~5 s
// au point de detonation, qu'il y ait eu un vehicule ou non.
struct Smoke { float x, y; uint16_t life; bool active; };
static const int      NUM_SMOKE         = 3;
static const uint16_t SMOKE_PUFF_FRAMES = 125;   // ~5 s a ~25 fps
static Smoke smokes[NUM_SMOKE];

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
static const uint16_t SARAH_COLOR      = 0xF81F; // magenta (Sarah, journaliste)
// Couleur de l'allie courant (compagnon EV_MARCO_JOIN) : Marco par defaut, Sarah
// si l'objectif JOIN porte count==1. Permet de reutiliser la meme mecanique
// d'escorte pour un allie different sans le peindre en "Marco".
static uint16_t allyColor = MARCO_COLOR;

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
// M4 (trame) : reutilise la mecanique Marco passager -> mort -> tueur qui fonce.
static const Objective OBJS_DEAL[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE,       "Le Garage",
    "Marco : un dernier rendez-vous, ce soir. Prends la caisse au Garage.", nullptr },
  { OBJ_GOTO,      0, 0, 14, true,  EV_MARCO_JOIN,  "Le Garage",
    "Passe prendre Marco devant le Garage.", "Marco monte. Marco : direction le Chantier." },
  { OBJ_GOTO,      0, 0, 16, true,  EV_MARCO_DIE,   "Chantier",
    "Conduis Marco au Chantier. Il est nerveux ce soir.",
    "Le Chantier. Marco descend, mefiant... une silhouette l'attend dans l'ombre." },
  { OBJ_KILL,      0, 0,  0, false, EV_NONE,        "Chantier",
    "Le tueur fonce sur toi. Pas question de le laisser filer !",
    "Justice est faite. ...pour l'instant.", 3, 0 },
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
// Livraison de pizza : remplace l'ancienne cabine bleue "Mauvaise affaire"
// (Marco). Mission jetable, chrono, 0 mecanique nouvelle (cf. campagne.md §9).
static const Objective OBJS_PIZZA[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "La Casse",
    "Pizza Express : prends la caisse.", nullptr },
  { OBJ_GOTO,      0, 0, 16, true,  EV_NONE, "Chinatown",
    "Livre la pizza avant qu'elle refroidisse !",
    "Pizza livree, pourboire empoche !", 0, 1100 },
};
// --- Trame Acte I ---
static const Objective OBJS_M1[] = {
  { OBJ_GOTO, 0, 0, 12, false, EV_NONE,       "Le Garage",
    "Premier jour. Marco, le bras droit du patron, t'attend au Garage. Vas-y a pied.",
    nullptr },
  { OBJ_TALK, 0, 0,  8, false, EV_MARCO_JOIN,  "Le Garage",
    "Marco : deux secondes petit, j'arrive !",
    "Marco : la caisse est garee a cote. Embarque, on a un colis a livrer." },
  { OBJ_GOTO, 0, 0, 16, true,  EV_NONE,         "Les Quais",
    "En route pour les Quais. Roule peinard, attire pas les flics.",
    "Colis livre. Marco : nickel. Maintenant ramene-moi chez moi, petit." },
  { OBJ_GOTO, 0, 0, 14, true,  EV_MARCO_LEAVE,  "Le Garage",
    "Ramene Marco au Garage.",
    nullptr },
};
static const Objective OBJS_M2[] = {
  { OBJ_GOTO,   0, 0, 14, false, EV_NONE, "Commerces",
    "Jour de tournee. Marco t'emmene encaisser le loyer aux Commerces.",
    "Marco : ce commercant fait le difficile. Regarde et apprends, petit." },
  { OBJ_SUBDUE, 0, 0,  0, false, EV_NONE, "Commerces",
    "Le commercant refuse et te saute dessus. Mate-le, mais le tue pas.",
    "Il crache l'argent. Marco : voila comment on fait.", 3, 0 },
  { OBJ_GOTO,   0, 0, 14, false, EV_NONE, "Chinatown",
    "Client suivant : une echoppe de Chinatown.", "Encaisse. Sans histoire, celui-la." },
  { OBJ_GOTO,   0, 0, 14, false, EV_NONE, "Le Bar",
    "Encore un : le vieux du Bar paie toujours rubis sur l'ongle.",
    "Le vieux paie, et t'offre meme un verre." },
  { OBJ_GOTO,   0, 0, 16, false, EV_NONE, "Chantier",
    "Dernier client, au Chantier. Marco : celui-la... je le sens pas.",
    "Personne en vue. Trop calme." },
  { OBJ_BEAT,   0, 0,  0, false, EV_NONE, "Chantier",
    "Embuscade ! Ils t'attendaient. Defends-toi !",
    "Marco : quelqu'un nous a vendus. On reglera ca.", 0, 0, 3, EK_THUG, SP_AMBUSH },
};
static const Objective OBJS_M3[] = {
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE, "Chinatown",
    "Un mauvais payeur se planque a Chinatown. Marco veut un exemple.",
    "Il a paye des gros bras pour te recevoir." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Chinatown",
    "Ecarte ses hommes de main.", "La voie est libre. Reste le payeur.",
    0, 0, 2, EK_THUG, SP_PRESENT },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Chinatown",
    "Il detale ! Rattrape-le.",
    "Dette reglee. Marco : il s'en souviendra... s'il s'en souvient encore.", 1, 0 },
};
// --- Trame Acte II : la vengeance manipulee (les Loups) ---
static const Objective OBJS_M5[] = {
  { OBJ_GOTO,   0, 0, 12, false, EV_NONE, "Le Bar",
    "Inconnu : tu veux savoir pour Marco ? Trouve Nico. Il traine au Bar.",
    nullptr },
  { OBJ_SUBDUE, 0, 0,  0, false, EV_NONE, "Le Bar",
    "Nico fait le malin et te bouscule. Secoue-le, sans le tuer : il parlera.",
    "Nico : les Loups cherchent un type depuis des semaines... c'est eux, surement.",
    3, 0 },
};
static const Objective OBJS_M6[] = {
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE, "Chinatown",
    "Inconnu : envoie-leur un message. Va dans le quartier des Loups.",
    "Les voila, accoudes au mur. Ils t'ont vu." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Chinatown",
    "Mets trois Loups au tapis. Qu'ils comprennent.",
    "Ca devrait attirer leur attention. Inconnu : bien joue.",
    0, 0, 3, EK_THUG, SP_PRESENT },
};
static const Objective OBJS_M7[] = {
  { OBJ_GOTO,      0, 0, 18, false, EV_NONE, "Les Quais",
    "Tony - oui, l'Inconnu a un nom : une caisse des Loups dort aux Quais.",
    "Deux gardes armes la surveillent." },
  { OBJ_KILL,      0, 0,  0, false, EV_NONE, "Les Quais",
    "Occupe-toi des deux gardes.", "La voie est libre. La caisse est a toi.",
    0, 0, 2, EK_GUNNER, SP_PRESENT },
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "Les Quais",
    "Embarque la caisse des Loups.", nullptr },
  { OBJ_GOTO,      0, 0, 18, true,  EV_NONE, "Commissariat",
    "Planque-la derriere le Commissariat. Roule peinard.",
    "Tony : parfait. Tu montes en grade, petit." },
};
static const Objective OBJS_M8[] = {
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Le Garage",
    "Tony : ils ont retrouve mon Garage ! Ramene-toi, et arme-toi en chemin.",
    "Les Loups debarquent. Tiens bon." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Le Garage",
    "Premiere vague : repousse-les !", "Ca se calme... non, ils reviennent !",
    0, 0, 3, EK_THUG, SP_PRESENT },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Le Garage",
    "Deuxieme vague, et ils sont armes. Couvre-toi !",
    "Tony : ils deviennent nerveux. Merci, petit.", 0, 0, 2, EK_GUNNER, SP_AMBUSH },
};
static const Objective OBJS_M9[] = {
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Commerces",
    "Tony : reprends la tournee de Marco aux Commerces. C'est toi, le patron, maintenant.",
    "Un commercant : c'est toi qui passes, desormais ? ...Desole, pour Marco." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Commerces",
    "Des Loups sont venus se servir. Renvoie-les chez eux.",
    "Pour toi, Marco.", 0, 0, 3, EK_THUG, SP_PRESENT },
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Le Bar",
    "Le vieux du Bar a vu quelque chose. Va l'ecouter.",
    "Le vieux : un type chic donnait des ordres aux Loups... Va savoir qui." },
};
static const Objective OBJS_M10[] = {
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE, "Les Quais",
    "Tony : on a trouve le stock des Loups, aux entrepots des Quais.",
    "Des gardes verrouillent l'entree." },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Les Quais",
    "Force le passage : deux gardes armes a la porte.", "Entree degagee.",
    0, 0, 2, EK_GUNNER, SP_PRESENT },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Les Quais",
    "Nettoie l'entrepot. Que personne ne ressorte.",
    "Le patron des Loups va nous tuer pour ca... tant pis.", 0, 0, 3, EK_THUG, SP_AMBUSH },
};
// M11 : boss (KILL count>1 -> encaisse plusieurs coups). Pivot de la campagne.
static const Objective OBJS_M11[] = {
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE, "Chantier",
    "Tony : Rico, le lieutenant des Loups, se terre a l'ancienne usine. Finis-le.",
    "Ses hommes te coupent la route." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Chantier",
    "Ecarte sa garde rapprochee.", "Reste Rico. Il ne tombera pas en un coup.",
    0, 0, 2, EK_GUNNER, SP_PRESENT },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Chantier",
    "Rico est coriace. Acharne-toi.",
    "Rico, a terre : tu crois qu'on a tue Marco ? Tu bosses pour le vrai coupable...",
    5, 0 },
};
// --- Trame Acte III : la verite (Sarah, les preuves) ---
static const Objective OBJS_M12[] = {
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Chinatown",
    "Sarah (numero inconnu) : je peux prouver ce que Rico a dit. Un parking, a Chinatown.",
    "Un homme mort, une mallette pres du corps. Mais tu n'es pas seul..." },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Chinatown",
    "Des nettoyeurs viennent pour la mallette. Prends-les de vitesse.",
    "Ramasse la mallette et file.", 0, 0, 2, EK_GUNNER, SP_AMBUSH },
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Planque",
    "Rapporte la mallette a la Planque.",
    "Sarah : c'est bien ce que je craignais. Il faut qu'on se voie." },
};
// M13 : escorte de Sarah. Le JOIN (objectif 1, count==1) la fait apparaitre en
// magenta et monter -- meme mecanique que Marco, allie different.
static const Objective OBJS_M13[] = {
  { OBJ_GOTO, 0, 0, 12, false, EV_NONE,       "Le Bar",
    "Sarah, la journaliste, t'attend au Bar. Vas-y a pied.", nullptr },
  { OBJ_TALK, 0, 0,  8, false, EV_MARCO_JOIN,  "Le Bar",
    "Sarah : Victor a peur, il efface les preuves. Sors-moi d'ici.",
    "Sarah monte. Direction la planque, et vite.", 1, 0 },
  { OBJ_GOTO, 0, 0, 16, true,  EV_NONE,        "Planque",
    "Conduis Sarah a la planque. Reste sur tes gardes.",
    "Sarah est a l'abri. Pour l'instant." },
};
static const Objective OBJS_M14[] = {
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Planque",
    "Sarah : ils nous ont trouves ! Reviens a la planque, vite !",
    "Les hommes de Victor encerclent la planque." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Planque",
    "Defends Sarah : premiere vague !", "Ils refluent... non, d'autres arrivent !",
    0, 0, 3, EK_THUG, SP_PRESENT },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Planque",
    "Des tireurs, cette fois. Tiens bon !",
    "Sarah : je sais ou il garde les dossiers. Ses Bureaux.", 0, 0, 2, EK_GUNNER, SP_AMBUSH },
};
static const Objective OBJS_M15[] = {
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Les Bureaux",
    "Les Bureaux de Victor. Les preuves sont a l'interieur. Entre.",
    "Des gardes patrouillent le hall." },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Les Bureaux",
    "Neutralise les gardes du hall.", "Tu tiens les dossiers. Maintenant, sors.",
    0, 0, 2, EK_GUNNER, SP_PRESENT },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Les Bureaux",
    "Des renforts te coupent la sortie. Force le passage.",
    "Sarah : 'Victor a ordonne l'assassinat de Marco.' On le tient.",
    0, 0, 3, EK_THUG, SP_AMBUSH },
};
// --- Trame Acte IV : Victor ---
static const Objective OBJS_M16[] = {
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "Chinatown",
    "Tony : on va lui faire mal au portefeuille. Vole sa voiture de luxe, a Chinatown.",
    nullptr },
  { OBJ_CRUSH,     0, 0, 14, true,  EV_NONE, "La Casse",
    "Amene-la a la Casse. Descends pres de la grue et fais-la BROYER.",
    "Une de moins. Tony : ca pique, hein Victor ?" },
  { OBJ_ENTER_CAR, 0, 0,  0, false, EV_NONE, "Le Casino",
    "Encore une, garee devant le Casino.", nullptr },
  { OBJ_CRUSH,     0, 0, 14, true,  EV_NONE, "La Casse",
    "Rebelote : au broyeur, et reste pres de la grue jusqu'au bout.",
    "Tony : ca va le rendre fou." },
};
static const Objective OBJS_M17[] = {
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE, "La Casse",
    "Tony : Bruno, l'homme de Victor, surveille la Casse. Descends-le.",
    "Bruno et ses hommes t'attendent." },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "La Casse",
    "Ses gardes d'abord.", "Reste Bruno. Il ne lachera pas facilement.",
    0, 0, 2, EK_THUG, SP_PRESENT },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "La Casse",
    "Bruno encaisse. Ne le lache pas.",
    "Bruno, mourant : le vieux casino... Victor t'y attend.", 5, 0 },
};
// M18 : boss final. Victor est le plus coriace (KILL count=7).
static const Objective OBJS_M18[] = {
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE, "Le Casino",
    "Victor : je t'attends au Casino. Viens donc, petit.",
    "Victor : tu es alle trop loin. Messieurs, occupez-vous de lui." },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Le Casino",
    "Ses gardes du corps t'attaquent ! Ouvre-toi un chemin jusqu'a Victor.",
    "La voie est libre. Reste Victor.", 0, 0, 4, EK_GUNNER, SP_PRESENT },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Le Casino",
    "Victor est coriace et bien protege. Acharne-toi.",
    "Tout ca... pour un ami. C'est fini, Victor.", 7, 0 },
};
// Le 4e champ = prime en $ versee a la reussite (selon la longueur/risque).
static const MissionDef MISSIONS[] = {
  { "Joe",              OBJS_JOE,      1, 150 },
  { "Le dernier trajet", OBJS_DEAL,    4, 250, true },   // index 1 = M4 (trame)
  { "Bagarre de rue",   OBJS_FIGHT,    1, 120 },
  { "Vengeance",        OBJS_VENGEANCE,1, 200 },
  { "Nettoyage",        OBJS_CLEAN,    2, 300 },
  { "Temoin genant",    OBJS_WITNESS,  1, 250 },
  { "Taxi clandestin",  OBJS_TAXI,     2, 200 },
  { "Course folle",     OBJS_RACE,     4, 400 },
  { "Livraison",        OBJS_DELIVERY, 2, 250 },
  { "Cavale",           OBJS_RUN,      1, 180 },
  { "Recouvrement",     OBJS_DEBT,     2, 300 },
  { "Racket",           OBJS_RACKET,   3, 350 },
  { "Vol de caisse",    OBJS_STEAL,    2, 350 },
  { "Le chauffard",     OBJS_ROADHOG,  1, 220 },
  { "Le livreur perdu", OBJS_LOST,     1, 150 },
  // index 15 : remplace la cabine bleue "Mauvaise affaire" (Marco). L'entree
  // "Mauvaise affaire" (index 1) reste definie ci-dessus mais n'est plus reliee
  // a aucune cabine : sa mecanique Marco est conservee pour la trame principale
  // (cf. campagne.md, mission M4).
  { "Livraison de pizza", OBJS_PIZZA,  2, 200 },
  { "Premier jour",     OBJS_M1, 4, 120, true },   // index 16 = M1 (trame)
  { "Les assurances",   OBJS_M2, 6, 150, true },   // index 17 = M2 (trame)
  { "Mauvaise dette",   OBJS_M3, 3, 180, true },   // index 18 = M3 (trame)
  { "Un nom",           OBJS_M5, 2, 200, true },   // index 19 = M5 (trame)
  { "Message aux Loups",OBJS_M6, 2, 250, true },   // index 20 = M6 (trame)
  { "Voiture volee",    OBJS_M7, 4, 350, true },   // index 21 = M7 (trame)
  { "Represailles",     OBJS_M8, 3, 350, true },   // index 22 = M8 (trame)
  { "Tournee de Marco", OBJS_M9, 3, 300, true },   // index 23 = M9 (trame)
  { "L'entrepot",       OBJS_M10,3, 400, true },   // index 24 = M10 (trame)
  { "Rico le Loup",     OBJS_M11,3, 450, true },   // index 25 = M11 (trame, boss)
  { "La mallette",      OBJS_M12,3, 350, true },   // index 26 = M12 (trame)
  { "Le temoin",        OBJS_M13,3, 400, true },   // index 27 = M13 (trame, escorte)
  { "Embuscade",        OBJS_M14,3, 450, true },   // index 28 = M14 (trame)
  { "Les dossiers",     OBJS_M15,3, 500, true },   // index 29 = M15 (trame)
  { "Sabotage",         OBJS_M16,4, 550, true },   // index 30 = M16 (trame)
  { "Bruno",            OBJS_M17,3, 600, true },   // index 31 = M17 (trame, boss)
  { "Le dernier appel", OBJS_M18,3, 800, true },   // index 32 = M18 (trame, boss final)
};
static const int NUM_MISSIONS = sizeof(MISSIONS) / sizeof(MISSIONS[0]);

// Sequence des missions de trame : campaignStep -> index dans MISSIONS[].
// La campagne avance d'un cran a chaque mission de trame reussie ; l'epilogue se
// declenche quand campaignStep atteint STORY_LEN (cf. boucle du telephone rouge).
// M4 = MISSION_DEAL (index 1) reusine ; M5+ ajoutees en fin de MISSIONS[].
static const uint8_t STORY_SEQ[] = { 16, 17, 18, 1, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
static const uint8_t STORY_LEN = sizeof(STORY_SEQ) / sizeof(STORY_SEQ[0]);

// --- Telephones : UN par mission, repartis sur toute la carte (grille ~4x4).
//     Position voulue en TUILES ; setup() la snappe sur la case libre la plus
//     proche (la carte change a chaque regeneration). `mission` = index dans
//     MISSIONS (meme ordre que ci-dessus). ---
struct PhoneDef { uint8_t tx, ty; uint8_t mission; };
static const PhoneDef PHONES[] = {
  { 14, 14,  0 },   // Joe
  { 38, 14, 15 },   // Livraison de pizza (ex-"Mauvaise affaire"/Marco)
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

// --- Telephone de la TRAME PRINCIPALE (couleur rouge, distinct des cabines
//     bleues de missions secondaires). UNE seule cabine, posee a cote de la
//     Planque (position calculee dans setup() depuis le POI "Planque", snappee
//     sur le trottoir le plus proche). Pour l'instant ELLE NE SONNE PAS : point
//     d'ancrage de la future campagne ; quand elle sera branchee, elle sonnera
//     et fera avancer l'histoire. ---
static const int NUM_STORY_PHONES = 1;
static int16_t storyPx[NUM_STORY_PHONES], storyPy[NUM_STORY_PHONES];  // px monde

// Couleurs de corps de cabine : bleu = missions secondaires, rouge = trame.
static const uint16_t PHONE_BODY_MISSION = 0x019F;   // bleu
static const uint16_t PHONE_BODY_STORY   = 0xC800;   // rouge fonce


// --- Pay'n'Spray : garages eparpilles, accessibles EN VOITURE. Positions
//     GENEREES par tools/pois.py (en bord de route, cote sans trottoir,
//     disseminees) et exportees dans citymap.h (citySprays[] en TUILES) -- plus
//     de placement aleatoire ni de snap runtime. Rouler dessus repeint la
//     voiture (nouvelle couleur) et remet la recherche police a zero (debit $).
static const int NUM_SPRAYS = CITY_NUM_SPRAYS;
static int16_t sprayPx[NUM_SPRAYS], sprayPy[NUM_SPRAYS];   // px monde
static const int   SPRAY_REACH = 10;    // px : rayon de declenchement (centre voiture)
static const int32_t SPRAY_COST = 50;   // $ preleve si on en a (sinon gratuit)
static bool sprayInside = false;         // dans la zone a la frame precedente (detection d'entree)

// --- AMU Nation : armureries accessibles A PIED. Comme les Pay'n'Spray, les
//     positions sont GENEREES (cityAmmus[] dans citymap.h : bord de route, cote
//     sans trottoir, disseminees sur la carte). S'en approcher et presser A
//     ouvre le magasin (UI modale).
static const int NUM_AMMUS = CITY_NUM_AMMUS;
static int16_t ammuPx[NUM_AMMUS], ammuPy[NUM_AMMUS];   // px monde
static const int AMMU_REACH = 12;        // px : portee d'ouverture (centre joueur)

// Tarifs : prix d'achat de l'arme (1re fois) et prix d'un rechargement (1 lot de
// munitions = WEAPONS[w].ammoPickup). Index par WeaponId ; FIST a 0 (jamais vendu).
static const int32_t WEAPON_PRICE[WEAPON_COUNT] = { 0, 100, 250, 200, 600, 300 };
static const int32_t AMMO_PRICE[WEAPON_COUNT]   = { 0,  40, 100,  80, 250, 120 };
static const int32_t ARMOR_PRICE = 200;    // gilet pare-balles (+3 PV)

// Etat du magasin (UI modale, monde gele pendant l'achat). Lignes : les armes
// (WEAPON_PISTOL..COUNT-1) puis une ligne GILET (index SHOP_ARMOR = WEAPON_COUNT).
static const uint8_t SHOP_ARMOR = WEAPON_COUNT;
static bool    shopOpen = false;
static uint8_t shopSel  = WEAPON_PISTOL;   // ligne surlignee (WEAPON_PISTOL..SHOP_ARMOR)

// --- Casino : machine a sous (UI modale, comme le magasin AMU) ---
// On entre a portee de la porte (POI "Le Casino") + A ; on sort avec MENU
// (comme pour sortir d'une voiture). Logique de tirage/gains : slot.h (pur).
#define CASINO_IDLE     0
#define CASINO_SPINNING 1
static const int32_t CASINO_BET_MAX = 50;   // mise plafond ($)
static bool     casinoOpen   = false;
static int      casinoPoiIdx = -1;                // index du POI "Le Casino"
static int32_t  casinoBet    = 1;
static uint8_t  casinoState  = CASINO_IDLE;
static uint8_t  casinoReel[3] = { SYM_SEVEN, SYM_SEVEN, SYM_SEVEN };
static uint8_t  casinoStop[3] = { 0, 0, 0 };      // frame d'arret par rouleau
static uint8_t  casinoSpinTimer = 0;
static int32_t  casinoLastWin   = 0;
static uint16_t casinoWinToast  = 0;              // frames d'affichage du resultat
static uint32_t casinoRng       = 0x1234567u;     // graine xorshift

// La Casse (broyeur) : GARER sa caisse sur la zone (point fixe genere
// cityCasse), DESCENDRE, et rester a portee -> la grue s'amorce puis broie
// l'epave contre une prime (selon les PV). Si on est au volant la grue ne se
// met pas en route ; remonter ou s'eloigner pendant l'amorcage l'annule.
// carGone = la voiture du joueur n'existe plus (broyee) : plus dessinee, plus
// re-montable tant qu'on n'en vole pas une autre.
static const int CASSE_REACH    = 14;      // px : rayon zone (centre voiture / joueur)
static const int CASSE_ARM      = 30;      // frames d'amorcage (~1 s a 30 fps)
static int       casseArm       = 0;       // compteur d'amorcage en cours (0 = inactif)
static bool      carGone        = false;   // voiture du joueur broyee / inexistante

// --- Etat runtime de la mission en cours. ---
static MissionRun missionRun = { 0, 0, false };
static uint8_t campaignStep = 0;          // index de la mission de trame courante (== STORY_LEN : epilogue ; au-dela : fini)
static bool    storyMissionActive = false;// la mission en cours est-elle une mission de trame ?
static uint16_t missionFailedTimer = 0;   // >0 : overlay plein ecran "MISSION ECHOUEE"
static const uint16_t MISSION_FAIL_FRAMES = 50;  // ~2 s a 25 fps
static uint16_t missionDoneTimer = 0;     // >0 : petit bandeau "MISSION ACCOMPLIE" (monte du bas, repart)
static const uint16_t MISSION_DONE_FRAMES = 60;  // ~2.4 s a 25 fps
static const int MISSION_DONE_SLIDE = 12;        // frames de glissement (montee / descente)
static uint16_t missionAnim = 0;          // compteur d'animation (clignotements)

// Copie runtime des objectifs de la mission active : les coords des objectifs
// references par un POI (champ `poi`) sont resolues ici au lancement, car la
// carte (donc les POI) change a chaque regeneration. curDef pointe sur curObjs.
static const int MAX_OBJS = 8;
static Objective curObjs[MAX_OBJS];
static MissionDef curDef = { "", curObjs, 0 };

// Compteurs de l'objectif courant : pietons mis KO et frames ecoulees.
static int      objBeat = 0;
static uint16_t objElapsed = 0;
static int      objSubdue = 0;            // coups portes a la cible de SUBDUE (objectif courant)
static bool     killerChase = false;      // le prochain OBJ_KILL spawne un TUEUR qui fonce (post EV_MARCO_DIE)
static uint8_t  targetHp = 1;             // PV de la cible KILL : 1 = mort au 1er coup ; >1 = BOSS (encaisse `count` coups)
static bool     missionCrushDone = false; // OBJ_CRUSH : la voiture de mission vient d'etre broyee

// --- Cinematique de mission (seqKind == SEQ_CUT : joueur fige) -------------
// Un acteur marche, des repliques defilent en bas, un evenement survient, puis
// le controle revient. cutsceneUpdate() pilote tout. CUT_MARCO_DEATH = Marco
// descend, parle au tueur, se fait abattre. CUT_TAUNT = bref face-a-face (boss/
// embuscade) : deux repliques puis les ennemis (deja poses) chargent.
enum { CUT_NONE = 0, CUT_MARCO_DEATH = 1, CUT_TAUNT = 2, CUT_MARCO_LEAVE = 3 };
static uint8_t  cutKind  = CUT_NONE;
static uint8_t  cutPhase = 0;
static uint16_t cutTimer = 0;
static const char *cutLine1 = nullptr, *cutLine2 = nullptr;  // repliques du TAUNT
static int16_t marcoLeaveX = 0, marcoLeaveY = 0;             // porte vers laquelle Marco rentre (CUT_MARCO_LEAVE)
static const uint16_t CUT_LINE_FRAMES  = 80;   // ~3 s par replique
static const uint16_t CUT_SHOOT_FRAMES = 26;   // temps sur le corps de Marco

// Cible de mission (Joe : erre + fuit ; tueur : fonce sur le joueur).
enum { T_WANDER = 0, T_FLEE = 1, T_EMERGE = 2 };  // T_EMERGE : sort du batiment vers son poste
struct Target {
  float x, y; uint8_t dir; int tgtx, tgty;
  uint8_t frame, animTimer; uint8_t phase; uint16_t loseTimer;
  bool active; bool chase;
};
static Target target;
static uint8_t targetDownTimer = 0;       // >0 : splat (cible/Marco) feedback
static int targetDownX = 0, targetDownY = 0;
static uint16_t targetAtkTimer = 0;       // recharge des coups de la cible AGRESSIVE (boss/tueur/SUBDUE)

// --- Ennemis scenarises (gardes, assaillants) : pool d'entites AGRESSIVES posees
//     par un objectif (enemyCount>0). Contrairement aux passants, ils foncent sur
//     le joueur et le frappent (ou tirent), et SEULS comptent pour l'objectif (un
//     passant tue n'avance plus jamais une mission). cf. mission.h. ---
enum { EN_IDLE = 0, EN_AGGRO = 1, EN_DOWN = 2 };   // phase d'un ennemi
struct Enemy {
  float x, y; uint8_t dir, frame, animTimer;
  int tgtx, tgty;        // point-cible courant (poursuite par voies)
  uint8_t hp;            // coups avant de tomber
  uint8_t kind;          // EK_THUG / EK_GUNNER
  uint8_t phase;         // EN_IDLE / EN_AGGRO / EN_DOWN
  uint16_t atkTimer;     // recharge entre deux frappes/tirs
  uint8_t downTimer;     // EN_DOWN : frames de splat avant despawn
  bool active;
};
static const int MAX_ENEMIES = 4;
static Enemy enemies[MAX_ENEMIES];
static const uint16_t ENEMY_COLOR_THUG   = 0xFA20;  // orange-rouge (gros bras)
static const uint16_t ENEMY_COLOR_GUNNER = 0xF800;  // rouge vif (tireur arme)
static const int   ENEMY_AGGRO_RANGE  = 56;   // px : distance d'eveil (IDLE -> AGGRO)
static const float ENEMY_SPEED        = 0.45f;// px/frame (un poil < joueur a pied)
static const int   ENEMY_MELEE_DIST   = 8;    // px : portee du coup au corps-a-corps
static const uint16_t ENEMY_MELEE_PERIOD = 26;// frames entre deux coups (~1 s)
static const int   ENEMY_MELEE_DMG    = 1;    // coeurs perdus par coup
static const int   ENEMY_SHOOT_RANGE  = 50;   // px : portee de tir (gunner)
static const uint16_t ENEMY_SHOOT_PERIOD = 42;// frames entre deux tirs
static const uint8_t ENEMY_HP_THUG    = 3;    // 3 coups de poing pour un gros bras
static const uint8_t ENEMY_HP_GUNNER  = 2;    // tireur plus fragile
static const uint8_t ENEMY_DOWN_FRAMES = 35;  // duree du splat avant despawn

// Voiture de mission a recuperer au parking (objectif ENTER_CAR).
static CarState mCar;
static bool mCarActive = false;           // garee, en attente d'etre prise
static bool carIsMission = false;         // le joueur conduit la voiture de mission

// Marco (passager scenarise). Trois etats successifs :
//   marcoWaiting : debout au marqueur, attend qu'on l'aborde (TALK / GOTO).
//   marcoFollow  : nous suit A PIED (companion) jusqu'a ce qu'on monte en caisse.
//   marcoAboard  : assis dans la voiture (plus dessine separement).
static bool marcoWaiting = false;
static bool marcoFollow  = false;
static bool marcoAboard  = false;
static float   marcoX = 0.0f, marcoY = 0.0f;  // position monde quand il suit a pied
static uint8_t marcoDir = DIR_SOUTH;          // orientation du sprite (suivi)
static uint8_t marcoFrame = 0, marcoAnimTimer = 0;  // anim de marche
static uint16_t marcoEmergeDelay = 0;           // >0 : Marco encore dans le batiment (a dit "j'arrive", sort apres)
static const float MARCO_FOLLOW_SPEED = 0.55f;  // px/frame (un poil > joueur a pied)
static const float MARCO_FOLLOW_GAP   = 10.0f;  // px : distance de confort derriere JW
static const uint16_t MARCO_EMERGE_DELAY = 32;  // frames avant qu'il sorte (laisse lire "j'arrive")

// Narration : file de messages flash ; bandeau bas auto-time + scroll horizontal.
static const char *narrQueue[4];
static uint8_t narrHead = 0, narrCount = 0;
static uint16_t narrTimer = 0;       // frames restantes du message courant
static uint16_t narrAge = 0;         // frames ecoulees sur le message courant
static int narrScroll = 0;           // decalage horizontal (px)
static const int NARR_HOLD    = 28;  // frames cale a gauche (lecture du debut)
static const int NARR_SPEED10 = 12;  // px de scroll par 10 frames (plus grand = rapide)
static const int NARR_ENDPAD  = 22;  // frames cale a la fin

// Declarations anticipees (utilisees avant leur definition).
static void killTarget(int px, int py);
static void startMission(uint8_t m);
static void narrate(const char *s);
static int  findPoi(const char *name);
static void hurtPlayer(int dmg, bool byCop);
static void explodeCarAt(int wx, int wy);
static void startPanic(AiPed &p, int srcx, int srcy);
static bool npcWalkToward(float &x, float &y, uint8_t &dir, uint8_t &frame,
                          uint8_t &animTimer, float tx, float ty, float speed);
static uint16_t getupFrames();
static void hitEnemy(Enemy &e, bool lethal);
static int  enemiesAliveCount();
static void clearEnemies();
static void startMarcoDeathCut();
static void startTauntCut(const char *l1, const char *l2);
static void cutsceneUpdate();

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

// Centre (px monde) d'une tuile d'IMMEUBLE (BUILDING_A/B) proche de (cx,cy) : sert
// de point d'apparition "porte" d'ou un PNJ SORT du batiment (anim d'emergence)
// au lieu de pop sur place. Spirale, rayon 6. false si aucun immeuble proche.
static bool findBuildingTileNear(int cx, int cy, int &ox, int &oy) {
  int ctx = cx >> 3, cty = cy >> 3;
  for (int r = 1; r <= 6; r++) {
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (abs(dx) != r && abs(dy) != r) continue;            // anneau
        int tx = ctx + dx, ty = cty + dy;
        if (tx < 0 || tx >= CITY_W || ty < 0 || ty >= CITY_H) continue;
        uint8_t t = cityMap[ty * CITY_W + tx];
        if (t == TILE_BUILDING_A || t == TILE_BUILDING_B) {
          ox = tx * TILE_W + TILE_W / 2;
          oy = ty * TILE_H + TILE_H / 2;
          return true;
        }
      }
    }
  }
  return false;
}

// Centre (px monde) d'une tuile de ROUTE proche de (cx,cy), en PRIVILEGIANT le
// cote droit (est) : la caisse de mission s'y gare "sur le cote" plutot que pile
// sur le marqueur, pour laisser voir le PNJ sortir. Spirale ponderee est, r<=8.
static bool findRoadSpotNear(int cx, int cy, int &ox, int &oy) {
  int ctx = cx >> 3, cty = cy >> 3;
  for (int r = 1; r <= 8; r++) {
    // ordre de balayage : on tente les colonnes les plus a droite d'abord.
    for (int dx = r; dx >= -r; dx--) {
      for (int dy = -r; dy <= r; dy++) {
        if (abs(dx) != r && abs(dy) != r) continue;            // anneau
        int tx = ctx + dx, ty = cty + dy;
        if (tx < 0 || tx >= CITY_W || ty < 0 || ty >= CITY_H) continue;
        uint8_t t = cityMap[ty * CITY_W + tx];
        if (t == TILE_ROAD_H || t == TILE_ROAD_V || t == TILE_ROAD_CROSS) {
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

  // Pas de voiture de depart : JW commence a pied (il en volera une au besoin).
  // carGone => caisse perso ni dessinee ni "remontable" ; voler une caisse IA
  // (ENTER_AI_DIST) repasse carGone a false et remplit car.*.
  car.x = 0.0f; car.y = 0.0f; car.angle = 0.0f; car.vx = 0.0f; car.vy = 0.0f;
  carGone = true;

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

  // Cabine rouge de la campagne : posee a cote de la Planque (muette pour
  // l'instant). On part du point-cible du POI "Planque" (devant la porte) et on
  // snappe sur le trottoir le plus proche ; repli case libre, puis spawn joueur.
  {
    int wx = PLAYER_START_X * TILE_W + TILE_W / 2;
    int wy = PLAYER_START_Y * TILE_H + TILE_H / 2;
    int pi = findPoi("Planque");
    if (pi >= 0) { wx = cityPois[pi].tx; wy = cityPois[pi].ty; }
    int ox, oy;
    if (findSidewalkSpot(wx, wy, ox, oy)) { wx = ox; wy = oy; }
    else if (findFootSpot(wx, wy, ox, oy)) { wx = ox + PLAYER_W / 2; wy = oy + PLAYER_H / 2; }
    storyPx[0] = wx; storyPy[0] = wy;
  }

  // Pay'n'Spray : positions generees (citySprays[]), deja en bord de route cote
  // sans trottoir -> centre de la tuile, aucun snap necessaire.
  for (int i = 0; i < NUM_SPRAYS; i++) {
    sprayPx[i] = citySprays[i].tx * TILE_W + TILE_W / 2;
    sprayPy[i] = citySprays[i].ty * TILE_H + TILE_H / 2;
  }
  sprayInside = false;

  // AMU Nation : positions generees (cityAmmus[]), meme regle de placement.
  for (int i = 0; i < NUM_AMMUS; i++) {
    ammuPx[i] = cityAmmus[i].tx * TILE_W + TILE_W / 2;
    ammuPy[i] = cityAmmus[i].ty * TILE_H + TILE_H / 2;
  }
  shopOpen = false; shopSel = WEAPON_PISTOL;

  // Casino : ouvrable dans toute la bbox du POI (la ou le bandeau affiche
  // "Le Casino"). Pas de donnees a regenerer.
  casinoPoiIdx = findPoi("Le Casino");
  casinoOpen = false; casinoState = CASINO_IDLE; casinoBet = 1;
  casinoWinToast = 0; casinoLastWin = 0;
  hospInside = false; commInside = false;

  casseArm = 0; carGone = false;

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
  playerDown = false; playerDownTimer = 0; playerDownArrest = false;
  playerJump = 0; playerJumpCool = 0; playerJumpAcc = 0.0f;
  wantedReset(wanted);
  carHp = CAR_MAX_HP;
  carFuse = 0; carRunaway = false; carImpactX = 0.0f; carImpactY = 0.0f;
  for (int i = 0; i < NUM_WRECKS; i++) wrecks[i].active = false;
  for (int i = 0; i < NUM_SMOKE; i++) smokes[i].active = false;
  boomTimer = 0;
  overlayMsg = nullptr; overlayTimer = 0;
  seqKind = SEQ_NONE; seqPhase = 0; seqTimer = 0; seqPoi = nullptr;
  hospInside = false; crushReward = 0;

  // Argent, butin au sol et projectiles : tout vide au demarrage.
  playerMoney = 10;                            // pecule de depart
  moneyShown = playerMoney; moneyRollStep = 1; moneyGainAmt = 0; moneyGainTimer = 0;
  for (int i = 0; i < NUM_LOOT; i++) loots[i].active = false;
  for (int i = 0; i < NUM_BULLETS; i++) bullets[i].active = false;
  for (int i = 0; i < NUM_PROJ; i++) projs[i].active = false;
  lootNext = 0;

  // Missions au repos : les telephones sonnent quand on s'en approche.
  missionRun.active = false;
  target.active = false;
  targetDownTimer = 0;
  mCarActive = false; carIsMission = false;
  marcoWaiting = false; marcoFollow = false; marcoAboard = false;
  killerChase = false;
  campaignStep = 0; storyMissionActive = false; missionFailedTimer = 0;  // trame remise a zero
  missionDoneTimer = 0;
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

// Girophare : 2 pixels rouge/bleu au centre du toit qui ALTERNENT (clignotement)
// selon la phase d'animation globale. Dessine par-dessus la caisse. Aucun sprite
// supplementaire en flash. Appele pour les voitures de police au-dela d'1 etoile.
static void drawGyro(int camX, int camY, int worldCx, int worldCy) {
  int cx = worldCx - camX, cy = worldCy - camY;
  bool phase = (missionAnim >> 2) & 1;            // bascule toutes les ~4 frames
  uint16_t left  = phase ? 0xF800 : 0x001F;       // rouge / bleu
  uint16_t right = phase ? 0x001F : 0xF800;
  if (cy >= 0 && cy < SCREEN_H) {
    uint16_t *row = fb + cy * SCREEN_W;
    if (cx - 1 >= 0 && cx - 1 < SCREEN_W) row[cx - 1] = left;
    if (cx     >= 0 && cx     < SCREEN_W) row[cx]     = right;
  }
}

// Index de frame de rotation pour l'angle courant de la voiture pilotee.
static inline int carFrameIdx() {
  int idx = (int)(car.angle / TWO_PI * CAR_FRAMES + 0.5f);
  idx %= CAR_FRAMES;
  if (idx < 0) idx += CAR_FRAMES;
  return idx;
}

// Blit d'un petit sprite de fumee pre-rendu (SMOKE_BOX), centre ecran (scx,scy).
static void blitSmoke(int scx, int scy, int tier, int frame) {
  const uint16_t *src = smokeFrames[tier][frame];
  int ox = scx - SMOKE_BOX / 2, oy = scy - SMOKE_BOX / 2;
  for (int ry = 0; ry < SMOKE_BOX; ry++) {
    int y = oy + ry;
    if (y < 0 || y >= SCREEN_H) continue;
    uint16_t *row = fb + y * SCREEN_W;
    const uint16_t *srow = src + ry * SMOKE_BOX;
    for (int rx = 0; rx < SMOKE_BOX; rx++) {
      uint16_t c = srow[rx];
      if (c == SMOKE_TRANSPARENT) continue;
      int x = ox + rx;
      if (x >= 0 && x < SCREEN_W) row[x] = c;
    }
  }
}


// Blit "epave brulee" : la silhouette d'une frame voiture recoloriee charbon
// (deux tons : corps fonce, vitres/feux clairs). Reutilise carFrames -> zero
// octet de flash supplementaire.
static void blitCarBurnt(int camX, int camY, int worldCx, int worldCy, int frameIdx) {
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
      int x = ox + rx;
      if (x < 0 || x >= SCREEN_W) continue;
      row[x] = (c == CAR_BODY_KEY) ? 0x2104 : 0x4208;   // charbon fonce / clair
    }
  }
}

// --- Epaves : naissance (petit saut), recyclage au loin, rendu (carcasse +
//     fumee residuelle). spawnWreck est appele a la destruction d'une voiture. ---
static void spawnWreck(float wx, float wy, uint8_t frameIdx, float hopx, float hopy) {
  float n = sqrtf(hopx * hopx + hopy * hopy);
  float ux = 0.0f, uy = 0.0f;
  if (n > 0.01f) { ux = hopx / n; uy = hopy / n; }
  int slot = -1;
  for (int i = 0; i < NUM_WRECKS; i++) if (!wrecks[i].active) { slot = i; break; }
  if (slot < 0) slot = 0;                       // pool plein : recycle le slot 0
  Wreck &w = wrecks[slot];
  w.x = wx; w.y = wy; w.vx = ux * 1.6f; w.vy = uy * 1.6f;
  w.frame = frameIdx; w.hop = WRECK_HOP_FRAMES; w.active = true;
}

static void updateWrecks(int fcx, int fcy) {
  const int rec2 = RECYCLE_DIST * RECYCLE_DIST;
  for (int i = 0; i < NUM_WRECKS; i++) {
    Wreck &w = wrecks[i];
    if (!w.active) continue;
    if (w.hop > 0) {                            // saut initial : avance, freine, bute
      float nx = w.x + w.vx, ny = w.y + w.vy;
      if (!carBoxHitsSolid(nx, w.y, CAR_HALF)) w.x = nx; else w.vx = 0.0f;
      if (!carBoxHitsSolid(w.x, ny, CAR_HALF)) w.y = ny; else w.vy = 0.0f;
      w.vx *= 0.7f; w.vy *= 0.7f; w.hop--;
    }
    int ddx = (int)w.x - fcx, ddy = (int)w.y - fcy;
    if (ddx * ddx + ddy * ddy > rec2) w.active = false;   // trop loin -> disparait (recyclage)
  }
}

static void drawWrecks(int camX, int camY) {
  for (int i = 0; i < NUM_WRECKS; i++) {
    Wreck &w = wrecks[i];
    if (!w.active) continue;
    blitCarBurnt(camX, camY, (int)w.x, (int)w.y, w.frame);
    int frame = (missionAnim / 6 + i) % SMOKE_FRAMES;     // fumee residuelle (legere)
    blitSmoke((int)w.x - camX, (int)w.y - camY, 0, frame);
  }
}

// --- Fumee d'explosion (roquette/grenade) : nuage dense ~5 s au point de tir. ---
static void spawnSmoke(float wx, float wy) {
  int slot = -1;
  for (int i = 0; i < NUM_SMOKE; i++) if (!smokes[i].active) { slot = i; break; }
  if (slot < 0) slot = 0;                       // pool plein : recycle le slot 0
  Smoke &s = smokes[slot];
  s.x = wx; s.y = wy; s.life = SMOKE_PUFF_FRAMES; s.active = true;
}

static void updateSmoke() {
  for (int i = 0; i < NUM_SMOKE; i++)
    if (smokes[i].active && --smokes[i].life == 0) smokes[i].active = false;
}

static void drawSmoke(int camX, int camY) {
  for (int i = 0; i < NUM_SMOKE; i++) {
    Smoke &s = smokes[i];
    if (!s.active) continue;
    int tier = (s.life < SMOKE_PUFF_FRAMES / 3) ? 0 : 1;   // s'amincit en fin de vie
    int sx = (int)s.x - camX, sy = (int)s.y - camY;
    int base = missionAnim / 3;
    blitSmoke(sx,     sy,     tier, (base + i) % SMOKE_FRAMES);
    blitSmoke(sx - 2, sy - 3, tier, (base + 1) % SMOKE_FRAMES);
    blitSmoke(sx + 2, sy - 2, tier, (base + 2) % SMOKE_FRAMES);
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

// Corps allonge au sol (assomme, PAS mort) : un ovale couche de la couleur du
// perso + une tete chair. Reutilise pour le joueur et les PNJ en etat 3.
static void blitDownBody(int camX, int camY, int worldCx, int worldCy, uint16_t color) {
  int cx = worldCx - camX, cy = worldCy - camY;
  for (int ry = -1; ry <= 1; ry++)
    for (int rx = -3; rx <= 2; rx++) {
      int x = cx + rx, y = cy + ry;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = color;
    }
  int hx = cx + 3, hy = cy;                       // tete (teint chair)
  if (hx >= 0 && hx < SCREEN_W && hy >= 0 && hy < SCREEN_H) fb[hy * SCREEN_W + hx] = 0xFE19;
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

// Eclat "coup recu" : petite etoile jaune (centre + 8 branches a 2 px) posee SUR
// le joueur quand un flic le matraque. Couleur distincte de la croix blanche de
// l'attaque joueur -> on lit tout de suite "je me fais taper".
static void blitCopHitFx(int camX, int camY) {
  int cx = playerX + PLAYER_W / 2 - camX, cy = playerY + PLAYER_H / 2 - camY;
  static const int8_t bx[9] = { 0, -2, 2,  0, 0, -2, 2, -2, 2 };
  static const int8_t by[9] = { 0,  0, 0, -2, 2, -2, -2, 2, 2 };
  for (int i = 0; i < 9; i++) {
    int x = cx + bx[i], y = cy + by[i];
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
      fb[y * SCREEN_W + x] = 0xFFE0;              // jaune vif
  }
}

// Piéton joueur : sprite recolore en teinte fixe (centre sur la boite PLAYER).
static void drawPlayer(int camX, int camY) {
  if (playerDown) {                               // assomme : corps au sol
    blitDownBody(camX, camY, playerX + PLAYER_W / 2, playerY + PLAYER_H / 2, PLAYER_BODY_COLOR);
    return;
  }
  if (playerJump > 0) {                           // en l'air : ombre au sol + sprite souleve
    int prog = JUMP_FRAMES - playerJump;          // 0..JUMP_FRAMES
    float t = (float)prog / (float)JUMP_FRAMES;   // progression normalisee
    int raise = (int)(JUMP_MAX_Z * 4.0f * t * (1.0f - t) + 0.5f);   // parabole 0->max->0
    int sx = playerX + PLAYER_W / 2 - camX, sy = playerY + PLAYER_H / 2 - camY;
    for (int rx = -2; rx <= 2; rx++) {            // petite ombre ovale sous les pieds
      int x = sx + rx, y = sy + 2;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0x2104;
    }
    blitPed(camX, camY, playerX + PLAYER_W / 2, playerY + PLAYER_H / 2 - raise,
            playerDir, playerFrame, PLAYER_BODY_COLOR);
    return;
  }
  blitPed(camX, camY, playerX + PLAYER_W / 2, playerY + PLAYER_H / 2,
          playerDir, playerFrame, PLAYER_BODY_COLOR);
}

// Voiture joueur : frame de rotation la plus proche de l'angle continu.
static void drawCar(int camX, int camY) {
  // Pendant la repeinture, la caisse est "dans" le garage : on la masque jusqu'a
  // ce qu'elle ressorte (PH_OUT). Au broyage, elle disparait des PH_CRUSH. Et si
  // elle a ete broyee, plus de voiture du tout.
  if (seqKind == SEQ_SPRAY && (seqPhase == PH_IN || seqPhase == PH_SPRAY)) return;
  // Au broyage, des que la grue l'a saisie (PH_CARRY) la voiture est dessinee
  // au bout de la fleche par drawSequence : on la masque ici.
  if (seqKind == SEQ_CRUSH &&
      (seqPhase == PH_CARRY || seqPhase == PH_CRUSH || seqPhase == PH_EJECT)) return;
  if (carGone) return;
  float a = car.angle;
  int idx = (int)(a / TWO_PI * CAR_FRAMES + 0.5f);
  idx %= CAR_FRAMES;
  if (idx < 0) idx += CAR_FRAMES;
  blitCar(camX, camY, (int)car.x, (int)car.y, idx, carColor);
  // Si on roule dans une voiture de police volee, le girophare suit la meme regle.
  if (carColor == POLICE_BLUE && wanted.level >= 2)
    drawGyro(camX, camY, (int)car.x, (int)car.y);
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

// % de pietons remplaces par des flics, et % de trafic en voiture de police, en
// fonction du niveau de recherche. Pur (level -> %), faciles a relire/ajuster.
static inline int copPedSpawnPct(uint8_t level) {
  int p = (int)level * COP_SPAWN_PER_STAR;
  return p > COP_SPAWN_MAX ? COP_SPAWN_MAX : p;
}
static inline int policeCarPct(uint8_t level) {
  return POLICE_CAR_BASE + (int)level * POLICE_CAR_PER_STAR;
}

static void aiRespawnCar(AiCar &c, int ccx, int ccy) {
  int tx, ty;
  if (aiFindTileInRing(ccx, ccy, RING_MIN, RING_MAX, aiIsDrivable, tx, ty)) {
    aiPlace(cityMap, CITY_W, CITY_H, c.x, c.y, c.dir, c.tgtx, c.tgty,
            tx, ty, aiIsDrivable, aiRng);
    c.isPolice = ((int)(aiRngNext(aiRng) % 100) < policeCarPct(wanted.level));   // part du trafic = police (croit avec les etoiles)
    c.color = c.isPolice ? POLICE_BLUE : AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N];
    c.hp = CAR_MAX_HP;
    c.driver = true;                       // trafic = voiture avec conducteur
    c.fleeing = false; c.fleeTimer = 0;    // arrive calme
    c.brakeReact = 0;                      // reflexes au repos
    c.backoff = 0;
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
    p.frame = 0; p.animTimer = 0; p.state = 0; p.downTimer = 0; p.panicTimer = 0;
    p.hp = 3; p.shootTimer = COP_SHOOT_PERIOD; p.meleeTimer = 0;
    // Si la police nous recherche, une partie des pietons spawn en flics (bleus)
    // qui foncent sur le joueur (cf. aiUpdate). Sinon, pieton civil recolore.
    p.isCop = (wanted.level > 0) && ((int)(aiRngNext(aiRng) % 100) < copPedSpawnPct(wanted.level));
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

// Le conducteur d'une voiture descend : un pieton apparait a cote.
//  - knockedDown : il tombe d'abord au sol (etat 3) puis se relevera et fuira
//    (vol de voiture) ; sinon il detale tout de suite (voiture mitraillee).
//  - asCop : c'est un policier (voiture de police) qui sort pour arreter/tirer.
static void aiEjectDriver(int atx_px, int aty_px, bool knockedDown, bool asCop, uint8_t carDir) {
  int slot = -1;
  for (int i = 0; i < NUM_AI_PEDS; i++) if (!aiPeds[i].active) { slot = i; break; }
  if (slot < 0) slot = 0;                  // pool plein : ecrase le premier
  AiPed &p = aiPeds[slot];
  bool placed = false;
  // Vol de voiture : le conducteur tombe a cote de SA portiere (cote gauche du
  // sens de la caisse), un peu en retrait pour ne pas etre ecrase au demarrage.
  if (knockedDown && !asCop) {
    uint8_t ld = AI_LEFT[carDir];
    int doorx = atx_px + AI_DX[ld] * DRIVER_DOOR_OFFSET;
    int doory = aty_px + AI_DY[ld] * DRIVER_DOOR_OFFSET;
    if (!isSolidAt(doorx >> 3, doory >> 3)) {     // pas dans un mur : pose a la portiere
      p.x = (float)doorx; p.y = (float)doory; p.dir = carDir;
      p.tgtx = doorx; p.tgty = doory; placed = true;
    }
  }
  if (!placed) {                           // sinon (flic/panique, ou portiere bloquee) : tuile marchable proche
    int tx, ty;
    if (!aiFindWalkTileNear(atx_px, aty_px, tx, ty)) return;   // pas de place
    aiPlace(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
            tx, ty, aiIsWalkable, aiRng);
  }
  p.frame = 0; p.animTimer = 0; p.downTimer = 0; p.panicTimer = 0;
  p.hp = 3; p.isCop = asCop; p.shootTimer = COP_SHOOT_PERIOD; p.meleeTimer = 0;
  p.color = asCop ? POLICE_BLUE : AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N];
  p.active = true;
  if (asCop) { p.state = 0; }                       // flic : debout, prend la chasse
  else if (knockedDown) { p.state = 3; p.downTimer = getupFrames(); }  // tombe -> se releve -> fuit
  else { p.state = 0; startPanic(p, atx_px, aty_px); }                 // detale aussitot
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

// Affole un PNJ civil : il part en courant loin du point (srcx,srcy) sans
// respecter les trottoirs (cf. aiPanicStep). Flics et PNJ au sol non concernes.
static void startPanic(AiPed &p, int srcx, int srcy) {
  if (p.isCop || p.state == 1) return;
  p.state = 2; p.panicTimer = PED_PANIC_FRAMES;
  p.panicX = (int16_t)srcx; p.panicY = (int16_t)srcy;
}

// Affole une voiture (trafic civil) temoin d'un crime : elle accelere, n'evite
// plus pietons ni joueur et ecrase en continuant. La police est exclue (elle
// poursuit deja). Re-arme le timer si deja en fuite.
// Arme le recul : la caisse va s'eloigner du joueur pendant CAR_BACKOFF_FRAMES.
// On colle la cible sur la position courante pour qu'au pas suivant elle
// choisisse tout de suite une direction qui l'eloigne (cf. aiCarStepAway).
static void startBackoff(AiCar &c) {
  c.backoff = CAR_BACKOFF_FRAMES;
  c.tgtx = (int)c.x; c.tgty = (int)c.y;
}

static void startCarFlee(AiCar &c) {
  if (!c.active || c.isPolice || !c.driver) return;
  c.fleeing = true; c.fleeTimer = CAR_FLEE_FRAMES;
  startBackoff(c);                       // panique : commence par s'eloigner du joueur
}

// Diffuse l'alarme aux voitures dans un rayon autour d'un crime (tir, meurtre,
// explosion). Memes points d'appel que la panique des pietons.
static void alarmCarsAround(int sx, int sy, int radius) {
  long r2 = (long)radius * radius;
  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (!c.active) continue;
    long dx = (long)c.x - sx, dy = (long)c.y - sy;
    if (dx * dx + dy * dy <= r2) startCarFlee(c);
  }
}

// Tue un pieton SANS rien crediter au joueur : sert quand c'est le trafic (et
// pas le joueur) qui le renverse. Le corps reste au sol puis despawn.
static void deadPed(AiPed &p) {
  p.state = 1; p.downTimer = PED_DOWN_FRAMES;
}

// Met un pieton au sol (KO/mort) + comptabilise l'objectif BEAT + fait monter la
// recherche police (un mort = un crime). Mutualise poing/arme/ecrasement JOUEUR.
static void knockDownPed(AiPed &p) {
  deadPed(p);
  if (missionRun.active) objBeat++;
  if (p.isCop) {
    wantedOnCopKill(wanted);               // tuer un flic : +1 etoile immediate
    spawnLoot((int)p.x, (int)p.y, LOOT_AMMO, WEAPON_PISTOL,
              WEAPONS[WEAPON_PISTOL].ammoPickup);   // 100% : son pistolet
  } else {
    wantedOnKill(wanted);                  // crime : streak -> etoiles
    dropLoot((int)p.x, (int)p.y);
  }
  // Temoins proches : un meurtre a cote affole (rayon court).
  for (int i = 0; i < NUM_AI_PEDS; i++) {
    AiPed &o = aiPeds[i];
    if (&o == &p || !o.active || o.state == 1) continue;
    int dx = (int)o.x - (int)p.x, dy = (int)o.y - (int)p.y;
    if (dx * dx + dy * dy <= KILL_PANIC_RANGE * KILL_PANIC_RANGE) startPanic(o, (int)p.x, (int)p.y);
  }
  alarmCarsAround((int)p.x, (int)p.y, KILL_PANIC_RANGE);   // voitures temoins : fuite
}

// Inflige un coup au pieton. lethal = arme a feu (1 balle -> mort immediate).
// Au POING : chaque coup le met AU SOL (state 3 : il se releve puis fuit) ; au
// 3e coup (PV epuises) il meurt (splat, butin, +recherche).
static void hitPed(AiPed &p, bool lethal) {
  if (lethal || p.hp <= 1) { knockDownPed(p); return; }   // arme a feu, ou coup fatal
  p.hp--;
  p.state = 3; p.downTimer = getupFrames();               // au sol, se relevera et detalera
  gb.sound.tone(150, 50);
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
    marcoWaiting = false; marcoFollow = false; marcoAboard = false;
    mCarActive = false;
    storyMissionActive = false; killerChase = false;
    clearEnemies();
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

// Dormir a la Planque (entree a pied, presser A) : ecran noir bref puis reveil
// devant la porte, vie pleine (3 coeurs ROUGES, gilet retire). Gratuit.
static void startSleepSeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_SLEEP; seqPhase = PH_FADE; seqTimer = SEQ_SLEEP_FRAMES;
  gb.sound.tone(330, 120); gb.sound.tone(247, 180);   // petit air de dodo
}

// Enfile le gilet pare-balles : +3 PV (les points du dessus, bleus), plafonne a
// 6. Generique : achat a AMU Nation ou montee dans une voiture de police.
static void giveBodyArmor() {
  if (playerHearts >= PLAYER_HP_MAX) return;     // gilet deja plein
  playerHearts += PLAYER_HEARTS_MAX;             // +3
  if (playerHearts > PLAYER_HP_MAX) playerHearts = PLAYER_HP_MAX;
  narrate("Gilet pare-balles");
  gb.sound.tone(660, 50); gb.sound.tone(880, 60);
}

// Mort du joueur a pied (plus de cœurs) : reapparition devant l'hopital.
static void wastedPlayer() { startEndSeq(SEQ_WASTED, "MORT", "Hopital", false); }

// Arrestation (un flic te touche) : retour au commissariat.
static void bustedPlayer() { startEndSeq(SEQ_BUSTED, "ARRETE", "Commissariat", false); }

// Le joueur encaisse dmg cœur(s), avec invulnerabilite breve pour eviter de tout
// perdre d'un coup. A 0 cœur : si le coup fatal vient d'un flic (matraque) ->
// ARRESTATION (commissariat) ; sinon (voiture, explosion, balle) -> MORT (hopital).
static void hurtPlayer(int dmg, bool byCop) {
  if (seqKind != SEQ_NONE) return;          // fige pendant une cinematique
  if (playerDown) return;                   // deja au sol : invulnerable le temps de l'anim
  if (playerHurtTimer > 0) return;
  playerHearts -= dmg;
  playerHurtTimer = PLAYER_HURT_COOLDOWN;
  hitFlashTimer = HIT_FLASH_FRAMES;         // flash rouge des LEDs de la console
  gb.sound.tone(180, 60);
  if (playerHearts <= 0) { playerHearts = 0; if (byCop) bustedPlayer(); else wastedPlayer(); }
}

// Duree au sol avant de se relever : base + alea (pour que ce ne soit pas mecanique).
static uint16_t getupFrames() {
  return (uint16_t)(GETUP_MIN_FRAMES + aiRngNext(aiRng) % (GETUP_RAND_FRAMES + 1));
}

// Le joueur roule-t-il sur un trottoir ? (caisse pilotee, posee sur une tuile
// TILE_PAVEMENT). Sert a affoler les pietons qu'on vient menacer sur le trottoir.
static bool carOnSidewalk() {
  if (!driving) return false;
  int tx = (int)car.x >> 3, ty = (int)car.y >> 3;
  return aiInBounds(CITY_W, CITY_H, tx, ty) && cityMap[ty * CITY_W + tx] == TILE_PAVEMENT;
}

// Met le JOUEUR au sol. arrest = coup de poing d'un flic -> arrestation au
// relevage ; sinon (percute par une voiture) il se releve simplement.
static void knockdownPlayer(bool arrest) {
  if (seqKind != SEQ_NONE || playerDown) return;
  playerDown = true; playerDownArrest = arrest;
  playerDownTimer = arrest ? COP_BUST_DELAY : getupFrames();
  gb.sound.tone(120, 90);
}

// La police sort le joueur de force de sa caisse : la voiture s'arrete, le perso
// est pose a cote (case libre) et le controle repasse a pied. Appele juste avant
// knockdownPlayer(true) pour enchainer sur l'arrestation.
static void dragPlayerFromCar() {
  // Pose le joueur a cote de SA portiere (gauche du sens de la caisse), pas
  // dessus. Forward = (cos a, sin a) -> gauche = (sin a, -cos a).
  float a = car.angle;
  int doorx = (int)(car.x + sinf(a) * DRIVER_DOOR_OFFSET);
  int doory = (int)(car.y - cosf(a) * DRIVER_DOOR_OFFSET);
  if (!isSolidAt(doorx >> 3, doory >> 3)) {       // portiere degagee
    playerX = doorx - PLAYER_W / 2; playerY = doory - PLAYER_H / 2;
  } else {                                        // bloquee (mur) : case libre proche
    int ox, oy;
    if (findFootSpot((int)car.x, (int)car.y, ox, oy)) { playerX = ox; playerY = oy; }
    else { playerX = (int)car.x - PLAYER_W / 2; playerY = (int)car.y - PLAYER_H / 2; }
  }
  car.vx = 0.0f; car.vy = 0.0f;             // la caisse cale sur place
  driving = false; carIsMission = false;
  playerFrame = 0; animTimer = 0;
}

// Avance l'etat "joueur au sol". A 0 : arrestation, ou on se releve (controle rendu).
// Relevage ANTICIPE : appuyer sur A se releve tout de suite (sauf arrestation par
// un flic, ou il faut subir le delai). L'auto-relevage reste un filet de securite.
static void updatePlayerDown() {
  if (!playerDownArrest && playerDownTimer > 1 && gb.buttons.pressed(BUTTON_A))
    playerDownTimer = 1;                       // se relevera des ce frame (decrement ci-dessous)
  if (playerDownTimer > 0 && --playerDownTimer == 0) {
    playerDown = false;
    if (playerDownArrest) bustedPlayer();
    else { playerHurtTimer = PLAYER_HURT_COOLDOWN; }   // breve invuln. au relevage
  }
}

// Le joueur a pied chevauche-t-il la boite d'une voiture en (cx,cy) ?
static bool playerOverlapsCar(float cx, float cy) {
  if (driving || playerDown || seqKind != SEQ_NONE) return false;
  if (playerJump > 0) return false;             // en l'air : on survole les voitures
  float dx = (playerX + PLAYER_W / 2) - cx, dy = (playerY + PLAYER_H / 2) - cy;
  return fabsf(dx) < COL_CP_HIT && fabsf(dy) < COL_CP_HIT;
}

// Voiture a l'arret (ou tres lente) : solide. On repousse le joueur hors de la
// boite par l'axe de moindre penetration. Aucun degat, pas de chute.
static void pushPlayerOffCar(float cx, float cy) {
  float dx = (playerX + PLAYER_W / 2) - cx, dy = (playerY + PLAYER_H / 2) - cy;
  float px = COL_CP_HIT - fabsf(dx), py = COL_CP_HIT - fabsf(dy);
  if (px <= 0.0f && py <= 0.0f) return;
  if (px < py) playerX += (dx < 0 ? -(int)(px + 0.5f) : (int)(px + 0.5f));
  else         playerY += (dy < 0 ? -(int)(py + 0.5f) : (int)(py + 0.5f));
}

// Voiture lancee qui renverse le joueur : -1 cœur, chute, et projection plus
// loin dans le sens du choc (recalee sur une case libre).
static void runOverPlayer(float cx, float cy) {
  hurtPlayer(1, false);                     // -1 cœur (voiture : fatal -> wasted)
  if (playerDown || playerHearts <= 0 || seqKind != SEQ_NONE) return;
  float dx = (playerX + PLAYER_W / 2) - cx, dy = (playerY + PLAYER_H / 2) - cy;
  float d = sqrtf(dx * dx + dy * dy);
  if (d < 0.5f) { dx = 0.0f; dy = 1.0f; d = 1.0f; }   // pile dessus : ejecte vers le bas
  int tcx = (int)((playerX + PLAYER_W / 2) + dx / d * CAR_EJECT_PUSH);
  int tcy = (int)((playerY + PLAYER_H / 2) + dy / d * CAR_EJECT_PUSH);
  int ox, oy;
  if (findFootSpot(tcx, tcy, ox, oy)) { playerX = ox; playerY = oy; }
  knockdownPlayer(false);
}

// Detruire une voiture (autre que la sienne) = crime majeur : compte comme 3
// pietons tues pour la jauge d'etoiles.
static void onCarWrecked() {
  for (int k = 0; k < 3; k++) wantedOnKill(wanted);
}

// Declenche une explosion en (wx,wy) : effet visuel + son + degats de zone (PNJ,
// joueur, vehicules avec chaine). NE pose PAS d'epave a son centre : seules les
// vraies voitures detruites (ici dans la chaine, ou par l'appelant) en laissent une.
static void explodeCarAt(int wx, int wy) {
  boomX = wx; boomY = wy; boomTimer = BOOM_FRAMES;
  gb.sound.tone(70, 200);
  // Degats de zone : PNJ debout tues dans le rayon letal ; affoles un peu au-dela.
  for (int i = 0; i < NUM_AI_PEDS; i++) {
    AiPed &p = aiPeds[i];
    if (!p.active || p.state == 1) continue;
    int dx = (int)p.x - wx, dy = (int)p.y - wy;
    int d2 = dx * dx + dy * dy;
    if (d2 <= BOOM_HURT_RADIUS * BOOM_HURT_RADIUS) knockDownPed(p);
    else if (d2 <= BOOM_PANIC_RADIUS * BOOM_PANIC_RADIUS) startPanic(p, wx, wy);
  }
  alarmCarsAround(wx, wy, BOOM_PANIC_RADIUS);   // voitures alentour : fuite
  // Joueur a pied dans le souffle -> projete a terre (-1 coeur + chute + ejection
  // loin du centre), meme tarif que les PNJ. Au volant : la mort est deja geree.
  if (!driving) {
    int dx = (playerX + PLAYER_W / 2) - wx, dy = (playerY + PLAYER_H / 2) - wy;
    if (dx * dx + dy * dy <= BOOM_HURT_RADIUS * BOOM_HURT_RADIUS) runOverPlayer(wx, wy);
  }
  // Degats de zone aux vehicules : decroissance lineaire ; au centre > PV (detruit).
  // Un vehicule tombe a 0 explose a son tour (CHAINE) -- desactive AVANT l'appel
  // recursif pour ne pas etre re-touche ; chaine bornee par NUM_AI_CARS.
  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (!c.active) continue;
    float ddx = c.x - wx, ddy = c.y - wy;
    float dist = sqrtf(ddx * ddx + ddy * ddy);
    if (dist >= BOOM_VEHICLE_RADIUS) continue;
    c.hp -= (int16_t)(BOOM_CENTER_DMG * (1.0f - dist / BOOM_VEHICLE_RADIUS));
    if (c.hp <= 0) {
      c.active = false;
      spawnWreck(c.x, c.y, AI_CAR_FRAME[c.dir], ddx, ddy);   // carcasse de CETTE voiture
      onCarWrecked();                                        // +3 (etoiles)
      explodeCarAt((int)c.x, (int)c.y);                      // chaine
    }
  }
  // Voiture du joueur dans le rayon (sans etre le centre source) : elle encaisse
  // aussi -> a 0 PV la meche s'allume via updateCarFuse (pas de cas special ici).
  if (!carGone) {
    float ddx = car.x - wx, ddy = car.y - wy;
    float dist = sqrtf(ddx * ddx + ddy * ddy);
    if (dist > 0.5f && dist < BOOM_VEHICLE_RADIUS)
      carHp -= (int16_t)(BOOM_CENTER_DMG * (1.0f - dist / BOOM_VEHICLE_RADIUS));
  }
}

// Tir d'arme a feu : engendre un ou plusieurs pixels-projectiles partant du
// joueur dans la direction du regard. La pompe crache une gerbe (3 pixels avec
// un leger ecart lateral) ; les autres armes, un seul. Portee = celle de l'arme.
static void fireBullets(int pcx, int pcy) {
  float fdx = AI_DX[playerDir], fdy = AI_DY[playerDir];
  float rdx = AI_DX[AI_RIGHT[playerDir]], rdy = AI_DY[AI_RIGHT[playerDir]];
  int pellets = (curWeapon == WEAPON_SHOTGUN) ? 3 : 1;
  uint8_t life = (uint8_t)(WEAPONS[curWeapon].reach / BULLET_SPEED + 1);
  int16_t carDmg = WEAPONS[curWeapon].area ? CAR_AREA_DMG : CAR_HIT_DMG;
  for (int k = 0; k < pellets; k++) {
    int slot = -1;
    for (int i = 0; i < NUM_BULLETS; i++) if (!bullets[i].active) { slot = i; break; }
    if (slot < 0) slot = k % NUM_BULLETS;
    float spread = (pellets > 1) ? (float)(k - 1) * 0.35f : 0.0f;   // -1,0,+1 -> ecart
    Bullet &b = bullets[slot];
    b.x = (float)pcx; b.y = (float)pcy;
    b.vx = (fdx + rdx * spread) * BULLET_SPEED;
    b.vy = (fdy + rdy * spread) * BULLET_SPEED;
    b.life = life; b.hostile = false; b.carDmg = carDmg; b.active = true;
  }
}

// Tir d'un policier : une balle HOSTILE partant de (sx,sy) vers la position
// courante du joueur (tx,ty). Elle voyage -> le joueur peut se decaler pour
// l'esquiver (cf. updateBullets). Aucune voiture endommagee (carDmg = 0).
static void fireCopBullet(float sx, float sy, int tx, int ty) {
  float dx = (float)tx - sx, dy = (float)ty - sy;
  float n = sqrtf(dx * dx + dy * dy);
  if (n < 0.01f) return;
  int slot = -1;
  for (int i = 0; i < NUM_BULLETS; i++) if (!bullets[i].active) { slot = i; break; }
  if (slot < 0) return;                          // pool plein : on saute ce tir
  Bullet &b = bullets[slot];
  b.x = sx; b.y = sy;
  b.vx = dx / n * BULLET_SPEED; b.vy = dy / n * BULLET_SPEED;
  b.life = (uint8_t)(COP_SHOOT_RANGE / BULLET_SPEED + 4);
  b.hostile = true; b.carDmg = CAR_HIT_DMG; b.active = true;   // une balle est une balle : abime aussi les caisses
}

// Cooldown de tir des flics selon le niveau de recherche : a 5 etoiles c'est le
// plus nerveux (COP_SHOOT_PERIOD) ; chaque etoile en moins ajoute COP_SHOOT_STEP
// frames -> a 2 etoiles ils tirent beaucoup plus posement.
static uint16_t copShootPeriod() {
  uint8_t lvl = wanted.level < WANTED_MAX ? wanted.level : WANTED_MAX;
  return COP_SHOOT_PERIOD + (uint16_t)(WANTED_MAX - lvl) * COP_SHOOT_STEP;
}

// Detruit une voiture IA : carcasse fumante (saut dans le sens du choc),
// explosion (souffle + chaine). credit = c'est le joueur qui l'a detruite
// (alors +3 etoiles) ; un tir de flic ne credite personne.
static void wreckAiCar(AiCar &c, float hopx, float hopy, bool credit) {
  spawnWreck(c.x, c.y, AI_CAR_FRAME[c.dir], hopx, hopy);
  if (credit) onCarWrecked();
  c.active = false;
  explodeCarAt((int)c.x, (int)c.y);
}

// Avance les projectiles ET applique les collisions. Une balle est une balle :
// elle s'arrete sur le PREMIER obstacle rencontre — mur, vehicule (le mien
// comme ceux du trafic) ou cible vivante — et y fait ses degats au besoin. Ni
// le joueur ni les flics ne tirent a travers les murs ou les caisses.
//  - mur (tuile solide)      : la balle s'eteint, aucun degat ;
//  - voiture (joueur ou IA)  : encaisse carDmg ; conducteur descend en panique ;
//                              detruite a 0 PV (etoiles seulement si tir joueur) ;
//  - balle joueur + pieton   : 1 touche = mort ;
//  - balle flic  + joueur a pied : 1 cœur ;
//  - balle flic  + joueur en caisse : carDmg sur la tole (pas le joueur).
static void updateBullets() {
  int pcx = driving ? (int)car.x : playerX + PLAYER_W / 2;
  int pcy = driving ? (int)car.y : playerY + PLAYER_H / 2;
  float hitR = driving ? 6.0f : (float)(PLAYER_W / 2 + 1);   // boite cible du joueur
  for (int i = 0; i < NUM_BULLETS; i++) {
    Bullet &b = bullets[i];
    if (!b.active) continue;
    b.x += b.vx; b.y += b.vy;
    if (b.life == 0 || --b.life == 0) { b.active = false; continue; }
    // 1) Mur / batiment : la balle s'ecrase, point.
    if (isSolidAt((int)b.x >> 3, (int)b.y >> 3)) { b.active = false; continue; }
    // 2) Cible vivante : joueur (balle flic) ou pieton (balle joueur).
    if (b.hostile) {
      if (fabsf(b.x - pcx) < hitR && fabsf(b.y - pcy) < hitR) {
        if (driving) carHp -= b.carDmg;     // en caisse : la tole encaisse (0 PV -> meche via updateCarFuse)
        else hurtPlayer(1, false);          // a pied : balle fatale -> mort (pas arrestation)
        b.active = false; continue;
      }
    } else {
      bool consumed = false;
      for (int q = 0; q < NUM_AI_PEDS; q++) {
        AiPed &p = aiPeds[q];
        if (!p.active || p.state == 1 || p.state == 3) continue;   // pas les corps au sol
        if (fabsf(b.x - p.x) < 4.0f && fabsf(b.y - p.y) < 4.0f) {
          hitPed(p, true); b.active = false; consumed = true; break;
        }
      }
      if (consumed) continue;
      for (int q = 0; q < MAX_ENEMIES; q++) {                      // ennemis scenarises (balle = mortelle)
        Enemy &e = enemies[q];
        if (!e.active || e.phase == EN_DOWN) continue;
        if (fabsf(b.x - e.x) < 4.0f && fabsf(b.y - e.y) < 4.0f) {
          hitEnemy(e, true); b.active = false; consumed = true; break;
        }
      }
      if (consumed) continue;
    }
    // 3) Ma propre caisse (garee : a pied elle est un obstacle plein) : tirable.
    if (!driving && !carGone &&
        fabsf(b.x - car.x) < 5.0f && fabsf(b.y - car.y) < 5.0f) {
      carHp -= b.carDmg;                          // 0 PV -> meche via updateCarFuse
      b.active = false; continue;
    }
    // 4) Voitures du trafic : encaissent, le conducteur descend, explosent a 0 PV.
    for (int q = 0; q < NUM_AI_CARS; q++) {
      AiCar &c = aiCars[q];
      if (!c.active) continue;
      if (fabsf(b.x - c.x) < 5.0f && fabsf(b.y - c.y) < 5.0f) {
        c.hp -= b.carDmg;
        if (c.hp <= 0) wreckAiCar(c, b.vx, b.vy, !b.hostile);   // etoiles si tir joueur
        else if (c.driver) { aiEjectDriver((int)c.x, (int)c.y, false, false, c.dir); c.driver = false; }   // mitraillee : le conducteur detale
        b.active = false; break;
      }
    }
  }
}

// Dessine les projectiles : un pixel vif + une tracee attenuee derriere.
static void drawBullets(int camX, int camY) {
  for (int i = 0; i < NUM_BULLETS; i++) {
    Bullet &b = bullets[i];
    if (!b.active) continue;
    uint16_t head = b.hostile ? 0xF800 : 0xFFE0;   // flic : rouge vif ; joueur : jaune
    uint16_t tail = b.hostile ? 0x8000 : 0xC600;
    int x = (int)b.x - camX, y = (int)b.y - camY;
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = head;
    int tx = (int)(b.x - b.vx * 0.5f) - camX, ty = (int)(b.y - b.vy * 0.5f) - camY;
    if (tx >= 0 && tx < SCREEN_W && ty >= 0 && ty < SCREEN_H) fb[ty * SCREEN_W + tx] = tail;
  }
}

// Lance un projectile explosif dans la direction visee (centre joueur pcx,pcy).
// kind 0 = roquette (tout droit), 1 = grenade (cloche + meche).
static void spawnProjectile(uint8_t kind, int pcx, int pcy) {
  int slot = -1;
  for (int i = 0; i < NUM_PROJ; i++) if (!projs[i].active) { slot = i; break; }
  if (slot < 0) slot = 0;                       // pool plein : recycle le slot 0
  Projectile &p = projs[slot];
  float fdx = AI_DX[playerDir], fdy = AI_DY[playerDir];
  p.x = (float)pcx; p.y = (float)pcy; p.kind = kind; p.z = 0; p.active = true;
  if (kind == 0) {                              // roquette
    p.vx = fdx * ROCKET_SPEED; p.vy = fdy * ROCKET_SPEED;
    p.flight = (uint16_t)(ROCKET_RANGE / ROCKET_SPEED); p.fuse = 0;
  } else {                                      // grenade : vitesse pour couvrir THROW_DIST en FLIGHT frames
    float sp = (float)GRENADE_THROW_DIST / GRENADE_FLIGHT;
    p.vx = fdx * sp; p.vy = fdy * sp;
    p.flight = GRENADE_FLIGHT; p.fuse = GRENADE_FUSE;
  }
}

// Avance les projectiles ; detone a l'impact (roquette) ou meche ecoulee (grenade).
static void updateProjectiles() {
  for (int i = 0; i < NUM_PROJ; i++) {
    Projectile &p = projs[i];
    if (!p.active) continue;
    if (p.kind == 0) {                          // ROQUETTE : tout droit, explose a l'impact
      float nx = p.x + p.vx, ny = p.y + p.vy;
      bool hit = isSolidAt((int)nx >> 3, (int)ny >> 3);
      for (int c = 0; c < NUM_AI_CARS && !hit; c++)
        if (aiCars[c].active && fabsf(aiCars[c].x - nx) < 4.0f && fabsf(aiCars[c].y - ny) < 4.0f) hit = true;
      for (int q = 0; q < NUM_AI_PEDS && !hit; q++)
        if (aiPeds[q].active && aiPeds[q].state != 1 &&
            fabsf(aiPeds[q].x - nx) < 4.0f && fabsf(aiPeds[q].y - ny) < 4.0f) hit = true;
      p.x = nx; p.y = ny;
      if (p.flight == 0 || --p.flight == 0) hit = true;   // portee epuisee -> explose
      if (hit) { spawnSmoke(p.x, p.y); explodeCarAt((int)p.x, (int)p.y); p.active = false; }
    } else {                                    // GRENADE : cloche puis meche
      if (p.flight > 0) {
        float nx = p.x + p.vx, ny = p.y + p.vy;
        if (isSolidAt((int)nx >> 3, (int)ny >> 3)) { p.vx = 0.0f; p.vy = 0.0f; p.flight = 1; }  // bute -> atterrit
        else { p.x = nx; p.y = ny; }
        p.flight--;
        if (p.flight == 0) { p.vx = 0.0f; p.vy = 0.0f; p.z = 0; }   // au sol
        else {
          float t = (float)(GRENADE_FLIGHT - p.flight) / (float)GRENADE_FLIGHT;
          p.z = (uint8_t)(GRENADE_MAX_Z * 4.0f * t * (1.0f - t));   // parabole (cloche)
        }
      }
      // Les PNJ qui voient la grenade paniquent AVANT l'explosion.
      for (int q = 0; q < NUM_AI_PEDS; q++) {
        AiPed &pd = aiPeds[q];
        if (!pd.active || pd.state == 1 || pd.isCop) continue;
        float dx = pd.x - p.x, dy = pd.y - p.y;
        if (dx * dx + dy * dy <= (float)(GRENADE_FEAR_RADIUS * GRENADE_FEAR_RADIUS))
          startPanic(pd, (int)p.x, (int)p.y);
      }
      if (p.fuse == 0 || --p.fuse == 0) { spawnSmoke(p.x, p.y); explodeCarAt((int)p.x, (int)p.y); p.active = false; }
    }
  }
}

// Dessine les projectiles : roquette = pixel orange + tracee ; grenade = ombre au
// sol + pastille verte en cloche, clignotante une fois posee (meche).
static void drawProjectiles(int camX, int camY) {
  for (int i = 0; i < NUM_PROJ; i++) {
    Projectile &p = projs[i];
    if (!p.active) continue;
    int x = (int)p.x - camX, y = (int)p.y - camY;
    if (p.kind == 0) {                          // roquette
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0xFD20;
      int tx = (int)(p.x - p.vx) - camX, ty = (int)(p.y - p.vy) - camY;
      if (tx >= 0 && tx < SCREEN_W && ty >= 0 && ty < SCREEN_H) fb[ty * SCREEN_W + tx] = 0x8200;
    } else {                                    // grenade
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0x2104;  // ombre
      int gy = y - (int)p.z;
      uint16_t col = (p.flight == 0 && (missionAnim & 4)) ? 0xFFFF : 0x3DA6;  // clignote quand posee
      if (x >= 0 && x < SCREEN_W && gy >= 0 && gy < SCREEN_H) fb[gy * SCREEN_W + x] = col;
    }
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
  else if (WEAPONS[curWeapon].explosive) gb.sound.tone(200, 80);  // souffle du lanceur (le projectile vole)
  else { gb.sound.tone(110, 50); fireBullets(pcx, pcy); }         // bang + pixels de tir
  bool firearm = (curWeapon != WEAPON_FIST);
  if (wd.explosive) {
    // Arme explosive : lance un PROJECTILE dans la direction visee. La roquette
    // file tout droit et explose a l'impact ; la grenade part en cloche et
    // explose au bout de ~3 s. L'explosion (boom + degats radiaux + chaine) a
    // lieu a la detonation du projectile (cf. updateProjectiles), pas ici.
    spawnProjectile(curWeapon == WEAPON_GRENADE ? 1 : 0, pcx, pcy);
  } else if (curWeapon == WEAPON_FIST) {
    // Poing : frappe instantanee du pieton le plus proche devant (cone), 3 coups.
    // Les armes A BALLES ne frappent plus par cone : leurs degats (pietons ET
    // voitures) viennent des projectiles, geres par collision dans updateBullets.
    float px[NUM_AI_PEDS], py[NUM_AI_PEDS];
    bool act[NUM_AI_PEDS];
    for (int i = 0; i < NUM_AI_PEDS; i++) {
      px[i] = aiPeds[i].x; py[i] = aiPeds[i].y;
      act[i] = aiPeds[i].active && aiPeds[i].state != 1 && aiPeds[i].state != 3;   // debout ou panique (pas au sol)
    }
    int hit = combatConeTarget(px, py, act, NUM_AI_PEDS, pcx, pcy, playerDir, wd.reach, wd.side);
    if (hit >= 0) hitPed(aiPeds[hit], false);
    // Ennemis scenarises : frappables au poing comme un pieton (cone), 3 coups.
    for (int i = 0; i < MAX_ENEMIES; i++) {
      Enemy &e = enemies[i];
      if (e.active && e.phase != EN_DOWN &&
          combatInCone(e.x, e.y, pcx, pcy, playerDir, wd.reach, wd.side)) {
        hitEnemy(e, false); break;
      }
    }
  }
  // Le bruit du tir affole les civils alentour (toutes armes a feu : ils detalent du tireur).
  if (firearm) {
    for (int i = 0; i < NUM_AI_PEDS; i++) {
      AiPed &p = aiPeds[i];
      if (!p.active || p.state != 0 || p.isCop) continue;
      int dx = (int)p.x - pcx, dy = (int)p.y - pcy;
      if (dx * dx + dy * dy <= GUNSHOT_PANIC_RANGE * GUNSHOT_PANIC_RANGE) startPanic(p, pcx, pcy);
    }
    alarmCarsAround(pcx, pcy, GUNSHOT_PANIC_RANGE);   // voitures qui entendent le tir : fuite
  }
  // La cible de mission est frappable comme un pieton (toujours dans le cone).
  if (missionRun.active && target.active &&
      combatInCone(target.x, target.y, pcx, pcy, playerDir, wd.reach, wd.side)) {
    if (curObjs[missionRun.step].type == OBJ_SUBDUE) {
      objSubdue++;                          // elle cede, elle ne meurt pas
      gb.sound.tone(150, 50);
    } else if (targetHp > 1) {
      targetHp--;                           // BOSS : encaisse le coup, vacille
      gb.sound.tone(150, 50);
    } else {
      killTarget(pcx, pcy);
    }
  }

  // Decompte des munitions ; arme videe -> retiree, retour au poing.
  if (!weaponInfinite(curWeapon) && --weaponAmmo[curWeapon] <= 0) {
    weaponOwned[curWeapon] = false;
    curWeapon = WEAPON_FIST;
  }
}

// Conduite de POURSUITE d'une voiture de police : avance vers son point-cible ;
// une fois atteint, choisit la sortie ROUTIERE (aiIsDrivable) qui rapproche le
// plus du joueur (fcx,fcy). Calque routier de missionChaseStep (qui, lui, vise
// les trottoirs pour les flics a pied).
static void policeCarStep(AiCar &c, int fcx, int fcy) {
  float dx = (float)c.tgtx - c.x, dy = (float)c.tgty - c.y;
  float d2 = dx * dx + dy * dy;
  if (d2 <= AI_CAR_SPEED * AI_CAR_SPEED) {
    c.x = (float)c.tgtx; c.y = (float)c.tgty;
    int tx = (int)c.x >> 3, ty = (int)c.y >> 3;
    long best = -1; uint8_t bestDir = 255; int ties = 0;
    for (uint8_t d = 0; d < 4; d++) {
      int nx = tx + AI_DX[d], ny = ty + AI_DY[d];
      if (!aiIsDrivable(cityMap, CITY_W, CITY_H, nx, ny)) continue;
      int ccx = nx * TILE_W + TILE_W / 2, ccy = ny * TILE_H + TILE_H / 2;
      long dd = (long)(ccx - fcx) * (ccx - fcx) + (long)(ccy - fcy) * (ccy - fcy);
      if (best < 0 || dd < best) { best = dd; bestDir = d; ties = 1; }
      else if (dd == best) { ties++; if (aiRngNext(aiRng) % (uint32_t)ties == 0) bestDir = d; }
    }
    if (bestDir != 255) {
      c.dir = bestDir;
      int ntx = tx + AI_DX[c.dir], nty = ty + AI_DY[c.dir];
      aiLanePoint(ntx, nty, c.dir, c.tgtx, c.tgty);
    }
  } else {
    float inv = AI_CAR_SPEED / sqrtf(d2);
    c.x += dx * inv; c.y += dy * inv;
  }
}

// Recul : meme logique que policeCarStep mais on choisit la case roulante voisine
// qui MAXIMISE la distance au joueur -> la caisse s'eloigne (anti-poursuite).
// Sert apres un choc (elle se degage au lieu de forcer) et en panique.
static void aiCarStepAway(AiCar &c, int fcx, int fcy) {
  float dx = (float)c.tgtx - c.x, dy = (float)c.tgty - c.y;
  if (dx * dx + dy * dy <= AI_CAR_SPEED * AI_CAR_SPEED) {
    c.x = (float)c.tgtx; c.y = (float)c.tgty;
    int tx = (int)c.x >> 3, ty = (int)c.y >> 3;
    long best = -1; uint8_t bestDir = 255; int ties = 0;
    for (uint8_t d = 0; d < 4; d++) {
      int nx = tx + AI_DX[d], ny = ty + AI_DY[d];
      if (!aiIsDrivable(cityMap, CITY_W, CITY_H, nx, ny)) continue;
      int ccx = nx * TILE_W + TILE_W / 2, ccy = ny * TILE_H + TILE_H / 2;
      long dd = (long)(ccx - fcx) * (ccx - fcx) + (long)(ccy - fcy) * (ccy - fcy);
      if (best < 0 || dd > best) { best = dd; bestDir = d; ties = 1; }   // MAX distance
      else if (dd == best) { ties++; if (aiRngNext(aiRng) % (uint32_t)ties == 0) bestDir = d; }
    }
    if (bestDir != 255) {
      c.dir = bestDir;
      int ntx = tx + AI_DX[c.dir], nty = ty + AI_DY[c.dir];
      aiLanePoint(ntx, nty, c.dir, c.tgtx, c.tgty);
    }
  } else {
    float inv = AI_CAR_SPEED / sqrtf(dx * dx + dy * dy);
    c.x += dx * inv; c.y += dy * inv;
  }
}

// Repousse une voiture IA de (ox,oy) px SI la case d'arrivee reste roulante
// (on ne la pousse pas dans un immeuble). Renvoie true si elle a bouge.
static bool nudgeAiCar(AiCar &c, float ox, float oy) {
  int tx = ((int)(c.x + ox)) >> 3, ty = ((int)(c.y + oy)) >> 3;
  if (!aiIsDrivable(cityMap, CITY_W, CITY_H, tx, ty)) return false;
  c.x += ox; c.y += oy;
  return true;
}

// Un coup de klaxon "tuut tuut" : 1er bip tout de suite, 2e bip CAR_HONK_GAP
// frames plus tard (declenche dans aiUpdate). Respecte le cooldown global.
static void carHonk() {
  if (carHonkCooldown > 0) return;
  carHonkCooldown = CAR_HONK_COOLDOWN;
  gb.sound.tone(CAR_HONK_FREQ, CAR_HONK_BEEP_MS);   // premier "tuut"
  carHonkBeep2 = CAR_HONK_GAP;                       // le second suivra
}

// Met a jour le trafic autour du point de vue (fcx,fcy = centre suivi, px monde).
// Recyclage des entites trop loin (culling), pas IA, collisions avec le joueur.
static void aiUpdate(int fcx, int fcy) {
  const int rec2 = RECYCLE_DIST * RECYCLE_DIST;
  if (carHonkCooldown > 0) carHonkCooldown--;
  if (carHonkBeep2 > 0 && --carHonkBeep2 == 0)
    gb.sound.tone(CAR_HONK_FREQ, CAR_HONK_BEEP_MS);  // second "tuut"
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
    bool pursue = c.isPolice && c.driver && wanted.level > 0;        // police lancee a mes trousses
    bool ram    = c.isPolice && c.driver && wanted.level >= WANTED_MAX;  // 5 etoiles : elle me fonce dessus
    if (c.fleeTimer > 0 && --c.fleeTimer == 0) c.fleeing = false;    // fin de fuite -> trafic calme
    bool ignorePeople = c.fleeing || ram;   // belier/fuite : ne freine plus, ecrase au passage
    // Arret si le joueur est juste devant (dans la voie). Belier/fuite : on NE
    // s'arrete PAS devant les gens, on les ecrase.
    float relx = obx - c.x, rely = oby - c.y;
    float fwd = relx * AI_DX[c.dir] + rely * AI_DY[c.dir];
    float lat = relx * AI_DX[AI_RIGHT[c.dir]] + rely * AI_DY[AI_RIGHT[c.dir]];
    // Le joueur barre la route : la caisse ne freine PAS tout de suite. Elle
    // accumule un temps de reaction et continue d'avancer le temps de BRAKE_REACT_
    // FRAMES -> laisse une vraie chance de me faucher (au lieu de l'arret instantane).
    bool playerAhead = (!ignorePeople && fwd > 0.0f && fwd < STOP_AHEAD && fabsf(lat) < STOP_SIDE);
    bool blocked = false;
    if (playerAhead) {
      if (c.brakeReact == 0) {                  // onset : le joueur vient de barrer la route
        if (driving) carHonk();                 // ta caisse devant la sienne -> klaxon
        else if (aiRngNext(aiRng) % 20 == 0) carHonk();  // a pied (je traverse devant) : ~5%
      }
      if (c.brakeReact < BRAKE_REACT_FRAMES) c.brakeReact++;   // reflexe en cours : roule encore
      else blocked = true;                                     // a fini par freiner
    } else if (c.brakeReact > 0) {
      c.brakeReact--;                                          // plus de menace : detend le pied
    }
    // Une epave en travers de la voie arrete TOUJOURS le trafic (obstacle solide).
    for (int wI = 0; wI < NUM_WRECKS && !blocked; wI++) {
      if (!wrecks[wI].active) continue;
      float wrx = wrecks[wI].x - c.x, wry = wrecks[wI].y - c.y;
      float wf = wrx * AI_DX[c.dir] + wry * AI_DY[c.dir];
      float wl = wrx * AI_DX[AI_RIGHT[c.dir]] + wry * AI_DY[AI_RIGHT[c.dir]];
      if (wf > 0.0f && wf < STOP_AHEAD && fabsf(wl) < STOP_SIDE) blocked = true;
    }
    // La caisse du joueur GAREE (hors conduite) est un obstacle solide comme une
    // autre voiture a l'arret : elle arrete aussi le trafic dans sa voie. (Pas de
    // regle "ma caisse" : meme traitement que toute caisse immobile.)
    if (!blocked && !driving && !carGone) {
      float crx = car.x - c.x, cry = car.y - c.y;
      float cf = crx * AI_DX[c.dir] + cry * AI_DY[c.dir];
      float cl = crx * AI_DX[AI_RIGHT[c.dir]] + cry * AI_DY[AI_RIGHT[c.dir]];
      if (cf > 0.0f && cf < STOP_AHEAD && fabsf(cl) < STOP_SIDE) blocked = true;
    }
    // Un pieton debout dans la voie arrete une voiture NORMALE (pas en fuite).
    for (int q = 0; q < NUM_AI_PEDS && !blocked && !ignorePeople; q++) {
      AiPed &pd = aiPeds[q];
      if (!pd.active || (pd.state != 0 && pd.state != 2)) continue;
      float prx = pd.x - c.x, pry = pd.y - c.y;
      float pf = prx * AI_DX[c.dir] + pry * AI_DY[c.dir];
      float pl = prx * AI_DX[AI_RIGHT[c.dir]] + pry * AI_DY[AI_RIGHT[c.dir]];
      if (pf > 0.0f && pf < STOP_AHEAD && fabsf(pl) < STOP_SIDE) blocked = true;
    }
    float cx0 = c.x, cy0 = c.y;             // position avant le pas (-> vitesse reelle)
    if (c.driver) {                         // sans conducteur (descendu) : reste sur place
      if (c.backoff > 0) {                  // recul : s'eloigne du joueur (prioritaire sur "bloque" et la poursuite)
        c.backoff--;
        aiCarStepAway(c, fcx, fcy);
      } else if (!blocked) {
        if (pursue) policeCarStep(c, fcx, fcy);   // poursuite routiere du joueur
        else aiStep(cityMap, CITY_W, CITY_H, c.x, c.y, c.dir, c.tgtx, c.tgty,
                    c.fleeing ? CAR_FLEE_SPEED : AI_CAR_SPEED, aiIsDrivable, aiRng);
      }
    }
    float moved2 = (c.x - cx0) * (c.x - cx0) + (c.y - cy0) * (c.y - cy0);
    bool fast = moved2 >= CAR_RUNOVER_SPEED2;   // assez rapide pour ecraser
    // Collisions avec les gens. Roule assez vite -> ECRASE ; a l'arret/tres lente
    // -> SOLIDE (repousse, sans degat). Une voiture normale qui percute le joueur
    // s'immobilise et son conducteur descend ; en fuite/belier elle ecrase et file.
    if (c.driver && fast && ignorePeople) {  // fuite/belier : fauche les PNJ au passage
      for (int q = 0; q < NUM_AI_PEDS; q++) {
        AiPed &pd = aiPeds[q];
        if (!pd.active || (pd.state != 0 && pd.state != 2)) continue;
        if (fabsf(pd.x - c.x) < COL_CP && fabsf(pd.y - c.y) < COL_CP) deadPed(pd);
      }
    }
    if (playerOverlapsCar(c.x, c.y)) {       // une voiture immobile (meme abandonnee) est solide
      if (!fast) {                                    // contact lent : solide, mais ne FORCE pas
        pushPlayerOffCar(c.x, c.y);
        if (c.driver) startBackoff(c);                // elle recule au lieu de me coincer
      }
      else if (ignorePeople) runOverPlayer(c.x, c.y); // fuite/belier : ecrase et continue
      else {                                          // accident : stop + conducteur sort + ejection
        aiEjectDriver((int)c.x, (int)c.y, false, false, c.dir);
        c.driver = false;
        runOverPlayer(c.x, c.y);
      }
    }
    // Police a portee du joueur a pied (hors belier) : un flic descend pour
    // m'arreter / me tirer dessus ; la voiture s'immobilise (conducteur parti).
    if (pursue && !ram && !driving) {
      float ex = (playerX + PLAYER_W / 2) - c.x, ey = (playerY + PLAYER_H / 2) - c.y;
      if (ex * ex + ey * ey < (float)(POLICE_EJECT_DIST * POLICE_EJECT_DIST)) {
        aiEjectDriver((int)c.x, (int)c.y, false, true, c.dir);   // policier debout
        c.driver = false;
      }
    }
    // Solide vis-a-vis de la voiture joueur : on repousse le joueur, et un
    // accident a vitesse use la caisse (PV). On retient le sens du choc pour le
    // saut a l'explosion. Trop d'accidents -> elle prend feu (cf. updateCarFuse).
    if (driving) {
      float dx = car.x - c.x, dy = car.y - c.y;
      if (fabsf(dx) < COL_CC && fabsf(dy) < COL_CC) {
        float px = COL_CC - fabsf(dx), py = COL_CC - fabsf(dy);
        // On separe sur l'axe de moindre penetration en repoussant d'ABORD la
        // caisse IA (elle ne doit pas me bulldozer dans un mur). Si elle est
        // bloquee (immeuble derriere elle), alors c'est moi qui me degage.
        if (px < py) {
          float s = (dx < 0 ? -1.0f : 1.0f);          // s = de quel cote je suis vs le PNJ
          if (!nudgeAiCar(c, -s * px, 0.0f)) car.x += s * px;
          car.vx *= -0.3f; carImpactX = s; carImpactY = 0.0f;
        } else {
          float s = (dy < 0 ? -1.0f : 1.0f);
          if (!nudgeAiCar(c, 0.0f, -s * py)) car.y += s * py;
          car.vy *= -0.3f; carImpactX = 0.0f; carImpactY = s;
        }
        if (spd2 > RUNOVER_SPEED2 && carCrashTimer == 0) {
          carHp -= CAR_CRASH_DMG; carCrashTimer = CAR_CRASH_COOLDOWN;
        }
        if (c.driver) { startCarFlee(c); startBackoff(c); }   // percutee : demi-tour, elle s'eloigne (police comprise)
      }
    }
  }

  // Caisse du joueur GAREE solide vis-a-vis du joueur A PIED : on le repousse, on
  // ne passe pas a travers. Meme traitement que pour une caisse IA immobile
  // (playerOverlapsCar renvoie false si on conduit). Plus de voiture fantome.
  if (!driving && !carGone && playerOverlapsCar(car.x, car.y))
    pushPlayerOffCar(car.x, car.y);

  // Collision voiture joueur <-> epaves : solide (repousse + use la caisse).
  if (driving) {
    for (int i = 0; i < NUM_WRECKS; i++) {
      if (!wrecks[i].active) continue;
      float dx = car.x - wrecks[i].x, dy = car.y - wrecks[i].y;
      if (fabsf(dx) < COL_CC && fabsf(dy) < COL_CC) {
        float px = COL_CC - fabsf(dx), py = COL_CC - fabsf(dy);
        if (px < py) { car.x += (dx < 0 ? -px : px); car.vx *= -0.3f; carImpactX = (dx < 0 ? -1.0f : 1.0f); carImpactY = 0.0f; }
        else { car.y += (dy < 0 ? -py : py); car.vy *= -0.3f; carImpactX = 0.0f; carImpactY = (dy < 0 ? -1.0f : 1.0f); }
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
    if (p.state == 1) continue;               // mort/renverse : immobile
    if (p.state == 3) {                        // AU SOL RECUPERABLE : se releve puis fuit
      if (p.downTimer == 0 || --p.downTimer == 0)
        startPanic(p, playerX + PLAYER_W / 2, playerY + PLAYER_H / 2);   // debout -> panique
      continue;                                // immobile le temps de se relever
    }
    if (p.state == 2) {                        // PANIQUE : fuite affolee (hors trottoirs)
      if (p.panicTimer > 0) p.panicTimer--;
      bool calm = (p.panicTimer == 0);
      int ptx = (int)p.x >> 3, pty = (int)p.y >> 3;
      bool onPave = aiInBounds(CITY_W, CITY_H, ptx, pty) &&
                    cityMap[pty * CITY_W + ptx] == TILE_PAVEMENT;
      if (calm && onPave) {                    // REGLE : fin de panique seulement sur trottoir
        aiPlace(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
                ptx, pty, aiIsWalkable, aiRng);   // recale la ronde sur SA tuile (pas de TP)
        p.state = 0; p.frame = 0; p.animTimer = 0;
        continue;
      }
      if (calm) {
        // Compteur ecoule mais pas encore sur un trottoir : COURT vers le plus
        // proche (aiPanicStep avec une "source" opposee au but -> fuite = approche).
        int ox, oy;
        if (findSidewalkSpot((int)p.x, (int)p.y, ox, oy))
          aiPanicStep(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
                      AI_PED_PANIC_SPEED, 2.0f * p.x - ox, 2.0f * p.y - oy);
        else { p.active = false; continue; }   // aucun trottoir a portee (rare) : recyclage
      } else {                                 // encore affole : FUIT la menace
        aiPanicStep(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
                    AI_PED_PANIC_SPEED, (float)p.panicX, (float)p.panicY);
      }
      if (++p.animTimer >= AI_PED_ANIM) { p.animTimer = 0; p.frame ^= 1; }
      if (driving && spd2 > RUNOVER_SPEED2 &&        // toujours renversable
          fabsf(car.x - p.x) < COL_CP && fabsf(car.y - p.y) < COL_CP)
        knockDownPed(p);
      continue;
    }
    if (p.isCop)                              // flic : fonce sur le joueur
      missionChaseStep(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
                       AI_PED_SPEED + 0.15f, fcx, fcy, aiRng);
    else                                      // civil : errance trottoirs
      aiStep(cityMap, CITY_W, CITY_H, p.x, p.y, p.dir, p.tgtx, p.tgty,
             AI_PED_SPEED, aiIsWalkable, aiRng);
    if (++p.animTimer >= AI_PED_ANIM) { p.animTimer = 0; p.frame ^= 1; }
    // Caisse qui monte sur le trottoir vers moi : le pieton AFFOLE et detale
    // (panique, comme sous un tir). S'il ne s'enfuit pas assez vite, il se fait
    // faucher juste apres. Ne vaut pas pour les flics (jamais en panique).
    if (!p.isCop && carOnSidewalk() && spd2 > RUNOVER_SPEED2) {
      int rx = (int)p.x - (int)car.x, ry = (int)p.y - (int)car.y;
      float fwd = rx * car.vx + ry * car.vy;        // >0 : pieton devant la caisse (sens de marche)
      if (fwd > 0.0f && rx * rx + ry * ry < SIDEWALK_SCARE_RANGE * SIDEWALK_SCARE_RANGE) {
        // Fuite a ANGLE DROIT de mon angle d'arrivee : le pieton deboite sur le
        // cote (sort de ma trajectoire) au lieu de courir devant la caisse. La
        // panique fuit le point source -> on le place dans l'axe lateral oppose.
        float perpx = -car.vy, perpy = car.vx;      // perpendiculaire a ma vitesse
        float side = (rx * perpx + ry * perpy >= 0.0f) ? 1.0f : -1.0f;   // son cote
        int srcx = (int)(p.x - side * perpx);       // source = oppose a la direction de fuite
        int srcy = (int)(p.y - side * perpy);
        startPanic(p, srcx, srcy);
      }
    }
    // Renversement par la voiture joueur lancee (vaut aussi pour les flics).
    if (driving && spd2 > RUNOVER_SPEED2 &&
        fabsf(car.x - p.x) < COL_CP && fabsf(car.y - p.y) < COL_CP) {
      knockDownPed(p);                        // ecrasement : KO + butin eventuel
      continue;
    }
    // Police : plus d'arrestation directe au contact. Le flic m'EXTRAIT de la
    // caisse seulement si je suis quasi a l'arret, puis me MATRAQUE a pied (perte
    // de vie progressive, je reste libre de fuir). Tomber a 0 sous la matraque =>
    // arrestation (gere dans hurtPlayer) ; sinon (balle/voiture) => mort.
    if (p.isCop) {
      float ddx = (float)fcx - p.x, ddy = (float)fcy - p.y;
      float d2 = ddx * ddx + ddy * ddy;
      // Extraction de la caisse : uniquement si je roule tres lentement (sinon je
      // file). Au-dela de COP_DRAG_MAX_SPEED, le flic ne peut pas me sortir.
      int dragDist = COP_ARREST_DIST + 5;     // caisse = plus grosse cible
      if (driving && fabsf(carForwardSpeed(car)) < COP_DRAG_MAX_SPEED
          && d2 < (float)(dragDist * dragDist)) {
        dragPlayerFromCar();                  // me pose a pied ; pas d'arrestation immediate
      }
      // Matraquage au contact (a pied) : -1 cœur par coup, cadence COP_MELEE_PERIOD.
      if (p.meleeTimer > 0) p.meleeTimer--;
      if (!driving && !playerDown && d2 < (float)(COP_MELEE_DIST * COP_MELEE_DIST)) {
        if (p.meleeTimer == 0) {
          p.meleeTimer = COP_MELEE_PERIOD;
          copHitTimer = COP_HIT_FX_FRAMES;    // eclat jaune sur le joueur (feedback)
          gb.sound.tone(90, 70);
          hurtPlayer(COP_MELEE_DMG, true);    // fatal sous la matraque -> arrestation
        }
      }
      // Tir a distance (>= 2 etoiles) : balle visee, esquivable. Mortelle (pas
      // d'arrestation) si elle acheve le joueur.
      if (wanted.level >= 2) {
        if (p.shootTimer > 0) p.shootTimer--;
        else if (d2 < (float)(COP_SHOOT_RANGE * COP_SHOOT_RANGE)) {
          p.shootTimer = copShootPeriod();
          gb.sound.tone(140, 40);
          fireCopBullet(p.x, p.y, fcx, fcy);
        }
      }
    }
  }
}

static void aiDraw(int camX, int camY) {
  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (!c.active) continue;
    blitCar(camX, camY, (int)c.x, (int)c.y, AI_CAR_FRAME[c.dir], c.color);
    // Girophare allume au-dela d'une etoile (>= 2) sur les voitures de police.
    if (c.isPolice && wanted.level >= 2) drawGyro(camX, camY, (int)c.x, (int)c.y);
  }
  for (int i = 0; i < NUM_AI_PEDS; i++) {
    AiPed &p = aiPeds[i];
    if (!p.active) continue;
    if (p.state == 1) blitSplat(camX, camY, (int)p.x, (int)p.y);
    else if (p.state == 3) blitDownBody(camX, camY, (int)p.x, (int)p.y, p.color);  // assomme (vol de voiture)
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

// --- Pool d'ennemis scenarises ---------------------------------------------
// Desactive tous les ennemis (changement d'objectif / fin de mission / mort).
static void clearEnemies() {
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
}

// Nombre d'ennemis encore DEBOUT (ni au sol, ni despawned). 0 => objectif rempli.
static int enemiesAliveCount() {
  int n = 0;
  for (int i = 0; i < MAX_ENEMIES; i++)
    if (enemies[i].active && enemies[i].phase != EN_DOWN) n++;
  return n;
}

// Pose les `o.enemyCount` ennemis AGRESSIFS de l'objectif autour de son point,
// de facon DETERMINISTE : chaque ennemi est calé sur la tuile marchable la plus
// proche d'un offset fixe en anneau autour de (o.x,o.y). Ils restent IDLE jusqu'a
// ce que le joueur entre dans ENEMY_AGGRO_RANGE (SP_PRESENT : visibles d'emblee ;
// SP_AMBUSH : memes positions, eveil au contact). Aucun RNG -> pas de pop hasardeux.
static void spawnEnemiesForObjective(const Objective &o) {
  clearEnemies();
  // Offsets en tuiles : devant, puis flancs, puis derriere le point d'ancrage.
  static const int OFFX[MAX_ENEMIES] = { 0,  2, -2,  0 };
  static const int OFFY[MAX_ENEMIES] = { 2,  0,  0, -2 };
  int n = o.enemyCount; if (n > MAX_ENEMIES) n = MAX_ENEMIES;
  uint8_t hp = (o.enemyKind == EK_GUNNER) ? ENEMY_HP_GUNNER : ENEMY_HP_THUG;
  for (int i = 0; i < n; i++) {
    Enemy &e = enemies[i];
    int wx = o.x + OFFX[i] * TILE_W, wy = o.y + OFFY[i] * TILE_H;
    int tx, ty;
    if (aiFindWalkTileNear(wx, wy, tx, ty)) { e.x = tx * 8 + 4; e.y = ty * 8 + 4; }
    else { e.x = (float)wx; e.y = (float)wy; }
    e.tgtx = (int)e.x; e.tgty = (int)e.y;
    e.dir = DIR_SOUTH; e.frame = 0; e.animTimer = 0;
    e.hp = hp; e.kind = o.enemyKind; e.phase = EN_IDLE;
    e.atkTimer = 0; e.downTimer = 0; e.active = true;
  }
}

// Inflige un coup a un ennemi. lethal = balle (mort immediate) ; sinon 3 coups de
// poing (le frapper l'eveille s'il dormait). A 0 PV : a terre (splat puis despawn).
static void hitEnemy(Enemy &e, bool lethal) {
  if (!e.active || e.phase == EN_DOWN) return;
  if (lethal || e.hp <= 1) {
    e.phase = EN_DOWN; e.downTimer = ENEMY_DOWN_FRAMES;
    targetDownX = (int)e.x; targetDownY = (int)e.y; targetDownTimer = ENEMY_DOWN_FRAMES;
    gb.sound.playOK();
  } else {
    e.hp--; e.phase = EN_AGGRO; gb.sound.tone(150, 50);
  }
}

// Met a jour les ennemis chaque frame. (fcx,fcy) = repere joueur (centre px).
// IDLE -> AGGRO quand le joueur approche ; AGGRO : fonce (gros bras) ou tire
// (gunner) ; renversable a la voiture lancee. EN_DOWN : decompte du splat.
static void updateEnemies(int fcx, int fcy) {
  if (seqKind == SEQ_CUT) return;                 // cinematique : ennemis figes (taunt)
  for (int i = 0; i < MAX_ENEMIES; i++) {
    Enemy &e = enemies[i];
    if (!e.active) continue;
    if (e.phase == EN_DOWN) {                       // a terre : despawn en fin de timer
      if (e.downTimer > 0 && --e.downTimer == 0) e.active = false;
      continue;
    }
    if (!missionRun.active) { e.active = false; continue; }  // mission finie : on nettoie
    float ddx = (float)fcx - e.x, ddy = (float)fcy - e.y;
    float d2 = ddx * ddx + ddy * ddy;
    if (e.phase == EN_IDLE) {
      if (d2 <= (float)(ENEMY_AGGRO_RANGE * ENEMY_AGGRO_RANGE)) e.phase = EN_AGGRO;
      else continue;                                // encore endormi : ne bouge pas
    }
    if (e.atkTimer > 0) e.atkTimer--;
    if (e.kind == EK_GUNNER) {
      // Tireur : se rapproche jusqu'a portee de tir, puis fait feu si ligne de vue.
      if (d2 > (float)(ENEMY_SHOOT_RANGE * ENEMY_SHOOT_RANGE) * 0.5f)
        missionChaseStep(cityMap, CITY_W, CITY_H, e.x, e.y, e.dir, e.tgtx, e.tgty,
                         ENEMY_SPEED, fcx, fcy, aiRng);
      if (e.atkTimer == 0 && d2 <= (float)(ENEMY_SHOOT_RANGE * ENEMY_SHOOT_RANGE) &&
          missionLineOfSight(cityMap, CITY_W, CITY_H, (int)e.x, (int)e.y, fcx, fcy, ENEMY_SHOOT_RANGE)) {
        e.atkTimer = ENEMY_SHOOT_PERIOD;
        gb.sound.tone(140, 40);
        fireCopBullet(e.x, e.y, fcx, fcy);          // balle hostile (esquivable)
      }
    } else {
      // Gros bras : fonce et frappe au contact.
      missionChaseStep(cityMap, CITY_W, CITY_H, e.x, e.y, e.dir, e.tgtx, e.tgty,
                       ENEMY_SPEED, fcx, fcy, aiRng);
      if (!driving && e.atkTimer == 0 && d2 < (float)(ENEMY_MELEE_DIST * ENEMY_MELEE_DIST)) {
        e.atkTimer = ENEMY_MELEE_PERIOD;
        gb.sound.tone(90, 70);
        hurtPlayer(ENEMY_MELEE_DMG, false);
      }
    }
    if (++e.animTimer >= AI_PED_ANIM) { e.animTimer = 0; e.frame ^= 1; }
    // Renversement par la voiture lancee (compte comme une mise a terre).
    if (driving) {
      float spd2 = car.vx * car.vx + car.vy * car.vy;
      if (spd2 > RUNOVER_SPEED2 && fabsf(car.x - e.x) < TARGET_RUNOVER_DIST &&
          fabsf(car.y - e.y) < TARGET_RUNOVER_DIST)
        hitEnemy(e, true);
    }
  }
}

// Dessine les ennemis : splat au sol (EN_DOWN) ou sprite recolore selon le type.
static void drawEnemies(int camX, int camY) {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    Enemy &e = enemies[i];
    if (!e.active) continue;
    if (e.phase == EN_DOWN) blitSplat(camX, camY, (int)e.x, (int)e.y);
    else blitPed(camX, camY, (int)e.x, (int)e.y, e.dir, e.frame,
                 e.kind == EK_GUNNER ? ENEMY_COLOR_GUNNER : ENEMY_COLOR_THUG);
  }
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
  curDef.reward = src.reward;
  curDef.isStory = src.isStory;
}

// Repliques de provocation jouees en face-a-face avant un combat de boss
// (CUT_TAUNT). Selon le titre de la mission. Renvoie false si pas de taunt.
static bool bossTauntLines(const char *title, const char *&l1, const char *&l2) {
  if (!title) return false;
  if (strcmp(title, "Rico le Loup") == 0) {
    l1 = "Rico : Tony t'envoie crever a ma place, gamin ?";
    l2 = "Rico : approche, que je t'apprenne le respect."; return true;
  }
  if (strcmp(title, "Bruno") == 0) {
    l1 = "Bruno : tu es alle trop loin. Victor veut ta tete.";
    l2 = "Bruno : et c'est moi qui vais la lui porter."; return true;
  }
  if (strcmp(title, "Le dernier appel") == 0) {
    l1 = "Victor : tu as du cran de venir jusqu'ici.";
    l2 = "Victor : ca ne te sauvera pas. Comme Marco."; return true;
  }
  return false;
}

// Active l'objectif courant : narration + spawn des entites necessaires.
static void enterObjective() {
  const MissionDef &def = curDef;
  const Objective &o = def.objectives[missionRun.step];
  narrate(o.text);
  objBeat = 0; objElapsed = 0; objSubdue = 0;   // compteurs propres a cet objectif
  targetAtkTimer = 0; missionCrushDone = false;
  clearEnemies();                                // pas d'ennemis residuels de l'objectif precedent
  // Ennemis scenarises AGRESSIFS (gardes, assaillants) poses des l'activation,
  // de facon deterministe autour du point d'objectif (cf. spawnEnemiesForObjective).
  if (o.enemyCount > 0) spawnEnemiesForObjective(o);
  if (o.event == EV_MARCO_JOIN) {
    allyColor = (o.count == 1) ? SARAH_COLOR : MARCO_COLOR;  // count==1 -> Sarah
    marcoWaiting = true;                          // l'allie va apparaitre puis etre pris (TALK ou GOTO)
    // Marco reste D'ABORD dans l'immeuble (il vient de dire "j'arrive" via o.text)
    // puis SORT et marche jusqu'au marqueur apres MARCO_EMERGE_DELAY frames. On le
    // pose sur la tuile MARCHABLE la plus proche du marqueur (a l'ecran a l'arrivee),
    // pas en surplomb du nord (qui pouvait le faire apparaitre hors champ).
    int wt, ht;
    if (aiFindWalkTileNear(o.x, o.y, wt, ht)) { marcoX = (float)(wt * 8 + 4); marcoY = (float)(ht * 8 + 4); }
    else { marcoX = (float)o.x; marcoY = (float)o.y; }
    marcoDir = DIR_SOUTH; marcoFrame = 0; marcoAnimTimer = 0;
    marcoEmergeDelay = MARCO_EMERGE_DELAY;        // attend la fin de "j'arrive" avant de sortir
    // (la caisse de mission est deja garee depuis le decrochage du telephone)
  }
  if (o.type == OBJ_ENTER_CAR) {
    mCar.x = o.x; mCar.y = o.y; mCar.angle = 0.0f; mCar.vx = 0.0f; mCar.vy = 0.0f;
    mCarActive = true;
  } else if (o.type == OBJ_KILL && o.enemyCount == 0 && !target.active) {
    // KILL "cible nommee" (pas de gardes) : boss (count>1) ou fugitif ordinaire.
    targetHp = o.count > 1 ? o.count : 1;          // o.count>1 -> BOSS qui encaisse plusieurs coups
    // AGRESSIF : le tueur (post mort de Marco) ET les boss FONCENT sur le joueur ;
    // seul le petit fugitif (Joe, count<=1) erre et fuit encore.
    if (killerChase || o.count > 1) spawnTargetAt(o.x, o.y);
    else                            spawnTargetWanderNear(o.x, o.y);
    // Boss a pied : bref face-a-face (joueur fige, deux repliques) avant la baston.
    const char *l1, *l2;
    if (o.count > 1 && !killerChase && !driving && bossTauntLines(curDef.title, l1, l2))
      startTauntCut(l1, l2);
  } else if (o.type == OBJ_SUBDUE && !target.active) {
    // La cible a tabasser est posee de facon DETERMINISTE pres du marqueur (pas
    // en surplomb hors champ) et se DEFEND : une fois sortie, elle fonce et frappe
    // (sans mourir) au lieu de rester passive (cf. missionUpdate).
    int wt, ht;
    if (aiFindWalkTileNear(o.x, o.y, wt, ht)) { target.x = (float)(wt * 8 + 4); target.y = (float)(ht * 8 + 4); }
    else { target.x = (float)o.x; target.y = (float)o.y; }
    target.tgtx = o.x; target.tgty = o.y;
    target.dir = DIR_SOUTH; target.frame = 0; target.animTimer = 0;
    target.phase = T_EMERGE; target.loseTimer = 0;
    target.active = true; target.chase = false;
  } else if (o.type == OBJ_SURVIVE) {
    int pcx = driving ? (int)car.x : playerX + PLAYER_W / 2;
    int pcy = driving ? (int)car.y : playerY + PLAYER_H / 2;
    spawnTargetAt(pcx, pcy);
  }
}

// Demarre une mission (telephone decroche).
static void startMission(uint8_t m) {
  missionRun.def = m; missionRun.step = 0; missionRun.active = true;
  buildMissionRuntime(m);                 // resout les coords POI de la mission
  target.active = false; marcoWaiting = false; marcoFollow = false; marcoAboard = false;
  marcoEmergeDelay = 0;
  mCarActive = false;
  killerChase = false;
  clearEnemies();
  // Caisse "compagnon" de Marco : si la mission le ramasse A PIED (EV_MARCO_JOIN
  // sans requireCar, ex. M1), sa voiture est garee SUR LE COTE (route a droite du
  // batiment) DES LE DECROCHAGE -- elle ne pop pas en arrivant, et reste la
  // jusqu'a ce qu'on l'utilise (elle devient alors la caisse du joueur).
  for (uint8_t i = 0; i < curDef.count; i++) {
    const Objective &o = curObjs[i];
    if (o.event == EV_MARCO_JOIN && !o.requireCar) {
      int cx, cy;
      if (!findRoadSpotNear(o.x, o.y, cx, cy)) { cx = o.x + 2 * TILE_W; cy = o.y; }
      mCar.x = (float)cx; mCar.y = (float)cy; mCar.angle = 0.0f; mCar.vx = 0.0f; mCar.vy = 0.0f;
      mCarActive = true;
      break;
    }
  }
  narrate(curDef.title);                  // annonce le nom de la mission
  enterObjective();
  gb.sound.playOK();
}

// Point d'entree GENERIQUE de tout gain d'argent (mission, butin au sol, broyage
// d'epave...) : credite la cagnotte et arme l'animation (compteur qui roule +
// "+$X" flottant). N'emet pas de son : l'appelant garde le sien, le compteur
// fait ses petits "tic" en roulant.
static void addMoney(int32_t amount) {
  if (amount <= 0) { playerMoney += amount; return; }   // depense/zero : pas d'anim
  playerMoney  += amount;
  moneyGainAmt  = amount;
  moneyGainTimer = MONEY_GAIN_FRAMES;
  int32_t step = (playerMoney - moneyShown) / MONEY_ROLL_FRAMES;  // pour finir en ~2 s
  if (step < 1) step = 1;
  if (step > moneyRollStep) moneyRollStep = step;       // gains cumules : on roule au moins aussi vite
}

// --- Le Bar : tournee du vieux poivrot --------------------------------------
// Presser A dans la bbox "Le Bar" : -10 $ et une replique au hasard (univers
// GTA, ton rigolard). Si trop fauche : pas de verre.
static const char *BAR_LINES[10] = {
  "Le vieux: repeins ta caisse au Pay'n'Spray, les poulets te calculent plus!",
  "Le vieux: a ton age je braquais 3 superettes avant le cafe.",
  "Le vieux: la Casse paye bien... surtout les caisses qui sont pas a toi.",
  "Le vieux: un bazooka a l'AMU Nation, moins cher qu'un bon dentiste.",
  "Le vieux: les poulets courent vite, mais pas plus qu'une bonne caisse.",
  "Le vieux: paie-moi un coup et je te dis ou est planque le fric... j'ai oublie.",
  "Le vieux: dors a la planque gamin, meme les durs ont besoin d'un dodo.",
  "Le vieux: j'ai vu un gus sauter par-dessus 3 bagnoles. C'etait toi?",
  "Le vieux: le casino t'aime pas, la maison gagne toujours. Crois le vieux.",
  "Le vieux: sante! A la tienne et a celle des pigeons du parc.",
};
static void barDrink() {
  if (playerMoney < 10) { narrate("Le barman: pas de thune, pas de tournee."); gb.sound.tone(120, 120); return; }
  addMoney(-10);
  narrate(BAR_LINES[aiRngNext(aiRng) % 10]);
  gb.sound.tone(330, 50); gb.sound.tone(294, 70);       // glouglou
}

// --- Les Commerces : braquage -----------------------------------------------
// Presser A dans la bbox "Commerces" : chance de reussite selon l'arme tenue.
// Reussite -> +100 $ ; echec -> -1 PV (le commercant riposte).
static void robStore() {
  if (seqKind != SEQ_NONE || playerDown) return;
  uint8_t chance;
  switch (curWeapon) {
    case WEAPON_PISTOL:               chance = 30;  break;
    case WEAPON_SMG: case WEAPON_SHOTGUN: chance = 80;  break;
    case WEAPON_BAZOOKA: case WEAPON_GRENADE: chance = 100; break;
    default:                          chance = 10;  break;   // poing / mains nues
  }
  if ((uint8_t)(aiRngNext(aiRng) % 100) < chance) {
    addMoney(100);
    narrate("Braquage reussi ! +$100");
    gb.sound.tone(988, 60); gb.sound.playOK();            // cha-ching
  } else {
    narrate("Braquage rate ! Le commercant riposte. -1 PV");
    gb.sound.tone(160, 160);
    hurtPlayer(1, false);
  }
}

// --- Les Bureaux : selon l'arme tenue ---------------------------------------
// Repliques quand on entre les mains vides (le type refuse de "bosser").
static const char *BUREAU_REFUSALS[6] = {
  "Je vais quand meme pas me mettre au vert.",
  "Les horaires de bureau, tres peu pour moi.",
  "Costard-cravate ? Plutot crever.",
  "Bosser huit heures par jour ? T'es malade.",
  "La paperasse, c'est pas pour les types comme moi.",
  "Pointer a la machine a cafe ? Sans facon.",
};
// Presser A dans la bbox "Les Bureaux". Mains nues -> on rebrousse chemin avec
// une vanne. Arme au poing -> braquage eclair : +200 $, alarme, et 3 etoiles.
static void bureauVisit() {
  if (seqKind != SEQ_NONE || playerDown) return;
  if (curWeapon == WEAPON_FIST) {                        // mains vides : demi-tour
    narrate(BUREAU_REFUSALS[aiRngNext(aiRng) % 6]);
    gb.sound.playCancel();
    return;
  }
  // Braquage eclair : tout le monde se planque, tu rafles la caisse...
  addMoney(200);
  narrate("Braquage eclair ! Panique, +$200... mais l'alarme hurle !");
  gb.sound.tone(988, 60); gb.sound.playOK();             // cha-ching
  gb.sound.tone(1568, 120); gb.sound.tone(1175, 120);    // alarme stridente
  if (wanted.level < 3) wanted.level = 3;                // ...et te voila a 3 etoiles
  wanted.streak = 0; wanted.streakTimer = 0;
  wanted.decayTimer = WANTED_DECAY_FRAMES;               // pleine duree de vie de l'etoile
}

// Anime la cagnotte chaque frame : moneyShown rattrape playerMoney en roulant
// (gain) ou d'un coup (depense). Petit "tic" sonore pendant la montee.
static void updateMoneyAnim() {
  if (moneyShown > playerMoney) {                        // depense : rattrapage instantane
    moneyShown = playerMoney;
  } else if (moneyShown < playerMoney) {                 // gain : on roule vers le total
    int32_t diff = playerMoney - moneyShown;
    int32_t s = diff < moneyRollStep ? diff : moneyRollStep;
    moneyShown += s;
    if ((moneyShown & 3) == 0) gb.sound.playTick();      // tic discret du compteur
  } else {
    moneyRollStep = 1;                                   // au repos : vitesse remise a zero
  }
  if (moneyGainTimer > 0) moneyGainTimer--;
}

// Mission echouee (limite de temps depassee) : message + retour au repos.
static void failMission(const char *msg) {
  narrate(msg);
  missionRun.active = false;
  target.active = false; marcoWaiting = false; marcoFollow = false; marcoAboard = false;
  mCarActive = false; carIsMission = false;
  killerChase = false; storyMissionActive = false;   // campaignStep inchange -> on rejoue la mission
  clearEnemies();
  missionFailedTimer = MISSION_FAIL_FRAMES;
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

// --- Cinematiques de mission -----------------------------------------------
// Marco descend de la caisse, va parler au tueur, se fait abattre (joueur fige).
// Le KILL est deja l'objectif courant (missionAdvance vient de passer dessus) :
// on positionne le tueur ICI et on l'arme (chase) seulement a la fin de la scene.
// Cloture la mission courante : retire l'allie/la caisse, credite la prime,
// declenche le bandeau "MISSION ACCOMPLIE" et fait avancer la trame le cas
// echeant. Appelable depuis missionProgress (etape finale) comme depuis une
// cinematique de fin (CUT_MARCO_LEAVE).
static void finishMission() {
  const MissionDef &def = curDef;
  marcoWaiting = false; marcoFollow = false; marcoAboard = false; marcoEmergeDelay = 0;
  if (mCarActive) mCarActive = false;            // caisse de mission jamais prise : on la retire
  addMoney(def.reward);
  missionDoneTimer = MISSION_DONE_FRAMES;
  gb.sound.tone(988, 60); gb.sound.playOK();     // cha-ching de fin de mission
  if (storyMissionActive) {                      // progression de la trame
    storyMissionActive = false;
    if (campaignStep < STORY_LEN) campaignStep++; // mission suivante : le tel rouge re-sonne
  }
}

static void startMarcoDeathCut() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_CUT; cutKind = CUT_MARCO_DEATH; cutPhase = 0; cutTimer = 0;
  car.vx = 0.0f; car.vy = 0.0f;                       // la caisse s'arrete net
  marcoAboard = false; marcoWaiting = false; marcoEmergeDelay = 0;
  marcoFollow = true;                                  // pour que drawMarco le dessine
  int ox, oy;                                          // Marco descend a cote de la caisse
  if (findFootSpot((int)car.x, (int)car.y, ox, oy)) { marcoX = ox + PLAYER_W / 2; marcoY = oy + PLAYER_H / 2; }
  else { marcoX = car.x; marcoY = car.y + TILE_H; }
  marcoFrame = 0; marcoAnimTimer = 0;
  // Le tueur ("l'autre gars") attend au point de rendez-vous (= coords du KILL).
  const Objective &k = curObjs[missionRun.step];
  int tx, ty;
  if (aiFindWalkTileNear(k.x, k.y, tx, ty)) { target.x = (float)(tx * 8 + 4); target.y = (float)(ty * 8 + 4); }
  else { target.x = (float)k.x; target.y = (float)k.y; }
  target.dir = DIR_SOUTH; target.frame = 0; target.animTimer = 0;
  target.phase = T_WANDER; target.loseTimer = 0; target.active = true; target.chase = false;
  targetHp = k.count > 1 ? k.count : 1;
}

// Fin amicale de M1 : la caisse s'arrete, Marco descend a cote, marche vers la
// porte (doorX,doorY = position du POI cible) en remerciant le joueur, puis
// disparait dans le batiment -> finishMission(). Joueur fige (SEQ_CUT).
static void startMarcoLeaveCut(int doorX, int doorY) {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_CUT; cutKind = CUT_MARCO_LEAVE; cutPhase = 0; cutTimer = 0;
  car.vx = 0.0f; car.vy = 0.0f;                       // la caisse s'arrete net
  marcoAboard = false; marcoWaiting = false; marcoEmergeDelay = 0;
  marcoFollow = true;                                  // pour que drawMarco le dessine
  int ox, oy;                                          // Marco descend a cote de la caisse
  if (findFootSpot((int)car.x, (int)car.y, ox, oy)) { marcoX = ox + PLAYER_W / 2; marcoY = oy + PLAYER_H / 2; }
  else { marcoX = car.x; marcoY = car.y + TILE_H; }
  marcoFrame = 0; marcoAnimTimer = 0;
  marcoLeaveX = (int16_t)doorX; marcoLeaveY = (int16_t)doorY;
}

// Bref face-a-face avant baston (boss / embuscade) : deux repliques, puis on rend
// la main -- les ennemis deja poses chargent des qu'on approche. Joueur fige.
static void startTauntCut(const char *l1, const char *l2) {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_CUT; cutKind = CUT_TAUNT; cutPhase = 1; cutTimer = CUT_LINE_FRAMES;
  cutLine1 = l1; cutLine2 = l2;
  if (l1) narrate(l1);
}

// Avance la cinematique courante (appelee par updateSequence quand SEQ_CUT).
static void cutsceneUpdate() {
  if (cutKind == CUT_TAUNT) {
    if (cutTimer > 0 && --cutTimer == 0) {
      if (cutPhase == 1) { cutPhase = 2; cutTimer = CUT_LINE_FRAMES; if (cutLine2) narrate(cutLine2); }
      else { seqKind = SEQ_NONE; cutKind = CUT_NONE; }   // fin : les ennemis chargent
    }
    return;
  }
  if (cutKind == CUT_MARCO_LEAVE) {
    switch (cutPhase) {
      case 0: {                                          // Marco marche vers sa porte
        float dx = (float)marcoLeaveX - marcoX, dy = (float)marcoLeaveY - marcoY;
        if (dx * dx + dy * dy > 12.0f * 12.0f) {
          npcWalkToward(marcoX, marcoY, marcoDir, marcoFrame, marcoAnimTimer,
                        (float)marcoLeaveX, (float)marcoLeaveY, MARCO_FOLLOW_SPEED);
        } else {
          marcoFrame = 0;
          cutPhase = 1; cutTimer = CUT_LINE_FRAMES;
          narrate("Marco : bon boulot pour un premier jour. Repose-toi, petit.");
        }
        break;
      }
      case 1:                                            // replique -> il rentre et disparait
        if (cutTimer > 0 && --cutTimer == 0) {
          marcoFollow = false;                           // Marco disparait dans son batiment
          seqKind = SEQ_NONE; cutKind = CUT_NONE;
          finishMission();                               // prime + bandeau "MISSION ACCOMPLIE"
        }
        break;
    }
    return;
  }
  if (cutKind != CUT_MARCO_DEATH) { seqKind = SEQ_NONE; cutKind = CUT_NONE; return; }
  switch (cutPhase) {
    case 0: {                                            // Marco marche vers le tueur
      float dx = target.x - marcoX, dy = target.y - marcoY;
      if (dx * dx + dy * dy > 16.0f * 16.0f) {
        npcWalkToward(marcoX, marcoY, marcoDir, marcoFrame, marcoAnimTimer,
                      target.x, target.y, MARCO_FOLLOW_SPEED);
      } else {
        marcoFrame = 0;
        marcoDir   = (marcoX < target.x) ? DIR_EAST : DIR_WEST;   // face a face
        target.dir = (target.x < marcoX) ? DIR_EAST : DIR_WEST;
        cutPhase = 1; cutTimer = CUT_LINE_FRAMES;
        narrate("Marco : qu'est-ce que tu fous la, toi ?");
      }
      break;
    }
    case 1:                                              // replique 1
      if (cutTimer > 0 && --cutTimer == 0) {
        cutPhase = 2; cutTimer = CUT_LINE_FRAMES;
        narrate("L'autre : desole, Marco. Rien de personnel.");
      }
      break;
    case 2:                                              // replique 2 -> il fait feu
      if (cutTimer > 0 && --cutTimer == 0) {
        cutPhase = 3; cutTimer = CUT_SHOOT_FRAMES;
        gb.sound.tone(140, 60); gb.sound.tone(90, 130);  // coup de feu
        marcoFollow = false; marcoAboard = false;        // Marco tombe comme un PNJ
        targetDownX = (int)marcoX; targetDownY = (int)marcoY;
        targetDownTimer = PED_DOWN_FRAMES * 3;           // le corps reste un moment
        narrate("Marco s'effondre.");
      }
      break;
    case 3:                                              // temps sur le corps -> reprise
      if (cutTimer > 0 && --cutTimer == 0) {
        seqKind = SEQ_NONE; cutKind = CUT_NONE;
        killerChase = true; target.chase = true;         // le tueur prend la fuite : a rattraper
        objElapsed = 0;
        narrate("Le tueur file ! Rattrape-le !");
        gb.sound.playCancel();
      }
      break;
  }
}

// Teste l'objectif courant ; s'il est rempli, applique l'evenement de
// transition (Marco monte / meurt) puis active l'objectif suivant, ou termine.
static void missionProgress() {
  if (!missionRun.active || seqKind == SEQ_CUT) return;
  // Echec de trame : un TUEUR (KILL en mode poursuite) qui sort des limites monde.
  if (target.active && killerChase && curObjs[missionRun.step].type == OBJ_KILL) {
    const int M = 8;  // marge px
    if (target.x < -M || target.y < -M ||
        target.x > WORLD_W + M || target.y > WORLD_H + M) {
      failMission("Le tueur s'est echappe.");
      return;
    }
  }
  const MissionDef &def = curDef;
  MissionState s;
  s.actorCx = driving ? (int)car.x : playerX + PLAYER_W / 2;
  s.actorCy = driving ? (int)car.y : playerY + PLAYER_H / 2;
  s.driving = driving;
  s.inMissionCar = driving && carIsMission;
  s.targetAlive = target.active;
  s.beatCount = objBeat;
  s.subdueCount = objSubdue;
  s.elapsed = objElapsed;
  s.enemiesAlive = enemiesAliveCount();
  s.crushDone = missionCrushDone;
  const Objective &cur = def.objectives[missionRun.step];
  if (missionTimedOut(cur, objElapsed)) { failMission("Trop tard ! Mission ratee."); return; }
  // Rencontre a pied avec Marco (TALK) : on ne la valide qu'une fois qu'il a fini
  // de SORTIR et de rejoindre son poste, pour qu'on le voie arriver ("j'arrive !").
  if (cur.type == OBJ_TALK && cur.event == EV_MARCO_JOIN && marcoWaiting) {
    float ddx = marcoX - (float)cur.x, ddy = marcoY - (float)cur.y;
    if (ddx * ddx + ddy * ddy > 4.0f) return;
  }
  if (!missionObjectiveDone(cur, s)) return;

  const Objective &done = def.objectives[missionRun.step];  // objectif accompli
  uint8_t ev = missionAdvance(missionRun, def);   // step++ (active=false si fini)
  if (done.doneText) narrate(done.doneText);      // message "objectif atteint"
  if (ev == EV_MARCO_JOIN) {
    marcoWaiting = false;
    if (driving) {                                // deja au volant (M4) : Marco monte direct
      marcoAboard = true;
    } else {                                      // a pied (M1) : Marco nous emboite le pas
      marcoFollow = true;                          // reprend sa position courante (deja sorti)
    }
  } else if (ev == EV_MARCO_DIE) {
    // Cinematique : Marco descend, parle au tueur, se fait abattre (joueur fige).
    // La scene arme elle-meme le KILL (tueur en fuite) a sa fin -> pas d'enterObjective.
    startMarcoDeathCut();
    return;
  } else if (ev == EV_MARCO_LEAVE) {
    // Fin amicale : Marco descend, remercie, rentre chez lui. La cinematique
    // cloture elle-meme la mission (finishMission) a sa fin -> pas de suite ici.
    startMarcoLeaveCut(done.x, done.y);
    return;
  }
  if (missionRun.active) enterObjective();
  else finishMission();                            // derniere etape franchie : prime + bandeau
}

// Deplace la cible chaque frame : tueur -> poursuite ; Joe -> vue/fuite/flanerie.
// Plus animation et ecrasement par la voiture lancee. (fcx,fcy) = repere joueur.
static void missionUpdate(int fcx, int fcy) {
  if (seqKind == SEQ_CUT) return;                 // cinematique : cutsceneUpdate gere les acteurs
  if (!missionRun.active || !target.active) return;
  if (targetAtkTimer > 0) targetAtkTimer--;
  if (curObjs[missionRun.step].type == OBJ_SUBDUE) {             // cible a tabasser : sort puis SE DEFEND
    if (target.phase == T_EMERGE) {
      if (npcWalkToward(target.x, target.y, target.dir, target.frame,
                        target.animTimer, (float)target.tgtx, (float)target.tgty,
                        TARGET_WANDER_SPEED))
        target.phase = T_WANDER;                                // sortie : passe a l'attaque
      return;
    }
    // Sortie : elle fonce sur le joueur et frappe (sans mourir : objectif = la mater).
    missionChaseStep(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
                     target.tgtx, target.tgty, TARGET_WANDER_SPEED, fcx, fcy, aiRng);
    if (++target.animTimer >= AI_PED_ANIM) { target.animTimer = 0; target.frame ^= 1; }
    float sdx = (float)fcx - target.x, sdy = (float)fcy - target.y;
    if (!driving && targetAtkTimer == 0 &&
        sdx * sdx + sdy * sdy < (float)(ENEMY_MELEE_DIST * ENEMY_MELEE_DIST)) {
      targetAtkTimer = ENEMY_MELEE_PERIOD; gb.sound.tone(90, 70); hurtPlayer(ENEMY_MELEE_DMG, false);
    }
    return;
  }

  if (target.chase) {
    missionChaseStep(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
                     target.tgtx, target.tgty, TARGET_CHASE_SPEED, fcx, fcy, aiRng);
    // Tueur / boss AGRESSIF : frappe le joueur au corps-a-corps.
    float kdx = (float)fcx - target.x, kdy = (float)fcy - target.y;
    if (!driving && targetAtkTimer == 0 &&
        kdx * kdx + kdy * kdy < (float)(ENEMY_MELEE_DIST * ENEMY_MELEE_DIST)) {
      targetAtkTimer = ENEMY_MELEE_PERIOD; gb.sound.tone(90, 70); hurtPlayer(ENEMY_MELEE_DMG, false);
    }
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

// Avance (x,y) vers (tx,ty) a `speed` px/frame, oriente le sprite et anime la
// marche. Renvoie true une fois arrive (a moins de `speed` du but : pose neutre).
static bool npcWalkToward(float &x, float &y, uint8_t &dir, uint8_t &frame,
                          uint8_t &animTimer, float tx, float ty, float speed) {
  float dx = tx - x, dy = ty - y;
  float d2 = dx * dx + dy * dy;
  if (d2 <= speed * speed) { x = tx; y = ty; frame = 0; return true; }
  float d = sqrtf(d2);
  x += dx / d * speed; y += dy / d * speed;
  if (fabsf(dx) > fabsf(dy)) dir = (dx > 0) ? DIR_EAST : DIR_WEST;
  else                       dir = (dy > 0) ? DIR_SOUTH : DIR_NORTH;
  if (++animTimer >= AI_PED_ANIM) { animTimer = 0; frame ^= 1; }
  return false;
}

// Marco companion (a pied). Trois temps : il SORT du batiment et marche jusqu'au
// marqueur (marcoWaiting), puis nous SUIT a distance de confort (marcoFollow), et
// EMBARQUE des qu'on prend une voiture. (fcx,fcy) = repere joueur (centre px).
static void marcoUpdate(int fcx, int fcy) {
  if (!missionRun.active || seqKind == SEQ_CUT) return;  // cinematique : Marco gere par cutsceneUpdate
  if (marcoWaiting) {                              // encore dans l'immeuble, puis emergence
    if (marcoEmergeDelay > 0) { marcoEmergeDelay--; return; }  // il a dit "j'arrive" : on patiente
    const Objective &o = curObjs[1];              // sort et rejoint le marqueur a pied
    npcWalkToward(marcoX, marcoY, marcoDir, marcoFrame, marcoAnimTimer,
                  (float)o.x, (float)o.y, MARCO_FOLLOW_SPEED);
    return;
  }
  if (!marcoFollow) return;
  if (driving) {                                   // on vient de monter -> Marco embarque
    marcoFollow = false; marcoAboard = true;
    narrate(allyColor == SARAH_COLOR ? "Sarah : vite, demarre !"
                                     : "Marco : roule, je monte derriere.");
    gb.sound.playOK();
    return;
  }
  float dx = (float)fcx - marcoX, dy = (float)fcy - marcoY;
  float d2 = dx * dx + dy * dy;
  if (d2 > MARCO_FOLLOW_GAP * MARCO_FOLLOW_GAP) {  // trop loin : il avance vers nous
    float d = sqrtf(d2);
    float step = (d > 40.0f) ? MARCO_FOLLOW_SPEED * 2.0f : MARCO_FOLLOW_SPEED;  // rattrapage
    marcoX += dx / d * step; marcoY += dy / d * step;
    if (fabsf(dx) > fabsf(dy)) marcoDir = (dx > 0) ? DIR_EAST : DIR_WEST;
    else                       marcoDir = (dy > 0) ? DIR_SOUTH : DIR_NORTH;
    if (++marcoAnimTimer >= AI_PED_ANIM) { marcoAnimTimer = 0; marcoFrame ^= 1; }
  } else {
    marcoFrame = 0;                                // a l'arret : pose neutre
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
      addMoney(loots[i].amount);
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

// Zone de depose de La Casse : carre raye jaune/noir (hazard) sur le point fixe
// genere (cityCasse). Repere ou GARER sa caisse pour la faire broyer a pied.
static void drawCasseZone(int camX, int camY) {
#if !CITY_HAS_CASSE
  (void)camX; (void)camY; return;
#else
  int sx = CITY_CASSE_TX - camX, sy = CITY_CASSE_TY - camY;
  for (int dy = -6; dy <= 6; dy++)
    for (int dx = -6; dx <= 6; dx++) {
      bool edge = (dx <= -5 || dx >= 5 || dy <= -5 || dy >= 5);
      if (!edge) continue;                         // contour seulement
      int x = sx + dx, y = sy + dy;
      if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
      fb[y * SCREEN_W + x] = (((dx + dy) >> 1) & 1) ? 0xFFE0 : 0x0000;  // raye jaune/noir
    }
#endif
}

// Entree dans un Pay'n'Spray : demarre la cinematique (la caisse rentre, on
// entend la bombe, elle ressort repeinte). Le joueur attend, fige.
static void startSpraySeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_SPRAY; seqPhase = PH_IN; seqTimer = SEQ_IN_FRAMES;
  car.vx = 0.0f; car.vy = 0.0f;                // on coupe l'elan en entrant
}

// Fin d'amorcage de la grue : demarre le broyage. Prime selon les PV restants
// (epave plus chere si la caisse est saine). On fige la caisse.
static void startCrushSeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_CRUSH; seqPhase = PH_SWING; seqTimer = SEQ_SWING_FRAMES;
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
  carHp = CAR_MAX_HP; carFuse = 0; carRunaway = false;   // service complet : reparee, feu eteint
  wantedClear(wanted);
  int32_t pay = playerMoney < SPRAY_COST ? playerMoney : SPRAY_COST;
  playerMoney -= pay;
  narrate("Repeinte. Plus recherche.");
  gb.sound.playOK();
}

static void drawPhones(int camX, int camY) {
  // Sonne pendant toute la campagne ET une derniere fois pour l'epilogue
  // (campaignStep == STORY_LEN). Au-dela : muette.
  bool storyRings = (campaignStep <= STORY_LEN) && !missionRun.active;
  for (int i = 0; i < NUM_STORY_PHONES; i++)
    drawPhoneBooth(camX, camY, storyPx[i], storyPy[i], PHONE_BODY_STORY, storyRings);
  if (missionRun.active) return;
  for (int i = 0; i < NUM_PHONES; i++)
    drawPhoneBooth(camX, camY, phonePx[i], phonePy[i], PHONE_BODY_MISSION, true);
}

// Voiture de mission garee (objectif ENTER_CAR) : orange, orientee est.
static void drawMissionCar(int camX, int camY) {
  if (!mCarActive) return;
  blitCar(camX, camY, (int)mCar.x, (int)mCar.y, AI_CAR_FRAME[DIR_EAST], MISSION_CAR_COLOR);
}

// Marco : pieton cyan. Debout au marqueur tant qu'on ne l'a pas aborde, puis
// nous suit a pied (marcoFollow) jusqu'a ce qu'on monte en caisse. Une fois a
// bord (marcoAboard) il n'est plus dessine separement.
static void drawMarco(int camX, int camY) {
  if (!missionRun.active) return;
  if (marcoWaiting && marcoEmergeDelay > 0) return;   // encore dans l'immeuble (pas encore sorti)
  if (marcoWaiting || marcoFollow)        // sort du batiment / attend / nous suit
    blitPed(camX, camY, (int)marcoX, (int)marcoY, marcoDir, marcoFrame, allyColor);
}

// Marqueur de destination clignotant (objectif GOTO / ENTER_CAR).
static void drawMarker(int camX, int camY) {
  if (!missionRun.active) return;
  const Objective &o = curObjs[missionRun.step];
  if (o.type != OBJ_GOTO && o.type != OBJ_ENTER_CAR && o.type != OBJ_CRUSH) return;
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
  if (narrCount > 0) return;                      // un message defile : on cache le POI
  int pi = poiAtTile(fcx >> 3, fcy >> 3);
  if (pi < 0) return;
  const char *name = cityPois[pi].name;
  int w = (int)strlen(name) * 4 + 1;
  const int top = SCREEN_H - 7;                  // tout en bas (meme ligne que la narration)
  for (int y = top; y < top + 6; y++)
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
static void printShadowCol(int x, int y, const char *s, uint16_t col) {
  gb.display.setColor(BLACK);      gb.display.setCursor(x + 1, y + 1); gb.display.print(s);
  gb.display.setColor((Color)col); gb.display.setCursor(x, y);         gb.display.print(s);
}
static void printShadow(int x, int y, const char *s) { printShadowCol(x, y, s, 0xFFFF); }

// Bandeau d'annonce GENERIQUE (mission accomplie / ratee, mort, arrestation) :
// une bande sombre qui MONTE du bas de l'ecran, tient en place, puis repart vers
// le bas. Non bloquant : le jeu continue derriere. Seule la couleur (liseres +
// texte) change selon le contexte. timer = frames restantes, total = duree.
static void drawSlideBanner(const char *msg, uint16_t color, int timer, int total) {
  const int BH = 11, restY = 40;                       // bande de 11 px, calee a y=40
  int e = total - timer;                               // frames ecoulees
  int hidden = SCREEN_H - restY;                        // course de glissement
  int off;                                              // decalage vers le bas (0 = en place)
  if (e < MISSION_DONE_SLIDE)       off = hidden * (MISSION_DONE_SLIDE - e) / MISSION_DONE_SLIDE;       // monte
  else if (timer < MISSION_DONE_SLIDE) off = hidden * (MISSION_DONE_SLIDE - timer) / MISSION_DONE_SLIDE; // repart
  else                              off = 0;                                                            // stable
  int y = restY + off;
  for (int yy = y; yy < y + BH && yy < SCREEN_H; yy++)  // bande assombrie (le jeu transparait)
    for (int x = 0; x < SCREEN_W; x++)
      fb[yy * SCREEN_W + x] = (uint16_t)((fb[yy * SCREEN_W + x] >> 1) & 0x7BEF);
  for (int x = 0; x < SCREEN_W; x++) {                  // liseres colores haut/bas
    if (y >= 0 && y < SCREEN_H)            fb[y * SCREEN_W + x] = color;
    int yb = y + BH - 1;
    if (yb >= 0 && yb < SCREEN_H)          fb[yb * SCREEN_W + x] = color;
  }
  printShadowCol((SCREEN_W - (int)strlen(msg) * 4) / 2, y + 3, msg, color);
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
static const uint16_t HEART_ARMOR = 0x04FF;                       // bleu : cœur protege par le gilet
static const uint16_t STAR_FULL  = 0xFFE0, STAR_EMPTY  = 0x4208;  // jaune / gris sombre

// Barre de stats du haut (GTA2-like) sur fond transparent : cœurs a gauche,
// etoiles a droite, arme + munitions au centre (a pied), toast central au
// changement d'arme. Dessinee au ras du haut (y=0).
static void drawTopHud() {
  // Cœurs (haut-gauche) : compteur unique 0..6. blue = gilet (points 4..6, a
  // gauche), red = vie (points 1..3), puis emplacements vides.
  int blue = playerHearts > PLAYER_HEARTS_MAX ? playerHearts - PLAYER_HEARTS_MAX : 0;
  int filled = playerHearts < PLAYER_HEARTS_MAX ? playerHearts : PLAYER_HEARTS_MAX;
  for (int i = 0; i < HUD_HEARTS_MAX; i++)
    blitIcon5(1 + i * 6, 1, ICON_HEART,
              i < blue ? HEART_ARMOR : (i < filled ? HEART_FULL : HEART_EMPTY));
  const int starsX = (SCREEN_W - HUD_STARS_MAX * 6) / 2;
  // L'etoile du dessus clignote dans ses 10 dernieres secondes (fin de vie).
  bool starOff = wantedBlinking(wanted) && ((missionAnim >> 2) & 1);
  for (int i = 0; i < HUD_STARS_MAX; i++) {     // etoiles (centre)
    bool lit = (i < wanted.level) && !(starOff && i == wanted.level - 1);
    blitIcon5(starsX + i * 6, 1, ICON_STAR, lit ? STAR_FULL : STAR_EMPTY);
  }
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
  // Argent (sous les cœurs) : "$" + montant (qui roule) en vert, ombre noire.
  char money[12];
  snprintf(money, sizeof(money), "$%ld", (long)moneyShown);
  gb.display.setColor(BLACK); gb.display.setCursor(2, 9); gb.display.print(money);
  gb.display.setColor((Color)0x07E0); gb.display.setCursor(1, 8); gb.display.print(money);
  // "+$X" flottant qui remonte depuis sous la cagnotte vers celle-ci (a chaque gain).
  if (moneyGainTimer > 0) {
    char g[12]; snprintf(g, sizeof(g), "+$%ld", (long)moneyGainAmt);
    int rise = MONEY_GAIN_FRAMES - moneyGainTimer;          // 0..N : il monte
    int gx = 2 + (int)strlen(money) * 4 + 2;                // juste a droite du montant
    int gy = 18 - rise / 2;                                 // de y~18 vers la cagnotte (y~8)
    if (!(moneyGainTimer < 4 && (moneyGainTimer & 1)))       // clignote en fin de course
      printShadowCol(gx, gy, g, 0xFFE0);                    // jaune (billets qui filent)
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

// Mecanisme d'auto-destruction : a 0 PV la caisse prend feu (mèche de ~10 s,
// grosse flamme + fumee), conduisible pendant ce temps. La mèche consommee ->
// explosion (epave + degats de zone) ; le joueur au volant meurt. Appelee chaque
// frame (que le joueur soit dedans ou non : la caisse lancee explose aussi).
static void updateCarFuse() {
  if (carCrashTimer > 0) carCrashTimer--;
  if (carGone) return;
  if (carFuse == 0) {
    if (carHp > 0) return;
    carFuse = CAR_FUSE_FRAMES;                   // 0 PV : la mèche s'allume
    gb.sound.tone(120, 200);
  }
  carFuse--;
  uint16_t period = carFuse > CAR_FUSE_FRAMES / 2 ? 18
                  : (carFuse > CAR_FUSE_FRAMES / 4 ? 9 : 4);  // bips accelerant
  if ((carFuse % period) == 0) gb.sound.tone(1400, 24);
  if (carFuse > 0) return;
  explodeCarAt((int)car.x, (int)car.y);
  spawnWreck(car.x, car.y, (uint8_t)carFrameIdx(), carImpactX, carImpactY);  // ma caisse: epave, pas d'etoiles
  carGone = true; carHp = CAR_MAX_HP; carRunaway = false; carFuse = 0;
  if (driving) startEndSeq(SEQ_WASTED, "MORT", "Hopital", true);  // dedans = mort
}

// Voiture sans conducteur (apres un saut en marche) : continue sur sa lancee,
// freine peu a peu, s'arrete aux murs. Si elle ne brule pas et s'immobilise,
// redevient une caisse garee ordinaire.
static void updateRunawayCar() {
  if (!carRunaway || driving || carGone || seqKind != SEQ_NONE) return;
  float nx = car.x + car.vx, ny = car.y + car.vy;
  if (!carBoxHitsSolid(nx, car.y, CAR_HALF)) car.x = nx; else car.vx = 0.0f;
  if (!carBoxHitsSolid(car.x, ny, CAR_HALF)) car.y = ny; else car.vy = 0.0f;
  car.vx *= CAR_RUNAWAY_FRICTION; car.vy *= CAR_RUNAWAY_FRICTION;
  if (car.vx * car.vx + car.vy * car.vy < 0.02f) {
    car.vx = 0.0f; car.vy = 0.0f;
    if (carFuse == 0) carRunaway = false;        // arretee et pas en feu -> garee
  }
}

// Fumee de la caisse du joueur (au volant OU lancee). Sprite pre-rendu ancre au
// CAPOT (avance (cos,sin) selon l'angle) : palier leger sous ~60 % de PV, dense
// sous ~30 %. En feu (mèche) : grosse flamme + 2e panache qui monte.
static void drawCarSmoke(int camX, int camY) {
  if (carGone) return;
  bool burning = carFuse > 0;
  if (!burning && carHp > (CAR_MAX_HP * 3) / 5) return;     // caisse saine : rien
  // Amochee : elle fume qu'on soit au volant, lancee ou simplement garee a cote.
  int tier = (burning || carHp <= (CAR_MAX_HP * 3) / 10) ? 1 : 0;
  int frame = (missionAnim / (tier ? 3 : 6)) % SMOKE_FRAMES;
  float a = car.angle;
  int hx = (int)(car.x + cosf(a) * SMOKE_HOOD_DIST) - camX;
  int hy = (int)(car.y + sinf(a) * SMOKE_HOOD_DIST) - camY;
  blitSmoke(hx, hy, tier, frame);
  if (burning) {
    blitSmoke(hx, hy - 4, 1, (frame + 2) % SMOKE_FRAMES);   // panache qui monte plus haut
    if (missionAnim & 2) {                                  // lechee de flamme clignotante
      uint16_t fcol = (missionAnim & 4) ? 0xFD20 : 0xFFE0;
      if (hx >= 0 && hx < SCREEN_W && hy >= 0 && hy < SCREEN_H) fb[hy * SCREEN_W + hx] = fcol;
      int hx2 = hx + ((missionAnim & 8) ? 1 : -1);
      if (hx2 >= 0 && hx2 < SCREEN_W && hy >= 0 && hy < SCREEN_H) fb[hy * SCREEN_W + hx2] = fcol;
    }
  }
}

// Fumee des voitures du trafic amochees (pas seulement celle du joueur) : panache
// ancre au centre, palier leger sous ~60 % de PV, dense sous ~30 %.
static void drawAiCarSmoke(int camX, int camY) {
  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (!c.active || c.hp > (CAR_MAX_HP * 3) / 5) continue;
    int tier = (c.hp <= (CAR_MAX_HP * 3) / 10) ? 1 : 0;
    int frame = (missionAnim / (tier ? 3 : 6) + i) % SMOKE_FRAMES;
    blitSmoke((int)c.x - camX, (int)c.y - camY - 2, tier, frame);
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
  if (seqKind == SEQ_CUT) { cutsceneUpdate(); return; }   // cinematique de mission
  if (seqTimer > 0) {
    if (seqKind == SEQ_SPRAY && seqPhase == PH_SPRAY && (seqTimer % 8) == 0)
      gb.sound.tone(2200, 70);                   // pschitt de la bombe de peinture
    if (seqKind == SEQ_HEAL) {                    // jingle de soin ascendant (do-mi-sol)
      if (seqTimer == SEQ_HEAL_FRAMES - 1) gb.sound.tone(523, 90);
      else if (seqTimer == SEQ_HEAL_FRAMES - 10) gb.sound.tone(659, 90);
      else if (seqTimer == SEQ_HEAL_FRAMES - 19) gb.sound.tone(784, 140);
    }
    if (seqKind == SEQ_CRUSH) {
      if ((seqPhase == PH_SWING || seqPhase == PH_CARRY) && (seqTimer % 6) == 0)
        gb.sound.tone(180 + (seqTimer & 31) * 6, 50);  // vrombissement du moteur de grue
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
      if (seqKind == SEQ_SLEEP) {                 // dodo : juste soin + repositionnement
        playerHearts = PLAYER_HEARTS_MAX;         // reveil : 3 coeurs rouges
        respawnAtPoi("Planque");                  // devant la porte, pret a partir
      } else {                                    // mort / arrestation : revie complete
        reviveCommon(); respawnAtPoi(seqPoi);
      }
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
    case PH_SWING:                               // fleche au-dessus de la voiture : on l'agrippe
      seqPhase = PH_CARRY; seqTimer = SEQ_CARRY_FRAMES;
      gb.sound.tone(440, 90); break;             // clac du crochet
    case PH_CARRY:                               // voiture au-dessus du broyeur : on la lache
      seqPhase = PH_CRUSH; seqTimer = SEQ_CRUSH_FRAMES;
      gb.sound.tone(60, 200); break;             // gros craquement
    case PH_CRUSH:                               // broyee : prime (joueur deja a pied)
      addMoney(crushReward);
      carGone = true;
      // OBJ_CRUSH : c'est la fin du broyage qui valide l'objectif (pas l'arrivee).
      if (missionRun.active && curObjs[missionRun.step].type == OBJ_CRUSH) {
        missionCrushDone = true; carIsMission = false;
      }
      narrate("Epave vendue !");
      gb.sound.tone(988, 60); gb.sound.playOK();  // cha-ching
      seqPhase = PH_EJECT; seqTimer = SEQ_EJECT_FRAMES; break;
    case PH_EJECT:                               // prime affichee : on rend la main
      seqKind = SEQ_NONE; seqPhase = 0; break;
  }
}

// Trait epais (2 px) entre deux points ecran, par interpolation entiere. Sert a
// la fleche de la grue (vue de dessus).
static void drawThickLine(int x0, int y0, int x1, int y1, uint16_t col) {
  int dx = x1 - x0, dy = y1 - y0;
  int n = (abs(dx) > abs(dy) ? abs(dx) : abs(dy));
  if (n < 1) n = 1;
  for (int i = 0; i <= n; i++) {
    int x = x0 + dx * i / n, y = y0 + dy * i / n;
    for (int oy = 0; oy <= 1; oy++)
      for (int ox = 0; ox <= 1; ox++) {
        int xx = x + ox, yy = y + oy;
        if (xx >= 0 && xx < SCREEN_W && yy >= 0 && yy < SCREEN_H)
          fb[yy * SCREEN_W + xx] = col;
      }
  }
}

// Petit carre plein cerne (cabine de grue / bouche du broyeur), centre ecran.
static void drawBox(int cx, int cy, int r, uint16_t fill, uint16_t edge) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++) {
      int x = cx + dx, y = cy + dy;
      if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
      bool e = (dx <= -r || dx >= r || dy <= -r || dy >= r);
      fb[y * SCREEN_W + x] = e ? edge : fill;
    }
}

// Grue + broyeur de La Casse dessines EN PERMANENCE (vue de dessus), au repos :
// le bras est parque en pointant la zone de depose et le broyeur reste visible.
// Plante sur la base de grue (tuile junk_crane). Pendant le broyage, c'est
// drawSequence qui anime la grue -> on s'efface pour ne pas dessiner deux fois.
#if CITY_HAS_CASSE
static void drawJunkyardRig(int camX, int camY) {
  if (seqKind == SEQ_CRUSH) return;
  const float SWEEP = 2.3f;                        // meme amplitude que la cinematique
  float px = CITY_CRANE_TX, py = CITY_CRANE_TY;    // pivot (base de grue)
  float dxz = (float)CITY_CASSE_TX - px, dyz = (float)CITY_CASSE_TY - py;
  float L = sqrtf(dxz * dxz + dyz * dyz);
  if (L < 10.0f) L = 10.0f;
  float aZone = atan2f(dyz, dxz);                  // bras au repos -> zone de depose
  float tx = px + L * cosf(aZone), ty = py + L * sinf(aZone);
  float bx = px + L * cosf(aZone + SWEEP), by = py + L * sinf(aZone + SWEEP);  // broyeur
  int pxS = (int)px - camX, pyS = (int)py - camY;
  int txS = (int)tx - camX, tyS = (int)ty - camY;
  int bxS = (int)bx - camX, byS = (int)by - camY;
  drawBox(bxS, byS, 4, 0x18E3, 0xFFE0);            // fosse du broyeur (contour jaune)
  drawThickLine(pxS, pyS, txS, tyS, 0x8410);       // bras parque (gris)
  drawBox(txS, tyS, 1, 0xFD20, 0xFD20);            // crochet
  drawBox(pxS, pyS, 3, 0x4208, 0xFD20);            // cabine (par-dessus la base)
}
#endif

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
  if (seqKind == SEQ_CRUSH) {                      // grue vue de dessus + broyeur
#if CITY_HAS_CASSE
    // Geometrie (px monde) : cabine = pivot, voiture garee = bout de fleche au
    // repos, broyeur = bout de fleche apres un balayage fixe. La fleche pivote
    // de la voiture (PH_SWING) puis revient avec elle vers le broyeur (PH_CARRY).
    const float SWEEP = 2.3f;                      // amplitude du balayage (rad)
    float px = CITY_CRANE_TX, py = CITY_CRANE_TY;  // pivot (cabine)
    float dxc = car.x - px, dyc = car.y - py;
    float L = sqrtf(dxc * dxc + dyc * dyc);
    if (L < 10.0f) L = 10.0f;                       // bras minimal lisible
    float aCar = atan2f(dyc, dxc);                  // angle pivot->voiture
    float aBro = aCar + SWEEP;                      // angle pivot->broyeur
    float bx = px + L * cosf(aBro), by = py + L * sinf(aBro);  // broyeur (monde)
    int pxS = (int)px - camX, pyS = (int)py - camY;            // pivot ecran
    int bxS = (int)bx - camX, byS = (int)by - camY;            // broyeur ecran

    // broyeur (bouche raye jaune/noir) + cabine de la grue : toujours visibles.
    drawBox(bxS, byS, 4, 0x18E3, 0xFFE0);          // fosse sombre, contour jaune
    drawBox(pxS, pyS, 3, 0x4208, 0xFD20);          // cabine gris fonce, contour orange

    if (seqPhase == PH_SWING || seqPhase == PH_CARRY) {
      bool carry = (seqPhase == PH_CARRY);
      int tot = carry ? SEQ_CARRY_FRAMES : SEQ_SWING_FRAMES;
      float prog = (float)(tot - seqTimer) / (float)tot;        // 0..1
      // SWING : du broyeur vers la voiture ; CARRY : de la voiture au broyeur.
      float ang = carry ? (aCar + SWEEP * prog) : (aCar + SWEEP * (1.0f - prog));
      float tx = px + L * cosf(ang), ty = py + L * sinf(ang);   // bout de fleche
      int txS = (int)tx - camX, tyS = (int)ty - camY;
      drawThickLine(pxS, pyS, txS, tyS, 0xFD20);                // la fleche
      if (carry) {                                              // voiture portee
        int idx = (int)(ang / TWO_PI * CAR_FRAMES + 0.5f) % CAR_FRAMES;
        if (idx < 0) idx += CAR_FRAMES;
        blitCar(camX, camY, (int)tx, (int)ty, idx, carColor);
      }
      drawBox(txS, tyS, 1, 0xFD20, 0xFD20);                     // crochet
    } else if (seqPhase == PH_CRUSH) {             // voiture lachee : ecrasee au broyeur
      drawThickLine(pxS, pyS, bxS, byS, 0xFD20);                // fleche au-dessus du broyeur
      int p = SEQ_CRUSH_FRAMES - seqTimer;
      int h = 1 + (7 * seqTimer) / SEQ_CRUSH_FRAMES;
      for (int dy = -h; dy <= h; dy++)
        for (int dx = -5; dx <= 5; dx++) {
          int x = bxS + dx, y = byS + dy;
          if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0x632C;
        }
      for (int k = 0; k < 6; k++) {                // eclats de metal
        int a = p * 5 + k * 41;
        int x = bxS + (a % 15) - 7, y = byS + ((a >> 2) % 9) - 4;
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0xC618;
      }
    } else {                                       // PH_EJECT : prime qui monte
#endif
      char r[10]; snprintf(r, sizeof(r), "+$%ld", (long)crushReward);
      int len = (int)strlen(r), yy = 28 - (SEQ_EJECT_FRAMES - seqTimer);
      gb.display.setColor(BLACK); gb.display.setCursor((SCREEN_W - len * 4) / 2 + 1, yy + 1); gb.display.print(r);
      gb.display.setColor((Color)0x07E0); gb.display.setCursor((SCREEN_W - len * 4) / 2, yy); gb.display.print(r);
    }
    return;
  }
  if (seqPhase == PH_FADE) {                      // ecran noir plein avant la TP
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = 0x0000;
    if (seqKind == SEQ_SLEEP)                      // dodo : petit "Zzz" centre
      printShadowCol((SCREEN_W - 3 * 4) / 2, SCREEN_H / 2 - 3, "Zzz", 0x7BEF);
    return;
  }
  if (seqPhase == PH_MSG && overlayMsg) {         // bandeau mort / arrestation
    uint16_t col = (seqKind == SEQ_WASTED) ? 0xF800 : 0x07FF;  // rouge / cyan
    drawSlideBanner(overlayMsg, col, (int)seqTimer, SEQ_MSG_FRAMES);
  }
  // PH_EXPLODE : rien a ajouter, drawBoom fait le spectacle.
}

// Achat de l'arme surlignee : 1re fois = prix d'arme (+ 1 lot de munitions),
// ensuite = prix de rechargement. Refuse (son grave) si solde insuffisant.
static void shopBuy() {
  if (shopSel == SHOP_ARMOR) {                  // ligne gilet pare-balles
    if (playerHearts >= PLAYER_HP_MAX) { gb.sound.tone(120, 120); return; }  // deja plein
    if (playerMoney < ARMOR_PRICE)     { gb.sound.tone(120, 120); return; }  // trop cher
    playerMoney -= ARMOR_PRICE;
    giveBodyArmor();
    return;
  }
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
  if (gb.buttons.pressed(BUTTON_DOWN) && shopSel < SHOP_ARMOR)       { shopSel++; gb.sound.playTick(); }
  if (gb.buttons.pressed(BUTTON_A)) shopBuy();
  if (gb.buttons.pressed(BUTTON_B) || gb.buttons.pressed(BUTTON_MENU)) {
    shopOpen = false; gb.sound.playCancel();
  }
}

// Rendu plein ecran du magasin : titre, liste d'armes (icone + nom + prix), solde.
static void drawShop() {
  fb = gb.display._buffer;
  for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = 0x0008;   // fond bleu nuit
  printShadow(1, 1, "AMU NATION");
  char money[12];                                  // solde sur la ligne titre, a droite, en vert
  snprintf(money, sizeof(money), "$%ld", (long)playerMoney);
  printShadowCol(SCREEN_W - (int)strlen(money) * 4 - 1, 1, money, 0x07E0);
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
  // Ligne GILET (apres les armes) : icone cœur bleu, prix ou "PLEIN".
  {
    int y = 11 + (WEAPON_COUNT - WEAPON_PISTOL) * 9;
    if (shopSel == SHOP_ARMOR)
      for (int yy = y - 1; yy < y + 7 && yy < SCREEN_H; yy++)
        for (int xx = 0; xx < SCREEN_W; xx++) fb[yy * SCREEN_W + xx] = 0x2945;
    blitIcon5(2, y, ICON_HEART, HEART_ARMOR);
    printShadow(10, y, "GILET");
    char st[8];
    if (playerHearts >= PLAYER_HP_MAX) snprintf(st, sizeof(st), "PLEIN");
    else                               snprintf(st, sizeof(st), "%ld", (long)ARMOR_PRICE);
    printShadow(SCREEN_W - (int)strlen(st) * 4 - 1, y, st);
  }
}

// --- Casino : machine a sous ---------------------------------------------

// Symboles 11x11 en pixel-art multicolore (vrais fruits/cloche/bar/sept au lieu
// d'une lettre). 'x' du motif -> couleur via slotPal ; ' ' = transparent. Source
// de verite : tools/preview_slot.py (rend l'ecran en PNG dans previews/).
static uint16_t slotPal(char c) {
  switch (c) {
    case 'k': return 0x0000;   // contour noir
    case 'r': return 0xE104;   // rouge vif
    case 'd': return 0x8861;   // rouge sombre
    case 'w': return 0xFFFF;   // reflet
    case 'y': return 0xFF00;   // jaune
    case 'o': return 0xFCC0;   // orange
    case 'g': return 0x2E06;   // vert
    case 'G': return 0x1383;   // vert sombre
    case 's': return 0xD69C;   // argent clair
    case 'S': return 0x7BD1;   // argent sombre
    default:  return 0x0000;
  }
}

static const char *const SLOT_SPRITE[SYM_COUNT][11] = {
  { "      gg   ", "     g G   ", "    g  G   ", "   g   G   ", "  g    GG  ",
    " dwd  d  G ", "drrwd dwd  ", "drrrd drrwd", "drrrd drrrd", " ddd  drrrd", "       ddd " },  // CHERRY
  { "           ", "    ooo    ", "  ooyyyo   ", " oyyyyyyo  ", " oyyywyyo  ",
    "oyyywyyyyo ", "oyyyyyyyyo ", " oyyyyyyo  ", " oyyyyyyo  ", "  ooyyoo   ", "    ooo    " },  // LEMON
  { "     k     ", "    kyk    ", "    yyy    ", "   yyyyk   ", "  yyyyyok  ",
    "  ysyyyok  ", " ysyyyyook ", " ysyyyyook ", "kyyyyyyyook", "kkkkbbkkkk ", "    kbbk   " },  // BELL
  { " sSSSSSSSs ", " swwwwwwws ", " sSSSSSSSs ", "           ", " sSSSSSSSs ",
    " swwwwwwws ", " sSSSSSSSs ", "           ", " sSSSSSSSs ", " swwwwwwws ", " sSSSSSSSs " },  // BAR
  { "           ", " rrrrrrrrr ", " rwwwwwddr ", " ddddddrrr ", "      drr  ",
    "     drr   ", "     rr    ", "    drr    ", "    rr     ", "   rr      ", "           " },  // SEVEN
};

// Dessine un symbole 11x11 (halo noir 1px puis remplissage) a (sx,sy) dans fb.
static void blitSlotSym(int sx, int sy, uint8_t sym) {
  static const int8_t nb[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
  const char *const *rows = SLOT_SPRITE[sym];
  for (int ry = 0; ry < 11; ry++)
    for (int rx = 0; rx < 11; rx++) {
      if (rows[ry][rx] == ' ') continue;
      for (int k = 0; k < 4; k++) {
        int x = sx + rx + nb[k][0], y = sy + ry + nb[k][1];
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0x0000;
      }
    }
  for (int ry = 0; ry < 11; ry++)
    for (int rx = 0; rx < 11; rx++) {
      char c = rows[ry][rx];
      if (c == ' ') continue;
      int x = sx + rx, y = sy + ry;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = slotPal(c);
    }
}

// Repliques du videur quand on tente de jouer sans un rond.
static const char *BOUNCER_LINES[6] = {
  "Le videur: pas de fric, pas de jackpot. Dehors!",
  "Le videur: reviens quand t'auras de quoi miser, clodo.",
  "Le videur: ici c'est pas la soupe populaire. Ouste!",
  "Le videur: tu mises avec quoi, des boutons? Dehors!",
  "Le videur: les fauches, c'est sur le trottoir. Allez!",
  "Le videur: la maison gagne, toi tu perds meme l'entree.",
};

// Tentative de jeu sans assez d'argent : le videur te vire. On ferme la machine,
// le joueur se retrouve allonge devant (se releve seul), -1 PV, et une replique.
static void casinoBouncerThrowOut() {
  casinoOpen = false;
  narrate(BOUNCER_LINES[aiRngNext(aiRng) % 6]);
  gb.sound.tone(120, 220);
  hurtPlayer(1, false);          // -1 PV (avant le knockdown, sinon ignore)
  knockdownPlayer(false);        // allonge devant la porte, relevage auto
}

// Amorce un spin : borne la mise au solde, deduit, demarre l'animation. Si le
// joueur est trop fauche pour miser, le videur le sort de force.
static void casinoSpinStart() {
  int32_t maxBet = playerMoney < CASINO_BET_MAX ? playerMoney : CASINO_BET_MAX;
  if (casinoBet > maxBet) casinoBet = maxBet;
  if (casinoBet < 1 || playerMoney < casinoBet) { casinoBouncerThrowOut(); return; }
  addMoney(-casinoBet);                         // depense : pas d'animation HUD
  casinoState   = CASINO_SPINNING;
  casinoSpinTimer = 0;
  casinoStop[0] = 24; casinoStop[1] = 38; casinoStop[2] = 52;  // arrets echelonnes
  casinoLastWin = 0; casinoWinToast = 0;
  gb.sound.playOK();
}

static void updateCasino() {
  // MENU : sortir (comme pour sortir d'une voiture).
  if (gb.buttons.pressed(BUTTON_MENU)) { casinoOpen = false; gb.sound.playCancel(); return; }

  if (casinoState == CASINO_IDLE) {
    int32_t maxBet = playerMoney < CASINO_BET_MAX ? playerMoney : CASINO_BET_MAX;
    if (gb.buttons.repeat(BUTTON_UP, 4)   && casinoBet < maxBet) { casinoBet++; gb.sound.playTick(); }
    if (gb.buttons.repeat(BUTTON_DOWN, 4) && casinoBet > 1)      { casinoBet--; gb.sound.playTick(); }
    if (gb.buttons.pressed(BUTTON_B) && maxBet >= 1)   { casinoBet = maxBet; gb.sound.playTick(); }
    if (gb.buttons.pressed(BUTTON_A))                  casinoSpinStart();
    if (casinoWinToast > 0) casinoWinToast--;
    return;
  }

  // SPINNING : chaque rouleau defile puis se fige a sa frame d'arret.
  casinoSpinTimer++;
  for (int r = 0; r < 3; r++) {
    if (casinoSpinTimer < casinoStop[r]) {
      if ((casinoSpinTimer & 1) == 0) casinoReel[r] = slotNext(&casinoRng);  // defile
    } else if (casinoSpinTimer == casinoStop[r]) {
      casinoReel[r] = slotNext(&casinoRng);     // symbole final
      gb.sound.playTick();
    }
  }
  if (casinoSpinTimer > casinoStop[2]) {        // les 3 sont arretes : on solde
    int32_t win = slotEvaluate(casinoReel[0], casinoReel[1], casinoReel[2], casinoBet);
    casinoLastWin = win;
    if (win > 0) { addMoney(win); casinoWinToast = 120; gb.sound.playOK(); }
    else         { casinoWinToast = 75;             gb.sound.tone(160, 160); }
    casinoState = CASINO_IDLE;
  }
}

// Rectangle plein / contour ecrits directement dans fb (cabine du casino).
static void fbFill(int x, int y, int w, int h, uint16_t col) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      if (xx >= 0 && xx < SCREEN_W && yy >= 0 && yy < SCREEN_H) fb[yy * SCREEN_W + xx] = col;
}
static void fbRect(int x, int y, int w, int h, uint16_t col) {
  fbFill(x, y, w, 1, col); fbFill(x, y + h - 1, w, 1, col);
  fbFill(x, y, 1, h, col); fbFill(x + w - 1, y, 1, h, col);
}

static void drawCasino() {
  fb = gb.display._buffer;
  const uint16_t GOLD = 0xD545, GOLD_D = 0x8B42, GOLD_HI = 0xFF0C, REELBG = 0x1085;
  for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = 0x1801;   // corps maroon sombre

  // Marquee dore en haut (titre + solde poses dessus).
  fbFill(0, 0, SCREEN_W, 7, GOLD_D);
  fbFill(0, 0, SCREEN_W, 6, GOLD);
  fbFill(0, 6, SCREEN_W, 1, GOLD_HI);
  printShadowCol((SCREEN_W - 6 * 4) / 2, 1, "CASINO", 0x8B42);   // brun sombre sur l'or
  char money[12];
  snprintf(money, sizeof(money), "$%ld", (long)playerMoney);
  printShadowCol(SCREEN_W - (int)strlen(money) * 4 - 1, 9, money, 0x07E0);

  // Fenetre des rouleaux : cadre dore en relief + 3 lucarnes sombres.
  const int fx = 4, fy = 16, fw = 72, fh = 19;
  fbFill(fx, fy, fw, fh, GOLD_D);
  fbRect(fx, fy, fw, fh, GOLD_HI);
  for (int r = 0; r < 3; r++) {
    int rx = fx + 3 + r * 23;
    fbFill(rx, fy + 2, 19, fh - 4, REELBG);
    fbRect(rx, fy + 2, 19, fh - 4, GOLD_D);
    blitSlotSym(rx + 4, fy + 4, casinoReel[r]);
  }
  fbFill(fx + 1, fy + fh / 2, fw - 2, 1, 0xC945);   // ligne de paiement rouge
  for (int k = 0; k < 3; k++) {                      // rivets dores lateraux
    int yy = 20 + k * 6;
    fb[yy * SCREEN_W + 1] = GOLD; fb[yy * SCREEN_W + SCREEN_W - 2] = GOLD;
  }

  // Mise
  char bet[16];
  snprintf(bet, sizeof(bet), "MISE $%ld", (long)casinoBet);
  printShadow((SCREEN_W - (int)strlen(bet) * 4) / 2, 38, bet);

  // Resultat du dernier spin (toast)
  if (casinoState == CASINO_IDLE && casinoWinToast > 0) {
    if (casinoLastWin > 0) {
      char w[16]; snprintf(w, sizeof(w), "GAGNE +$%ld", (long)casinoLastWin);
      printShadowCol((SCREEN_W - (int)strlen(w) * 4) / 2, 47, w, 0xFFE0);
    } else {
      printShadowCol((SCREEN_W - 7 * 4) / 2, 47, "PERDU !", 0xF800);
    }
  }

  // Aide en bas
  printShadow(1, 58, "A:spin B:max MENU:sortir");
}

// LEDs RGB de la console (gb.lights = image 2x4, poussee au matos a chaque
// gb.update). Priorite au coup recu (flash rouge) ; sinon, au-dela de 3 etoiles
// de recherche (donc 4* et 5*), elles clignotent rouge/bleu facon girophare,
// que l'on soit a pied ou au volant. Appelee une fois par frame en tete de loop.
static void updateLights() {
  gb.lights.clear();
  if (hitFlashTimer > 0) {                  // coup encaisse : flash rouge (prioritaire)
    hitFlashTimer--;
    gb.lights.fill(RED);
    return;
  }
  if (wanted.level > 3) {                   // recherche elevee (4* / 5*) : girophare LEDs
    bool phase = (missionAnim >> 2) & 1;    // meme cadence que drawGyro
    gb.lights.fill(phase ? (Color)0xF800 : (Color)0x001F);  // rouge / bleu
  }
}

void loop() {
  while (!gb.update());
  updateLights();

  // Magasin ouvert (AMU Nation) : UI modale, monde gele. On traite la nav et on
  // dessine le menu, puis on sort de la frame.
  if (shopOpen) { updateShop(); drawShop(); return; }

  // Casino ouvert : machine a sous modale, monde gele (idem magasin).
  if (casinoOpen) { updateCasino(); drawCasino(); return; }

  // Cinematique en cours (mort / arrestation / repeinture) : le joueur est fige,
  // aucun input ne passe. Le monde (IA, police, fumee...) tourne quand meme,
  // plus bas. Sinon, on lit les commandes normalement.
  if (seqKind != SEQ_NONE) {
    // joueur fige
  } else if (playerDown) {
    updatePlayerDown();                        // au sol : aucun input, on attend le relevage/arrestation
  } else if (!driving && playerJump > 0) {
    // --- SAUT EN COURS (a pied) --- bond tout droit dans le sens du regard ;
    // commandes ignorees, on survole les voitures (cf. playerOverlapsCar).
    playerJump--;
    playerJumpAcc += JUMP_SPEED;
    int step = (int)playerJumpAcc;
    if (step > 0) {
      playerJumpAcc -= step;
      tryMove(playerX, playerY, AI_DX[playerDir] * step, AI_DY[playerDir] * step);  // les murs stoppent quand meme
    }
    if (++animTimer >= ANIM_PERIOD) { animTimer = 0; playerFrame ^= 1; }
    if (playerJump == 0) { playerJumpCool = JUMP_COOLDOWN; playerJumpAcc = 0.0f; }  // atterrissage
    tryPickupWeapons(playerX + PLAYER_W / 2, playerY + PLAYER_H / 2);
    tryPickupLoot(playerX + PLAYER_W / 2, playerY + PLAYER_H / 2);
  } else if (!driving) {
    // --- A PIED ---
    if (playerJumpCool > 0) playerJumpCool--;
    // B : saut (bond en avant) -> on passe au-dessus des voitures.
    if (gb.buttons.pressed(BUTTON_B) && playerJumpCool == 0) {
      playerJump = JUMP_FRAMES; playerJumpAcc = 0.0f;
      gb.sound.tone(440, 30);
    }
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
      // Sur une tuile du POI Casino (la ou le bandeau affiche "Le Casino") ?
      // Ouvre la machine a sous.
      if (!answered && casinoPoiIdx >= 0
          && poiAtTile(pcx >> 3, pcy >> 3) == casinoPoiIdx) {
        casinoOpen = true; casinoState = CASINO_IDLE; casinoWinToast = 0;
        if (casinoBet < 1) casinoBet = 1;
        casinoRng ^= ((uint32_t)missionAnim << 16) ^ ((uint32_t)playerX << 8)
                   ^ (uint32_t)playerY ^ 0x9E3779B9u;   // melange un peu d'entropie
        gb.sound.playOK(); answered = true;
      }
      // Autres POI a "porte" (a pied, A sur la bbox) : Planque (dodo), Le Bar
      // (tournee du poivrot), Les Commerces (braquage).
      if (!answered) {
        int pt = poiAtTile(pcx >> 3, pcy >> 3);
        const char *pn = (pt >= 0) ? cityPois[pt].name : "";
        if (strcmp(pn, "Planque") == 0)        { startSleepSeq(); answered = true; }
        else if (strcmp(pn, "Le Bar") == 0)    { barDrink();      answered = true; }
        else if (strcmp(pn, "Commerces") == 0) { robStore();      answered = true; }
        // Les Bureaux : interaction libre seulement hors mission (la trame Sarah
        // s'y deroule plus tard et exige d'y entrer sans declencher le braquage).
        else if (strcmp(pn, "Les Bureaux") == 0 && !missionRun.active) {
          bureauVisit(); answered = true;
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
      // Cabine rouge de la campagne : decroche -> mission de trame courante.
      if (!answered && !missionRun.active) {
        long dS = (long)(pcx - storyPx[0]) * (pcx - storyPx[0])
                + (long)(pcy - storyPy[0]) * (pcy - storyPy[0]);
        if (dS <= (long)PHONE_REACH * PHONE_REACH) {
          if (campaignStep < STORY_LEN) {
            storyMissionActive = true;
            startMission(STORY_SEQ[campaignStep]);
          } else if (campaignStep == STORY_LEN) {
            // Epilogue : on decroche une derniere fois. Silence... puis la ligne
            // coupe. Le telephone se tait ensuite pour de bon.
            narrate("Tu decroches. Silence au bout du fil... puis la ligne coupe. FIN.");
            campaignStep++;
            gb.sound.playCancel();
          } else {
            narrate("Le telephone reste muet.");
            gb.sound.playOK();
          }
          answered = true;
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
        // Monter dans une AUTRE caisse (mission/volee) coupe une mèche/lancee
        // heritee de l'ancienne. Remonter dans SA propre caisse (best == -1) ne
        // reinitialise rien : PV et feu appartiennent a la voiture (persistants).
        if (best == -3 || best >= 0) { carFuse = 0; carRunaway = false; }
        if (best == -3) {                   // voiture de mission au parking
          car = mCar; car.vx = 0.0f; car.vy = 0.0f;
          carColor = MISSION_CAR_COLOR; carIsMission = true;
          mCarActive = false; driving = true; carHp = CAR_MAX_HP; carGone = false;
        } else if (best == -1) {
          driving = true; carIsMission = false;   // remonter dans sa voiture (PV gardes)
        } else if (best >= 0) {
          AiCar &c = aiCars[best];           // vol : un conducteur (s'il y en a un) tombe au sol puis fuit
          if (c.driver) aiEjectDriver((int)c.x, (int)c.y, true, false, c.dir);  // caisse vide -> personne a ejecter
          car.x = c.x; car.y = c.y; car.vx = 0.0f; car.vy = 0.0f;
          car.angle = AI_CAR_FRAME[c.dir] * (TWO_PI / CAR_FRAMES);
          carColor = c.color; carIsMission = false;
          carHp = c.hp > 0 ? c.hp : CAR_MAX_HP;   // herite de l'usure de la caisse volee
          if (c.isPolice) {                  // voiture de police : gilet enfile + pompe a bord
            giveBodyArmor();
            weaponOwned[WEAPON_SHOTGUN] = true;
            weaponAmmo[WEAPON_SHOTGUN] += WEAPONS[WEAPON_SHOTGUN].ammoPickup;
          }
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
    // Descendre (MENU) : poser le perso sur une case libre a cote. A vitesse,
    // c'est un SAUT en marche : -1 coeur et la caisse continue sur sa lancee
    // (runaway) -> elle explose plus loin si elle est en feu.
    if (gb.buttons.pressed(BUTTON_MENU)) {
      int ox, oy;
      if (findFootSpot((int)car.x, (int)car.y, ox, oy)) {
        playerX = ox; playerY = oy; driving = false;
        playerFrame = 0; animTimer = 0;
        float bail2 = car.vx * car.vx + car.vy * car.vy;
        if (bail2 <= CAR_BAIL_PARK_SPEED2) {
          car.vx = 0.0f; car.vy = 0.0f;       // quasi a l'arret : on se gare proprement
        } else {
          carRunaway = true; gb.sound.tone(300, 70);  // la caisse continue sur sa lancee
          if (bail2 > CAR_BAIL_HURT_SPEED2)
            hurtPlayer(1, false);             // tres vite : ca fait mal (fatal -> mort)
          knockdownPlayer(false);             // saut en marche -> on finit au sol
        }
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
  updateEnemies(focusX, focusY);
  marcoUpdate(focusX, focusY);
  missionProgress();
  if (targetDownTimer > 0) targetDownTimer--;
  missionAnim++;                               // clignotement marqueurs/telephones
  updateMoneyAnim();                           // cagnotte qui roule + "+$X" flottant
  if (missionDoneTimer > 0)   missionDoneTimer--;    // bandeaux d'annonce (non bloquants)
  if (missionFailedTimer > 0) missionFailedTimer--;
  if (weaponToast > 0) weaponToast--;          // toast d'arme : disparait tout seul
  updateBullets();                             // projectiles de tir (visuel)
  updateProjectiles();                         // roquettes/grenades : vol + detonation
  narrUpdate();

  // Recherche police + minuteries de degats/effets ; explosion eventuelle de la
  // voiture pilotee (peut tuer le joueur -> reapparition hopital).
  wantedTick(wanted);
  if (playerHurtTimer > 0) playerHurtTimer--;
  if (boomTimer > 0) boomTimer--;
  if (overlayTimer > 0) overlayTimer--;
  updateRunawayCar();                          // caisse sans conducteur sur sa lancee
  updateCarFuse();                             // mèche -> explosion (mort si dedans)
  updateWrecks(focusX, focusY);                // epaves : petit saut puis despawn au loin
  updateSmoke();                               // panaches de fumee d'explosion (~5 s)
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

  // La Casse : GARER sa caisse (pas la voiture de mission) sur la zone fixe,
  // DESCENDRE et rester a portee -> la grue s'amorce (~1 s) puis broie l'epave
  // contre une prime. Au volant la grue ne demarre pas ; remonter ou s'eloigner
  // pendant l'amorcage l'annule (la grue s'arrete).
#if CITY_HAS_CASSE
  bool casseReady = false;
  // La voiture de mission est broyable UNIQUEMENT pendant un objectif OBJ_CRUSH
  // (on doit l'amener au broyeur) ; sinon, seules les caisses "ordinaires" le sont.
  bool crushObjective = missionRun.active &&
                        curObjs[missionRun.step].type == OBJ_CRUSH;
  if (seqKind == SEQ_NONE && !carGone && !driving && (!carIsMission || crushObjective)) {
    long dcx = (int)car.x - CITY_CASSE_TX, dcy = (int)car.y - CITY_CASSE_TY;
    bool carOnZone = (dcx * dcx + dcy * dcy <= (long)CASSE_REACH * CASSE_REACH);
    long dpx = playerX + PLAYER_W / 2 - CITY_CASSE_TX;
    long dpy = playerY + PLAYER_H / 2 - CITY_CASSE_TY;
    long pr = (long)CASSE_REACH + PLAYER_W;       // joueur "assez pres" de la zone
    bool playerNear = (dpx * dpx + dpy * dpy <= pr * pr);
    casseReady = carOnZone && playerNear;
  }
  if (casseReady) {
    if (casseArm == 0) gb.sound.tone(120, 80);    // la grue s'amorce (reveil)
    if (++casseArm >= CASSE_ARM) { casseArm = 0; startCrushSeq(); }
  } else {
    casseArm = 0;                                 // hors conditions -> grue arretee
  }
#endif

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

  // Commissariat : entrer (a pied ou en caisse) avec au moins une etoile ->
  // arrestation directe (meme cinematique que dans la rue). Une fois par entree.
  bool onComm = false;
  if (seqKind == SEQ_NONE && !playerDown) {
    int cxw = driving ? (int)car.x : playerX + PLAYER_W / 2;
    int cyw = driving ? (int)car.y : playerY + PLAYER_H / 2;
    int pt = poiAtTile(cxw >> 3, cyw >> 3);
    onComm = (pt >= 0 && strcmp(cityPois[pt].name, "Commissariat") == 0);
  }
  if (onComm && !commInside && wanted.level >= 1) bustedPlayer();
  commInside = onComm;

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
#if CITY_HAS_CASSE
  drawJunkyardRig(camX, camY);                  // grue + broyeur permanents (La Casse)
#endif
  drawCasseZone(camX, camY);                    // zone de broyage (La Casse)
  drawWrecks(camX, camY);                       // epaves fumantes (sous le trafic)
  aiDraw(camX, camY);
  drawWeaponPickups(camX, camY);
  drawLoot(camX, camY);
  drawMissionCar(camX, camY);
  drawMarco(camX, camY);
  drawTarget(camX, camY);
  drawEnemies(camX, camY);
  drawPhones(camX, camY);
  drawMarker(camX, camY);
  drawCar(camX, camY);
  if (!driving) drawPlayer(camX, camY);
  if (!driving && punchTimer > 0) { blitAttackFx(camX, camY); punchTimer--; }
  if (copHitTimer > 0) { blitCopHitFx(camX, camY); copHitTimer--; }   // eclat "coup recu"
  drawBullets(camX, camY);                     // pixels de tir par-dessus la scene
  drawProjectiles(camX, camY);                 // roquettes/grenades en vol
  drawSmoke(camX, camY);                        // panaches de fumee d'explosion
  drawCarSmoke(camX, camY);                    // fumee si ma caisse est amochee
  drawAiCarSmoke(camX, camY);                   // fumee des voitures du trafic amochees
  drawBoom(camX, camY);                        // explosion de voiture

  // HUD : nom du POI courant (haut), fleche vers l'objectif/cible, narration.
  drawPoiHud(focusX, focusY);
  drawMissionArrow(camX, camY, focusX, focusY);
  drawMissionStatus();
  drawTopHud();                                // barre stats : cœurs, etoiles, arme
  narrDraw();
  if (missionDoneTimer > 0)                    // bandeau "mission accomplie" (monte du bas)
    drawSlideBanner("MISSION ACCOMPLIE", 0x07E0, missionDoneTimer, MISSION_DONE_FRAMES);
  else if (missionFailedTimer > 0)             // bandeau "mission ratee"
    drawSlideBanner("MISSION RATEE", 0xF800, missionFailedTimer, MISSION_FAIL_FRAMES);
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
