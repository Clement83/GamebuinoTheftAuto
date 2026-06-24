// genere par tools/build_boat.py -- NE PAS editer
#pragma once
#include <stdint.h>

#define BOAT_FRAMES 16
#define BOAT_BOX 16
#define BOAT_TRANSPARENT 0xF81F
#define BOAT_HULL_KEY 0x07E0
#define BOAT_DECK_KEY 0x001F
#define BOAT_TRIM_KEY 0xFFE0

extern const uint16_t boatFrames[BOAT_FRAMES][BOAT_BOX*BOAT_BOX];
