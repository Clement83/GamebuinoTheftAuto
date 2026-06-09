// genere par tools/build_player.py -- NE PAS editer
#pragma once
#include <stdint.h>

#define PLAYER_BOX 7
#define PLAYER_WALK 2
#define PLAYER_TRANSP 0xF81F

extern const uint16_t playerFrames[4][PLAYER_WALK][PLAYER_BOX*PLAYER_BOX];
