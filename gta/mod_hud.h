#pragma once
// mod_hud.h - HUD: bandeaux, narration, icones, statut mission
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


// Empile un message de narration (pointeur flash, pas de copie). File de 4.
static void narrate(const char *s) {
  if (narrCount < 4) { narrQueue[(narrHead + narrCount) % 4] = s; narrCount++; }
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
  if (missionFireTimer > 0) {                          // feu de mission allume : decompte avant echec
    snprintf(buf, sizeof(buf), "%ds", (missionFireTimer + 24) / 25);   // ~25 fps -> secondes
  } else if (o.type == OBJ_BEAT) {
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
  int gx = 2 + (int)strlen(money) * 4 + 2;                  // juste a droite du montant
  // "+$X" flottant qui remonte depuis sous la cagnotte vers celle-ci (a chaque gain).
  if (moneyGainTimer > 0) {
    char g[12]; snprintf(g, sizeof(g), "+$%ld", (long)moneyGainAmt);
    int rise = MONEY_GAIN_FRAMES - moneyGainTimer;          // 0..N : il monte
    int gy = 18 - rise / 2;                                 // de y~18 vers la cagnotte (y~8)
    if (!(moneyGainTimer < 4 && (moneyGainTimer & 1)))       // clignote en fin de course
      printShadowCol(gx, gy, g, 0xFFE0);                    // jaune (billets qui filent)
  }
  // "-$X" flottant qui tombe depuis la cagnotte vers le bas (a chaque depense).
  if (moneyLossTimer > 0) {
    char g[12]; snprintf(g, sizeof(g), "-$%ld", (long)moneyLossAmt);
    int fall = MONEY_GAIN_FRAMES - moneyLossTimer;          // 0..N : il descend
    int gy = 8 + fall / 2;                                  // de la cagnotte (y~8) vers y~18
    if (!(moneyLossTimer < 4 && (moneyLossTimer & 1)))       // clignote en fin de course
      printShadowCol(gx, gy, g, 0xF800);                    // rouge (le fric qui s'envole)
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
