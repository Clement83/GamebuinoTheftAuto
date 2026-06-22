#pragma once
// mod_save.h - sauvegarde: profils, menu de selection, slot
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


// Lit un profil depuis la SD. Renvoie false (et p.magic force a 0) si vide.
static bool profileRead(uint8_t slot, SaveProfile &p) {
  p.magic = 0;                            // get() zero-remplit un blob absent
  gb.save.get(slot, p);
  return profileValid(p);
}


// Ecrit l'etat de jeu courant dans le profil actif (appel a la Planque).
static void profileWriteCurrent() {
  SaveProfile p;
  profilePack(p, playerMoney, campaignStep, weaponOwned, weaponAmmo);
  gb.save.set(currentProfile, p);
}


// Charge le profil actif sur l'etat de jeu courant (s'il existe). Sans profil,
// les valeurs de depart posees par setup() restent en place (nouvelle partie).
static void profileLoadCurrent() {
  SaveProfile p;
  if (profileRead(currentProfile, p))
    profileApply(p, playerMoney, campaignStep, weaponOwned, weaponAmmo);
  moneyShown = playerMoney;               // resync du compteur HUD
}


// --- Ecran de selection de profil (modal, au boot) --------------------------

// Navigation : HAUT/BAS choisit un slot, A le lance (charge le profil ou
// demarre une partie neuve sur un slot vide), B efface le slot (2 appuis pour
// confirmer). Le monde a deja ete construit par setup() avec les valeurs de
// depart ; charger un profil ne fait que les recouvrir.
static void updateProfileMenu() {
  if (gb.buttons.pressed(BUTTON_UP)) {
    menuSel = (uint8_t)((menuSel + NUM_PROFILES - 1) % NUM_PROFILES);
    menuConfirmDelete = false; gb.sound.playTick();
  }
  if (gb.buttons.pressed(BUTTON_DOWN)) {
    menuSel = (uint8_t)((menuSel + 1) % NUM_PROFILES);
    menuConfirmDelete = false; gb.sound.playTick();
  }
  if (gb.buttons.pressed(BUTTON_B)) {
    SaveProfile p;
    if (!profileRead(menuSel, p)) { gb.sound.playCancel(); return; }  // deja vide
    if (!menuConfirmDelete) { menuConfirmDelete = true; gb.sound.playTick(); return; }
    gb.save.del(menuSel); menuConfirmDelete = false; gb.sound.playCancel();
    return;
  }
  if (gb.buttons.pressed(BUTTON_A)) {
    currentProfile = menuSel;
    profileLoadCurrent();                 // applique le profil s'il existe
    // CHEAT TEMPORAIRE (a supprimer) : bazooka + molotov + cash, meme apres chargement.
    weaponOwned[WEAPON_BAZOOKA] = true; weaponAmmo[WEAPON_BAZOOKA] = 10;
    weaponOwned[WEAPON_MOLOTOV] = true; weaponAmmo[WEAPON_MOLOTOV] = 10;
    playerMoney += 5000; moneyShown = playerMoney;
    inMenu = false; menuConfirmDelete = false;
    gb.sound.playOK();
  }
}


static void drawProfileMenu() {
  fb = gb.display._buffer;
  for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = 0x0008;       // fond bleu nuit
  printShadow(1, 1, "GTA DEMAKE");
  printShadowCol(1, 9, "Choisis un profil", 0xC618);

  for (uint8_t s = 0; s < NUM_PROFILES; s++) {
    int y = 20 + s * 12;
    if (s == menuSel)
      for (int yy = y - 1; yy < y + 9 && yy < SCREEN_H; yy++)
        for (int xx = 0; xx < SCREEN_W; xx++) fb[yy * SCREEN_W + xx] = 0x2945;
    SaveProfile p;
    char line[24];
    if (profileRead(s, p)) {
      int done = (p.campaignStep > STORY_LEN) ? STORY_LEN : (int)p.campaignStep;
      snprintf(line, sizeof(line), "P%d $%ld M%d/%d", s + 1, (long)p.money, done, (int)STORY_LEN);
    } else {
      snprintf(line, sizeof(line), "P%d  - VIDE -", s + 1);
    }
    printShadow(3, y, line);
  }

  if (menuConfirmDelete) printShadowCol(1, 57, "B encore = effacer", 0xF800);
  else                   printShadow(1, 57, "A:jouer  B:effacer");
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
