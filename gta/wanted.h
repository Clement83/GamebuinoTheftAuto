// Couche pure : machine d'etat du niveau de recherche police (etoiles GTA).
// Aucune dependance Gamebuino : compilable et testable en natif (host).
//
// Regles (cf. demande joueur) :
//  - tuer 3 pietons "d'affilee" (moins de 20 s entre deux meurtres) => +1 etoile ;
//  - une etoile dure 60 s sans nouveau crime, puis on redescend d'un cran ;
//  - chaque crime relance ce decompte de 60 s ;
//  - au niveau 5, plus de decroissance automatique : il faut repeindre la
//    voiture (Pay'n'Spray) ou se faire arreter/tuer pour repartir a zero.
#pragma once
#include <stdint.h>

static const uint8_t  WANTED_MAX            = 5;
static const uint16_t WANTED_STREAK_WINDOW  = 500;   // 20 s @ ~25 fps : delai max entre 2 meurtres
static const uint8_t  WANTED_STREAK_NEEDED  = 3;     // meurtres "d'affilee" pour une etoile
static const uint16_t WANTED_DECAY_FRAMES   = 1500;  // 60 s @ ~25 fps : duree de vie d'une etoile

struct WantedState {
  uint8_t  level;        // 0..WANTED_MAX (etoiles affichees)
  uint8_t  streak;       // meurtres dans la fenetre glissante courante
  uint16_t streakTimer;  // frames restantes avant reset du streak
  uint16_t decayTimer;   // frames restantes avant de perdre une etoile
};

inline void wantedReset(WantedState &w) {
  w.level = 0; w.streak = 0; w.streakTimer = 0; w.decayTimer = 0;
}

// Un crime (pieton tue par le joueur). Accumule le streak ; au seuil, +1 etoile.
// Chaque crime relance la fenetre (20 s) et le decompte de decroissance (60 s).
inline void wantedOnKill(WantedState &w) {
  w.streak = (w.streakTimer > 0) ? (uint8_t)(w.streak + 1) : 1;
  w.streakTimer = WANTED_STREAK_WINDOW;
  if (w.streak >= WANTED_STREAK_NEEDED) {
    w.streak = 0;
    if (w.level < WANTED_MAX) w.level++;
  }
  if (w.level > 0) w.decayTimer = WANTED_DECAY_FRAMES;
}

// Avance d'une frame. Le streak expire apres 20 s sans crime. Hors niveau 5,
// l'expiration du decompte fait perdre une etoile (et relance pour la suivante).
inline void wantedTick(WantedState &w) {
  if (w.streakTimer > 0 && --w.streakTimer == 0) w.streak = 0;
  if (w.level == 0 || w.level >= WANTED_MAX) return;   // 0 : rien ; 5 : gele
  if (w.decayTimer > 0 && --w.decayTimer == 0) {
    w.level--;
    if (w.level > 0) w.decayTimer = WANTED_DECAY_FRAMES;
  }
}

// Repeinture (Pay'n'Spray) / arrestation / mort : remet la recherche a zero.
inline void wantedClear(WantedState &w) { wantedReset(w); }
