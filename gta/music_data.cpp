// genere par tools/build_music.py -- NE PAS editer
#include "music.h"

// piste 0 : downtown (tempo 5, 32 pas)
static const uint16_t bass0[] = {   55,   55,    0,   55,    0,   55,    0,    0,   55,    0,    0,   65,    0,   73,    0,    0,   82,   82,    0,   82,    0,   73,    0,    0,   65,    0,   65,    0,   55,    0,    0,    0 };
static const uint16_t lead0[] = {    0,    0,    0,    0,    0,    0,    0,    0,  440,    0,  523,    0,  659,    0,  587,    0,  659,    0,  587,    0,  523,    0,  440,    0,    0,    0,  392,    0,  440,    0,    0,    0 };

// piste 1 : docks (tempo 6, 32 pas)
static const uint16_t bass1[] = {   82,    0,   82,    0,   82,    0,  123,    0,   65,    0,   65,    0,   65,    0,   98,    0,   98,    0,   98,    0,   98,    0,  147,    0,   73,    0,   73,    0,   73,    0,  110,    0 };
static const uint16_t lead1[] = {  494,    0,  587,    0,  659,    0,  587,  494,  523,    0,  659,    0,  784,    0,  659,    0,  587,    0,  784,    0,  587,    0,  494,    0,  440,    0,  587,    0,  740,    0,  587,    0 };

// piste 2 : beach (tempo 6, 16 pas)
static const uint16_t bass2[] = {   65,    0,   98,    0,   65,    0,   98,    0,  110,    0,   82,    0,   98,    0,   98,    0 };
static const uint16_t lead2[] = {  330,  392,  523,    0,    0,  392,  330,    0,  349,  440,  523,    0,    0,  392,    0,    0 };

// piste 3 : hood (tempo 5, 16 pas)
static const uint16_t bass3[] = {   55,    0,   55,    0,    0,   55,    0,   65,    0,   73,    0,    0,   82,    0,   73,    0 };
static const uint16_t lead3[] = {    0,    0,  440,    0,  523,    0,  587,    0,  659,    0,  587,  523,    0,  440,    0,    0 };

// piste 4 : casino (tempo 5, 16 pas)
static const uint16_t bass4[] = {   65,    0,   82,    0,   98,    0,  110,    0,   87,    0,  110,    0,   98,    0,   82,    0 };
static const uint16_t lead4[] = {  392,    0,  330,    0,  523,    0,    0,  494,  523,    0,  392,    0,  330,    0,    0,    0 };

// piste 5 : chinatown (tempo 5, 16 pas)
static const uint16_t bass5[] = {   65,    0,    0,   98,    0,    0,   65,    0,  110,    0,    0,   98,    0,    0,   65,    0 };
static const uint16_t lead5[] = {  659,  784,  880,    0,  784,  659,  587,    0,  659,    0,  523,  587,  659,    0,    0,    0 };

// piste 6 : bar (tempo 6, 16 pas)
static const uint16_t bass6[] = {   55,    0,    0,   82,    0,    0,   55,    0,   65,    0,    0,   82,    0,   73,    0,    0 };
static const uint16_t lead6[] = {    0,    0,  440,    0,  523,  622,    0,  523,    0,  440,    0,    0,  330,    0,    0,    0 };

// piste 7 : ammunation (tempo 4, 16 pas)
static const uint16_t bass7[] = {   82,   82,    0,   82,   82,    0,   82,    0,   98,   98,    0,   98,   82,    0,   73,    0 };
static const uint16_t lead7[] = {    0,    0,    0,    0,  494,    0,    0,    0,    0,    0,  523,    0,  494,    0,    0,    0 };

const MusicTrack musicTracks[MUSIC_NUM_TRACKS] = {
  { bass0, lead0, 32, 5 },
  { bass1, lead1, 32, 6 },
  { bass2, lead2, 16, 6 },
  { bass3, lead3, 16, 5 },
  { bass4, lead4, 16, 5 },
  { bass5, lead5, 16, 5 },
  { bass6, lead6, 16, 6 },
  { bass7, lead7, 16, 4 },
};

const MusicPoi musicPoiMap[MUSIC_NUM_POI_MAP] = {
  { "Pillbox", 0 },
  { "St. Mark's", 0 },
  { "Bedford Pt", 0 },
  { "Las Colinas", 0 },
  { "Portland", 0 },
  { "Garcia", 0 },
  { "Les Bureaux", 0 },
  { "Les Quais", 1 },
  { "Chantier", 1 },
  { "La Casse", 1 },
  { "Rockford", 1 },
  { "Le Garage", 1 },
  { "Vice Point", 2 },
  { "Ocean Beach", 2 },
  { "Verona Bch", 2 },
  { "Bayview", 2 },
  { "Planque", 2 },
  { "Little Haiti", 3 },
  { "Hashbury", 3 },
  { "Cedar Grove", 3 },
  { "Commerces", 3 },
  { "Le Casino", 4 },
  { "Chinatown", 5 },
  { "Le Bar", 6 },
};
