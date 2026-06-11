// Couche IA pure : errance sur grille (voitures routes, piétons trottoirs).
// Portage C++ de tools/ai.py -- doit rester en PARITE avec lui.
// Aucune dependance Gamebuino : compilable et testable en natif (host).
// Repere : tuiles 8 px, directions cardinales N E S W (enum Dir), y vers le bas.
#pragma once
#include <stdint.h>
#include <math.h>

static const int AI_TILE = 8;

// Directions cardinales (ordre enum Dir). Noms courts, sans clash avec assets.h.
enum { DIR_N = 0, DIR_E = 1, DIR_S = 2, DIR_W = 3 };

// Deltas et tables de rotation (indices Dir). RIGHT = 90 deg horaire (y bas).
static const int8_t AI_DX[4]   = { 0,  1,  0, -1 };   // N E S W
static const int8_t AI_DY[4]   = { -1, 0,  1,  0 };
static const uint8_t AI_RIGHT[4] = { 1, 2, 3, 0 };
static const uint8_t AI_LEFT[4]  = { 3, 0, 1, 2 };
static const uint8_t AI_BACK[4]  = { 2, 3, 0, 1 };

static const int AI_LANE = 3;             // decalage lateral a droite (px)
// Pondere le choix au croisement : tout droit domine, puis on prefere TOURNER A
// DROITE plutot qu'a gauche (un virage a gauche traverse la voie opposee -> moins
// naturel). Demi-tour reserve au vrai cul-de-sac (cf. aiPickExit).
static const int AI_STRAIGHT_WEIGHT = 6;  // poids tout droit
static const int AI_RIGHT_WEIGHT    = 2;  // poids virage a droite
static const int AI_LEFT_WEIGHT     = 1;  // poids virage a gauche
static const uint8_t AI_STUCK = 0xFF;     // aucune sortie valide (tuile isolee)

// Classifieur de tuile : carrossable / marchable. Signature commune.
typedef bool (*AiClassify)(const uint8_t *grid, int w, int h, int tx, int ty);

inline bool aiInBounds(int w, int h, int tx, int ty) {
  return tx >= 0 && tx < w && ty >= 0 && ty < h;
}

inline bool aiIsDrivable(const uint8_t *grid, int w, int h, int tx, int ty) {
  if (!aiInBounds(w, h, tx, ty)) return false;
  uint8_t t = grid[ty * w + tx];
  return t == 1 || t == 2 || t == 3;   // ROAD_H / ROAD_V / ROAD_CROSS
}

inline bool aiIsWalkable(const uint8_t *grid, int w, int h, int tx, int ty) {
  if (!aiInBounds(w, h, tx, ty)) return false;
  uint8_t t = grid[ty * w + tx];
  return t == 4 || t == 0;             // PAVEMENT ou GRASS (trottoirs + herbe)
}

// Tuile franchissable a pied EN PANIQUE : herbe, trottoir OU route (le piéton
// paniqué traverse les routes), jamais un batiment. Parite ai.py:is_open.
inline bool aiIsOpen(const uint8_t *grid, int w, int h, int tx, int ty) {
  if (!aiInBounds(w, h, tx, ty)) return false;
  uint8_t t = grid[ty * w + tx];
  return t == 0 || t == 1 || t == 2 || t == 3 || t == 4;   // GRASS/ROADs/PAVEMENT
}

// True si au moins une des 4 tuiles voisines est valide (sortie possible).
inline bool aiHasExit(const uint8_t *grid, int w, int h, int tx, int ty,
                      AiClassify classify) {
  for (int i = 0; i < 4; i++)
    if (classify(grid, w, h, tx + AI_DX[i], ty + AI_DY[i])) return true;
  return false;
}

// Point-cible (px monde) de la voie pour traverser (tx,ty) dans le sens d.
// Centre de tuile decale de AI_LANE vers la droite -> roulage a droite.
inline void aiLanePoint(int tx, int ty, uint8_t d, int &px, int &py) {
  int cx = tx * AI_TILE + AI_TILE / 2;
  int cy = ty * AI_TILE + AI_TILE / 2;
  px = cx + AI_DX[AI_RIGHT[d]] * AI_LANE;
  py = cy + AI_DY[AI_RIGHT[d]] * AI_LANE;
}

