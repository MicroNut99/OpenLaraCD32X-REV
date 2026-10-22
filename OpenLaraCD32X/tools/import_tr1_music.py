#!/usr/bin/env python3
"""import_tr1_music.py - the game's own music cues and voice lines onto the disc (CUES-1).

    python3 tools/import_tr1_music.py "<folder with track02.flac ... track60.flac>" soundtrack_wav music_map.txt

The PC game CD has only 9 audio tracks.  The music cues the levels trigger (game tracks 6-21)
and the voice lines (26-50 Lara's Home hints, 51-56 Baldy, the cowboy, Larson, Natla, Pierre
and the skater) are on the PlayStation / Saturn discs - and in the "music" folder of the PS1
soundtrack set many players use with TR1X (track02.flac ... track60.flac, numbered by game
track, see TRX docs/trx/MUSIC.md).

This converts tracks 6-21 and 26-56 with ffmpeg to soundtrack_wav/trNN.wav (44.1 kHz, 16-bit
stereo) and adds a line "NN  trNN" to music_map.txt for each game track the map does not list
yet (your own choices stay).  Disc track N = game track N, so the engine finds them where the
game asks; build.sh tells the engine which tracks are on the disc (the others stay ignored, so
the level music is not interrupted by silence).  About 14 minutes of CD audio.
Track 13 (the secret chime) and 22-25 (the cut-scenes) are left as they are."""
import os, re, subprocess, sys

TRACKS = list(range(6, 22)) + list(range(26, 57))

def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    src, out, mapfile = sys.argv[1:]
    os.makedirs(out, exist_ok=True)
    lines = open(mapfile).read().splitlines() if os.path.isfile(mapfile) else []
    listed = set()
    for l in lines:
        m = re.match(r"\s*(\d+)\s+\S", l)
        if m:
            listed.add(int(m.group(1)))
    added = []
    for t in TRACKS:
        flac = None
        for name in ("track%02d.flac" % t, "track%02d.FLAC" % t, "%02d.flac" % t):
            if os.path.isfile(os.path.join(src, name)):
                flac = os.path.join(src, name); break
        if not flac:
            print("  game track %d: no file - left out" % t)
            continue
        wav = os.path.join(out, "tr%02d.wav" % t)
        if not os.path.isfile(wav):
            r = subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", flac, "-ar", "44100", "-ac", "2",
                                "-sample_fmt", "s16", wav])
            if r.returncode != 0:
                sys.exit("*** ffmpeg failed on " + flac)
        if t not in listed:
            added.append("%02d  tr%02d" % (t, t))
    if added:
        with open(mapfile, "a") as f:
            f.write("\n# CUES-1: the game's own cues (6-21) and voice lines (26-56), tools/import_tr1_music.py\n")
            f.write("\n".join(added) + "\n")
    print("==> %d tracks converted, %d added to %s" % (len(TRACKS), len(added), mapfile))

if __name__ == "__main__":
    main()
