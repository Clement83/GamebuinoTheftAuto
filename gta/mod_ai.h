#pragma once
// mod_ai.h - IA trafic: voitures/pietons, police, degats pietons
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


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
    // Police et camion s'excluent : un camion n'est jamais une voiture de police.
    c.isTruck = !c.isPolice && ((int)(aiRngNext(aiRng) % 100) < TRUCK_SPAWN_PCT);
    if (c.isTruck) {
      c.variant = (uint8_t)(aiRngNext(aiRng) % TRUCK_KIND_N);
      c.color = TRUCK_VARIANTS[c.variant].body;
    } else {
      c.color = c.isPolice ? POLICE_BLUE : AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N];
    }
    c.hp = CAR_MAX_HP;
    c.driver = true;                       // trafic = voiture avec conducteur
    c.fleeing = false; c.fleeTimer = 0;    // arrive calme
    c.brakeReact = 0;                      // reflexes au repos
    c.backoff = 0;
    c.fuse = 0; c.fireTick = 0;            // pas en feu a l'apparition
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
    if (&o == &p || !o.active || o.state == 1 || o.state == 4) continue;
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
      if (p.downTimer == 0 || --p.downTimer == 0) {
        // Corps qui attend l'ambulance (max MAX_PENDING_CORPSES en meme temps ;
        // au-dela, on retombe sur l'ancien despawn par timer).
        if (pendingCorpses < MAX_PENDING_CORPSES) {
          p.state = 4; pendingCorpses++;
          p.downTimer = CORPSE_WAIT_FRAMES;   // arme le cooldown DUR (~30 s) du corps
          dispatchAmbulance((int)p.x, (int)p.y);
        } else {
          p.active = false;
        }
      }
    } else if (p.state == 4) {                // corps en attente : cooldown DUR, jamais recycle par distance
      if (p.downTimer == 0 || --p.downTimer == 0) {   // 30 s ecoulees, ambulance arrivee ou non : il disparait
        p.active = false;
        if (pendingCorpses > 0) pendingCorpses--;
      }
    } else if (p.active) {
      int ddx = (int)p.x - fcx, ddy = (int)p.y - fcy;
      if (ddx * ddx + ddy * ddy > rec2) p.active = false;
    }
    // Plus de recherche : les flics debout redeviennent de simples passants.
    if (p.isCop && wanted.level == 0 && p.state == 0) { p.active = false; aiRespawnPed(p, fcx, fcy); continue; }
    if (!p.active) { aiRespawnPed(p, fcx, fcy); continue; }
    if (p.state == 1 || p.state == 4) continue;   // mort/renverse, ou corps en attente : immobile
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
  // Redispatch : un corps/feu reste en attente sans vehicule en route (job
  // annule par un vol, ou pool plein au moment du premier appel) -> on retente
  // chaque frame ; dispatchResponder est un no-op si un vehicule du meme type
  // est deja en mission, donc l'appel repete ne cree pas de doublons.
  if (pendingCorpses > 0) {
    for (int q = 0; q < NUM_AI_PEDS; q++)
      if (aiPeds[q].state == 4) { dispatchAmbulance((int)aiPeds[q].x, (int)aiPeds[q].y); break; }
  }
  // Redispatch generique : feux au sol d'abord, puis caisse du joueur, puis caisses IA.
  // dispatchResponder est no-op si un pompier est deja actif -> un seul foyer a la fois.
  // Le feu de mission pompier (missionFireSlot) est exclu : le joueur est le pompier.
  for (int q = 0; q < NUM_GROUND_FIRES; q++) {
    if (q == missionFireSlot) continue;
    if (groundFires[q].active && !groundFires[q].smoking)
      { dispatchFireTruck((int)groundFires[q].x, (int)groundFires[q].y, false, (int8_t)q); break; }
  }
  if (!carGone && carFuse > 0)
    dispatchFireTruck((int)car.x, (int)car.y, true, -1, -1);
  for (int q = 0; q < NUM_AI_CARS; q++)
    if (aiCars[q].active && aiCars[q].fuse > 0)
      { dispatchFireTruck((int)aiCars[q].x, (int)aiCars[q].y, true, -1, (int8_t)q); break; }
  responderUpdate(fcx, fcy);
}


// Enregistre un appel d'intervention (ambulance/pompier) pour (tx,ty) si aucun
// vehicule du meme type n'est deja en mission (cf. NUM_RESPONDERS = 1 par type
// max) et qu'un slot est libre. Le vehicule ne se materialise pas tout de
// suite : phase RESP_PENDING, ~3 s de delai (RESP_DISPATCH_DELAY) avant de
// spawner, pour simuler un temps de reaction sans le faire apparaitre trop
// loin (un anneau plus large faisait parfois echouer aiFindTileInRing -> le
// secours n'arrivait jamais).
static void dispatchResponder(uint8_t kind, int tx, int ty, bool fireIsCar, int8_t fireSlot, int8_t fireCarIdx) {
  for (int i = 0; i < NUM_RESPONDERS; i++)
    if (responders[i].active && responders[i].kind == kind) return;   // deja un de ce type en mission/en attente
  int slot = -1;
  for (int i = 0; i < NUM_RESPONDERS; i++) if (!responders[i].active) { slot = i; break; }
  if (slot < 0) return;                       // les 2 places sont prises par l'autre type
  Responder &r = responders[slot];
  r.kind = kind; r.phase = RESP_PENDING; r.tgx = (int16_t)tx; r.tgy = (int16_t)ty;
  r.workTimer = RESP_DISPATCH_DELAY; r.fireIsCar = fireIsCar; r.fireSlot = fireSlot;
  r.fireCarIdx = fireCarIdx;
  r.medicActive = false; r.pedIdx = -1; r.active = true;
  r.car.active = false;
}