// xorshift32 deterministe : met a jour state et renvoie la valeur. state != 0.
inline uint32_t aiRngNext(uint32_t &state) {
  uint32_t x = state ? state : 0x1234567u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

// Choisit une direction de sortie depuis (tx,ty) venant du sens d.
// Candidats {tout droit (poids 4), droite (1), gauche (1)} filtres par classify ;
// demi-tour seulement si aucune autre issue. Met a jour rng.
inline uint8_t aiPickExit(const uint8_t *grid, int w, int h, int tx, int ty,
                          uint8_t d, AiClassify classify, uint32_t &rng) {
  uint8_t turns[3]; int cum[3]; int n = 0, total = 0;
  const uint8_t cand[3] = { d, AI_RIGHT[d], AI_LEFT[d] };
  const int wts[3] = { AI_STRAIGHT_WEIGHT, AI_RIGHT_WEIGHT, AI_LEFT_WEIGHT };
  for (int i = 0; i < 3; i++) {
    int nx = tx + AI_DX[cand[i]], ny = ty + AI_DY[cand[i]];
    if (classify(grid, w, h, nx, ny)) {
      total += wts[i];
      turns[n] = cand[i]; cum[n] = total; n++;
    }
  }
  if (n == 0) {                           // cul-de-sac
    int bx = tx + AI_DX[AI_BACK[d]], by = ty + AI_DY[AI_BACK[d]];
    return classify(grid, w, h, bx, by) ? AI_BACK[d] : AI_STUCK;
  }
  int r = (int)(aiRngNext(rng) % (uint32_t)total);
  for (int i = 0; i < n; i++) if (r < cum[i]) return turns[i];
  return turns[n - 1];                     // garde-fou
}

// Place un agent sur la tuile (tx,ty) : direction = une sortie valide, position
// = lane point, cible = lane point de la tuile suivante.
inline void aiPlace(const uint8_t *grid, int w, int h, float &x, float &y,
                    uint8_t &dir, int &tgtx, int &tgty, int tx, int ty,
                    AiClassify classify, uint32_t &rng) {
  dir = aiPickExit(grid, w, h, tx, ty, DIR_N, classify, rng);
  if (dir == AI_STUCK) {                    // tuile isolee : reste sur place
    int px, py; aiLanePoint(tx, ty, DIR_N, px, py);
    dir = DIR_N; x = (float)px; y = (float)py; tgtx = px; tgty = py;
    return;
  }
  int px, py;
  aiLanePoint(tx, ty, dir, px, py);
  x = (float)px; y = (float)py;
  int ntx = tx + AI_DX[dir], nty = ty + AI_DY[dir];
  aiLanePoint(ntx, nty, dir, tgtx, tgty);
}

// Avance un agent vers son point-cible a vitesse constante ; au point, snap puis
// choisit la tuile suivante (aiPickExit) et le nouveau point-cible.
inline void aiStep(const uint8_t *grid, int w, int h, float &x, float &y,
                   uint8_t &dir, int &tgtx, int &tgty, float speed,
                   AiClassify classify, uint32_t &rng) {
  float dx = (float)tgtx - x, dy = (float)tgty - y;
  float d2 = dx * dx + dy * dy;
  if (d2 <= speed * speed) {
    x = (float)tgtx; y = (float)tgty;
    int tx = (int)x >> 3, ty = (int)y >> 3;
    uint8_t nd = aiPickExit(grid, w, h, tx, ty, dir, classify, rng);
    if (nd != AI_STUCK) {                   // sinon : reste sur place (isole)
      dir = nd;
      int ntx = tx + AI_DX[dir], nty = ty + AI_DY[dir];
      aiLanePoint(ntx, nty, dir, tgtx, tgty);
    }
  } else {
    float inv = speed / sqrtf(d2);
    x += dx * inv; y += dy * inv;
  }
}

// Direction de fuite depuis (tx,ty) : tuile OUVERTE voisine la mieux alignee
// avec (awayDx,awayDy) « loin de la menace ». Deterministe (depart par ordre
// N,E,S,W). AI_STUCK si acculee. Parite ai.py:panic_dir.
inline uint8_t aiPanicDir(const uint8_t *grid, int w, int h, int tx, int ty,
                          float awayDx, float awayDy) {
  uint8_t best = AI_STUCK;
  float bestDot = 0.0f; bool has = false;
  for (uint8_t di = 0; di < 4; di++) {
    if (!aiIsOpen(grid, w, h, tx + AI_DX[di], ty + AI_DY[di])) continue;
    float dot = AI_DX[di] * awayDx + AI_DY[di] * awayDy;
    if (!has || dot > bestDot) { bestDot = dot; best = di; has = true; }
  }
  return best;
}

// Avance un pieton paniqué qui FUIT (srcx,srcy) : cible = centre de tuile, choix
// = aiPanicDir, classifieur = aiIsOpen, sans rng. Parite ai.py:panic_step.
inline void aiPanicStep(const uint8_t *grid, int w, int h, float &x, float &y,
                        uint8_t &dir, int &tgtx, int &tgty, float speed,
                        float srcx, float srcy) {
  float dx = (float)tgtx - x, dy = (float)tgty - y;
  float d2 = dx * dx + dy * dy;
  if (d2 <= speed * speed) {
    x = (float)tgtx; y = (float)tgty;
    int tx = (int)x >> 3, ty = (int)y >> 3;
    uint8_t nd = aiPanicDir(grid, w, h, tx, ty, x - srcx, y - srcy);
    if (nd != AI_STUCK) {
      dir = nd;
      int ntx = tx + AI_DX[dir], nty = ty + AI_DY[dir];
      tgtx = ntx * AI_TILE + AI_TILE / 2;
      tgty = nty * AI_TILE + AI_TILE / 2;
    }
  } else {
    float inv = speed / sqrtf(d2);
    x += dx * inv; y += dy * inv;
  }
}
