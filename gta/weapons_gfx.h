// genere par tools/build_weapons.py -- NE PAS editer
#pragma once
#include <stdint.h>

#define WEAPON_BOX 6
#define WEAPON_GFX_COUNT 6
#define WEAPON_TRANSP 0xF81F

extern const uint16_t weaponSprites[WEAPON_GFX_COUNT][WEAPON_BOX*WEAPON_BOX];
