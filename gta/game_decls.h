#pragma once
// game_decls.h - prototypes de toutes les fonctions libres du jeu.
// Permet les appels croises entre modules (inclus une seule fois,
// apres game_state.h, avant setup()/loop() et les modules).

// --- world ---
static int freeRam();
static bool findFootSpot(int cx, int cy, int &ox, int &oy);
static bool findDryFootSpot(int cx, int cy, int &ox, int &oy);
static bool findSidewalkSpot(int cx, int cy, int &ox, int &oy);
static bool findBuildingTileNear(int cx, int cy, int &ox, int &oy);
static bool findRoadSpotNear(int cx, int cy, int &ox, int &oy);
static void readFootInput(int &dx, int &dy);
static int findPoi(const char *name);
static int poiAtTile(int tx, int ty);
static void updateLights();

// --- bande son (mod_music.h) ---
static void musicStart();
static void musicStop();
static void musicTick();

// --- render ---
static void drawTile(uint8_t id, int sx, int sy);
static void blitCar(int camX, int camY, int worldCx, int worldCy, int frameIdx, uint16_t color);
static void blitTruck(int camX, int camY, int worldCx, int worldCy, int frameIdx, const TruckVariant &v);
static void drawGyro(int camX, int camY, int worldCx, int worldCy, uint16_t colorA, uint16_t colorB);
static inline int carFrameIdx();
static void blitSmoke(int scx, int scy, int tier, int frame);
static void blitCarBurnt(int camX, int camY, int worldCx, int worldCy, int frameIdx);
static void blitTruckBurnt(int camX, int camY, int worldCx, int worldCy, int frameIdx);
static void spawnWreck(float wx, float wy, uint8_t frameIdx, float hopx, float hopy, bool truck);
static void updateWrecks(int fcx, int fcy);
static void drawWrecks(int camX, int camY);
static void spawnSmoke(float wx, float wy);
static void updateSmoke();
static void drawSmoke(int camX, int camY);
static void blitPed(int camX, int camY, int worldCx, int worldCy, uint8_t dir, uint8_t frame, uint16_t color, int clipBottom = 0);
static void blitWeapon(int camX, int camY, int worldCx, int worldCy, uint8_t weapon);
static void blitSplat(int camX, int camY, int worldCx, int worldCy);
static void blitDownBody(int camX, int camY, int worldCx, int worldCy, uint16_t color);
static void blitAttackFx(int camX, int camY);
static void blitCopHitFx(int camX, int camY);
static void drawPlayer(int camX, int camY);
static void drawCar(int camX, int camY);
static void drawPhoneBooth(int camX, int camY, int px, int py, uint16_t body, bool canRing);
static bool fireBeingHosed(bool isPlayerCar, int aiIdx);
static void drawWaterJet(int camX, int camY, float wx, float wy, int tgx, int tgy);
static void drawCarSmoke(int camX, int camY);
static void drawAiCarSmoke(int camX, int camY);
static void drawBoom(int camX, int camY);
static void drawGroundFires(int camX, int camY);

// --- ai ---
static bool aiFindTileInRing(int ccx, int ccy, int minR, int maxR, AiClassify classify, int &otx, int &oty);
static inline int copPedSpawnPct(uint8_t level);
static inline int policeCarPct(uint8_t level);
static void aiRespawnCar(AiCar &c, int ccx, int ccy);
static void aiRespawnPed(AiPed &p, int ccx, int ccy);
static bool aiFindWalkTileNear(int cx, int cy, int &otx, int &oty);
static void aiEjectDriver(int atx_px, int aty_px, bool knockedDown, bool asCop, uint8_t carDir, bool inWater = false);
static void startPanic(AiPed &p, int srcx, int srcy);
static void startBackoff(AiCar &c);
static void startCarFlee(AiCar &c);
static void alarmCarsAround(int sx, int sy, int radius);
static void deadPed(AiPed &p);
static void knockDownPed(AiPed &p);
static void hitPed(AiPed &p, bool lethal);
static void policeCarStep(AiCar &c, int fcx, int fcy);
static void aiCarStepAway(AiCar &c, int fcx, int fcy);
static bool nudgeAiCar(AiCar &c, float ox, float oy);
static void carHonk();
static void dispatchAmbulance(int x, int y);
static void dispatchFireTruck(int x, int y, bool fireIsCar, int8_t fireSlot, int8_t fireCarIdx = -1);
static void dispatchLifeboat(int x, int y, int8_t pedIdx);
static void responderUpdate(int fcx, int fcy);
static void responderDraw(int camX, int camY);
static void aiUpdate(int fcx, int fcy);
static void aiDraw(int camX, int camY);
static bool npcWalkToward(float &x, float &y, uint8_t &dir, uint8_t &frame, uint8_t &animTimer, float tx, float ty, float speed);

