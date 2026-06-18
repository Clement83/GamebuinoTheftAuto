// Profils de sauvegarde : struct compact + helpers PURS (validite / pack /
// apply), sans aucune dependance Gamebuino. L'I/O reelle (gb.save) vit dans
// gta.ino. Teste en natif par tools/host_test/test_save_host.cpp.
//
// Modele : 3 profils independants. Chaque profil retient l'etat persistant
// minimal pour reprendre une partie : argent, avancement de la campagne, et
// l'inventaire d'armes (possession + munitions). Le reste (position, trafic IA,
// niveau de recherche...) est transitoire et regenere au boot.
#pragma once
#include <stdint.h>
#include "weapons.h"   // WEAPON_COUNT, WEAPON_FIST

// Marqueur de validite : un slot jamais ecrit (octets a zero) ou d'un ancien
// format n'a pas ce magic -> traite comme VIDE. Bump si le struct change.
static const uint32_t SAVE_MAGIC = 0x47544131u;   // 'GTA1'

struct SaveProfile {
  uint32_t magic;                 // SAVE_MAGIC si valide, 0 sinon
  int32_t  money;                 // playerMoney
  uint8_t  campaignStep;          // avancement campagne (index dans STORY_SEQ)
  uint8_t  weaponOwnedMask;       // bit w = arme w possedee (le poing est implicite)
  int16_t  ammo[WEAPON_COUNT];    // munitions par arme
};

// Un profil est exploitable seulement s'il porte le bon marqueur.
static inline bool profileValid(const SaveProfile &p) {
  return p.magic == SAVE_MAGIC;
}

// Empaquette l'etat de jeu courant dans un profil pret a ecrire.
static inline void profilePack(SaveProfile &p, int32_t money, uint8_t campaignStep,
                               const bool *owned, const int16_t *ammo) {
  p.magic = SAVE_MAGIC;
  p.money = money;
  p.campaignStep = campaignStep;
  p.weaponOwnedMask = 0;
  for (uint8_t w = 0; w < WEAPON_COUNT; w++) {
    if (owned[w]) p.weaponOwnedMask |= (uint8_t)(1u << w);
    p.ammo[w] = ammo[w];
  }
}

// Applique un profil (suppose valide) a l'etat de jeu. Le poing reste toujours
// possede, quel que soit le masque enregistre.
static inline void profileApply(const SaveProfile &p, int32_t &money, uint8_t &campaignStep,
                                bool *owned, int16_t *ammo) {
  money = p.money;
  campaignStep = p.campaignStep;
  for (uint8_t w = 0; w < WEAPON_COUNT; w++) {
    owned[w] = (w == WEAPON_FIST) || (p.weaponOwnedMask & (uint8_t)(1u << w)) != 0;
    ammo[w] = p.ammo[w];
  }
}
