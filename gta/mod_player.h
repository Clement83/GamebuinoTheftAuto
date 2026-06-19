#pragma once
// mod_player.h - joueur: degats, KO, reanimation, armure
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


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
    allyStands = false; allyDead = false;
    sceneNpcActive = false; sceneNpcDead = false;
    mCarActive = false; carIsMission = false;
    storyMissionActive = false; killerChase = false;
    clearEnemies();
    missionFailedTimer = MISSION_FAIL_FRAMES;   // bandeau "MISSION RATEE" apres la TP
  }
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
