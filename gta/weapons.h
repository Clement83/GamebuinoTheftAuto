// Couche armes pure : enum, metadonnees (portee/cone/munitions/zone) et cycle
// d'armes possedees. Aucune dependance Gamebuino : compilable et testable en
// natif (host). Les sprites 6x6 sont separes (weapons_gfx.h, generes).
// L'ordre de l'enum DOIT suivre l'ordre des sprites de tools/build_weapons.py.
#pragma once
#include <stdint.h>
#include "combat.h"   // PUNCH_REACH / PUNCH_SIDE (cone du poing)

enum WeaponId : uint8_t {
  WEAPON_FIST = 0,   // arme par defaut, toujours possedee, munitions infinies
  WEAPON_PISTOL,
  WEAPON_SMG,
  WEAPON_SHOTGUN,
  WEAPON_BAZOOKA,
  WEAPON_GRENADE,
  WEAPON_MOLOTOV,
  WEAPON_COUNT
};

struct WeaponDef {
  const char *name;
  float    reach;      // px : portee du cone d'attaque devant le joueur
  float    side;       // px : demi-largeur du cone (precision)
  bool     area;       // true : frappe TOUS les pietons du cone (sinon le + proche)
  int16_t  ammoPickup; // munitions par ramassage (FIST : -1 = infini)
  bool     explosive;  // true : declenche une explosion de zone au point d'impact
                       //        (bazooka/grenade) au lieu d'user les PV par coups
};

// Equilibrage arcade. Poing = cone court herite de combat.h ; armes a feu =
// portee plus longue. Pompe = courte mais large (zone). Bazooka = longue + zone.
// Grenade = jet court mais large rayon (zone). 1 touche met le pieton au sol.
static const WeaponDef WEAPONS[WEAPON_COUNT] = {
  { "Poing",    PUNCH_REACH, PUNCH_SIDE, false, -1, false },
  { "Pistolet", 40.0f,  4.0f,  false, 12, false },
  { "PM",       34.0f,  6.0f,  false, 30, false },
  { "Pompe",    22.0f, 11.0f,  true,   8, false },
  { "Bazooka",  58.0f,  8.0f,  true,   4, true  },
  { "Grenade",  20.0f, 16.0f,  true,   3, true  },
  { "Molotov",  18.0f, 14.0f,  true,   3, true  },
};

// Munitions infinies pour cette arme (le poing).
inline bool weaponInfinite(uint8_t w) { return WEAPONS[w].ammoPickup < 0; }

// Arme possedee suivante a partir de `cur` (cycle en boucle, sens avant). Le
// poing est toujours considere possede (owned[WEAPON_FIST] ignore). Renvoie au
// pire WEAPON_FIST. `owned` : tableau de WEAPON_COUNT booleens.
inline uint8_t weaponCycleNext(const bool *owned, uint8_t cur) {
  for (uint8_t i = 1; i <= WEAPON_COUNT; i++) {
    uint8_t w = (uint8_t)((cur + i) % WEAPON_COUNT);
    if (w == WEAPON_FIST || owned[w]) return w;
  }
  return WEAPON_FIST;
}
