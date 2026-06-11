// Test host de la logique de mission (mission.h, pur, sans Gamebuino).
// Compile en natif :
//   g++ -std=c++11 -I gta tools/host_test/test_mission_host.cpp -o /tmp/mission_host && /tmp/mission_host
#include <cstdio>
#include <stdint.h>
#include "mission.h"

static int failures = 0;
static void check(const char *what, bool ok) {
  if (!ok) { printf("FAIL %s\n", what); failures++; }
}

// Petite carte 8x8 tuiles. Par defaut tout en trottoir (4). On pose une colonne
// d'immeubles (6) pour bloquer la vue.
//   . = trottoir(4), # = immeuble(6), ~ = route_v(2)
static void buildGrid(uint8_t *g, int w, int h, uint8_t fill) {
  for (int i = 0; i < w * h; i++) g[i] = fill;
}

int main() {
  const int W = 12, H = 12;
  uint8_t g[W * H];

  // --- Ligne de vue degagee sur trottoirs, a portee ---
  {
    buildGrid(g, W, H, 4);
    // joueur tuile (1,1) -> centre (12,12) ; cible tuile (5,1) -> (44,12)
    check("LOS degagee courte", missionLineOfSight(g, W, H, 12, 12, 44, 12, 64));
  }
  // --- Ligne de vue coupee par un immeuble entre les deux ---
  {
    buildGrid(g, W, H, 4);
    for (int ty = 0; ty < H; ty++) g[ty * W + 3] = 6;  // mur d'immeubles col 3
    check("LOS bloquee par immeuble", !missionLineOfSight(g, W, H, 12, 12, 44, 12, 64));
  }
  // --- Ligne de vue degagee mais hors de portee ---
  {
    buildGrid(g, W, H, 4);
    // (8,8) a (8, 8+70) -> distance 70 > 64
    check("LOS hors portee", !missionLineOfSight(g, W, H, 8, 8, 8, 78, 64));
  }

  // --- Fuite : la cible choisit la sortie qui eloigne du joueur ---
  {
    buildGrid(g, W, H, 4);                  // tout franchissable
    // cible en (5,5), joueur a l'OUEST en (1,5)px -> doit fuir vers l'EST (dir 1).
    uint32_t rng = 0xABCDEF;
    int pcx = 1 * 8 + 4, pcy = 5 * 8 + 4;   // joueur ouest
    uint8_t d = missionFleeExit(g, W, H, 5, 5, DIR_E, pcx, pcy, rng);
    check("fuite vers l'est (loin du joueur ouest)", d == DIR_E);
    // joueur au SUD -> fuit vers le NORD (dir 0).
    int pcx2 = 5 * 8 + 4, pcy2 = 9 * 8 + 4;
    uint8_t d2 = missionFleeExit(g, W, H, 5, 5, DIR_N, pcx2, pcy2, rng);
    check("fuite vers le nord (loin du joueur sud)", d2 == DIR_N);
  }

  // --- Vecteur fleche HUD : bon quadrant ---
  {
    float nx, ny;
    missionArrowVec(100, 50, 50, 50, nx, ny);   // cible a l'est
    check("fleche -> est", nx > 0.9f && ny > -0.1f && ny < 0.1f);
    missionArrowVec(50, 0, 50, 50, nx, ny);      // cible au nord
    check("fleche -> nord", ny < -0.9f && nx > -0.1f && nx < 0.1f);
    missionArrowVec(50, 50, 50, 50, nx, ny);     // confondus -> (0,-1)
    check("fleche confondus -> nord par defaut", nx == 0.0f && ny == -1.0f);
  }

  // --- Poursuite : le chasseur choisit la sortie qui RAPPROCHE du joueur ---
  {
    buildGrid(g, W, H, 4);                  // tout franchissable
    uint32_t rng = 0xABCDEF;
    // chasseur en (5,5), joueur a l'OUEST en (1,5) -> doit aller vers l'OUEST.
    int pcx = 1 * 8 + 4, pcy = 5 * 8 + 4;
    uint8_t d = missionChaseExit(g, W, H, 5, 5, DIR_E, pcx, pcy, rng);
    check("poursuite vers l'ouest (vers le joueur ouest)", d == DIR_W);
    // joueur au SUD -> chasse vers le SUD (dir 2).
    int pcx2 = 5 * 8 + 4, pcy2 = 9 * 8 + 4;
    uint8_t d2 = missionChaseExit(g, W, H, 5, 5, DIR_N, pcx2, pcy2, rng);
    check("poursuite vers le sud (vers le joueur sud)", d2 == DIR_S);
  }

  // --- Objectif GOTO : valide dans le rayon, pas hors rayon ---
  {
    Objective o = { OBJ_GOTO, 100, 100, 8, false, EV_NONE, nullptr, "go" };
    MissionState s = {};
    s.actorCx = 104; s.actorCy = 100;        // 4 px -> dans rayon 8
    check("GOTO dans rayon", missionObjectiveDone(o, s));
    s.actorCx = 120; s.actorCy = 100;        // 20 px -> hors rayon
    check("GOTO hors rayon", !missionObjectiveDone(o, s));
  }
  // --- Objectif GOTO requireCar : seulement si au volant ---
  {
    Objective o = { OBJ_GOTO, 100, 100, 8, true, EV_NONE, nullptr, "drive" };
    MissionState s = {};
    s.actorCx = 100; s.actorCy = 100; s.driving = false;
    check("GOTO requireCar a pied -> non", !missionObjectiveDone(o, s));
    s.driving = true;
    check("GOTO requireCar au volant -> oui", missionObjectiveDone(o, s));
  }
  // --- Objectif ENTER_CAR : valide quand au volant de la voiture de mission ---
  {
    Objective o = { OBJ_ENTER_CAR, 0, 0, 0, false, EV_NONE, nullptr, "enter" };
    MissionState s = {};
    s.inMissionCar = false;
    check("ENTER_CAR pas dans la voiture -> non", !missionObjectiveDone(o, s));
    s.inMissionCar = true;
    check("ENTER_CAR dans la voiture -> oui", missionObjectiveDone(o, s));
  }
  // --- Objectif KILL : valide quand la cible est morte ---
  {
    Objective o = { OBJ_KILL, 0, 0, 0, false, EV_NONE, nullptr, "kill" };
    MissionState s = {};
    s.targetAlive = true;
    check("KILL cible vivante -> non", !missionObjectiveDone(o, s));
    s.targetAlive = false;
    check("KILL cible morte -> oui", missionObjectiveDone(o, s));
  }
  // --- Objectif BEAT : valide quand le compteur de KO atteint count ---
  {
    Objective o = { OBJ_BEAT, 0, 0, 0, false, EV_NONE, nullptr, "beat", nullptr, 3, 0 };
    MissionState s = {};
    s.beatCount = 2;
    check("BEAT 2/3 -> non", !missionObjectiveDone(o, s));
    s.beatCount = 3;
    check("BEAT 3/3 -> oui", missionObjectiveDone(o, s));
  }
  // --- Objectif SURVIVE : valide quand le temps ecoule atteint limit ---
  {
    Objective o = { OBJ_SURVIVE, 0, 0, 0, false, EV_NONE, nullptr, "surv", nullptr, 0, 100 };
    MissionState s = {};
    s.elapsed = 99;
    check("SURVIVE 99/100 -> non", !missionObjectiveDone(o, s));
    s.elapsed = 100;
    check("SURVIVE 100/100 -> oui", missionObjectiveDone(o, s));
    check("SURVIVE jamais timeout", !missionTimedOut(o, 200));
  }
  // --- Objectif TALK : valide a pied dans le petit rayon (comme GOTO sans voiture) ---
  {
    Objective o = { OBJ_TALK, 100, 100, 6, false, EV_NONE, nullptr, "parle" };
    MissionState s = {};
    s.actorCx = 103; s.actorCy = 100;        // 3 px -> dans rayon 6
    check("TALK dans rayon", missionObjectiveDone(o, s));
    s.actorCx = 130; s.actorCy = 100;        // 30 px -> hors rayon
    check("TALK hors rayon", !missionObjectiveDone(o, s));
  }
  // --- Objectif SUBDUE : valide quand la cible nommee a pris `count` coups ---
  {
    Objective o = { OBJ_SUBDUE, 0, 0, 0, false, EV_NONE, nullptr, "mate", nullptr, 3, 0 };
    MissionState s = {};
    s.subdueCount = 2;
    check("SUBDUE 2/3 -> non", !missionObjectiveDone(o, s));
    s.subdueCount = 3;
    check("SUBDUE 3/3 -> oui", missionObjectiveDone(o, s));
  }
  // --- Limite de temps : GOTO chronometré echoue une fois depassee ---
  {
    Objective o = { OBJ_GOTO, 0, 0, 8, false, EV_NONE, nullptr, "timed", nullptr, 0, 50 };
    check("GOTO dans les temps", !missionTimedOut(o, 50));
    check("GOTO hors temps -> timeout", missionTimedOut(o, 51));
  }

  // --- Enchainement : missionAdvance renvoie l'evenement et termine au bout ---
  {
    static const Objective objs[] = {
      { OBJ_ENTER_CAR, 0, 0, 0, false, EV_NONE,      nullptr, "o0" },
      { OBJ_GOTO,    50, 50, 8, true,  EV_MARCO_JOIN, nullptr, "o1" },
      { OBJ_GOTO,    90, 90, 8, true,  EV_MARCO_DIE,  nullptr, "o2" },
      { OBJ_KILL,      0, 0, 0, false, EV_NONE,       nullptr, "o3" },
    };
    MissionDef def = { "test", objs, 4 };
    MissionRun run = { 0, 0, true };
    check("advance0 sans event", missionAdvance(run, def) == EV_NONE && run.step == 1 && run.active);
    check("advance1 -> MARCO_JOIN", missionAdvance(run, def) == EV_MARCO_JOIN && run.step == 2 && run.active);
    check("advance2 -> MARCO_DIE", missionAdvance(run, def) == EV_MARCO_DIE && run.step == 3 && run.active);
    check("advance3 -> fin mission", missionAdvance(run, def) == EV_NONE && run.step == 4 && !run.active);
  }

  // --- Enchainement type M1 : GOTO->TALK(JOIN)->GOTO(car)->GOTO(car), isStory ---
  {
    static const Objective objs[] = {
      { OBJ_GOTO, 10, 10, 8, false, EV_NONE,       nullptr, "va au garage" },
      { OBJ_TALK, 10, 10, 6, false, EV_MARCO_JOIN, nullptr, "parle a Marco" },
      { OBJ_GOTO, 90, 90, 12, true, EV_NONE,       nullptr, "roule aux quais" },
      { OBJ_GOTO, 95, 95, 12, true, EV_NONE,       nullptr, "livre" },
    };
    MissionDef def = { "M1", objs, 4, 120, true };   // reward 120, isStory true
    check("isStory lu", def.isStory);
    MissionRun run = { 0, 0, true };
    check("M1 a0 GOTO", missionAdvance(run, def) == EV_NONE && run.step == 1);
    check("M1 a1 TALK->JOIN", missionAdvance(run, def) == EV_MARCO_JOIN && run.step == 2);
    check("M1 a2 GOTO car", missionAdvance(run, def) == EV_NONE && run.step == 3 && run.active);
    check("M1 a3 fin", missionAdvance(run, def) == EV_NONE && run.step == 4 && !run.active);
  }

  if (failures == 0) { printf("OK : LOS, fuite, poursuite, objectifs, enchainement, fleche HUD valides\n"); return 0; }
  printf("%d echec(s)\n", failures);
  return 1;
}
