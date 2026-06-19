#pragma once
// mod_sequence.h - sequences: wasted/busted/spray/crush/heal/sleep
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).


// Demarre une sequence de mort/arrestation : on fige le joueur, on prend
// l'argent tout de suite, et on laisse la cinematique (message -> ecran noir ->
// TP) se derouler dans updateSequence(). fromCar => on attend d'abord que
// l'explosion de la caisse finisse de jouer.
static void startEndSeq(uint8_t kind, const char *msg, const char *poi, bool fromCar) {
  if (seqKind != SEQ_NONE) return;             // deja en cinematique : on ignore
  seqKind = kind; overlayMsg = msg; seqPoi = poi;
  uint8_t lvl = wanted.level <= WANTED_MAX ? wanted.level : WANTED_MAX;
  int32_t fine = (kind == SEQ_BUSTED) ? BUST_FINE[lvl]   // arrestation : amende police
                                      : HOSP_BILL;        // mort : facture hopital fixe
  if (fine > playerMoney) fine = playerMoney;  // jamais plus que ce qu'on a
  addMoney(-fine);
  if (fromCar) { seqPhase = PH_EXPLODE; seqTimer = BOOM_FRAMES + 6; }
  else { seqPhase = PH_MSG; seqTimer = SEQ_MSG_FRAMES; gb.sound.playCancel(); }
}


// Soin a l'hopital (entree a pied, blesse) : on prend le fric tout de suite et
// on lance une courte anim (croix verte + jingle) ; la vie remonte a la fin.
static void startHealSeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_HEAL; seqPhase = PH_HEAL; seqTimer = SEQ_HEAL_FRAMES;
  int32_t pay = playerMoney < HEAL_COST ? playerMoney : HEAL_COST;
  addMoney(-pay);
  narrate("Soigne. -$40");
}


// Dormir a la Planque (entree a pied, presser A) : ecran noir bref puis reveil
// devant la porte, vie pleine (3 coeurs ROUGES, gilet retire). Gratuit.
static void startSleepSeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_SLEEP; seqPhase = PH_FADE; seqTimer = SEQ_SLEEP_FRAMES;
  gb.sound.tone(330, 120); gb.sound.tone(247, 180);   // petit air de dodo
  profileWriteCurrent();                               // dormir a la Planque = sauvegarder
  narrate("Partie sauvegardee.");
}


// Entree dans un Pay'n'Spray : demarre la cinematique (la caisse rentre, on
// entend la bombe, elle ressort repeinte). Le joueur attend, fige.
static void startSpraySeq(int sprayIdx) {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_SPRAY; seqPhase = PH_IN; seqTimer = SEQ_IN_FRAMES;
  sprayActive = (int8_t)sprayIdx;              // garage en cours (sortie auto en PH_OUT)
  car.vx = 0.0f; car.vy = 0.0f;                // on coupe l'elan une fois dans le garage
}


// Fin d'amorcage de la grue : demarre le broyage. Prime selon les PV restants
// (epave plus chere si la caisse est saine). On fige la caisse.
static void startCrushSeq() {
  if (seqKind != SEQ_NONE) return;
  seqKind = SEQ_CRUSH; seqPhase = PH_SWING; seqTimer = SEQ_SWING_FRAMES;
  crushReward = 40 + (carHp > 0 ? carHp : 0) * 4;   // ~40..160 $
  car.vx = 0.0f; car.vy = 0.0f;
  gb.sound.tone(200, 120);                           // demarrage de la grue
}


// Repeinture (rouler dans un Pay'n'Spray) : nouvelle couleur de caisse, recherche
// police a zero, debit $ (gratuit si fauche pour ne pas bloquer a 5 etoiles).
static void repaintCar() {
  uint16_t nc;
  do { nc = AI_PALETTE[aiRngNext(aiRng) % AI_PALETTE_N]; } while (nc == carColor);
  carColor = nc;
  carHp = CAR_MAX_HP; carFuse = 0; carRunaway = false;   // service complet : reparee, feu eteint
  wantedClear(wanted);
  int32_t pay = playerMoney < SPRAY_COST ? playerMoney : SPRAY_COST;
  addMoney(-pay);
  narrate("Repeinte. Plus recherche.");
  gb.sound.playOK();
}


