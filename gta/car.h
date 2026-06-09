// genere par tools/build_car.py -- NE PAS editer
#pragma once
#include <stdint.h>

#define CAR_FRAMES 24
#define CAR_BOX 16
#define CAR_TRANSPARENT 0xF81F
#define CAR_BODY_KEY 0x07E0

extern const uint16_t carFrames[CAR_FRAMES][CAR_BOX*CAR_BOX];
