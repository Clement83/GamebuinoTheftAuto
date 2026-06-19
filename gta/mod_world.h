#pragma once
// mod_world.h - monde: input, helpers de spawn, POI, lumieres, freeRam
// Extrait de gta.ino (refactor). #include par gta.ino (une seule TU).

static int freeRam() {
  char top;
  return &top - reinterpret_cast<char *>(sbrk(0));
}


// Cherche une case 8x8 non-solide proche du centre (cx,cy) en spirale.
// Renvoie le coin haut-gauche dans ox,oy. true si trouve.
static bool findFootSpot(int cx, int cy, int &ox, int &oy) {
  for (int r = 0; r <= 6; r++) {
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (r > 0 && abs(dx) != r && abs(dy) != r) continue; // anneau
        int px = cx - PLAYER_W / 2 + dx * TILE_W;
        int py = cy - PLAYER_H / 2 + dy * TILE_H;
        if (!boxHitsSolid(px, py)) { ox = px; oy = py; return true; }
      }
    }
  }
  return false;
}


// Cherche le centre (px monde) d'une tuile trottoir (PAVEMENT) proche de
// (cx,cy), en spirale. Les cabines doivent etre POSEES sur un trottoir pour
// rester accessibles a pied. Renvoie false si aucun trottoir dans le rayon.
static bool findSidewalkSpot(int cx, int cy, int &ox, int &oy) {
  int ctx = cx >> 3, cty = cy >> 3;
  for (int r = 0; r <= 14; r++) {
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (r > 0 && abs(dx) != r && abs(dy) != r) continue;   // anneau
        int tx = ctx + dx, ty = cty + dy;
        if (tx < 0 || tx >= CITY_W || ty < 0 || ty >= CITY_H) continue;
        if (cityMap[ty * CITY_W + tx] == TILE_PAVEMENT) {
          ox = tx * TILE_W + TILE_W / 2;
          oy = ty * TILE_H + TILE_H / 2;
          return true;
        }
      }
    }
  }
  return false;
}


// Centre (px monde) d'une tuile d'IMMEUBLE (BUILDING_A/B) proche de (cx,cy) : sert
// de point d'apparition "porte" d'ou un PNJ SORT du batiment (anim d'emergence)
// au lieu de pop sur place. Spirale, rayon 6. false si aucun immeuble proche.
static bool findBuildingTileNear(int cx, int cy, int &ox, int &oy) {
  int ctx = cx >> 3, cty = cy >> 3;
  for (int r = 1; r <= 6; r++) {
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (abs(dx) != r && abs(dy) != r) continue;            // anneau
        int tx = ctx + dx, ty = cty + dy;
        if (tx < 0 || tx >= CITY_W || ty < 0 || ty >= CITY_H) continue;
        uint8_t t = cityMap[ty * CITY_W + tx];
        if (t == TILE_BUILDING_A || t == TILE_BUILDING_B) {
          ox = tx * TILE_W + TILE_W / 2;
          oy = ty * TILE_H + TILE_H / 2;
          return true;
        }
      }
    }
  }
  return false;
}


// Centre (px monde) d'une tuile de ROUTE proche de (cx,cy), en PRIVILEGIANT le
// cote droit (est) : la caisse de mission s'y gare "sur le cote" plutot que pile
// sur le marqueur, pour laisser voir le PNJ sortir. Spirale ponderee est, r<=8.
static bool findRoadSpotNear(int cx, int cy, int &ox, int &oy) {
  int ctx = cx >> 3, cty = cy >> 3;
  for (int r = 1; r <= 8; r++) {
    // ordre de balayage : on tente les colonnes les plus a droite d'abord.
    for (int dx = r; dx >= -r; dx--) {
      for (int dy = -r; dy <= r; dy++) {
        if (abs(dx) != r && abs(dy) != r) continue;            // anneau
        int tx = ctx + dx, ty = cty + dy;
        if (tx < 0 || tx >= CITY_W || ty < 0 || ty >= CITY_H) continue;
        uint8_t t = cityMap[ty * CITY_W + tx];
        if (t == TILE_ROAD_H || t == TILE_ROAD_V || t == TILE_ROAD_CROSS) {
          ox = tx * TILE_W + TILE_W / 2;
          oy = ty * TILE_H + TILE_H / 2;
          return true;
        }
      }
    }
  }
  return false;
}


static void readFootInput(int &dx, int &dy) {
#ifdef DEMO_AUTOWALK
  demoInput(dx, dy);
#else
  dx = gb.buttons.repeat(BUTTON_RIGHT, 1) ? 1 : (gb.buttons.repeat(BUTTON_LEFT, 1) ? -1 : 0);
  dy = gb.buttons.repeat(BUTTON_DOWN, 1) ? 1 : (gb.buttons.repeat(BUTTON_UP, 1) ? -1 : 0);
#endif
}


// Index du POI nomme `name` dans la table exportee, ou -1 si absent.
static int findPoi(const char *name) {
#if CITY_NUM_POIS > 0
  if (name) for (int i = 0; i < CITY_NUM_POIS; i++)
    if (strcmp(cityPois[i].name, name) == 0) return i;
#else
  (void)name;
#endif
  return -1;
}


// Plus petit POI (bbox) contenant la tuile (tx,ty), ou -1. Les stamps (3x3)
// l'emportent ainsi sur les grands quartiers en cas de chevauchement.
static int poiAtTile(int tx, int ty) {
#if CITY_NUM_POIS > 0
  int best = -1; long bestArea = 0;
  for (int i = 0; i < CITY_NUM_POIS; i++) {
    const CityPoi &p = cityPois[i];
    if (tx < p.x0 || tx > p.x1 || ty < p.y0 || ty > p.y1) continue;
    long area = (long)(p.x1 - p.x0 + 1) * (p.y1 - p.y0 + 1);
    if (best < 0 || area < bestArea) { best = i; bestArea = area; }
  }
  return best;
#else
  (void)tx; (void)ty; return -1;
#endif
}


// LEDs RGB de la console (gb.lights = image 2x4, poussee au matos a chaque
// gb.update). Priorite au coup recu (flash rouge) ; sinon, au-dela de 3 etoiles
// de recherche (donc 4* et 5*), elles clignotent rouge/bleu facon girophare,
// que l'on soit a pied ou au volant. Appelee une fois par frame en tete de loop.
static void updateLights() {
  gb.lights.clear();
  if (hitFlashTimer > 0) {                  // coup encaisse : flash rouge (prioritaire)
    hitFlashTimer--;
    gb.lights.fill(RED);
    return;
  }
  if (wanted.level > 3) {                   // recherche elevee (4* / 5*) : girophare LEDs
    bool phase = (missionAnim >> 2) & 1;    // meme cadence que drawGyro
    gb.lights.fill(phase ? (Color)0xF800 : (Color)0x001F);  // rouge / bleu
  }
}