// Materialise le vehicule (aiPlace autour de la cible) une fois le delai de
// dispatch ecoule. Si aucune tuile carrossable n'est trouvee, abandonne
// l'appel (il sera retente par la boucle de redispatch dans aiUpdate).
static void responderSpawnCar(Responder &r) {
  AiCar &c = r.car;
  int spx, spy;
  if (!aiFindTileInRing(r.tgx, r.tgy, RING_MIN, RING_MAX, aiIsDrivable, spx, spy)) { r.active = false; return; }
  aiPlace(cityMap, CITY_W, CITY_H, c.x, c.y, c.dir, c.tgtx, c.tgty, spx, spy, aiIsDrivable, aiRng);
  c.hp = CAR_MAX_HP; c.driver = true; c.fleeing = false; c.fleeTimer = 0;
  c.brakeReact = 0; c.backoff = 0; c.isPolice = false; c.isTruck = true;
  c.fuse = 0; c.fireTick = 0;
  c.variant = (r.kind == RESP_AMBULANCE) ? TRUCK_AMBULANCE : TRUCK_FIRE;
  c.color = TRUCK_VARIANTS[c.variant].body;
  c.active = true;
  r.phase = RESP_INBOUND; r.workTimer = 0;
}

static void dispatchAmbulance(int x, int y) { dispatchResponder(RESP_AMBULANCE, x, y, false, -1, -1); }
static void dispatchFireTruck(int x, int y, bool fireIsCar, int8_t fireSlot, int8_t fireCarIdx) {
  dispatchResponder(RESP_FIRETRUCK, x, y, fireIsCar, fireSlot, fireCarIdx);
}


// Avance les vehicules d'intervention : roulent vers la cible (conduite trafic
// reutilisee), travaillent sur place (ambulancier a pied qui ramasse TOUS les
// corps en attente / canon a eau du pompier), puis repartent et despawn. Volable
// a tout instant comme une caisse normale (cf. scan de vol dans gta.ino) : si
// `car.active` tombe a false (volee/detruite), la mission est simplement
// abandonnee ici (le vol lui-meme gere le reste : ejection, panique ambulancier).
static void responderUpdate(int fcx, int fcy) {
  for (int i = 0; i < NUM_RESPONDERS; i++) {
    Responder &r = responders[i];
    if (!r.active) continue;
    if (r.phase == RESP_PENDING) {
      if (--r.workTimer == 0) responderSpawnCar(r);
      continue;
    }
    AiCar &c = r.car;
    if (!c.active) { r.active = false; continue; }
    if (r.phase == RESP_INBOUND) {
      if (r.kind == RESP_FIRETRUCK && r.fireIsCar) {
        if (r.fireCarIdx >= 0) {
          AiCar &fc = aiCars[r.fireCarIdx];
          if (!fc.active || fc.fuse == 0) { r.phase = RESP_LEAVING; continue; }   // feu eteint/voiture disparue
          r.tgx = (int16_t)fc.x; r.tgy = (int16_t)fc.y;
        } else {
          r.tgx = (int16_t)car.x; r.tgy = (int16_t)car.y;
        }
      }
      policeCarStep(c, r.tgx, r.tgy);
      float ddx = (float)r.tgx - c.x, ddy = (float)r.tgy - c.y;
      if (ddx * ddx + ddy * ddy < 14.0f * 14.0f) r.phase = RESP_WORKING;
    } else if (r.phase == RESP_WORKING) {
      if (r.kind == RESP_AMBULANCE) {
        if (!r.medicActive) {
          int best = -1; float bestd = 1e18f;
          for (int q = 0; q < NUM_AI_PEDS; q++) {
            AiPed &p = aiPeds[q];
            if (p.state != 4) continue;
            float dx = p.x - c.x, dy = p.y - c.y, d2 = dx * dx + dy * dy;
            if (d2 < bestd) { bestd = d2; best = q; }
          }
          if (best < 0) { r.phase = RESP_LEAVING; continue; }   // plus de corps en attente
          r.pedIdx = (int8_t)best; r.medicActive = true; r.medicGoingBack = false;
          r.medicX = c.x; r.medicY = c.y; r.medicDir = c.dir; r.medicFrame = 0; r.medicAnimTimer = 0;
        } else {
          AiPed &corpse = aiPeds[r.pedIdx];
          if (!r.medicGoingBack && corpse.state != 4) {  // corps disparu entre-temps (cooldown 30 s) : on renonce
            r.medicActive = false; continue;
          }
          if (!r.medicGoingBack) {
            bool arrived = npcWalkToward(r.medicX, r.medicY, r.medicDir, r.medicFrame, r.medicAnimTimer,
                                          corpse.x, corpse.y, AI_PED_SPEED);
            if (arrived) {
              r.medicGoingBack = true;
              corpse.active = false;
              if (pendingCorpses > 0) pendingCorpses--;
            }
          } else {
            bool arrived = npcWalkToward(r.medicX, r.medicY, r.medicDir, r.medicFrame, r.medicAnimTimer,
                                          c.x, c.y, AI_PED_SPEED);
            if (arrived) r.medicActive = false;   // de retour : cherchera un autre corps au prochain passage
          }
        }
      } else {                                   // FIRETRUCK : canon a eau, simple compte a rebours
        if (r.workTimer == 0) {
          r.workTimer = RESPONDER_WORK_FRAMES;
          // Le jet d'eau commence : le feu passe direct en fumee (coupe la
          // flamme, peu importe le temps de combustion restant).
          if (!r.fireIsCar && r.fireSlot >= 0 && !groundFires[r.fireSlot].smoking) {
            groundFires[r.fireSlot].smoking = true;
            groundFires[r.fireSlot].life = GROUND_FIRE_SMOKE;
          }
        }
        if (--r.workTimer == 0) {
          if (r.fireIsCar) {
            if (r.fireCarIdx >= 0) { if (aiCars[r.fireCarIdx].active) aiCars[r.fireCarIdx].fuse = 0; }
            else { if (!carGone) { carFuse = 0; carFireTick = 0; } }
          }
          r.phase = RESP_LEAVING;
        }
      }
    } else {                                     // RESP_LEAVING : repart et despawn au loin
      aiCarStepAway(c, fcx, fcy);
      int ddx = (int)c.x - fcx, ddy = (int)c.y - fcy;
      if (ddx * ddx + ddy * ddy > RECYCLE_DIST * RECYCLE_DIST) { c.active = false; r.active = false; }
    }
  }
}


