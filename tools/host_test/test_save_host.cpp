// Test host de la logique de sauvegarde (save.h, pur, sans Gamebuino).
// Compile en natif :
//   g++ -std=c++11 -I gta tools/host_test/test_save_host.cpp -o /tmp/save_host && /tmp/save_host
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include "save.h"

static int failures = 0;
static void check(const char *what, bool ok) {
  if (!ok) { printf("FAIL %s\n", what); failures++; }
}

int main() {
  // --- Validite : un slot a zero (jamais ecrit) est invalide ---
  {
    SaveProfile p;
    memset(&p, 0, sizeof(p));
    check("slot zero invalide", !profileValid(p));
    p.magic = SAVE_MAGIC;
    check("magic correct valide", profileValid(p));
    p.magic = 0xDEADBEEFu;
    check("mauvais magic invalide", !profileValid(p));
  }

  // --- Round-trip pack -> apply : tout l'etat se conserve ---
  {
    bool owned[WEAPON_COUNT]    = { true, true, false, true, false, false };
    int16_t ammo[WEAPON_COUNT]  = { 0, 12, 0, 8, 0, 0 };
    SaveProfile p;
    profilePack(p, 1234, 5, owned, ammo);
    check("pack pose le magic", profileValid(p));

    int32_t money = 0; uint8_t step = 0;
    bool o2[WEAPON_COUNT]; int16_t a2[WEAPON_COUNT];
    profileApply(p, money, step, o2, a2);
    check("money round-trip", money == 1234);
    check("campaignStep round-trip", step == 5);
    bool wpnOk = true, ammoOk = true;
    for (int w = 0; w < WEAPON_COUNT; w++) {
      if (o2[w] != owned[w]) wpnOk = false;
      if (a2[w] != ammo[w]) ammoOk = false;
    }
    check("armes possedees round-trip", wpnOk);
    check("munitions round-trip", ammoOk);
  }

  // --- Le poing reste toujours possede, meme si le masque ne le porte pas ---
  {
    bool owned[WEAPON_COUNT]   = { false, false, false, false, false, false };
    int16_t ammo[WEAPON_COUNT] = { 0, 0, 0, 0, 0, 0 };
    SaveProfile p;
    profilePack(p, 0, 0, owned, ammo);
    int32_t money; uint8_t step;
    bool o2[WEAPON_COUNT]; int16_t a2[WEAPON_COUNT];
    profileApply(p, money, step, o2, a2);
    check("poing toujours possede", o2[WEAPON_FIST]);
    check("autres armes restent non possedees", !o2[WEAPON_PISTOL]);
  }

  // --- Valeurs limites (gros pactole, dernier acte) ---
  {
    bool owned[WEAPON_COUNT]   = { true, true, true, true, true, true };
    int16_t ammo[WEAPON_COUNT] = { 0, 999, 999, 999, 999, 999 };
    SaveProfile p;
    profilePack(p, 2000000000, 250, owned, ammo);
    int32_t money; uint8_t step;
    bool o2[WEAPON_COUNT]; int16_t a2[WEAPON_COUNT];
    profileApply(p, money, step, o2, a2);
    check("gros montant conserve", money == 2000000000);
    check("step large conserve", step == 250);
    check("toutes armes possedees", o2[WEAPON_BAZOOKA] && o2[WEAPON_GRENADE]);
  }

  if (failures == 0) printf("OK save host (tous les tests passent)\n");
  else               printf("%d test(s) en echec\n", failures);
  return failures ? 1 : 0;
}
