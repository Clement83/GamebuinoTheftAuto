// genere par tools/build_car.py -- NE PAS editer
#pragma once
#include <stdint.h>

#define SMOKE_BOX 6
#define SMOKE_FRAMES 4
#define SMOKE_TRANSPARENT 0xF81F

// [tier 0=leger,1=dense][frame][SMOKE_BOX*SMOKE_BOX]
extern const uint16_t smokeFrames[2][SMOKE_FRAMES][SMOKE_BOX*SMOKE_BOX];
