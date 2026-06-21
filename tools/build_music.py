"""Genere la bande son chiptune en flash (gta/music.{h,cpp}).

Plusieurs pistes (une par ambiance de quartier), 2 voix chacune (basse + melodie),
jouees a l'execution par un sequenceur pilote par frame (cf. gta/mod_music.h) qui
re-declenche gb.sound.tone() a chaque pas. Le sequenceur choisit la piste selon le
POI courant (poiAtTile) -> mapping ZONE dans assets/music.txt. Compos *originales*
(facon G-funk / Vice City / dub) -> aucun souci de droits.

On aplatit la notation en tableaux uint16 de frequences (Hz, 0 = silence) par voix
et par piste, plus une table de mapping nom-de-POI -> piste (strcmp au runtime).

Lancer depuis la racine : .venv/bin/python -m tools.build_music
"""
import os
import re

SRC = "assets/music.txt"
HDR = "gta/music.h"
CPP = "gta/music_data.cpp"

# Notation anglaise -> classe de hauteur (do=0). A4 = 440 Hz = midi 69.
_PITCH = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
_NOTE_RE = re.compile(r"^([A-G])([#b]?)(-?\d)$")


def note_to_freq(tok):
    """'A4' -> 440. '-' -> 0 (repos). '.' est resolu par l'appelant (re-pluck)."""
    m = _NOTE_RE.match(tok)
    if not m:
        raise ValueError("note invalide: %r" % tok)
    letter, accidental, octave = m.group(1), m.group(2), int(m.group(3))
    semitone = _PITCH[letter] + (1 if accidental == "#" else -1 if accidental == "b" else 0)
    midi = 12 * (octave + 1) + semitone
    return round(440.0 * (2.0 ** ((midi - 69) / 12.0)))


def _tokens_to_freqs(rest, acc):
    for tok in rest.split():
        if tok == "-":
            acc.append(0)
        elif tok == ".":
            acc.append(acc[-1] if acc else 0)   # re-pluck = meme note
        else:
            acc.append(note_to_freq(tok))


def parse(path):
    """Lit music.txt -> (tracks, zones, default).
    tracks : liste de dict {name, tempo, bass[], lead[]} dans l'ordre du fichier.
    zones  : liste de (poi_name, track_index). default : track_index de repli."""
    tracks = []
    order = {}                 # nom de piste -> index
    cur = None
    zones = []
    default_name = None

    def flush():
        if cur is not None:
            n = max(len(cur["bass"]), len(cur["lead"]))
            cur["bass"].extend([0] * (n - len(cur["bass"])))
            cur["lead"].extend([0] * (n - len(cur["lead"])))
            order[cur["name"]] = len(tracks)
            tracks.append(cur)

    with open(path, encoding="utf-8") as f:
        for raw in f:
            # '#' = commentaire seulement en debut de ligne ou apres un espace
            # (sinon c'est un diese : D#5). On preserve l'espace separateur.
            line = re.sub(r"(^|\s)#.*$", r"\1", raw).strip()
            if not line:
                continue
            head, _, rest = line.partition(":")
            key, rest = head.strip().upper(), rest.strip()
            if key == "TRACK":
                flush()
                cur = {"name": rest, "tempo": None, "bass": [], "lead": []}
            elif key == "TEMPO":
                cur["tempo"] = int(rest)
            elif key == "BASS":
                _tokens_to_freqs(rest, cur["bass"])
            elif key == "LEAD":
                _tokens_to_freqs(rest, cur["lead"])
            elif key == "ZONE":
                tname, _, pois = rest.partition("=")
                zones.append((tname.strip(), [p.strip() for p in pois.split(",") if p.strip()]))
            elif key == "DEFAULT":
                default_name = rest
            else:
                raise ValueError("ligne inconnue: %r" % raw.rstrip())
    flush()

    if not tracks:
        raise ValueError("aucune piste (TRACK:) dans %s" % path)
    for t in tracks:
        if t["tempo"] is None:
            raise ValueError("TEMPO: manquant pour la piste %r" % t["name"])

    # Resout les mappings nom-de-piste -> index, aplatit en (poi, track_idx).
    poi_map = []
    for tname, pois in zones:
        if tname not in order:
            raise ValueError("ZONE vise une piste inconnue: %r" % tname)
        for p in pois:
            poi_map.append((p, order[tname]))
    default_idx = order.get(default_name, 0) if default_name else 0
    return tracks, poi_map, default_idx


def _row(freqs):
    return ", ".join("%4d" % f for f in freqs)


def build():
    tracks, poi_map, default_idx = parse(SRC)
    n = len(tracks)

    with open(HDR, "w", encoding="utf-8") as f:
        f.write("// genere par tools/build_music.py -- NE PAS editer\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define MUSIC_NUM_TRACKS %d\n" % n)
        f.write("#define MUSIC_DEFAULT_TRACK %d\n" % default_idx)
        for i, t in enumerate(tracks):
            f.write("#define MUS_%s %d\n" % (re.sub(r"[^A-Z0-9]", "_", t["name"].upper()), i))
        f.write("\n// Une piste = deux voix (Hz par pas, 0 = silence) + tempo (frames/pas).\n")
        f.write("struct MusicTrack {\n")
        f.write("  const uint16_t* bass;\n  const uint16_t* lead;\n")
        f.write("  uint16_t steps;\n  uint8_t stepFrames;\n};\n")
        f.write("extern const MusicTrack musicTracks[MUSIC_NUM_TRACKS];\n\n")
        f.write("// Mapping nom-de-POI -> piste (strcmp au runtime). POI absent = garde la piste courante.\n")
        f.write("#define MUSIC_NUM_POI_MAP %d\n" % len(poi_map))
        f.write("struct MusicPoi {\n  const char* name;\n  uint8_t track;\n};\n")
        f.write("extern const MusicPoi musicPoiMap[MUSIC_NUM_POI_MAP];\n")

    with open(CPP, "w", encoding="utf-8") as f:
        f.write("// genere par tools/build_music.py -- NE PAS editer\n")
        f.write('#include "music.h"\n\n')
        for i, t in enumerate(tracks):
            f.write("// piste %d : %s (tempo %d, %d pas)\n"
                    % (i, t["name"], t["tempo"], len(t["bass"])))
            f.write("static const uint16_t bass%d[] = { %s };\n" % (i, _row(t["bass"])))
            f.write("static const uint16_t lead%d[] = { %s };\n\n" % (i, _row(t["lead"])))
        f.write("const MusicTrack musicTracks[MUSIC_NUM_TRACKS] = {\n")
        for i, t in enumerate(tracks):
            f.write("  { bass%d, lead%d, %d, %d },\n" % (i, i, len(t["bass"]), t["tempo"]))
        f.write("};\n\n")
        f.write("const MusicPoi musicPoiMap[MUSIC_NUM_POI_MAP] = {\n")
        for name, idx in poi_map:
            f.write('  { "%s", %d },\n' % (name, idx))
        f.write("};\n")

    flash = sum(len(t["bass"]) * 2 * 2 for t in tracks)
    print("OK: %d pistes, %d mappings POI -> %s, %s (~%d o flash de notes)"
          % (n, len(poi_map), HDR, CPP, flash))


if __name__ == "__main__":
    build()
