// Test host de la couche armes (weapons.h) et du cone generalise (combat.h).
// Pur, sans Gamebuino. Compile en natif :
//   g++ -std=c++11 -I gta tools/host_test/test_weapons_host.cpp -o /tmp/weapons_host && /tmp/weapons_host
#include <cstdio>
#include <stdint.h>
#include "weapons.h"

static int failures = 0;
static void check_int(const char *what, long got, long want) {
  if (got != want) { printf("FAIL %s : got %ld, want %ld\n", what, got, want); failures++; }
}
static void check_true(const char *what, bool cond) {
  if (!cond) { printf("FAIL %s\n", what); failures++; }
}

int main() {
  // --- cycle d'armes -------------------------------------------------------
  // Rien de possede : on reste sur le poing (boucle complete revient a FIST).
  {
    bool owned[WEAPON_COUNT] = { true, false, false, false, false, false };
    check_int("cycle empty stays fist", weaponCycleNext(owned, WEAPON_FIST), WEAPON_FIST);
  }
  // Poing -> pistolet -> SMG -> retour poing (seules ces armes possedees).
  {
    bool owned[WEAPON_COUNT] = { true, true, true, false, false, false };
    uint8_t w = WEAPON_FIST;
    w = weaponCycleNext(owned, w); check_int("fist->pistol", w, WEAPON_PISTOL);
    w = weaponCycleNext(owned, w); check_int("pistol->smg",  w, WEAPON_SMG);
    w = weaponCycleNext(owned, w); check_int("smg->fist",    w, WEAPON_FIST);
  }
  // Saute les armes non possedees (pistolet absent : poing -> SMG).
  {
    bool owned[WEAPON_COUNT] = { true, false, true, false, false, false };
    check_int("skip unowned", weaponCycleNext(owned, WEAPON_FIST), WEAPON_SMG);
  }
  // Tout possede : cycle complet jusqu'a reboucler.
  {
    bool owned[WEAPON_COUNT] = { true, true, true, true, true, true };
    check_int("grenade wraps to fist", weaponCycleNext(owned, WEAPON_GRENADE), WEAPON_FIST);
    check_int("bazooka->grenade", weaponCycleNext(owned, WEAPON_BAZOOKA), WEAPON_GRENADE);
  }

  // --- munitions -----------------------------------------------------------
  check_true("fist infinite", weaponInfinite(WEAPON_FIST));
  check_true("pistol finite", !weaponInfinite(WEAPON_PISTOL));

  // --- cone par arme (portee croissante) -----------------------------------
  // Joueur en (50,50) regardant l'est (dir=1). Cible a 30 px devant :
  //   hors de portee du poing, dans la portee du pistolet.
  {
    const int PCX = 50, PCY = 50;
    const WeaponDef &fist = WEAPONS[WEAPON_FIST];
    const WeaponDef &pist = WEAPONS[WEAPON_PISTOL];
    check_true("fist short: 30px out",
               !combatInCone(50 + 30.0f, 50.0f, PCX, PCY, 1, fist.reach, fist.side));
    check_true("pistol long: 30px in",
               combatInCone(50 + 30.0f, 50.0f, PCX, PCY, 1, pist.reach, pist.side));
  }
  // Cone large de la pompe : cible tres decalee lateralement mais proche -> dans
  // la zone (la pompe a side=11) la ou le pistolet (side=4) raterait.
  {
    const int PCX = 50, PCY = 50;
    const WeaponDef &shot = WEAPONS[WEAPON_SHOTGUN];
    const WeaponDef &pist = WEAPONS[WEAPON_PISTOL];
    check_true("shotgun wide hits offset",
               combatInCone(50 + 10.0f, 50 + 9.0f, PCX, PCY, 1, shot.reach, shot.side));
    check_true("pistol narrow misses offset",
               !combatInCone(50 + 10.0f, 50 + 9.0f, PCX, PCY, 1, pist.reach, pist.side));
  }
  // combatConeTarget : le plus proche devant dans le cone du pistolet.
  {
    float px[2] = { 50 + 35.0f, 50 + 15.0f };
    float py[2] = { 50.0f, 50.0f };
    bool act[2] = { true, true };
    const WeaponDef &p = WEAPONS[WEAPON_PISTOL];
    check_int("cone nearest", combatConeTarget(px, py, act, 2, 50, 50, 1, p.reach, p.side), 1);
  }

  if (failures == 0) { printf("OK : weapons.h verifie\n"); return 0; }
  printf("%d echec(s)\n", failures);
  return 1;
}
