// genere par tools/build_city.py -- NE PAS editer
#pragma once
#include <stdint.h>
#include "assets.h"

#define CITY_W 96
#define CITY_H 96
#define PLAYER_START_X 49
#define PLAYER_START_Y 47
#define PLAYER_START_DIR DIR_SOUTH

#define CITY_NUM_POIS 7

// Point d'interet : bbox en TUILES (inclus) pour la detection
// HUD, point-cible (tx,ty) en PX monde pour les missions/fleche.
struct CityPoi {
  const char *name;
  uint8_t x0, y0, x1, y1;
  int16_t tx, ty;
};
extern const uint8_t cityMap[CITY_H*CITY_W];
extern const CityPoi cityPois[CITY_NUM_POIS];
