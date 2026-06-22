// GTA demake -- sketch Gamebuino META (M4 + voiture).
// Pieton qui se deplace a la croix dans la ville scrollante, collision +
// camera clampee. Une voiture conduisible (physique arcade + drift) est garee
// a cote du spawn : MENU pour monter/descendre (meme bouton). Rendu pixel par
// pixel (drawPixel) depuis des tableaux RGB565 en flash ; la voiture est un
// rectangle oriente (angle continu) trace a la volee, sans sprite.
//
// A pied  : croix = deplacer, MENU = entrer/interagir (voiture, batiment-POI,
//           telephone ; rien a portee -> change d'arme), A = attaquer, B = saut.
// Au volant : A = accelerer, B = freiner (puis marche arriere a l'arret),
//             GAUCHE/DROITE = braquer, MENU = descendre (meme bouton qu'a pied).
//             Drift auto : freiner en virage a vitesse part en glisse.
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
#include "truck.h"
#include "smoke.h"
#include "player.h"
#include "ai.h"
#include "combat.h"
#include "mission.h"
#include "weapons.h"
#include "weapons_gfx.h"
#include "wanted.h"
#include "slot.h"
#include "save.h"
#include "music.h"

#include "game_state.h"
#include "game_decls.h"


void setup() {
  gb.begin();
  SerialUSB.begin(9600);
  playerX = PLAYER_START_X * TILE_W;
  playerY = PLAYER_START_Y * TILE_H;
  playerDir = PLAYER_START_DIR;
  playerFrame = 0;
  animTimer = 0;
  gb.sound.setVolume(2);                       // 0-8 (defaut 6) : adoucit la BO (et un poil les SFX)
  musicStart();                                // boucle de fond chiptune (cf. mod_music.h)

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

  // Pay'n'Spray : citySprays[] = cases gar_door (deja dans la grille). Aucune
  // position runtime a calculer ; le declenchement se fait par la tuile.
  sprayInside = false;
  sprayActive = -1;

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
  bouncerBrawl = false;
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
  moneyShown = playerMoney; moneyRollStep = 1;
  moneyGainAmt = 0; moneyGainTimer = 0; moneyLossAmt = 0; moneyLossTimer = 0;
  for (int i = 0; i < NUM_LOOT; i++) loots[i].active = false;
  for (int i = 0; i < NUM_BULLETS; i++) bullets[i].active = false;
  for (int i = 0; i < NUM_PROJ; i++) projs[i].active = false;
  lootNext = 0;

  // Missions au repos : les telephones sonnent quand on s'en approche.
  missionRun.active = false;
  target.active = false;
  targetDownTimer = 0;
  targetStunTimer = 0;
  mCarActive = false; carIsMission = false;
  marcoWaiting = false; marcoFollow = false; marcoAboard = false;
  killerChase = false;
  campaignStep = 0; storyMissionActive = false; missionFailedTimer = 0;  // trame remise a zero
  missionDoneTimer = 0;
  narrHead = 0; narrCount = 0; narrTimer = 0;
}


