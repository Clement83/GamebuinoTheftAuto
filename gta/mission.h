// Couche mission pure : ligne de vue, fuite de la cible, vecteur de fleche HUD.
// Aucune dependance Gamebuino : compilable et testable en natif (host).
// Reutilise les tables de direction et les voies de ai.h (ordre N E S W).
// Codes de tuiles : 0 herbe, 1 road_h, 2 road_v, 3 road_cross, 4 trottoir,
// 5 eau, 6 building_a, 7 building_b (voir assets/tileset.csv).
#pragma once
#include <stdint.h>
#include <math.h>
#include "ai.h"

// Une tuile bloque la vue si c'est de l'eau ou un immeuble (hors carte = bloque).
inline bool missionBlocksSight(const uint8_t *grid, int w, int h, int tx, int ty) {
  if (tx < 0 || tx >= w || ty < 0 || ty >= h) return true;
  uint8_t t = grid[ty * w + tx];
  return t == 5 || t == 6 || t == 7;
}

// Tout ce qui n'est PAS solide est franchissable par la cible (herbe, trottoir,
// routes). En fuite elle emprunte donc naturellement les voies pour s'eloigner.
inline bool missionFleeWalkable(const uint8_t *grid, int w, int h, int tx, int ty) {
  if (!aiInBounds(w, h, tx, ty)) return false;
  uint8_t t = grid[ty * w + tx];
  return t == 0 || t == 1 || t == 2 || t == 3 || t == 4;
}

// Ligne de vue entre deux points px monde : vrai si la distance <= maxRange ET
// aucune tuile traversee ne bloque la vue. Echantillonnage ~1 point / 2 px.
inline bool missionLineOfSight(const uint8_t *grid, int w, int h,
                               int x0, int y0, int x1, int y1, int maxRange) {
  int dx = x1 - x0, dy = y1 - y0;
  long d2 = (long)dx * dx + (long)dy * dy;
  if (d2 > (long)maxRange * maxRange) return false;
  int steps = (int)(sqrtf((float)d2) / 2.0f) + 1;
  for (int i = 0; i <= steps; i++) {
    int x = x0 + dx * i / steps;
    int y = y0 + dy * i / steps;
    if (missionBlocksSight(grid, w, h, x >> 3, y >> 3)) return false;
  }
  return true;
}

// Direction de fuite depuis la tuile (tx,ty) : parmi les sorties franchissables,
// celle dont la tuile suivante MAXIMISE la distance au joueur (depart aleatoire
// en cas d'egalite). AI_STUCK si aucune sortie. rng mis a jour.
inline uint8_t missionFleeExit(const uint8_t *grid, int w, int h, int tx, int ty,
                               uint8_t curDir, int playerCx, int playerCy,
                               uint32_t &rng) {
  (void)curDir;
  long best = -1; uint8_t bestDir = AI_STUCK; int ties = 0;
  for (uint8_t d = 0; d < 4; d++) {
    int nx = tx + AI_DX[d], ny = ty + AI_DY[d];
    if (!missionFleeWalkable(grid, w, h, nx, ny)) continue;
    int cx = nx * AI_TILE + AI_TILE / 2, cy = ny * AI_TILE + AI_TILE / 2;
    long dd = (long)(cx - playerCx) * (cx - playerCx)
            + (long)(cy - playerCy) * (cy - playerCy);
    if (dd > best) { best = dd; bestDir = d; ties = 1; }
    else if (dd == best) { ties++; if (aiRngNext(rng) % (uint32_t)ties == 0) bestDir = d; }
  }
  return bestDir;
}

// (Re)calage de la cible en fuite : choisit une sortie d'eloignement et fixe le
// point-cible (lane point de la tuile suivante). Sans effet si tuile isolee.
inline void missionFleeRetarget(const uint8_t *grid, int w, int h, float x, float y,
                                uint8_t &dir, int &tgtx, int &tgty,
                                int playerCx, int playerCy, uint32_t &rng) {
  int tx = (int)x >> 3, ty = (int)y >> 3;
  uint8_t nd = missionFleeExit(grid, w, h, tx, ty, dir, playerCx, playerCy, rng);
  if (nd == AI_STUCK) return;
  dir = nd;
  int ntx = tx + AI_DX[dir], nty = ty + AI_DY[dir];
  aiLanePoint(ntx, nty, dir, tgtx, tgty);
}