// Fait avancer la cinematique en cours (mort / arrestation / repeinture). La
// teleportation et la repeinture se font sur les transitions de phase, pas a
// l'entree : on laisse le temps de voir l'explosion, le message, l'ecran noir,
// la bombe de peinture. Le monde, lui, continue de tourner autour.
static void updateSequence() {
  if (seqKind == SEQ_NONE) return;
  if (seqKind == SEQ_CUT) { cutsceneUpdate(); return; }   // cinematique de mission
  if (seqTimer > 0) {
    if (seqKind == SEQ_SPRAY && seqPhase == PH_SPRAY && (seqTimer % 8) == 0)
      gb.sound.tone(2200, 70);                   // pschitt de la bombe de peinture
    if (seqKind == SEQ_HEAL) {                    // jingle de soin ascendant (do-mi-sol)
      if (seqTimer == SEQ_HEAL_FRAMES - 1) gb.sound.tone(523, 90);
      else if (seqTimer == SEQ_HEAL_FRAMES - 10) gb.sound.tone(659, 90);
      else if (seqTimer == SEQ_HEAL_FRAMES - 19) gb.sound.tone(784, 140);
    }
    if (seqKind == SEQ_CRUSH) {
      if ((seqPhase == PH_SWING || seqPhase == PH_CARRY) && (seqTimer % 6) == 0)
        gb.sound.tone(180 + (seqTimer & 31) * 6, 50);  // vrombissement du moteur de grue
      else if (seqPhase == PH_CRUSH && (seqTimer % 5) == 0)
        gb.sound.tone(70, 90);                     // grincement du broyeur
    }
    seqTimer--; return;
  }
  switch (seqPhase) {
    case PH_EXPLODE:                             // l'explosion a fini de jouer
      seqPhase = PH_MSG; seqTimer = SEQ_MSG_FRAMES; gb.sound.playCancel(); break;
    case PH_MSG:                                 // le bandeau a tenu sa duree
      seqPhase = PH_FADE; seqTimer = SEQ_FADE_FRAMES; break;
    case PH_FADE:                                // derriere l'ecran noir : revie + TP
      if (seqKind == SEQ_SLEEP) {                 // dodo : juste soin + repositionnement
        playerHearts = PLAYER_HEARTS_MAX;         // reveil : 3 coeurs rouges
        respawnAtPoi("Planque");                  // devant la porte, pret a partir
      } else {                                    // mort / arrestation : revie complete
        reviveCommon(); respawnAtPoi(seqPoi);
      }
      seqKind = SEQ_NONE; seqPhase = 0; overlayMsg = nullptr; break;
    case PH_IN:                                  // la caisse a disparu dans le garage
      seqPhase = PH_SPRAY; seqTimer = SEQ_SPRAY_FRAMES; break;
    case PH_SPRAY:                               // bombe finie : repeinte, elle ressort
      repaintCar();
      if (sprayActive >= 0 && sprayActive < NUM_SPRAYS) {   // ressort SEULE sur la route au sud de la porte
        int rtx = citySprays[sprayActive].tx, rty = citySprays[sprayActive].ty + 1;
        car.x = (float)(rtx * TILE_W + TILE_W / 2);
        car.y = (float)(rty * TILE_H + TILE_H / 2);
        car.angle = 1.5708f;                     // cap sud (vers l'exterieur du garage)
        car.vx = 0.0f; car.vy = 0.0f;
      }
      seqPhase = PH_OUT; seqTimer = SEQ_OUT_FRAMES; break;
    case PH_OUT:                                 // ressortie : on rend la main au joueur
      seqKind = SEQ_NONE; seqPhase = 0; sprayActive = -1; break;
    case PH_HEAL:                                // soin termine : vie au max
      playerHearts = PLAYER_HEARTS_MAX;
      seqKind = SEQ_NONE; seqPhase = 0; break;
    case PH_SWING:                               // fleche au-dessus de la voiture : on l'agrippe
      seqPhase = PH_CARRY; seqTimer = SEQ_CARRY_FRAMES;
      gb.sound.tone(440, 90); break;             // clac du crochet
    case PH_CARRY:                               // voiture au-dessus du broyeur : on la lache
      seqPhase = PH_CRUSH; seqTimer = SEQ_CRUSH_FRAMES;
      gb.sound.tone(60, 200); break;             // gros craquement
    case PH_CRUSH:                               // broyee : prime (joueur deja a pied)
      addMoney(crushReward);
      carGone = true;
      // OBJ_CRUSH : c'est la fin du broyage qui valide l'objectif (pas l'arrivee).
      if (missionRun.active && curObjs[missionRun.step].type == OBJ_CRUSH) {
        missionCrushDone = true; carIsMission = false;
      }
      narrate("Epave vendue !");
      gb.sound.tone(988, 60); gb.sound.playOK();  // cha-ching
      seqPhase = PH_EJECT; seqTimer = SEQ_EJECT_FRAMES; break;
    case PH_EJECT:                               // prime affichee : on rend la main
      seqKind = SEQ_NONE; seqPhase = 0; break;
  }
}


