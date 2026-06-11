// Couche pure : machine d'etat du niveau de recherche police (etoiles GTA).
// Aucune dependance Gamebuino : compilable et testable en natif (host).
//
// Regles (cf. demande joueur) :
//  - tuer 3 pietons "d'affilee" (moins de 20 s entre deux meurtres) => +1 etoile ;
//  - une etoile dure 30 s sans nouveau crime, puis on redescend d'un cran ;
//  - chaque crime relance ce decompte de 30 s ;
//  - dans les 10 dernieres secondes, l'etoile du dessus CLIGNOTE (fin de vie) ;
//    pendant ce clignotement, un crime qui ajouterait une etoile la RENOUVELLE
//    (decompte remis a plein) au lieu d'en ajouter une ;
//  - au niveau 5, plus de decroissance automatique (ni clignotement) : il faut
//    repeindre la voiture (Pay'n'Spray) ou se faire arreter/tuer pour repartir.
#pragma once
#include <stdint.h>

static const uint8_t  WANTED_MAX            = 5;
static const uint16_t WANTED_STREAK_WINDOW  = 500;   // 20 s @ ~25 fps : delai max entre 2 meurtres
static const uint8_t  WANTED_STREAK_NEEDED  = 3;     // meurtres "d'affilee" pour une etoile
static const uint16_t WANTED_DECAY_FRAMES   = 750;   // 30 s @ ~25 fps : duree de vie d'une etoile
static const uint16_t WANTED_BLINK_FRAMES   = 250;   // 10 s @ ~25 fps : l'etoile du dessus clignote

struct WantedState {
  uint8_t  level;        // 0..WANTED_MAX (etoiles affichees)
  uint8_t  streak;       // meurtres dans la fenetre glissante courante
  uint16_t streakTimer;  // frames restantes avant reset du streak
  uint16_t decayTimer;   // frames restantes avant de perdre une etoile
};

inline void wantedReset(WantedState &w) {
  w.level = 0; w.streak = 0; w.streakTimer = 0; w.decayTimer = 0;
}

// Vrai quand l'etoile du dessus est dans ses 10 dernieres secondes (clignote).
// Faux au niveau 0 (rien) et au niveau 5 (gele, pas de decroissance).
inline bool wantedBlinking(const WantedState &w) {
  return w.level > 0 && w.level < WANTED_MAX &&
         w.decayTimer > 0 && w.decayTimer <= WANTED_BLINK_FRAMES;
}

// Un crime (pieton tue par le joueur). Accumule le streak ; au seuil, +1 etoile.
// Chaque crime relance la fenetre (20 s) et le decompte de decroissance (30 s).
// Si l'etoile du dessus clignote (fin de vie), le crime la RENOUVELLE sans en
// ajouter (le decompte est remis a plein de toute facon plus bas).
inline void wantedOnKill(WantedState &w) {
  w.streak = (w.streakTimer > 0) ? (uint8_t)(w.streak + 1) : 1;
  w.streakTimer = WANTED_STREAK_WINDOW;
  if (w.streak >= WANTED_STREAK_NEEDED) {
    w.streak = 0;
    if (!wantedBlinking(w) && w.level < WANTED_MAX) w.level++;
  }
  if (w.level > 0) w.decayTimer = WANTED_DECAY_FRAMES;
}

// Tuer un policier = crime majeur : +1 etoile IMMEDIATEMENT (sans passer par le
// streak des 3 meurtres) et relance le decompte de decroissance (30 s). Si
// l'etoile du dessus clignote, on la RENOUVELLE sans en ajouter. Au niveau max,
// on reste au plafond mais on rafraichit quand meme.
inline void wantedOnCopKill(WantedState &w) {
  if (!wantedBlinking(w) && w.level < WANTED_MAX) w.level++;
  w.decayTimer = WANTED_DECAY_FRAMES;
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
