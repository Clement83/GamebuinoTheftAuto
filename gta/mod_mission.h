#pragma once
// mod_mission.h - missions: objectifs, ennemis/cibles, cutscenes, Marco, rendu mission
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


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
    target.loseTimer = 0; target.active = true; target.chase = false; targetStunTimer = 0;
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
  target.loseTimer = 0; target.active = true; target.chase = false; targetStunTimer = 0;
  return true;
}


// Fait apparaitre le tueur (poursuite) sur une tuile marchable proche de (wx,wy).
static void spawnTargetAt(int wx, int wy) {
  int tx, ty;
  if (!aiFindWalkTileNear(wx, wy, tx, ty)) { tx = wx >> 3; ty = wy >> 3; }
  aiPlace(cityMap, CITY_W, CITY_H, target.x, target.y, target.dir,
          target.tgtx, target.tgty, tx, ty, aiIsWalkable, aiRng);
  target.frame = 0; target.animTimer = 0; target.phase = T_WANDER;
  target.loseTimer = 0; target.active = true; target.chase = true; targetStunTimer = 0;
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


// Deux gros bras (EK_THUG) surgissent autour du joueur, deja AGGRO : ils foncent
// et cognent. bouncerBrawl les garde vivants hors mission (cf. updateEnemies).
static void spawnCasinoBouncers(int pcx, int pcy) {
  clearEnemies();
  bouncerBrawl = true;
  static const int OFFX[2] = { -2, 2 };   // un de chaque cote du joueur
  static const int OFFY[2] = {  1, 1 };
  for (int i = 0; i < 2; i++) {
    Enemy &e = enemies[i];
    int wx = pcx + OFFX[i] * TILE_W, wy = pcy + OFFY[i] * TILE_H;
    int tx, ty;
    if (aiFindWalkTileNear(wx, wy, tx, ty)) { e.x = tx * 8 + 4; e.y = ty * 8 + 4; }
    else { e.x = (float)wx; e.y = (float)wy; }
    e.tgtx = (int)e.x; e.tgty = (int)e.y;
    e.dir = DIR_SOUTH; e.frame = 0; e.animTimer = 0;
    e.hp = ENEMY_HP_THUG; e.kind = EK_THUG; e.phase = EN_AGGRO;  // pas de sommeil : ils chargent
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
    // Coup encaisse : il tombe au sol (comme un passant), se relevera et re-aggro.
    e.hp--; e.phase = EN_STAGGER; e.downTimer = (uint8_t)getupFrames();
    gb.sound.tone(150, 50);
  }
}


// Un coup au corps-a-corps connecte-t-il ? ENEMY_MELEE_MISS_PCT % de chances de
// rater (l'ennemi cogne dans le vide). Met a jour le rng. Partage gros bras/boss.
static bool enemyMeleeHits() {
  return (aiRngNext(aiRng) % 100) >= ENEMY_MELEE_MISS_PCT;
}


// Inflige un coup a l'allie defendu (Tony / Sarah). A 0 PV : il tombe, ce qui
// echoue la mission (detecte par missionProgress via allyDead).
static void hurtAlly() {
  if (!allyStands || allyHp == 0) return;
  if (--allyHp == 0) {
    allyStands = false; allyDead = true;
    targetDownX = (int)marcoX; targetDownY = (int)marcoY; targetDownTimer = PED_DOWN_FRAMES;
    gb.sound.tone(90, 140);
  } else {
    gb.sound.tone(150, 50);
  }
}


// Met a jour les ennemis chaque frame. (fcx,fcy) = repere joueur (centre px).
// IDLE -> AGGRO quand le joueur approche ; AGGRO : fonce (gros bras) ou tire
// (gunner) ; renversable a la voiture lancee. EN_DOWN : decompte du splat ;
// EN_STAGGER : au sol apres un coup, se releve en EN_AGGRO.
static void updateEnemies(int fcx, int fcy) {
  if (seqKind == SEQ_CUT) return;                 // cinematique : ennemis figes (taunt)
  for (int i = 0; i < MAX_ENEMIES; i++) {
    Enemy &e = enemies[i];
    if (!e.active) continue;
    if (e.phase == EN_DOWN) {                       // a terre : despawn en fin de timer
      if (e.downTimer > 0 && --e.downTimer == 0) e.active = false;
      continue;
    }
    if (e.phase == EN_STAGGER) {                    // sonne : se releve en fin de timer (ni course ni coup)
      if (e.downTimer == 0 || --e.downTimer == 0) e.phase = EN_AGGRO;
      continue;
    }
    if (!missionRun.active && !bouncerBrawl) { e.active = false; continue; }  // mission finie : on nettoie (sauf brawl casino)
    float ddx = (float)fcx - e.x, ddy = (float)fcy - e.y;
    float d2 = ddx * ddx + ddy * ddy;
    if (e.phase == EN_IDLE) {
      if (d2 <= (float)(ENEMY_AGGRO_RANGE * ENEMY_AGGRO_RANGE)) e.phase = EN_AGGRO;
      else continue;                                // encore endormi : ne bouge pas
    }
    if (e.atkTimer > 0) e.atkTimer--;
    // Cible de l'ennemi : le joueur par defaut, mais l'ALLIE DEFENDU s'il est
    // plus proche (l'aggro reste base sur le joueur ci-dessus). Le joueur doit
    // alors s'interposer entre les assaillants et l'allie.
    int tcx = fcx, tcy = fcy; bool onAlly = false; float td2 = d2;
    if (allyStands && curDef.failOnAllyDeath) {
      float adx = marcoX - e.x, ady = marcoY - e.y; float d2a = adx * adx + ady * ady;
      if (d2a < td2) { tcx = (int)marcoX; tcy = (int)marcoY; td2 = d2a; onAlly = true; }
    }
    if (e.kind == EK_GUNNER) {
      // Tireur : se rapproche jusqu'a portee de tir, puis fait feu si ligne de vue.
      if (td2 > (float)(ENEMY_SHOOT_RANGE * ENEMY_SHOOT_RANGE) * 0.5f)
        missionChaseStep(cityMap, CITY_W, CITY_H, e.x, e.y, e.dir, e.tgtx, e.tgty,
                         ENEMY_SPEED, tcx, tcy, aiRng);
      if (e.atkTimer == 0 && td2 <= (float)(ENEMY_SHOOT_RANGE * ENEMY_SHOOT_RANGE) &&
          missionLineOfSight(cityMap, CITY_W, CITY_H, (int)e.x, (int)e.y, tcx, tcy, ENEMY_SHOOT_RANGE)) {
        e.atkTimer = ENEMY_SHOOT_PERIOD;
        gb.sound.tone(140, 40);
        if (onAlly) hurtAlly();                     // tir sur l'allie : touche directe (abstrait)
        else        fireCopBullet(e.x, e.y, tcx, tcy);  // balle hostile (esquivable) sur le joueur
      }
    } else {
      // Gros bras : fonce et frappe au contact (le joueur ou l'allie).
      missionChaseStep(cityMap, CITY_W, CITY_H, e.x, e.y, e.dir, e.tgtx, e.tgty,
                       ENEMY_SPEED, tcx, tcy, aiRng);
      if ((onAlly || !driving) && e.atkTimer == 0 && td2 < (float)(ENEMY_MELEE_DIST * ENEMY_MELEE_DIST)) {
        e.atkTimer = ENEMY_MELEE_PERIOD;            // cooldown meme sur un rate (sinon re-tente chaque frame)
        if (enemyMeleeHits()) {
          gb.sound.tone(90, 70);
          if (onAlly) hurtAlly(); else hurtPlayer(ENEMY_MELEE_DMG, false);
        } else gb.sound.tone(55, 40);               // coup dans le vide
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


// Dessine les ennemis : sang (EN_DOWN, mort), corps au sol (EN_STAGGER, assomme)
// ou sprite recolore selon le type.
static void drawEnemies(int camX, int camY) {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    Enemy &e = enemies[i];
    if (!e.active) continue;
    uint16_t col = e.kind == EK_GUNNER ? ENEMY_COLOR_GUNNER : ENEMY_COLOR_THUG;
    if (e.phase == EN_DOWN) blitSplat(camX, camY, (int)e.x, (int)e.y);
    else if (e.phase == EN_STAGGER) blitDownBody(camX, camY, (int)e.x, (int)e.y, col);
    else blitPed(camX, camY, (int)e.x, (int)e.y, e.dir, e.frame, col);
  }
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
  curDef.failOnCarLoss = src.failOnCarLoss;
  curDef.failOnAllyDeath = src.failOnAllyDeath;
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
  sceneNpcActive = false; sceneNpcDead = false;  // contact/client du stop precedent : retire
  clearEnemies();                                // pas d'ennemis residuels de l'objectif precedent
  // Ennemis scenarises AGRESSIFS (gardes, assaillants) poses des l'activation,
  // de facon deterministe autour du point d'objectif (cf. spawnEnemiesForObjective).
  if (o.enemyCount > 0) spawnEnemiesForObjective(o);
  // Allie DEFENDU (Tony M8, Sarah M14) : pose stationnaire sur le lieu de defense
  // des le 1er objectif, et persiste (PV inchanges) sur toutes les vagues.
  if (curDef.failOnAllyDeath && !allyStands) {
    int wt, ht;
    if (aiFindWalkTileNear(o.x, o.y, wt, ht)) { marcoX = (float)(wt * 8 + 4); marcoY = (float)(ht * 8 + 4); }
    else { marcoX = (float)o.x; marcoY = (float)o.y; }
    marcoDir = DIR_SOUTH; marcoFrame = 0; marcoAnimTimer = 0;
    allyStands = true; allyDead = false; allyHp = ALLY_HP;
    allyColor = (curDef.title && strcmp(curDef.title, "Embuscade") == 0) ? SARAH_COLOR : TONY_COLOR;
  }
  // Contact de scene scriptee (EV_DELIVERY) : pose un PNJ qui ATTEND a destination.
  // La scene (CUT_DELIVERY) se joue a la completion de l'objectif ; l'ecraser avant
  // echoue la mission (sceneNpcDead, cf. missionProgress).
  // Contact de scene (EV_DELIVERY : livraison avec cinematique) ou CLIENT
  // cooperatif (EV_CLIENT : racket, sans cinematique) : meme entite posee a
  // destination. Dans les deux cas, le TUER echoue la mission.
  if (o.event == EV_DELIVERY || o.event == EV_CLIENT) {
    int wt, ht;
    if (aiFindWalkTileNear(o.x, o.y, wt, ht)) { sceneNpcX = (float)(wt * 8 + 4); sceneNpcY = (float)(ht * 8 + 4); }
    else { sceneNpcX = (float)o.x; sceneNpcY = (float)o.y; }
    sceneNpcDir = DIR_SOUTH; sceneNpcFrame = 0; sceneNpcAnimTimer = 0;
    sceneNpcActive = true; sceneNpcDead = false;
  }
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
    target.active = true; target.chase = false; targetStunTimer = 0;
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
  allyStands = false; allyDead = false; allyHp = 0;   // allie defendu : remis a zero
  sceneNpcActive = false; sceneNpcDead = false;       // contact de scene : remis a zero
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


// Mission echouee (limite de temps depassee) : message + retour au repos.
static void failMission(const char *msg) {
  narrate(msg);
  missionRun.active = false;
  target.active = false; marcoWaiting = false; marcoFollow = false; marcoAboard = false;
  mCarActive = false; carIsMission = false;
  allyStands = false; allyDead = false;
  sceneNpcActive = false; sceneNpcDead = false;
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
  allyStands = false; allyDead = false;          // allie defendu : fin de mission
  sceneNpcActive = false; sceneNpcDead = false;  // contact de scene : fin de mission
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
  target.phase = T_WANDER; target.loseTimer = 0; target.active = true; target.chase = false; targetStunTimer = 0;
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


// Scene scriptee a destination (EV_DELIVERY). Deux variantes :
//  - AVEC compagnon (solo=false, ex. M1) : la caisse s'arrete, le compagnon
//    descend, marche jusqu'au contact, echange l1/l2, puis revient et REMONTE.
//  - SOLO (solo=true, ex. M7 : Marco mort) : c'est le CONTACT qui vient a la
//    caisse, echange l1/l2, puis la scene se cloture.
// endText = narration de cloture (doneText). Le contact disparait a la fin.
// Joueur fige (SEQ_CUT).
static void startDeliveryCut(const char *l1, const char *l2, const char *endText, bool solo) {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_CUT; cutKind = CUT_DELIVERY; cutPhase = 0; cutTimer = 0;
  cutLine1 = l1; cutLine2 = l2; cutEndText = endText; sceneSolo = solo;
  car.vx = 0.0f; car.vy = 0.0f;                       // la caisse s'arrete net
  if (!solo) {
    marcoAboard = false; marcoWaiting = false; marcoEmergeDelay = 0;
    marcoFollow = true;                                // pour que drawMarco le dessine
    int ox, oy;                                        // le compagnon descend a cote de la caisse
    if (findFootSpot((int)car.x, (int)car.y, ox, oy)) { marcoX = ox + PLAYER_W / 2; marcoY = oy + PLAYER_H / 2; }
    else { marcoX = car.x; marcoY = car.y + TILE_H; }
    marcoFrame = 0; marcoAnimTimer = 0;
  }
}


// Death-beat de boss : a la mort d'un boss (KILL count>=5), on FIGE un instant
// sur son corps pendant que defile sa replique de revelation (endText), avant la
// banniere de fin. Joueur fige (SEQ_CUT). La scene enchaine sur la suite/fin.
static void startBossDownCut(const char *endText) {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_CUT; cutKind = CUT_BOSS_DOWN; cutPhase = 0;
  cutTimer = CUT_LINE_FRAMES + CUT_LINE_FRAMES / 2;   // ~4.5 s sur le corps
  cutEndText = endText;
  car.vx = 0.0f; car.vy = 0.0f;
  if (endText) narrate(endText);                       // "Rico, a terre : ..." / "Bruno, mourant : ..."
}


// Repliques de la scene de livraison (EV_DELIVERY) selon la mission. l1 = la
// replique du livreur (compagnon) ou du contact, l2 = la reponse.
static void deliveryLines(const char *title, const char *&l1, const char *&l2) {
  if (title && strcmp(title, "Voiture volee") == 0) {      // M7 : receleur (solo)
    l1 = "Le receleur : la caisse des Loups... beau bebe. Tony sera content.";
    l2 = "Le receleur : file, petit. Je m'occupe d'elle.";
    return;
  }
  l1 = "Marco : tiens, le paquet. C'est tout bon.";        // defaut : M1 (colis aux Quais)
  l2 = "Le contact : nickel. Filez avant que les flics rappliquent.";
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
  if (cutKind == CUT_BOSS_DOWN) {                        // figer sur le boss a terre
    if (cutTimer > 0 && --cutTimer == 0) {
      seqKind = SEQ_NONE; cutKind = CUT_NONE;
      if (missionRun.active) enterObjective();
      else                   finishMission();
    }
    return;
  }
  if (cutKind == CUT_DELIVERY) {
    if (sceneSolo) {                                     // SANS compagnon : le contact vient a la caisse
      switch (cutPhase) {
        case 0: {                                        // le contact s'approche du vehicule
          float dx = car.x - sceneNpcX, dy = car.y - sceneNpcY;
          if (dx * dx + dy * dy > 14.0f * 14.0f) {
            npcWalkToward(sceneNpcX, sceneNpcY, sceneNpcDir, sceneNpcFrame, sceneNpcAnimTimer,
                          car.x, car.y, MARCO_FOLLOW_SPEED);
          } else {
            sceneNpcFrame = 0;
            sceneNpcDir = (sceneNpcX < car.x) ? DIR_EAST : DIR_WEST;
            cutPhase = 1; cutTimer = CUT_LINE_FRAMES;
            if (cutLine1) narrate(cutLine1);
          }
          break;
        }
        case 1:
          if (cutTimer > 0 && --cutTimer == 0) {
            cutPhase = 2; cutTimer = CUT_LINE_FRAMES;
            if (cutLine2) narrate(cutLine2);
          }
          break;
        case 2:
          if (cutTimer > 0 && --cutTimer == 0) {
            sceneNpcActive = false;
            seqKind = SEQ_NONE; cutKind = CUT_NONE;
            if (cutEndText) narrate(cutEndText);
            if (missionRun.active) enterObjective();
            else                   finishMission();
          }
          break;
      }
      return;
    }
    switch (cutPhase) {
      case 0: {                                          // le compagnon marche vers le contact
        float dx = sceneNpcX - marcoX, dy = sceneNpcY - marcoY;
        if (dx * dx + dy * dy > 12.0f * 12.0f) {
          npcWalkToward(marcoX, marcoY, marcoDir, marcoFrame, marcoAnimTimer,
                        sceneNpcX, sceneNpcY, MARCO_FOLLOW_SPEED);
        } else {
          marcoFrame = 0;
          marcoDir    = (marcoX < sceneNpcX) ? DIR_EAST : DIR_WEST;   // face a face
          sceneNpcDir = (sceneNpcX < marcoX) ? DIR_EAST : DIR_WEST;
          cutPhase = 1; cutTimer = CUT_LINE_FRAMES;
          if (cutLine1) narrate(cutLine1);
        }
        break;
      }
      case 1:                                            // replique du contact
        if (cutTimer > 0 && --cutTimer == 0) {
          cutPhase = 2; cutTimer = CUT_LINE_FRAMES;
          if (cutLine2) narrate(cutLine2);
        }
        break;
      case 2:                                            // -> le compagnon repart
        if (cutTimer > 0 && --cutTimer == 0) cutPhase = 3;
        break;
      case 3: {                                          // retour a la caisse puis remontee
        float dx = car.x - marcoX, dy = car.y - marcoY;
        if (dx * dx + dy * dy > 12.0f * 12.0f) {
          npcWalkToward(marcoX, marcoY, marcoDir, marcoFrame, marcoAnimTimer,
                        car.x, car.y, MARCO_FOLLOW_SPEED);
        } else {
          marcoFrame = 0;
          marcoFollow = false; marcoAboard = true;       // le compagnon remonte
          sceneNpcActive = false;                        // le contact s'en va
          seqKind = SEQ_NONE; cutKind = CUT_NONE;
          if (cutEndText) narrate(cutEndText);           // "Colis livre..."
          if (missionRun.active) enterObjective();       // active l'objectif suivant (deja avance)
          else                   finishMission();
        }
        break;
      }
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
  // Vehicule SPECIFIQUE detruit alors qu'on est A PIED (bail + explosion) : la
  // mort au volant est deja geree par startEndSeq -> on ne couvre que !driving.
  s.missionCarLost = carIsMission && carGone && !driving;
  s.allyDead = allyDead;
  const Objective &cur = def.objectives[missionRun.step];
  if (missionTimedOut(cur, objElapsed)) { failMission("Trop tard ! Mission ratee."); return; }
  if (missionCarLossFail(def, cur, s)) { failMission("La caisse est detruite ! Mission ratee."); return; }
  if (missionAllyDeathFail(def, s)) {
    failMission(def.title && strcmp(def.title, "Embuscade") == 0
                ? "Sarah est morte ! Mission ratee." : "Tony est mort ! Mission ratee.");
    return;
  }
  if (sceneNpcDead) {
    bool racket = def.title && (strcmp(def.title, "Les assurances") == 0 ||
                                strcmp(def.title, "Tournee de Marco") == 0);
    failMission(racket ? "On rackette, on ne tue pas les clients ! Mission ratee."
                       : "Marco : t'as tue notre contact, abruti ! Mission ratee.");
    return;
  }
  // Rencontre a pied avec Marco (TALK) : on ne la valide qu'une fois qu'il a fini
  // de SORTIR et de rejoindre son poste, pour qu'on le voie arriver ("j'arrive !").
  if (cur.type == OBJ_TALK && cur.event == EV_MARCO_JOIN && marcoWaiting) {
    float ddx = marcoX - (float)cur.x, ddy = marcoY - (float)cur.y;
    if (ddx * ddx + ddy * ddy > 4.0f) return;
  }
  if (!missionObjectiveDone(cur, s)) return;

  const Objective &done = def.objectives[missionRun.step];  // objectif accompli
  // Boss vaincu (KILL coriace, count>=5 : Rico/Bruno/Victor) -> death-beat fige.
  bool bossKill = (done.type == OBJ_KILL && done.count >= 5);
  uint8_t ev = missionAdvance(missionRun, def);   // step++ (active=false si fini)
  // EV_DELIVERY et death-beat : le doneText est narre PAR la scene (pas tout de suite).
  if (done.doneText && ev != EV_DELIVERY && !bossKill) narrate(done.doneText);  // message "objectif atteint"
  if (bossKill) { startBossDownCut(done.doneText); return; }   // fige sur le corps + revelation
  if (ev == EV_DELIVERY) {
    // Scene scriptee a destination. Avec compagnon (marcoAboard) il descend
    // livrer puis remonte ; solo (Acte II+, Marco mort) c'est le contact qui
    // vient. La cinematique enchaine elle-meme sur l'objectif suivant.
    const char *l1, *l2; deliveryLines(def.title, l1, l2);
    startDeliveryCut(l1, l2, done.doneText, !marcoAboard);
    return;
  }
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
  // PNJ de scene (contact qui attend) : ecrasable a la voiture -> echec (capte
  // ensuite par missionProgress via sceneNpcDead).
  if (sceneNpcActive && !sceneNpcDead && driving) {
    float spd2 = car.vx * car.vx + car.vy * car.vy;
    if (spd2 > RUNOVER_SPEED2 && fabsf(car.x - sceneNpcX) < TARGET_RUNOVER_DIST &&
        fabsf(car.y - sceneNpcY) < TARGET_RUNOVER_DIST) {
      sceneNpcDead = true;
      targetDownX = (int)sceneNpcX; targetDownY = (int)sceneNpcY; targetDownTimer = PED_DOWN_FRAMES;
      gb.sound.tone(90, 120);
    }
  }
  if (!missionRun.active || !target.active) return;
  if (targetAtkTimer > 0) targetAtkTimer--;
  if (targetStunTimer > 0) { targetStunTimer--; return; }   // au sol : ni poursuite ni coup
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
      targetAtkTimer = ENEMY_MELEE_PERIOD;
      if (enemyMeleeHits()) { gb.sound.tone(90, 70); hurtPlayer(ENEMY_MELEE_DMG, false); }
      else gb.sound.tone(55, 40);
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
      targetAtkTimer = ENEMY_MELEE_PERIOD;
      if (enemyMeleeHits()) { gb.sound.tone(90, 70); hurtPlayer(ENEMY_MELEE_DMG, false); }
      else gb.sound.tone(55, 40);
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
  if (marcoWaiting || marcoFollow || allyStands)  // sort / attend / suit / allie defendu stationnaire
    blitPed(camX, camY, (int)marcoX, (int)marcoY, marcoDir, marcoFrame, allyColor);
}


// PNJ de scene scriptee (contact qui attend a destination). Dessine tant qu'il
// est vivant ; une fois ecrase, c'est le splat (targetDownTimer) qui s'affiche.
static void drawSceneNpc(int camX, int camY) {
  if (!missionRun.active || !sceneNpcActive || sceneNpcDead) return;
  blitPed(camX, camY, (int)sceneNpcX, (int)sceneNpcY, sceneNpcDir, sceneNpcFrame, SCENE_NPC_COLOR);
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
  if (missionRun.active && target.active) {
    if (targetStunTimer > 0) blitDownBody(camX, camY, (int)target.x, (int)target.y, TARGET_COLOR);   // au sol apres un coup
    else blitPed(camX, camY, (int)target.x, (int)target.y, target.dir, target.frame, TARGET_COLOR);
  }
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