// Un pas de fuite : avance vers le point-cible ; au point, choisit la prochaine
// sortie d'eloignement. Meme structure que aiStep mais avec missionFleeExit.
inline void missionFleeStep(const uint8_t *grid, int w, int h, float &x, float &y,
                            uint8_t &dir, int &tgtx, int &tgty, float speed,
                            int playerCx, int playerCy, uint32_t &rng) {
  float dx = (float)tgtx - x, dy = (float)tgty - y;
  float d2 = dx * dx + dy * dy;
  if (d2 <= speed * speed) {
    x = (float)tgtx; y = (float)tgty;
    int tx = (int)x >> 3, ty = (int)y >> 3;
    uint8_t nd = missionFleeExit(grid, w, h, tx, ty, dir, playerCx, playerCy, rng);
    if (nd != AI_STUCK) {
      dir = nd;
      int ntx = tx + AI_DX[dir], nty = ty + AI_DY[dir];
      aiLanePoint(ntx, nty, dir, tgtx, tgty);
    }
  } else {
    float inv = speed / sqrtf(d2);
    x += dx * inv; y += dy * inv;
  }
}

// Poursuite : miroir de la fuite. Parmi les sorties franchissables, celle dont
// la tuile suivante MINIMISE la distance au joueur (depart aleatoire en cas
// d'egalite). AI_STUCK si aucune sortie. rng mis a jour.
inline uint8_t missionChaseExit(const uint8_t *grid, int w, int h, int tx, int ty,
                                uint8_t curDir, int playerCx, int playerCy,
                                uint32_t &rng) {
  (void)curDir;
  long best = -1; uint8_t bestDir = AI_STUCK; int ties = 0;
  for (uint8_t d = 0; d < 4; d++) {
    int nx = tx + AI_DX[d], ny = ty + AI_DY[d];
    if (!missionFleeWalkable(grid, w, h, nx, ny)) continue;
    int cx = nx * AI_TILE + AI_TILE / 2, cy = ny * AI_TILE + AI_TILE / 2;
    long dd = (long)(cx - playerCx) * (cx - playerCx)
            + (long)(cy - playerCy) * (cy - playerCy);
    if (best < 0 || dd < best) { best = dd; bestDir = d; ties = 1; }
    else if (dd == best) { ties++; if (aiRngNext(rng) % (uint32_t)ties == 0) bestDir = d; }
  }
  return bestDir;
}

// Un pas de poursuite : avance vers le point-cible ; au point, choisit la
// prochaine sortie de rapprochement. Meme structure que missionFleeStep.
inline void missionChaseStep(const uint8_t *grid, int w, int h, float &x, float &y,
                             uint8_t &dir, int &tgtx, int &tgty, float speed,
                             int playerCx, int playerCy, uint32_t &rng) {
  float dx = (float)tgtx - x, dy = (float)tgty - y;
  float d2 = dx * dx + dy * dy;
  if (d2 <= speed * speed) {
    x = (float)tgtx; y = (float)tgty;
    int tx = (int)x >> 3, ty = (int)y >> 3;
    uint8_t nd = missionChaseExit(grid, w, h, tx, ty, dir, playerCx, playerCy, rng);
    if (nd != AI_STUCK) {
      dir = nd;
      int ntx = tx + AI_DX[dir], nty = ty + AI_DY[dir];
      aiLanePoint(ntx, nty, dir, tgtx, tgty);
    }
  } else {
    float inv = speed / sqrtf(d2);
    x += dx * inv; y += dy * inv;
  }
}

// ----------------------------------------------------------------------------
//  Systeme de mission modulaire (pur, host-testable). Une mission = un tableau
//  d'objectifs enchaines. Le moteur teste la completion de l'objectif courant a
//  partir d'un instantane d'etat du jeu, et avance en renvoyant un evenement
//  scripte eventuel (Marco monte, Marco meurt...). La glue (gta.ino) fournit
//  l'etat, place les entites et affiche la narration.
// ----------------------------------------------------------------------------
enum ObjType {
  OBJ_GOTO      = 0,  // atteindre un point monde dans un rayon
  OBJ_ENTER_CAR = 1,  // etre au volant de la voiture de mission
  OBJ_KILL      = 2,  // la cible de mission est morte
  OBJ_BEAT      = 3,  // mettre KO `count` pietons (compteur cumule)
  OBJ_SURVIVE   = 4,  // tenir `limit` frames (le temps qui s'ecoule remplit l'objectif)
  OBJ_TALK      = 5,  // s'approcher d'un PNJ nomme (a pied, petit rayon) : auto-dialogue
  OBJ_SUBDUE    = 6,  // frapper une cible nommee `count` fois : elle cede (ne meurt pas)
};

enum MissionEvent {
  EV_NONE       = 0,
  EV_MARCO_JOIN = 1,  // Marco monte dans la voiture (passager + message)
  EV_MARCO_DIE  = 2,  // Marco meurt, le tueur (chasseur) apparait
};

