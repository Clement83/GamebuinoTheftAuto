#pragma once
// mod_music.h - sequenceur multi-pistes de la bande son chiptune.
// Donnees generees par tools/build_music.py (music.h / music_data.cpp).
//
// Une piste par ambiance de quartier (2 voix : basse + melodie). musicTick() est
// appele une fois par frame. Tous les stepFrames frames on avance d'un pas et on
// re-declenche gb.sound.tone() pour les 2 voix. A chaque debut de boucle (pas 0)
// on regarde le POI sous le joueur (poiAtTile, le meme nom qu'affiche le HUD) et
// on bascule de piste si le quartier en demande une autre -> changement cale sur
// le temps fort, sans coupure brutale. Un POI non mappe garde la piste courante
// (routes, petits services -> pas de coupure). On occupe au plus 2 des 4 canaux
// son ; les 2 autres restent aux SFX. Le mute console coupe tout.

#include "music.h"

#define MUSIC_MS_PER_FRAME 40                 // 25 fps (defaut Gamebuino, non modifie)

static bool    musicOn       = false;
static uint8_t musicTrack    = MUSIC_DEFAULT_TRACK;
static uint8_t musicStep     = 0;             // pas courant dans la boucle
static uint8_t musicFrameCtr = 0;             // frames restantes avant le prochain pas
static bool    musicWasShop  = false;         // etat magasin a la frame precedente (bascule immediate)

// Piste demandee par le contexte courant, ou -1 pour "garder la piste courante"
// (hors POI, ou POI non mappe -> pas de coupure sur les routes/petits services).
// Le magasin d'armes ouvert (AMU Nation) a priorite sur la position.
static int musicWantedTrack() {
  if (shopOpen) return MUS_AMMUNATION;
  int cx = driving ? (int)car.x : playerX + PLAYER_W / 2;
  int cy = driving ? (int)car.y : playerY + PLAYER_H / 2;
  int pi = poiAtTile(cx >> 3, cy >> 3);
  if (pi < 0) return -1;
  const char *name = cityPois[pi].name;
  for (uint8_t i = 0; i < MUSIC_NUM_POI_MAP; i++) {
    if (strcmp(name, musicPoiMap[i].name) == 0) return musicPoiMap[i].track;
  }
  return -1;
}

// Demarre la boucle sur la piste du quartier de depart (sinon piste par defaut).
static void musicStart() {
  int w = musicWantedTrack();
  musicTrack = (w >= 0) ? (uint8_t)w : MUSIC_DEFAULT_TRACK;
  musicStep = 0;
  musicFrameCtr = 0;
  musicOn = true;
}

static void musicStop() {
  musicOn = false;
}

// A appeler une fois par frame. Avance le sequenceur, declenche les notes et
// gere la bascule de piste au debut de chaque boucle.
static void musicTick() {
  if (!musicOn) return;
  if (shopOpen != musicWasShop) {             // ouverture/fermeture du magasin -> bascule immediate
    musicWasShop = shopOpen;
    musicStep = 0;
    musicFrameCtr = 0;
  }
  if (musicFrameCtr == 0) {
    if (musicStep == 0) {                     // temps fort : on peut changer de quartier
      int w = musicWantedTrack();
      if (w >= 0 && (uint8_t)w != musicTrack) musicTrack = (uint8_t)w;
    }
    const MusicTrack &t = musicTracks[musicTrack];
    int32_t dur = (int32_t)t.stepFrames * MUSIC_MS_PER_FRAME;
    uint16_t b = t.bass[musicStep];
    if (b) gb.sound.tone(b, dur);             // basse punchy : un pas
    uint16_t l = t.lead[musicStep];
    if (l) gb.sound.tone(l, dur * 2);         // lead legato : traine sur deux pas
    if (++musicStep >= t.steps) musicStep = 0;
    musicFrameCtr = t.stepFrames;
  }
  musicFrameCtr--;
}
