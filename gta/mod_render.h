#pragma once
// mod_render.h - rendu: tuiles, sprites joueur/voiture, fumee, epaves, fx
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


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


// Blit recolore d'un sprite camion/bus : forme partagee (truckFrames), 4 cles
// (corps/cabine/fenetres/echelle) recolorees selon la livree -- cf. TruckVariant
// et tools/build_truck.py. Meme principe que blitCar, juste plus de cles.
static void blitTruck(int camX, int camY, int worldCx, int worldCy,
                       int frameIdx, const TruckVariant &v) {
  const uint16_t *src = truckFrames[frameIdx];
  int ox = worldCx - camX - TRUCK_BOX / 2;
  int oy = worldCy - camY - TRUCK_BOX / 2;
  for (int ry = 0; ry < TRUCK_BOX; ry++) {
    int y = oy + ry;
    if (y < 0 || y >= SCREEN_H) continue;
    uint16_t *row = fb + y * SCREEN_W;
    const uint16_t *srow = src + ry * TRUCK_BOX;
    for (int rx = 0; rx < TRUCK_BOX; rx++) {
      uint16_t c = srow[rx];
      if (c == TRUCK_TRANSPARENT) continue;
      if (c == TRUCK_BODY_KEY) c = v.body;
      else if (c == TRUCK_CAB_KEY) c = v.cab;
      else if (c == TRUCK_WINDOW_KEY) c = v.window;
      else if (c == TRUCK_LADDER_KEY) c = v.ladder;
      int x = ox + rx;
      if (x >= 0 && x < SCREEN_W) row[x] = c;
    }
  }
}


// Blit recolore d'un sprite bateau : coque partagee (boatFrames), 3 cles
// (coque/pont/liseree) recolorees selon la livree -- cf. BoatVariant et
// tools/build_boat.py. Pare-brise et moteur restent des couleurs fixes (baked).
static void blitBoat(int camX, int camY, int worldCx, int worldCy,
                     int frameIdx, const BoatVariant &v) {
  const uint16_t *src = boatFrames[frameIdx];
  int ox = worldCx - camX - BOAT_BOX / 2;
  int oy = worldCy - camY - BOAT_BOX / 2;
  for (int ry = 0; ry < BOAT_BOX; ry++) {
    int y = oy + ry;
    if (y < 0 || y >= SCREEN_H) continue;
    uint16_t *row = fb + y * SCREEN_W;
    const uint16_t *srow = src + ry * BOAT_BOX;
    for (int rx = 0; rx < BOAT_BOX; rx++) {
      uint16_t c = srow[rx];
      if (c == BOAT_TRANSPARENT) continue;
      if (c == BOAT_HULL_KEY) c = v.hull;
      else if (c == BOAT_DECK_KEY) c = v.deck;
      else if (c == BOAT_TRIM_KEY) c = v.trim;
      int x = ox + rx;
      if (x >= 0 && x < SCREEN_W) row[x] = c;
    }
  }
}