// --- combat ---
static void onCarWrecked();
static void explodeCarAt(int wx, int wy);
static void fireBullets(int pcx, int pcy);
static void fireCopBullet(float sx, float sy, int tx, int ty);
static uint16_t copShootPeriod();
static void wreckAiCar(AiCar &c);
static void igniteAiCar(AiCar &c, float hopx, float hopy, bool credit);
static void ignitePlayerCar();
static void fireProximityTick(float fx, float fy);
static void updateAiCarFuses();
static void updateBullets();
static void drawBullets(int camX, int camY);
static void spawnProjectile(uint8_t kind, int pcx, int pcy);
static void updateProjectiles();
static void drawProjectiles(int camX, int camY);
static void tryAttack();
static void updateCarFuse();
static void updateRunawayCar();
static int8_t spawnGroundFireNoDispatch(float fx, float fy);
static void spawnGroundFire(float fx, float fy);
static void updateGroundFires(int fcx, int fcy);

// --- mission ---
static bool spawnTargetWander(int pcx, int pcy);
static bool spawnTargetWanderNear(int wx, int wy);
static void spawnTargetAt(int wx, int wy);
static void clearEnemies();
static int enemiesAliveCount();
static void spawnEnemiesForObjective(const Objective &o);
static void spawnCasinoBouncers(int pcx, int pcy);
static void hitEnemy(Enemy &e, bool lethal);
static bool enemyMeleeHits();
static void updateEnemies(int fcx, int fcy);
static void drawEnemies(int camX, int camY);
static void buildMissionRuntime(uint8_t m);
static bool bossTauntLines(const char *title, const char *&l1, const char *&l2);
static void enterObjective();
static void startMission(uint8_t m);
static void failMission(const char *msg);
static void killTarget(int px, int py);
static void finishMission();
static void startMarcoDeathCut();
static void startMarcoLeaveCut(int doorX, int doorY);
static void startTauntCut(const char *l1, const char *l2);
static void cutsceneUpdate();
static void missionProgress();
static void missionUpdate(int fcx, int fcy);
static void marcoUpdate(int fcx, int fcy);
static void drawPhones(int camX, int camY);
static void drawMissionCar(int camX, int camY);
static void drawPlayerFireJet(int camX, int camY);
static void drawMarco(int camX, int camY);
static void drawSceneNpc(int camX, int camY);
static void drawMarker(int camX, int camY);
static void drawTarget(int camX, int camY);
static void drawMissionArrow(int camX, int camY, int fcx, int fcy);

// --- economy ---
static void spawnLoot(int wx, int wy, uint8_t kind, uint8_t weapon, int amount);
static void dropLoot(int wx, int wy);
static void addMoney(int32_t amount);
static void barDrink();
static void robStore();
static void bureauVisit();
static void updateMoneyAnim();
static void drawWeaponPickups(int camX, int camY);
static void drawLoot(int camX, int camY);
static void tryPickupLoot(int pcx, int pcy);
static void tryPickupWeapons(int pcx, int pcy);
static void drawAmmuShop(int camX, int camY, int px, int py);
static void drawAmmuShops(int camX, int camY);
static void drawCasseZone(int camX, int camY);
static void shopBuy();
static void updateShop();
static void drawShop();
static void casinoBouncerThrowOut();
static void casinoSpinStart();
static void updateCasino();
static void fbFill(int x, int y, int w, int h, uint16_t col);
static void fbRect(int x, int y, int w, int h, uint16_t col);
static void drawCasino();

// --- player ---
static void respawnAtPoi(const char *poiName);
static void reviveCommon();
static void giveBodyArmor();
static void wastedPlayer();
static void bustedPlayer();
static void hurtPlayer(int dmg, bool byCop);
static uint16_t getupFrames();
static bool carOnSidewalk();
static void knockdownPlayer(bool arrest);
static void dragPlayerFromCar();
static void updatePlayerDown();
static bool playerOverlapsCar(float cx, float cy);
static void pushPlayerOffCar(float cx, float cy);
static void runOverPlayer(float cx, float cy);

// --- hud ---
static void narrate(const char *s);
static void narrUpdate();
static void drawPoiHud(int fcx, int fcy);
static void drawMissionStatus();
static void printShadowCol(int x, int y, const char *s, uint16_t col);
static void printShadow(int x, int y, const char *s);
static void drawSlideBanner(const char *msg, uint16_t color, int timer, int total);
static void blitWeaponHudIcon(int sx, int sy, uint8_t weapon);
static void blitIcon5(int sx, int sy, const char *const rows[5], uint16_t color);
static void drawTopHud();
static void narrDraw();

// --- sequence ---
static void startEndSeq(uint8_t kind, const char *msg, const char *poi, bool fromCar);
static void startHealSeq();
static void startSleepSeq();
static void startSpraySeq(int sprayIdx);
static void startCrushSeq();
static void repaintCar();
static void updateSequence();
static void drawThickLine(int x0, int y0, int x1, int y1, uint16_t col);
static void drawBox(int cx, int cy, int r, uint16_t fill, uint16_t edge);
#if CITY_HAS_CASSE
static void drawJunkyardRig(int camX, int camY);
#endif
static void drawSequence(int camX, int camY);

// --- save ---
static bool profileRead(uint8_t slot, SaveProfile &p);
static void profileWriteCurrent();
static void profileLoadCurrent();
static void updateProfileMenu();
static void drawProfileMenu();
static uint16_t slotPal(char c);
static void blitSlotSym(int sx, int sy, uint8_t sym);

