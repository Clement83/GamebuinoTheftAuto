// genere par tools/build_music.py -- NE PAS editer
#pragma once
#include <stdint.h>

#define MUSIC_NUM_TRACKS 8
#define MUSIC_DEFAULT_TRACK 0
#define MUS_DOWNTOWN 0
#define MUS_DOCKS 1
#define MUS_BEACH 2
#define MUS_HOOD 3
#define MUS_CASINO 4
#define MUS_CHINATOWN 5
#define MUS_BAR 6
#define MUS_AMMUNATION 7

// Une piste = deux voix (Hz par pas, 0 = silence) + tempo (frames/pas).
struct MusicTrack {
  const uint16_t* bass;
  const uint16_t* lead;
  uint16_t steps;
  uint8_t stepFrames;
};
extern const MusicTrack musicTracks[MUSIC_NUM_TRACKS];

// Mapping nom-de-POI -> piste (strcmp au runtime). POI absent = garde la piste courante.
#define MUSIC_NUM_POI_MAP 24
struct MusicPoi {
  const char* name;
  uint8_t track;
};
extern const MusicPoi musicPoiMap[MUSIC_NUM_POI_MAP];