struct Objective {
  uint8_t type;        // ObjType
  int16_t x, y;        // point monde (centre px) : GOTO -> destination ;
                       //   ENTER_CAR/KILL -> ou spawner la voiture/cible.
                       //   Resolu au lancement depuis le POI `poi` s'il existe.
  uint8_t radius;      // GOTO : rayon de validation (px)
  bool    requireCar;  // GOTO : exige d'etre au volant ("conduis jusqu'a")
  uint8_t event;       // MissionEvent declenche a la completion, ou EV_NONE
  const char *poi;     // nom de POI cible (resolu en x,y au lancement), ou nullptr
  const char *text;    // narration affichee quand l'objectif devient ACTIF
  const char *doneText;// narration affichee quand l'objectif est ATTEINT (ou nullptr)
  // Champs ajoutes en fin de struct : les anciennes initialisations restent
  // valides (membres absents -> 0 en initialisation aggregat C++).
  uint8_t  count;      // OBJ_BEAT : nombre de pietons a mettre KO
  uint16_t limit;      // frames : OBJ_SURVIVE = duree a tenir ; sinon limite de
                       //   temps (0 = aucune ; depassee -> mission echouee)
};

struct MissionDef {
  const char *title;
  const Objective *objectives;
  uint8_t count;
  // Champ ajoute en fin de struct : les anciennes initialisations restent
  // valides (membre absent -> 0 en initialisation aggregat C++).
  int16_t reward;  // $ credites a la completion de la mission (0 = aucune prime)
};

struct MissionRun {
  uint8_t def;     // index de la mission active
  uint8_t step;    // index de l'objectif courant
  bool    active;
};

// Instantane d'etat du jeu pour tester la completion d'un objectif.
struct MissionState {
  int  actorCx, actorCy;  // point suivi (voiture si driving, sinon joueur)
  bool driving;           // au volant d'une voiture quelconque
  bool inMissionCar;      // au volant de la voiture de mission precisement
  bool targetAlive;       // cible de mission encore vivante
  int      beatCount;     // pietons mis KO depuis le debut de l'objectif
  uint16_t elapsed;       // frames ecoulees sur l'objectif courant
  int      subdueCount;   // coups portes a la cible de SUBDUE depuis le debut de l'objectif
};

// L'objectif o est-il rempli compte tenu de l'etat s ?
inline bool missionObjectiveDone(const Objective &o, const MissionState &s) {
  switch (o.type) {
    case OBJ_GOTO: {
      if (o.requireCar && !s.driving) return false;
      long dx = s.actorCx - o.x, dy = s.actorCy - o.y;
      return dx * dx + dy * dy <= (long)o.radius * o.radius;
    }
    case OBJ_ENTER_CAR: return s.inMissionCar;
    case OBJ_KILL:      return !s.targetAlive;
    case OBJ_BEAT:      return s.beatCount >= (int)o.count;
    case OBJ_SURVIVE:   return s.elapsed >= o.limit;
    case OBJ_TALK: {                        // proximite a pied (jamais requireCar)
      long dx = s.actorCx - o.x, dy = s.actorCy - o.y;
      return dx * dx + dy * dy <= (long)o.radius * o.radius;
    }
    case OBJ_SUBDUE:    return s.subdueCount >= (int)o.count;
  }
  return false;
}

// Objectif a-t-il depasse sa limite de temps ? (SURVIVE n'echoue jamais : le
// temps qui passe le REMPLIT, cf. missionObjectiveDone.)
inline bool missionTimedOut(const Objective &o, uint16_t elapsed) {
  return o.limit > 0 && o.type != OBJ_SURVIVE && elapsed > o.limit;
}

// Passe a l'objectif suivant. Renvoie l'evenement scripte de l'objectif qui
// vient d'etre accompli (EV_NONE si aucun). Termine la mission au dernier.
inline uint8_t missionAdvance(MissionRun &run, const MissionDef &def) {
  uint8_t ev = def.objectives[run.step].event;
  run.step++;
  if (run.step >= def.count) run.active = false;
  return ev;
}

// Vecteur unitaire focus -> cible (pour orienter la fleche HUD). (0,-1) si
// confondus. nx,ny en sortie.
inline void missionArrowVec(int targetCx, int targetCy, int focusCx, int focusCy,
                            float &nx, float &ny) {
  float dx = (float)(targetCx - focusCx), dy = (float)(targetCy - focusCy);
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 1e-3f) { nx = 0.0f; ny = -1.0f; return; }
  nx = dx / len; ny = dy / len;
}
