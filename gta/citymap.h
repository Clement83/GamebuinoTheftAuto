// genere par tools/build_city.py -- NE PAS editer
#pragma once
#include <stdint.h>
#include "assets.h"

#define CITY_W 150
#define CITY_H 150
#define PLAYER_START_X 140
#define PLAYER_START_Y 141
#define PLAYER_START_DIR DIR_SOUTH

#define CITY_NUM_POIS 37

// Point d'interet : bbox en TUILES (inclus) pour la detection
// HUD, point-cible (tx,ty) en PX monde pour les missions/fleche.
struct CityPoi {
  const char *name;
  uint8_t x0, y0, x1, y1;
  int16_t tx, ty;
};
extern const uint8_t cityMap[CITY_H*CITY_W];
extern const CityPoi cityPois[CITY_NUM_POIS];

#define CITY_NUM_SPRAYS 6
#define CITY_NUM_AMMUS 8

// Service en bord de route : position en TUILES.
struct CityService {
  uint8_t tx, ty;
};
extern const CityService citySprays[CITY_NUM_SPRAYS];
extern const CityService cityAmmus[CITY_NUM_AMMUS];

#define CITY_HAS_CASSE 1
#define CITY_CASSE_TX 1092
#define CITY_CASSE_TY 916
#define CITY_CRANE_TX 1092
#define CITY_CRANE_TY 900
