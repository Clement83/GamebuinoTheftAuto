#pragma once
// mod_economy.h - economie: argent, boutiques, casino, butin, ramassages
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


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


// Point d'entree GENERIQUE de tout mouvement d'argent (mission, butin, broyage,
// braquage... mais aussi soin, Pay'n'Spray, tournee au bar) : credite/debite la
// cagnotte et arme l'animation. Gain -> "+$X" qui monte, compteur qui roule en
// ~2 s. Depense -> "-$X" rouge qui tombe, compteur qui chute plus vite (~0.7 s).
// N'emet pas de son : l'appelant garde le sien, le compteur fait ses "tic".
static void addMoney(int32_t amount) {
  if (amount == 0) return;
  playerMoney += amount;
  if (amount > 0) {                                      // gain : le compteur monte
    moneyGainAmt = amount; moneyGainTimer = MONEY_GAIN_FRAMES;
    moneyLossTimer = 0;                                  // un gain efface un "-$X" en cours
    int32_t step = (playerMoney - moneyShown) / MONEY_ROLL_FRAMES;  // pour finir en ~2 s
    if (step < 1) step = 1;
    if (step > moneyRollStep) moneyRollStep = step;      // mouvements cumules : au moins aussi vite
  } else {                                               // depense : le compteur chute
    moneyLossAmt = -amount; moneyLossTimer = MONEY_GAIN_FRAMES;
    moneyGainTimer = 0;                                  // une depense efface un "+$X" en cours
    int32_t step = (moneyShown - playerMoney) / MONEY_LOSS_FRAMES;  // chute en ~0.7 s
    if (step < 1) step = 1;
    if (step > moneyRollStep) moneyRollStep = step;
  }
}

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


// Anime la cagnotte chaque frame : moneyShown rattrape playerMoney en roulant,
// vers le haut (gain) ou vers le bas (depense). Petit "tic" sonore en roulant.
static void updateMoneyAnim() {
  if (moneyShown < playerMoney) {                        // gain : on roule vers le total
    int32_t diff = playerMoney - moneyShown;
    int32_t s = diff < moneyRollStep ? diff : moneyRollStep;
    moneyShown += s;
    if ((moneyShown & 3) == 0) gb.sound.playTick();      // tic discret du compteur
  } else if (moneyShown > playerMoney) {                 // depense : on chute vers le total
    int32_t diff = moneyShown - playerMoney;
    int32_t s = diff < moneyRollStep ? diff : moneyRollStep;
    moneyShown -= s;
    if ((moneyShown & 3) == 0) gb.sound.playTick();      // tic discret du compteur
  } else {
    moneyRollStep = 1;                                   // au repos : vitesse remise a zero
  }
  if (moneyGainTimer > 0) moneyGainTimer--;
  if (moneyLossTimer > 0) moneyLossTimer--;
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
// Pay'n'Spray : plus d'overlay -- les garages sont desormais de VRAIS stamps
// (gar_facade/gar_sign/gar_door) tamponnes dans la grille, dessines par le
// moteur de tuiles. La porte gar_door est l'entree carrossable.

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


// Achat de l'arme surlignee : 1re fois = prix d'arme (+ 1 lot de munitions),
// ensuite = prix de rechargement. Refuse (son grave) si solde insuffisant.
static void shopBuy() {
  if (shopSel == SHOP_ARMOR) {                  // ligne gilet pare-balles
    if (playerHearts >= PLAYER_HP_MAX) { gb.sound.tone(120, 120); return; }  // deja plein
    if (playerMoney < ARMOR_PRICE)     { gb.sound.tone(120, 120); return; }  // trop cher
    addMoney(-ARMOR_PRICE);
    giveBodyArmor();
    return;
  }
  uint8_t w = shopSel;
  bool owned = weaponOwned[w];
  int32_t cost = owned ? AMMO_PRICE[w] : WEAPON_PRICE[w];
  if (playerMoney < cost) { gb.sound.tone(120, 120); return; }   // trop cher
  addMoney(-cost);
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