// Girophare : 2 pixels (gauche/droite) au centre du toit qui ALTERNENT entre
// colorA et colorB (clignotement) selon la phase d'animation globale. Dessine
// par-dessus la caisse. Aucun sprite supplementaire en flash. Appele pour les
// voitures de police au-dela d'1 etoile, et pour les camions pompier/ambulance
// (cf. TruckVariant.hasGyro).
static void drawGyro(int camX, int camY, int worldCx, int worldCy,
                      uint16_t colorA, uint16_t colorB) {
  int cx = worldCx - camX, cy = worldCy - camY;
  bool phase = (missionAnim >> 2) & 1;            // bascule toutes les ~4 frames
  uint16_t left  = phase ? colorA : colorB;
  uint16_t right = phase ? colorB : colorA;
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


// Idem blitCarBurnt, pour l'epave d'un camion (truckFrames, 4 cles -> toutes
// charbon fonce indistinctement, seuls phares/pare-brise restent clairs).
static void blitTruckBurnt(int camX, int camY, int worldCx, int worldCy, int frameIdx) {
  const uint16_t *src = truckFrames[frameIdx];
  int ox = worldCx - camX - TRUCK_BOX / 2;
  int oy = worldCy - camY - TRUCK_BOX / 2;
  for (int ry = 0; ry < TRUCK_BOX; ry++) {
    int y = oy + ry;
    if (y < 0 || y >= SCREEN_H) continue;
    uint16_t *row = fb + y * SCREEN_W;
    const uint16_t *srow = src + ry * TRUCK_BOX;
    for (int rx = 0; rx < TRUCK_BOX; rx++) {
      uint16_t c = srow[rx];
      if (c == TRUCK_TRANSPARENT) continue;
      int x = ox + rx;
      if (x < 0 || x >= SCREEN_W) continue;
      bool dark = (c == TRUCK_BODY_KEY || c == TRUCK_CAB_KEY ||
                   c == TRUCK_WINDOW_KEY || c == TRUCK_LADDER_KEY);
      row[x] = dark ? 0x2104 : 0x4208;   // charbon fonce / clair
    }
  }
}


// --- Epaves : naissance (petit saut), recyclage au loin, rendu (carcasse +
//     fumee residuelle). spawnWreck est appele a la destruction d'une voiture. ---
static void spawnWreck(float wx, float wy, uint8_t frameIdx, float hopx, float hopy, bool truck) {
  float n = sqrtf(hopx * hopx + hopy * hopy);
  float ux = 0.0f, uy = 0.0f;
  if (n > 0.01f) { ux = hopx / n; uy = hopy / n; }
  int slot = -1;
  for (int i = 0; i < NUM_WRECKS; i++) if (!wrecks[i].active) { slot = i; break; }
  if (slot < 0) slot = 0;                       // pool plein : recycle le slot 0
  Wreck &w = wrecks[slot];
  w.x = wx; w.y = wy; w.vx = ux * 1.6f; w.vy = uy * 1.6f;
  w.frame = frameIdx; w.hop = WRECK_HOP_FRAMES; w.active = true; w.truck = truck;
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
    if (w.truck) blitTruckBurnt(camX, camY, (int)w.x, (int)w.y, w.frame);
    else blitCarBurnt(camX, camY, (int)w.x, (int)w.y, w.frame);
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
// clipBottom > 0 : on ne dessine pas les `clipBottom` lignes du bas (jambes
// sous l'eau) -> rendu mi-immerge pour la nage, sans sprite dedie (0 flash).
static void blitPed(int camX, int camY, int worldCx, int worldCy,
                    uint8_t dir, uint8_t frame, uint16_t color, int clipBottom) {
  const uint16_t *src = playerFrames[dir][frame];
  int ox = worldCx - camX - PLAYER_BOX / 2;
  int oy = worldCy - camY - PLAYER_BOX / 2;
  for (int ry = 0; ry < PLAYER_BOX - clipBottom; ry++) {
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
  // Nage : buste qui depasse (jambes coupees) + une ligne d'eau sous le sprite.
  if (playerSwimming) {
    blitPed(camX, camY, playerX + PLAYER_W / 2, playerY + PLAYER_H / 2,
            playerDir, playerFrame, PLAYER_BODY_COLOR, 2);
    return;
  }
  blitPed(camX, camY, playerX + PLAYER_W / 2, playerY + PLAYER_H / 2,
          playerDir, playerFrame, PLAYER_BODY_COLOR, 0);
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
  if (drivingBoat) {                                 // bateau : 16 frames (≠ voiture)
    int bidx = (int)(a / TWO_PI * BOAT_FRAMES + 0.5f);
    bidx %= BOAT_FRAMES;
    if (bidx < 0) bidx += BOAT_FRAMES;
    const BoatVariant &bv = BOAT_VARIANTS[drivingVariant];
    blitBoat(camX, camY, (int)car.x, (int)car.y, bidx, bv);
    if (bv.hasGyro && wanted.level >= 2)
      drawGyro(camX, camY, (int)car.x, (int)car.y, bv.gyroL, bv.gyroR);
    return;
  }
  int idx = (int)(a / TWO_PI * CAR_FRAMES + 0.5f);   // TRUCK_FRAMES == CAR_FRAMES : meme formule
  idx %= CAR_FRAMES;
  if (idx < 0) idx += CAR_FRAMES;
  if (drivingTruck) {
    const TruckVariant &v = TRUCK_VARIANTS[drivingVariant];
    blitTruck(camX, camY, (int)car.x, (int)car.y, idx, v);
    if (v.hasGyro && wanted.level >= 2)
      drawGyro(camX, camY, (int)car.x, (int)car.y, v.gyroL, v.gyroR);
    return;
  }
  blitCar(camX, camY, (int)car.x, (int)car.y, idx, carColor);
  // Si on roule dans une voiture de police volee, le girophare suit la meme regle.
  if (carColor == POLICE_BLUE && wanted.level >= 2)
    drawGyro(camX, camY, (int)car.x, (int)car.y, 0xF800, 0x001F);
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


// Icone de flamme generique (3 points rouge/orange autour d'un centre jaune,
// clignotant) : meme silhouette pour TOUS les feux du jeu (caisse en feu,
// feu au sol du molotov...). Pas de nouveau sprite : juste 4 pixels poses
// directement dans le framebuffer, comme drawGyro/drawBoom.
static void drawFireIcon(int cx, int cy) {
  static const int8_t FX[3] = { -1, 1, 0 };
  static const int8_t FY[3] = {  1, 1, -2 };
  uint16_t dot = (missionAnim & 2) ? 0xFC00 : 0xF800;     // orange / rouge qui clignote
  if (cx >= 0 && cx < SCREEN_W && cy >= 0 && cy < SCREEN_H) fb[cy * SCREEN_W + cx] = 0xFFE0;  // centre jaune
  for (int k = 0; k < 3; k++) {
    int x = cx + FX[k], y = cy + FY[k];
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = dot;
  }
}


// Verifie si un pompier est en train d'arroser cette cible (RESP_WORKING) :
// isPlayerCar=true -> caisse du joueur ; sinon -> aiCars[aiIdx].
static bool fireBeingHosed(bool isPlayerCar, int aiIdx) {
  for (int i = 0; i < NUM_RESPONDERS; i++) {
    Responder &r = responders[i];
    if (!r.active || r.kind != RESP_FIRETRUCK || r.phase != RESP_WORKING || !r.fireIsCar) continue;
    if (isPlayerCar && r.fireCarIdx < 0) return true;
    if (!isPlayerCar && r.fireCarIdx == aiIdx) return true;
  }
  return false;
}


// Fumee de la caisse du joueur (au volant OU lancee). Sprite pre-rendu ancre au
// CAPOT : palier leger sous ~60 % de PV, dense sous ~30 %. En feu (meche) :
// flamme + double panache. Pompier qui arrose -> plus que la fumee (flamme masquee).
static void drawCarSmoke(int camX, int camY) {
  if (carGone) return;
  bool burning = carFuse > 0;
  if (!burning && carHp > (CAR_MAX_HP * 3) / 5) return;
  int tier = (burning || carHp <= (CAR_MAX_HP * 3) / 10) ? 1 : 0;
  int frame = (missionAnim / (tier ? 3 : 6)) % SMOKE_FRAMES;
  float a = car.angle;
  int hx = (int)(car.x + cosf(a) * SMOKE_HOOD_DIST) - camX;
  int hy = (int)(car.y + sinf(a) * SMOKE_HOOD_DIST) - camY;
  blitSmoke(hx, hy, tier, frame);
  if (burning) {
    blitSmoke(hx, hy - 4, 1, (frame + 2) % SMOKE_FRAMES);
    if (!fireBeingHosed(true, -1)) drawFireIcon(hx, hy - 2);
  }
}


// Fumee des voitures du trafic amochees : panache ancre au centre (palier leger /
// dense). En feu (fuse > 0) : meme combo flamme + double panache que drawCarSmoke.
static void drawAiCarSmoke(int camX, int camY) {
  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (!c.active) continue;
    bool burning = c.fuse > 0;
    if (!burning && c.hp > (CAR_MAX_HP * 3) / 5) continue;
    int tier = (burning || c.hp <= (CAR_MAX_HP * 3) / 10) ? 1 : 0;
    int frame = (missionAnim / (tier ? 3 : 6) + i) % SMOKE_FRAMES;
    int cx = (int)c.x - camX, cy = (int)c.y - camY;
    blitSmoke(cx, cy - 2, tier, frame);
    if (burning) {
      blitSmoke(cx, cy - 6, 1, (frame + 2) % SMOKE_FRAMES);
      if (!fireBeingHosed(false, i)) drawFireIcon(cx, cy - 4);
    }
  }
}


// Jet d'eau du camion de pompiers en action (RESP_WORKING) : degre de bleu/blanc
// depuis le capot vers la cible, vague laterale sinusoidale via table (sans trig),
// eclaboussures scintillantes a l'impact. Tout en pixels directs, zero sprite.
static void drawWaterJet(int camX, int camY, float wx, float wy, int tgx, int tgy) {
  static const int8_t WAVE[8] = { 0, 1, 1, 0, 0, -1, -1, 0 };  // vague douce
  float dx = (float)tgx - wx, dy = (float)tgy - wy;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 1.0f) return;
  float fdx = dx / len, fdy = dy / len;   // vecteur unitaire camion->feu
  float pdx = -fdy, pdy = fdx;            // perpendiculaire
  int cx = (int)wx - camX, cy = (int)wy - camY;
  for (int k = 2; k <= 19; k++) {
    int side = WAVE[(missionAnim + k) & 7];
    int x = cx + (int)(fdx * k + pdx * side);
    int y = cy + (int)(fdy * k + pdy * side);
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
    uint16_t col = (k <= 7) ? 0xBFFF   // bleu tres clair pres du camion
                 : (k <= 13) ? 0x3FFF  // bleu moyen
                 : 0x07FF;             // cyan vif a la pointe
    fb[y * SCREEN_W + x] = col;
  }
  // Eclaboussures a l'impact : 4 pixels en croix, clignotement en quinconce
  static const int8_t SX[4] = { -1,  1,  0,  0 };
  static const int8_t SY[4] = {  0,  0, -1,  1 };
  int ex = cx + (int)(fdx * 19), ey = cy + (int)(fdy * 19);
  for (int k = 0; k < 4; k++) {
    if ((missionAnim + k) & 2) continue;
    int sx = ex + SX[k], sy = ey + SY[k];
    if (sx >= 0 && sx < SCREEN_W && sy >= 0 && sy < SCREEN_H)
      fb[sy * SCREEN_W + sx] = 0xFFFF;
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


// Feu au sol (molotov, explosion, chaine...) : flamme + panache de fumee quand
// actif ; que de la fumee qui s'estompe une fois g.smoking (pompier ou fin de vie).
static void drawGroundFires(int camX, int camY) {
  for (int i = 0; i < NUM_GROUND_FIRES; i++) {
    GroundFire &g = groundFires[i];
    if (!g.active) continue;
    int cx = (int)g.x - camX, cy = (int)g.y - camY;
    int frame = (missionAnim / 4 + i) % SMOKE_FRAMES;
    if (g.smoking) {
      blitSmoke(cx, cy, 1, frame);
    } else {
      blitSmoke(cx, cy - 4, 1, frame);   // panache au-dessus de la flamme
      drawFireIcon(cx, cy);
    }
  }
}
