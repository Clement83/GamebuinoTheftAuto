#pragma once
// mod_combat.h - combat: balles, projectiles, attaque, explosions
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


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
      // PNJ de scene (contact/client) : une balle le tue -> echec de mission.
      if (missionRun.active && sceneNpcActive && !sceneNpcDead &&
          fabsf(b.x - sceneNpcX) < 4.0f && fabsf(b.y - sceneNpcY) < 4.0f) {
        sceneNpcDead = true; b.active = false;
        targetDownX = (int)sceneNpcX; targetDownY = (int)sceneNpcY; targetDownTimer = PED_DOWN_FRAMES;
        gb.sound.tone(90, 120); continue;
      }
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
      if (e.active && e.phase != EN_DOWN && e.phase != EN_STAGGER &&
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
  if (missionRun.active && target.active && targetStunTimer == 0 &&
      combatInCone(target.x, target.y, pcx, pcy, playerDir, wd.reach, wd.side)) {
    if (curObjs[missionRun.step].type == OBJ_SUBDUE) {
      objSubdue++;                          // elle cede, elle ne meurt pas
      targetStunTimer = getupFrames();      // tombe au sol comme un passant
      gb.sound.tone(150, 50);
    } else if (targetHp > 1) {
      targetHp--;                           // BOSS : encaisse le coup et tombe
      targetStunTimer = getupFrames();
      gb.sound.tone(150, 50);
    } else {
      killTarget(pcx, pcy);
    }
  }
  // PNJ de scene (contact de livraison / client de racket) : le frapper ou le
  // toucher le met a terre -> echec (capte par missionProgress via sceneNpcDead).
  if (missionRun.active && sceneNpcActive && !sceneNpcDead &&
      combatInCone(sceneNpcX, sceneNpcY, pcx, pcy, playerDir, wd.reach, wd.side)) {
    sceneNpcDead = true;
    targetDownX = (int)sceneNpcX; targetDownY = (int)sceneNpcY; targetDownTimer = PED_DOWN_FRAMES;
    gb.sound.tone(90, 120);
  }

  // Decompte des munitions ; arme videe -> retiree, retour au poing.
  if (!weaponInfinite(curWeapon) && --weaponAmmo[curWeapon] <= 0) {
    weaponOwned[curWeapon] = false;
    curWeapon = WEAPON_FIST;
  }
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