void loop() {
  while (!gb.update());
  updateLights();
  musicTick();                                 // avance la bande son (joue partout, menus compris)

  // Ecran de selection de profil au demarrage (modal) : on choisit/charge un
  // des 3 profils avant que le monde ne tourne. Sortie du menu -> jeu normal.
  if (inMenu) { updateProfileMenu(); drawProfileMenu(); return; }

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
    // Ramassage d'une arme au sol (en marchant dessus).
    tryPickupWeapons(playerX + PLAYER_W / 2, playerY + PLAYER_H / 2);
    tryPickupLoot(playerX + PLAYER_W / 2, playerY + PLAYER_H / 2);
    // A : attaquer (poing / arme courante).
    if (gb.buttons.pressed(BUTTON_A)) tryAttack();
    // MENU ("select") : ENTRER / interagir -- MEME bouton que pour SORTIR de la
    // voiture. Decroche un telephone qui sonne, ouvre un batiment-POI
    // (armurerie, casino, Planque, Bar, Commerces, Bureaux) ou monte dans la
    // voiture la plus proche a portee. Rien a portee -> change d'arme (boucle).
    // best : -3 voiture de mission, -2 aucune, -1 voiture joueur, >=0 IA.
    if (gb.buttons.pressed(BUTTON_MENU)) {
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
      // Arme au poing -> les videurs te sortent (punch line + baston). Mains
      // nues -> on ouvre la machine a sous.
      if (!answered && casinoPoiIdx >= 0
          && poiAtTile(pcx >> 3, pcy >> 3) == casinoPoiIdx) {
        if (curWeapon != WEAPON_FIST) {                  // arme visible : expulsion
          narrate(BOUNCER_ARMED_LINES[aiRngNext(aiRng) % 6]);
          gb.sound.tone(110, 140); gb.sound.tone(80, 120);  // "dehors!"
          spawnCasinoBouncers(pcx, pcy);
        } else {
          casinoOpen = true; casinoState = CASINO_IDLE; casinoWinToast = 0;
          if (casinoBet < 1) casinoBet = 1;
          casinoRng ^= ((uint32_t)missionAnim << 16) ^ ((uint32_t)playerX << 8)
                     ^ (uint32_t)playerY ^ 0x9E3779B9u;   // melange un peu d'entropie
          gb.sound.playOK();
        }
        answered = true;
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
            // Epilogue : on decroche une derniere fois -- la voix de Sarah, la
            // journaliste sauvee, qui remercie. Puis la ligne coupe pour de bon.
            narrate("Sarah : c'est sorti ce matin. Tout le monde sait, pour Victor. Pour Marco.");
            narrate("Sarah : t'aurais pu finir comme lui. Prends soin de toi, petit. *clic* FIN.");
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
          carColor = MISSION_CAR_COLOR; carIsMission = true; drivingTruck = false;
          mCarActive = false; driving = true; carHp = CAR_MAX_HP; carGone = false;
        } else if (best == -1) {
          driving = true; carIsMission = false; drivingTruck = false;  // sa propre voiture : jamais un camion
        } else if (best >= 0) {
          AiCar &c = aiCars[best];           // vol : un conducteur (s'il y en a un) tombe au sol puis fuit
          if (c.driver) aiEjectDriver((int)c.x, (int)c.y, true, false, c.dir);  // caisse vide -> personne a ejecter
          car.x = c.x; car.y = c.y; car.vx = 0.0f; car.vy = 0.0f;
          car.angle = AI_CAR_FRAME[c.dir] * (TWO_PI / CAR_FRAMES);
          carColor = c.color; carIsMission = false;
          drivingTruck = c.isTruck; drivingVariant = c.variant;
          carHp = c.hp > 0 ? c.hp : CAR_MAX_HP;   // herite de l'usure de la caisse volee
          if (c.isPolice) {                  // voiture de police : gilet enfile + pompe a bord
            giveBodyArmor();
            weaponOwned[WEAPON_SHOTGUN] = true;
            weaponAmmo[WEAPON_SHOTGUN] += WEAPONS[WEAPON_SHOTGUN].ammoPickup;
          }
          c.active = false;                  // la voiture quitte le pool IA
          driving = true; carGone = false;
        }
        if (best != -2) answered = true;     // monte dans une caisse -> interaction faite
      }
      if (!answered) {                        // rien a portee : changer d'arme (boucle)
        uint8_t w = weaponCycleNext(weaponOwned, curWeapon);
        if (w != curWeapon) { curWeapon = w; weaponToast = WEAPON_TOAST_FRAMES; }
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
        if (marcoAboard) {            // le compagnon descend AVEC le joueur
          marcoAboard = false; marcoFollow = true;
          int mx, my;
          if (findFootSpot((int)car.x, (int)car.y, mx, my)) {
            marcoX = (float)(mx + PLAYER_W / 2); marcoY = (float)(my + PLAYER_H / 2);
          } else { marcoX = car.x; marcoY = car.y; }
          marcoDir = DIR_SOUTH; marcoFrame = 0; marcoAnimTimer = 0;
        }
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
  if (bouncerBrawl && enemiesAliveCount() == 0) bouncerBrawl = false;  // videurs au tapis : fin du brawl
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

  // Pay'n'Spray : entrer en voiture par la porte du garage (case gar_door) et
  // s'Y ARRETER repeint la caisse et efface les etoiles. On ne declenche qu'une
  // fois par visite : sprayInside reste arme tant qu'on n'a pas quitte la porte
  // (la caisse ressort d'elle-meme une case au sud, ce qui re-arme la suivante).
  bool onSpray = false;
  if (driving) {
    int ctx = (int)car.x >> 3, cty = (int)car.y >> 3;     // tuile sous le centre de la caisse
    for (int i = 0; i < NUM_SPRAYS; i++)
      if (ctx == citySprays[i].tx && cty == citySprays[i].ty) {
        onSpray = true;
        bool stopped = (car.vx * car.vx + car.vy * car.vy) <= SPRAY_STOP_SPEED2;
        if (stopped && !sprayInside && seqKind == SEQ_NONE) {
          startSpraySeq(i);                               // arrete sur la porte -> repeinture
          sprayInside = true;                             // arme l'anti-repeat pour ce passage
        }
        break;
      }
  }
  if (!onSpray) sprayInside = false;                      // quitte la porte : re-armable

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
  // (Pay'n'Spray : plus d'overlay -- garages dessines comme tuiles de la map.)
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
  drawSceneNpc(camX, camY);
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

// ---- modules (corps de fonctions ; une seule unite de compilation) ----
#include "mod_world.h"
#include "mod_render.h"
#include "mod_ai.h"
#include "mod_combat.h"
#include "mod_mission.h"
#include "mod_economy.h"
#include "mod_player.h"
#include "mod_hud.h"
#include "mod_sequence.h"
#include "mod_save.h"
#include "mod_music.h"
