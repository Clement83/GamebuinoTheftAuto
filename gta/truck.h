// genere par tools/build_truck.py -- NE PAS editer
#pragma once
#include <stdint.h>

#define TRUCK_FRAMES 24
#define TRUCK_BOX 20
#define TRUCK_TRANSPARENT 0xF81F
#define TRUCK_BODY_KEY 0x07E0
#define TRUCK_CAB_KEY 0x001F
#define TRUCK_WINDOW_KEY 0xF800
#define TRUCK_LADDER_KEY 0xFFE0

extern const uint16_t truckFrames[TRUCK_FRAMES][TRUCK_BOX*TRUCK_BOX];
