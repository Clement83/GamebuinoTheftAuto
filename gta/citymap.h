// genere par tools/build_city.py -- NE PAS editer
#pragma once
#include <stdint.h>
#include "assets.h"

#define CITY_W 96
#define CITY_H 96
#define PLAYER_START_X 47
#define PLAYER_START_Y 46
#define PLAYER_START_DIR DIR_SOUTH

extern const uint8_t cityMap[CITY_H*CITY_W];