// Trait epais (2 px) entre deux points ecran, par interpolation entiere. Sert a
// la fleche de la grue (vue de dessus).
static void drawThickLine(int x0, int y0, int x1, int y1, uint16_t col) {
  int dx = x1 - x0, dy = y1 - y0;
  int n = (abs(dx) > abs(dy) ? abs(dx) : abs(dy));
  if (n < 1) n = 1;
  for (int i = 0; i <= n; i++) {
    int x = x0 + dx * i / n, y = y0 + dy * i / n;
    for (int oy = 0; oy <= 1; oy++)
      for (int ox = 0; ox <= 1; ox++) {
        int xx = x + ox, yy = y + oy;
        if (xx >= 0 && xx < SCREEN_W && yy >= 0 && yy < SCREEN_H)
          fb[yy * SCREEN_W + xx] = col;
      }
  }
}


// Petit carre plein cerne (cabine de grue / bouche du broyeur), centre ecran.
static void drawBox(int cx, int cy, int r, uint16_t fill, uint16_t edge) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++) {
      int x = cx + dx, y = cy + dy;
      if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) continue;
      bool e = (dx <= -r || dx >= r || dy <= -r || dy >= r);
      fb[y * SCREEN_W + x] = e ? edge : fill;
    }
}


// Grue + broyeur de La Casse dessines EN PERMANENCE (vue de dessus), au repos :
// le bras est parque en pointant la zone de depose et le broyeur reste visible.
// Plante sur la base de grue (tuile junk_crane). Pendant le broyage, c'est
// drawSequence qui anime la grue -> on s'efface pour ne pas dessiner deux fois.
#if CITY_HAS_CASSE
static void drawJunkyardRig(int camX, int camY) {
  if (seqKind == SEQ_CRUSH) return;
  const float SWEEP = 2.3f;                        // meme amplitude que la cinematique
  float px = CITY_CRANE_TX, py = CITY_CRANE_TY;    // pivot (base de grue)
  float dxz = (float)CITY_CASSE_TX - px, dyz = (float)CITY_CASSE_TY - py;
  float L = sqrtf(dxz * dxz + dyz * dyz);
  if (L < 10.0f) L = 10.0f;
  float aZone = atan2f(dyz, dxz);                  // bras au repos -> zone de depose
  float tx = px + L * cosf(aZone), ty = py + L * sinf(aZone);
  float bx = px + L * cosf(aZone + SWEEP), by = py + L * sinf(aZone + SWEEP);  // broyeur
  int pxS = (int)px - camX, pyS = (int)py - camY;
  int txS = (int)tx - camX, tyS = (int)ty - camY;
  int bxS = (int)bx - camX, byS = (int)by - camY;
  drawBox(bxS, byS, 4, 0x18E3, 0xFFE0);            // fosse du broyeur (contour jaune)
  drawThickLine(pxS, pyS, txS, tyS, 0x8410);       // bras parque (gris)
  drawBox(txS, tyS, 1, 0xFD20, 0xFD20);            // crochet
  drawBox(pxS, pyS, 3, 0x4208, 0xFD20);            // cabine (par-dessus la base)
}
#endif