// Dessine les vehicules d'intervention : camion (sprite truck + girophare),
// ambulancier a pied (sprite pieton standard, blanc), et le jet d'eau du
// pompier (overlay 2 px clignotant, meme principe que drawGyro -- aucun sprite
// dedie n'est necessaire).
static void responderDraw(int camX, int camY) {
  for (int i = 0; i < NUM_RESPONDERS; i++) {
    Responder &r = responders[i];
    if (!r.active || !r.car.active) continue;
    const TruckVariant &v = TRUCK_VARIANTS[r.car.variant];
    blitTruck(camX, camY, (int)r.car.x, (int)r.car.y, AI_CAR_FRAME[r.car.dir], v);
    if (v.hasGyro) drawGyro(camX, camY, (int)r.car.x, (int)r.car.y, v.gyroL, v.gyroR);
    if (r.medicActive) blitPed(camX, camY, (int)r.medicX, (int)r.medicY, r.medicDir, r.medicFrame, 0xFFFF);
    if (r.kind == RESP_FIRETRUCK && (r.phase == RESP_INBOUND || r.phase == RESP_WORKING)) {
      float ddx = r.car.x - r.tgx, ddy = r.car.y - r.tgy;
      if (ddx * ddx + ddy * ddy < 20.0f * 20.0f)
        drawWaterJet(camX, camY, r.car.x, r.car.y, r.tgx, r.tgy);
    }
  }
}


static void aiDraw(int camX, int camY) {
  for (int i = 0; i < NUM_AI_CARS; i++) {
    AiCar &c = aiCars[i];
    if (!c.active) continue;
    if (c.isTruck) {
      const TruckVariant &v = TRUCK_VARIANTS[c.variant];
      blitTruck(camX, camY, (int)c.x, (int)c.y, AI_CAR_FRAME[c.dir], v);
      if (v.hasGyro) drawGyro(camX, camY, (int)c.x, (int)c.y, v.gyroL, v.gyroR);
      continue;
    }
    blitCar(camX, camY, (int)c.x, (int)c.y, AI_CAR_FRAME[c.dir], c.color);
    // Girophare allume au-dela d'une etoile (>= 2) sur les voitures de police.
    if (c.isPolice && wanted.level >= 2) drawGyro(camX, camY, (int)c.x, (int)c.y, 0xF800, 0x001F);
  }
  for (int i = 0; i < NUM_AI_PEDS; i++) {
    AiPed &p = aiPeds[i];
    if (!p.active) continue;
    if (p.state == 1) blitSplat(camX, camY, (int)p.x, (int)p.y);
    else if (p.state == 3) blitDownBody(camX, camY, (int)p.x, (int)p.y, p.color);  // assomme (vol de voiture)
    else if (p.state == 4) blitDownBody(camX, camY, (int)p.x, (int)p.y, p.color);  // corps en attente de l'ambulance
    else blitPed(camX, camY, (int)p.x, (int)p.y, p.dir, p.frame, p.color);
  }
  responderDraw(camX, camY);
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
