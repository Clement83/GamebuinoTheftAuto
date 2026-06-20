#pragma once
// game_state.h - types, donnees et etat global du jeu (extrait de gta.ino).
// Inclus une seule fois par gta.ino, AVANT game_decls.h et les modules.
// Tout reste 'static' : une seule unite de compilation, aucun extern.


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

// --- animation de la cagnotte : ni gain ni depense ne saute d'un coup.
//     moneyShown rattrape playerMoney en "roulant" dollar par dollar, et un
//     "+$X" flottant remonte vers la cagnotte (gain) tandis qu'un "-$X" rouge
//     tombe sous elle (depense). Generique : tout passe par addMoney(). Les
//     depenses roulent plus vite que les gains (l'argent file). ---
static int32_t moneyShown    = 0;     // $ affiche dans le HUD (suit playerMoney)
static int32_t moneyRollStep = 1;     // $/frame du compteur qui tourne
static int32_t moneyGainAmt  = 0;     // montant du dernier "+$X" flottant
static uint16_t moneyGainTimer = 0;   // >0 : "+$X" en cours de montee
static int32_t moneyLossAmt  = 0;     // montant (positif) du dernier "-$X" flottant
static uint16_t moneyLossTimer = 0;   // >0 : "-$X" en cours de chute
static const uint16_t MONEY_GAIN_FRAMES = 24;   // ~1 s : duree du "+$X"/"-$X" flottant
static const int32_t  MONEY_ROLL_FRAMES = 50;   // ~2 s : duree cible du compteur (gain)
static const int32_t  MONEY_LOSS_FRAMES = 18;   // ~0.7 s : compteur qui chute (depense)

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
  { OBJ_GOTO, 0, 0, 16, true,  EV_DELIVERY,     "Les Quais",
    "En route pour les Quais. Roule peinard, attire pas les flics.",
    "Colis livre. Marco : nickel. Maintenant ramene-moi chez moi, petit." },
  { OBJ_GOTO, 0, 0, 14, true,  EV_MARCO_LEAVE,  "Le Garage",
    "Ramene Marco au Garage.",
    nullptr },
};
// M2 : tournee de racket AVEC Marco compagnon a pied. On va le chercher au
// Garage (TALK -> il suit), on encaisse (collecte scriptee EV_DELIVERY aux stops
// cooperatifs ; tuer un client = echec), puis on le RAMENE au Garage
// (EV_MARCO_LEAVE). Marco invulnerable (pas de failOnAllyDeath) : il ne meurt
// qu'en M4.
static const Objective OBJS_M2[] = {
  { OBJ_GOTO,   0, 0, 12, false, EV_NONE,       "Le Garage",
    "Jour de tournee. Marco t'attend au Garage. Vas-y a pied le chercher.", nullptr },
  { OBJ_TALK,   0, 0,  8, false, EV_MARCO_JOIN, "Le Garage",
    "Marco : deux secondes petit, j'arrive !",
    "Marco : la tournee du loyer. Tu regardes et t'apprends. Suis-moi." },
  { OBJ_GOTO,   0, 0, 14, false, EV_DELIVERY,   "Commerces",
    "Premier client : les Commerces. Regarde Marco faire.",
    "Marco : tu vois ? Facile. ...Le suivant, lui, fait le difficile." },
  { OBJ_GOTO,   0, 0, 14, false, EV_NONE,       "Les Bureaux",
    "Le difficile tient un bureau aux Bureaux. Rejoins-le avec Marco.", nullptr },
  { OBJ_SUBDUE, 0, 0,  0, false, EV_NONE,       "Les Bureaux",
    "Ce gerant-la refuse et te saute dessus. Mate-le, mais le tue pas.",
    "Il crache l'argent. Marco : voila comment on fait.", 3, 0 },
  { OBJ_GOTO,   0, 0, 14, false, EV_DELIVERY,   "Le Bar",
    "Encore un : le vieux du Bar paie toujours rubis sur l'ongle.",
    "Le vieux paie et t'offre un verre. Ca requinque : pleine forme !" },
  { OBJ_GOTO,   0, 0, 14, false, EV_NONE,       "Chantier",
    "Dernier client, au Chantier. Marco : celui-la... je le sens pas. Avance, doucement.",
    nullptr },
  { OBJ_BEAT,   0, 0,  0, false, EV_NONE,       "Chantier",
    "Un type t'attend, plante entre deux gros bras.",
    "Marco : quelqu'un nous a vendus. On reglera ca.", 0, 0, 3, EK_THUG, SP_PRESENT, 1 },
  { OBJ_GOTO,   0, 0, 14, false, EV_MARCO_LEAVE,"Le Garage",
    "Tournee finie. Ramene Marco au Garage.", nullptr },
};
// M3 : Marco veut faire un exemple d'un mauvais payeur. Comme M1/M2 (Acte I), on
// va CHERCHER Marco au Garage (TALK -> il suit a pied, en retrait), confrontation
// a Chinatown (le payeur et ses gros bras : scene de face-a-face, cf.
// ambushTauntLines), on ecarte les hommes, le payeur detale, on l'acheve, puis on
// RAMENE Marco (EV_MARCO_LEAVE). Marco invulnerable (pas de failOnAllyDeath).
static const Objective OBJS_M3[] = {
  { OBJ_GOTO, 0, 0, 12, false, EV_NONE,       "Le Garage",
    "Un mauvais payeur fait le mort. Marco veut un exemple. Va le chercher au Garage.",
    nullptr },
  { OBJ_TALK, 0, 0,  8, false, EV_MARCO_JOIN, "Le Garage",
    "Marco : deux secondes petit, j'arrive !",
    "Marco : ce gars-la rit de nous depuis trop longtemps. Suis-moi." },
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE,       "Chinatown",
    "Le payeur se planque a Chinatown, entoure de ses gros bras. Approche.",
    nullptr },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE,       "Chinatown",
    "Ecarte ses hommes de main.", "La voie est libre. Reste le payeur.",
    0, 0, 2, EK_THUG, SP_PRESENT, 1 },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE,       "Chinatown",
    "Il detale ! Rattrape-le.",
    "Dette reglee. Marco : il s'en souviendra... s'il s'en souvient encore.", 1, 0 },
  { OBJ_GOTO, 0, 0, 14, false, EV_MARCO_LEAVE,"Le Garage",
    "C'est fait. Ramene Marco au Garage.", nullptr },
};
// --- Trame Acte II : la vengeance manipulee (les Loups) ---
static const Objective OBJS_M5[] = {
  { OBJ_GOTO,   0, 0, 12, false, EV_NONE, "Le Bar",
    "Inconnu : tu veux savoir pour Marco ? Trouve Nico. Il traine au Bar.",
    nullptr },
  { OBJ_SUBDUE, 0, 0,  0, false, EV_NONE, "Le Bar",
    "Nico crane devant ses deux copains de comptoir et te bouscule. Secoue-le, sans le tuer.",
    "Ses copains se sont debines. Nico : les Loups cherchent un type depuis des semaines... c'est eux.",
    3, 0 },
};
static const Objective OBJS_M6[] = {
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE, "Chinatown",
    "Inconnu : envoie-leur un message. Va dans le quartier des Loups.",
    "Les voila, accoudes au mur. Ils t'ont vu." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Chinatown",
    "Mets trois Loups au tapis. Qu'ils comprennent.",
    "Ca devrait attirer leur attention. Inconnu : bien joue.",
    0, 0, 3, EK_THUG, SP_PRESENT, 1 },
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
  { OBJ_GOTO,      0, 0, 18, true,  EV_DELIVERY, "Commissariat",
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
  { OBJ_GOTO, 0, 0, 14, false, EV_CLIENT, "Commerces",
    "Tony : reprends la tournee de Marco aux Commerces. C'est toi, le patron, maintenant.",
    "Un commercant : c'est toi qui passes, desormais ? ...Desole, pour Marco." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Commerces",
    "Des Loups sont venus se servir. Renvoie-les chez eux.",
    "Pour toi, Marco.", 0, 0, 3, EK_THUG, SP_PRESENT },
  { OBJ_GOTO, 0, 0, 14, false, EV_CLIENT, "Le Bar",
    "Le vieux du Bar a vu quelque chose. Va l'ecouter.",
    "Le vieux : un type chic donnait des ordres aux Loups... Va savoir qui." },
};
static const Objective OBJS_M10[] = {
  { OBJ_GOTO, 0, 0, 16, false, EV_NONE, "Les Quais",
    "Tony : on a trouve le stock des Loups, aux entrepots des Quais.",
    "Des gardes verrouillent l'entree." },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Les Quais",
    "Force le passage : deux gardes armes a la porte.", "Entree degagee.",
    0, 0, 2, EK_GUNNER, SP_PRESENT, 1 },
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
    "Les nettoyeurs sont a terre. La mallette est restee pres du corps.",
    0, 0, 2, EK_GUNNER, SP_AMBUSH },
  { OBJ_GOTO, 0, 0, 10, false, EV_NONE, "Chinatown",
    "Marche jusqu'au corps et empoigne la mallette.",
    "Tu empoignes la mallette. Maintenant, file a la Planque." },
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
  { OBJ_GOTO, 0, 0, 16, true,  EV_NONE,        "Chinatown",
    "Prends une caisse et file vers la planque en passant par Chinatown.",
    "Une berline pile en travers de la route. Embuscade !" },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE,        "Chinatown",
    "Les hommes de Victor ont repere Sarah ! Ecarte-les.",
    "La voie est libre. Sarah, tassee sur le siege : roule, roule !",
    0, 0, 2, EK_GUNNER, SP_AMBUSH },
  { OBJ_GOTO, 0, 0, 16, true,  EV_NONE,        "Planque",
    "Reprends la route vers la planque, vite !",
    "Sarah est a l'abri. Pour l'instant." },
};
static const Objective OBJS_M14[] = {
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Planque",
    "Sarah : ils nous ont trouves ! Reviens a la planque, vite !",
    "Les hommes de Victor encerclent la planque." },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Planque",
    "Defends Sarah : premiere vague !", "Ils refluent... non, d'autres arrivent !",
    0, 0, 3, EK_THUG, SP_PRESENT, 1 },
  { OBJ_BEAT, 0, 0,  0, false, EV_NONE, "Planque",
    "Des tireurs, cette fois. Tiens bon !",
    "Sarah : je sais ou il garde les dossiers. Ses Bureaux.", 0, 0, 2, EK_GUNNER, SP_AMBUSH },
};
static const Objective OBJS_M15[] = {
  { OBJ_GOTO, 0, 0, 14, false, EV_NONE, "Les Bureaux",
    "Les Bureaux de Victor. Les preuves sont a l'interieur. Entre.",
    "Des gardes patrouillent le hall." },
  { OBJ_KILL, 0, 0,  0, false, EV_NONE, "Les Bureaux",
    "Neutralise les gardes du hall.", "Hall degage. Les dossiers sont dans le coffre.",
    0, 0, 2, EK_GUNNER, SP_PRESENT, 1 },
  { OBJ_GOTO, 0, 0, 10, false, EV_NONE, "Les Bureaux",
    "Rafle les dossiers de Victor.", "Tu rafles les dossiers. Maintenant, sors." },
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
  { "Les assurances",   OBJS_M2, 9, 150, true },   // index 17 = M2 (trame ; Marco compagnon a pied)
  { "Mauvaise dette",   OBJS_M3, 6, 180, true },   // index 18 = M3 (trame ; Marco compagnon a pied)
  { "Un nom",           OBJS_M5, 2, 200, true },   // index 19 = M5 (trame)
  { "Message aux Loups",OBJS_M6, 2, 250, true },   // index 20 = M6 (trame)
  { "Voiture volee",    OBJS_M7, 4, 350, true, true },   // index 21 = M7 (trame ; caisse des Loups = vehicule requis)
  { "Represailles",     OBJS_M8, 3, 350, true, false, true },   // index 22 = M8 (trame ; Tony defendu)
  { "Tournee de Marco", OBJS_M9, 3, 300, true },   // index 23 = M9 (trame)
  { "L'entrepot",       OBJS_M10,3, 400, true },   // index 24 = M10 (trame)
  { "Rico le Loup",     OBJS_M11,3, 450, true },   // index 25 = M11 (trame, boss)
  { "La mallette",      OBJS_M12,4, 350, true },   // index 26 = M12 (trame ; ramassage mallette scripte)
  { "Le temoin",        OBJS_M13,5, 400, true },   // index 27 = M13 (trame, escorte + embuscade en route)
  { "Embuscade",        OBJS_M14,3, 450, true, false, true },   // index 28 = M14 (trame ; Sarah defendue)
  { "Les dossiers",     OBJS_M15,4, 500, true },   // index 29 = M15 (trame ; ramassage dossiers scripte)
  { "Sabotage",         OBJS_M16,4, 550, true, true },   // index 30 = M16 (caisses de luxe = vehicules requis)
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


// --- Pay'n'Spray : VRAIS garages (art du Garage de Marco) tamponnes dans la
//     map par tools/pois.py. citySprays[] = les cases gar_door (porte
//     carrossable, deja non-solide, route au sud). On entre EN VOITURE par la
//     porte, on s'ARRETE dans le garage -> repeinture + recherche a zero (debit
//     $) + la caisse RESSORT seule sur la route. Le Garage de Marco en fait
//     partie (Pay'n'Spray normal, fixe).
static const int NUM_SPRAYS = CITY_NUM_SPRAYS;
static const int32_t SPRAY_COST = 50;   // $ preleve si on en a (sinon gratuit)
static const float SPRAY_STOP_SPEED2 = 0.10f;  // (px/frame)^2 : "a l'arret" dans le garage
static bool sprayInside = false;         // repeinture deja lancee durant ce passage sur la porte (anti-repeat ; reset en quittant la porte)
static int8_t sprayActive = -1;          // index du garage en cours de repeinture (sortie auto)

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

// --- Profils de sauvegarde (3 slots independants) ---------------------------
// On stocke un blob SaveProfile par slot (indices gb.save 0..2). La logique
// pure (validite / pack / apply) vit dans save.h ; ici c'est la glue I/O et le
// petit ecran de selection au boot (cf. updateProfileMenu / drawProfileMenu).
static const uint8_t NUM_PROFILES = 3;
static uint8_t currentProfile = 0;        // slot actif (0..2), choisi au menu
static bool    inMenu = true;             // ecran de selection au boot (modal)
static uint8_t menuSel = 0;               // curseur du menu (0..NUM_PROFILES-1)
static bool    menuConfirmDelete = false; // B presse une fois : confirme l'effacement

// Copie runtime des objectifs de la mission active : les coords des objectifs
// references par un POI (champ `poi`) sont resolues ici au lancement, car la
// carte (donc les POI) change a chaque regeneration. curDef pointe sur curObjs.
static const int MAX_OBJS = 10;
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
enum { CUT_NONE = 0, CUT_MARCO_DEATH = 1, CUT_TAUNT = 2, CUT_MARCO_LEAVE = 3, CUT_DELIVERY = 4, CUT_BOSS_DOWN = 5 };
static uint8_t  cutKind  = CUT_NONE;
static uint8_t  cutPhase = 0;
static uint16_t cutTimer = 0;
static const char *cutLine1 = nullptr, *cutLine2 = nullptr;  // repliques (TAUNT, contact de scene)
static const char *cutEndText = nullptr;                     // narration jouee a la fin d'un CUT_DELIVERY
static int16_t marcoLeaveX = 0, marcoLeaveY = 0;             // porte vers laquelle Marco rentre (CUT_MARCO_LEAVE)
static const uint16_t CUT_LINE_FRAMES  = 80;   // ~3 s par replique
static const uint16_t CUT_SHOOT_FRAMES = 26;   // temps sur le corps de Marco

// --- PNJ de SCENE scriptee : un contact qui ATTEND a une destination (le docker
//     des Quais en M1, un receleur, un commercant...). Pose a l'activation de
//     l'objectif EV_DELIVERY ; le compagnon va lui parler (CUT_DELIVERY). Le tuer
//     (l'ecraser) echoue la mission. Entite dediee (n'entre pas en conflit avec
//     target/marco). ---
static bool    sceneNpcActive = false;
static bool    sceneNpcDead   = false;   // ecrase par le joueur -> echec
static bool    sceneSolo      = false;   // scene SANS compagnon (Acte II+) : c'est le contact qui vient au vehicule
static float   sceneHomeX = 0.0f, sceneHomeY = 0.0f;  // point de retour du compagnon (voiture si au volant, sinon joueur)
static float   sceneNpcX = 0.0f, sceneNpcY = 0.0f;
static uint8_t sceneNpcDir = DIR_SOUTH, sceneNpcFrame = 0, sceneNpcAnimTimer = 0;
static const uint16_t SCENE_NPC_COLOR = 0xCE59;  // tan : PNJ contact neutre

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
static uint16_t targetStunTimer = 0;      // >0 : cible/boss au sol (frappe encaissee), ni poursuite ni attaque

// --- Ennemis scenarises (gardes, assaillants) : pool d'entites AGRESSIVES posees
//     par un objectif (enemyCount>0). Contrairement aux passants, ils foncent sur
//     le joueur et le frappent (ou tirent), et SEULS comptent pour l'objectif (un
//     passant tue n'avance plus jamais une mission). cf. mission.h. ---
enum { EN_IDLE = 0, EN_AGGRO = 1, EN_DOWN = 2, EN_STAGGER = 3 };   // phase d'un ennemi (EN_STAGGER : au sol, se relevera)
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
// Videurs du casino : si on se presente arme a la porte, 2 gros bras nous
// sortent. Ce drapeau garde ces ennemis VIVANTS hors mission (cf. updateEnemies,
// qui despawn sinon tout ennemi des que missionRun est inactif).
static bool bouncerBrawl = false;
static const uint16_t ENEMY_COLOR_THUG   = 0xFA20;  // orange-rouge (gros bras)
static const uint16_t ENEMY_COLOR_GUNNER = 0xF800;  // rouge vif (tireur arme)
static const int   ENEMY_AGGRO_RANGE  = 56;   // px : distance d'eveil (IDLE -> AGGRO)
static const float ENEMY_SPEED        = 0.45f;// px/frame (un poil < joueur a pied)
static const int   ENEMY_MELEE_DIST   = 8;    // px : portee du coup au corps-a-corps
static const uint16_t ENEMY_MELEE_PERIOD = 26;// frames entre deux coups (~1 s)
static const int   ENEMY_MELEE_DMG    = 1;    // coeurs perdus par coup
static const uint8_t ENEMY_MELEE_MISS_PCT = 90;// % de coups au corps-a-corps qui ratent (cogne dans le vide) -> 10% touchent
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
// --- Allie DEFENDU (missions de defense : Tony M8, Sarah M14). Reutilise
//     l'entite allie (marcoX/Y/dir/frame) en mode STATIONNAIRE : il reste sur le
//     lieu, les ennemis les plus proches de lui le prennent pour cible, et le
//     joueur doit s'interposer. S'il tombe -> echec (failOnAllyDeath).
static bool    allyStands = false;   // allie stationnaire present sur le lieu
static bool    allyDead   = false;   // l'allie est tombe -> echec a la prochaine frame (missionProgress)
static uint8_t allyHp     = 0;       // PV de l'allie defendu
static const uint8_t ALLY_HP = 5;    // coups encaisses avant de tomber (laisse le temps de s'interposer)
static const uint16_t TONY_COLOR = 0x07E0;  // vert (Tony, patron du Garage)
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

// Arrestation (SEQ_BUSTED) : amende police FIXE selon les etoiles au moment des
// faits. Index = wanted.level (0..WANTED_MAX).
static const int32_t BUST_FINE[WANTED_MAX + 1] = { 0, 100, 250, 450, 700, 1000 };
// Mort (SEQ_WASTED) : facture d'hopital fixe, independante des etoiles (on peut
// mourir sans flics).
static const int32_t HOSP_BILL = 100;

// --- Videurs du casino : entrer ARME a la porte = expulsion musclee ----------
// Punch lines GTA, ton rigolard. Tirees au hasard a chaque expulsion.
static const char *BOUNCER_ARMED_LINES[6] = {
  "Le videur: range ton flingue, ducon, ici on parie pas sa peau!",
  "Le videur: une arme au casino? La maison aime pas. Dehors!",
  "Le videur: tu comptes braquer la banque tout seul? Mignon. Sors.",
  "Le videur: pas d'acier sur le tapis vert. On va t'apprendre les manieres.",
  "Le videur: le seul pigeon qu'on plume ici, c'est l'arme au poing.",
  "Le videur: ton calibre reste au vestiaire... ou c'est toi qu'on plie.",
};

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

// Motifs 5x5 (cœur, etoile).
static const char *const ICON_HEART[5] = { "x x x", "xxxxx", "xxxxx", " xxx ", "  x  " };
static const char *const ICON_STAR[5]  = { "  x  ", " xxx ", "xxxxx", " xxx ", " x x " };

// Bornes d'affichage (la valeur courante vient de playerHearts / wanted.level).
static const int HUD_HEARTS_MAX = PLAYER_HEARTS_MAX;   // vie : 3 cœurs
static const int HUD_STARS_MAX  = WANTED_MAX;          // recherche police : 5 etoiles
static const uint16_t HEART_FULL = 0xF800, HEART_EMPTY = 0x4208;  // rouge / gris sombre
static const uint16_t HEART_ARMOR = 0x04FF;                       // bleu : cœur protege par le gilet
static const uint16_t STAR_FULL  = 0xFFE0, STAR_EMPTY  = 0x4208;  // jaune / gris sombre

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

// Repliques du videur quand on tente de jouer sans un rond.
static const char *BOUNCER_LINES[6] = {
  "Le videur: pas de fric, pas de jackpot. Dehors!",
  "Le videur: reviens quand t'auras de quoi miser, clodo.",
  "Le videur: ici c'est pas la soupe populaire. Ouste!",
  "Le videur: tu mises avec quoi, des boutons? Dehors!",
  "Le videur: les fauches, c'est sur le trottoir. Allez!",
  "Le videur: la maison gagne, toi tu perds meme l'entree.",
};