// Calque cinematique dessine par-dessus la scene selon la phase courante.
static void drawSequence(int camX, int camY) {
  if (seqKind == SEQ_NONE) return;
  if (seqKind == SEQ_SPRAY) {                    // bombe : nuage de peinture sur le garage
    if (seqPhase != PH_SPRAY) return;
    int cx = (int)car.x - camX, cy = (int)car.y - camY;
    static const uint16_t puff[3] = { 0x07FF, 0xFFE0, 0xFD20 };
    for (int k = 0; k < 12; k++) {
      int a = missionAnim * 3 + k * 37;
      int x = cx + (a % 13) - 6, y = cy + ((a >> 2) % 13) - 6;
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = puff[k % 3];
    }
    return;
  }
  if (seqKind == SEQ_HEAL) {                       // croix verte pulsante au-dessus du joueur
    int px = playerX + PLAYER_W / 2 - camX, py = playerY - 5 - camY;
    int s = 2 + ((missionAnim >> 2) & 1);          // taille qui pulse (2/3 px)
    for (int d = -s; d <= s; d++) {
      int hx = px + d, hy = py + d;
      if (px >= 0 && px < SCREEN_W && hy >= 0 && hy < SCREEN_H) fb[hy * SCREEN_W + px] = 0x07E0;
      if (hx >= 0 && hx < SCREEN_W && py >= 0 && py < SCREEN_H) fb[py * SCREEN_W + hx] = 0x07E0;
    }
    return;
  }
  if (seqKind == SEQ_CRUSH) {                      // grue vue de dessus + broyeur
#if CITY_HAS_CASSE
    // Geometrie (px monde) : cabine = pivot, voiture garee = bout de fleche au
    // repos, broyeur = bout de fleche apres un balayage fixe. La fleche pivote
    // de la voiture (PH_SWING) puis revient avec elle vers le broyeur (PH_CARRY).
    const float SWEEP = 2.3f;                      // amplitude du balayage (rad)
    float px = CITY_CRANE_TX, py = CITY_CRANE_TY;  // pivot (cabine)
    float dxc = car.x - px, dyc = car.y - py;
    float L = sqrtf(dxc * dxc + dyc * dyc);
    if (L < 10.0f) L = 10.0f;                       // bras minimal lisible
    float aCar = atan2f(dyc, dxc);                  // angle pivot->voiture
    float aBro = aCar + SWEEP;                      // angle pivot->broyeur
    float bx = px + L * cosf(aBro), by = py + L * sinf(aBro);  // broyeur (monde)
    int pxS = (int)px - camX, pyS = (int)py - camY;            // pivot ecran
    int bxS = (int)bx - camX, byS = (int)by - camY;            // broyeur ecran

    // broyeur (bouche raye jaune/noir) + cabine de la grue : toujours visibles.
    drawBox(bxS, byS, 4, 0x18E3, 0xFFE0);          // fosse sombre, contour jaune
    drawBox(pxS, pyS, 3, 0x4208, 0xFD20);          // cabine gris fonce, contour orange

    if (seqPhase == PH_SWING || seqPhase == PH_CARRY) {
      bool carry = (seqPhase == PH_CARRY);
      int tot = carry ? SEQ_CARRY_FRAMES : SEQ_SWING_FRAMES;
      float prog = (float)(tot - seqTimer) / (float)tot;        // 0..1
      // SWING : du broyeur vers la voiture ; CARRY : de la voiture au broyeur.
      float ang = carry ? (aCar + SWEEP * prog) : (aCar + SWEEP * (1.0f - prog));
      float tx = px + L * cosf(ang), ty = py + L * sinf(ang);   // bout de fleche
      int txS = (int)tx - camX, tyS = (int)ty - camY;
      drawThickLine(pxS, pyS, txS, tyS, 0xFD20);                // la fleche
      if (carry) {                                              // voiture portee
        int idx = (int)(ang / TWO_PI * CAR_FRAMES + 0.5f) % CAR_FRAMES;
        if (idx < 0) idx += CAR_FRAMES;
        blitCar(camX, camY, (int)tx, (int)ty, idx, carColor);
      }
      drawBox(txS, tyS, 1, 0xFD20, 0xFD20);                     // crochet
    } else if (seqPhase == PH_CRUSH) {             // voiture lachee : ecrasee au broyeur
      drawThickLine(pxS, pyS, bxS, byS, 0xFD20);                // fleche au-dessus du broyeur
      int p = SEQ_CRUSH_FRAMES - seqTimer;
      int h = 1 + (7 * seqTimer) / SEQ_CRUSH_FRAMES;
      for (int dy = -h; dy <= h; dy++)
        for (int dx = -5; dx <= 5; dx++) {
          int x = bxS + dx, y = byS + dy;
          if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0x632C;
        }
      for (int k = 0; k < 6; k++) {                // eclats de metal
        int a = p * 5 + k * 41;
        int x = bxS + (a % 15) - 7, y = byS + ((a >> 2) % 9) - 4;
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) fb[y * SCREEN_W + x] = 0xC618;
      }
    } else {                                       // PH_EJECT : la prime s'affiche
      // via le compteur HUD generique (addMoney au PH_CRUSH) : pas de "+$X"
      // centre ici, sinon double affichage avec la cagnotte qui roule.
    }
#endif
    return;
  }
  if (seqPhase == PH_FADE) {                      // ecran noir plein avant la TP
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = 0x0000;
    if (seqKind == SEQ_SLEEP)                      // dodo : petit "Zzz" centre
      printShadowCol((SCREEN_W - 3 * 4) / 2, SCREEN_H / 2 - 3, "Zzz", 0x7BEF);
    return;
  }
  if (seqPhase == PH_MSG && overlayMsg) {         // bandeau mort / arrestation
    uint16_t col = (seqKind == SEQ_WASTED) ? 0xF800 : 0x07FF;  // rouge / cyan
    drawSlideBanner(overlayMsg, col, (int)seqTimer, SEQ_MSG_FRAMES);
  }
  // PH_EXPLODE : rien a ajouter, drawBoom fait le spectacle.
}
