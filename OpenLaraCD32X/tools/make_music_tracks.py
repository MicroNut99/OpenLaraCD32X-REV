#!/usr/bin/env python3
"""make_music_tracks.py - CD audio tracks for OpenLara CD32X
    python3 make_music_tracks.py music_map.txt soundtrack_wav soundtrack_cd
Writes soundtrack_cd/Track02.wav ... Track59.wav: disc track N = game track N.  A number listed
in music_map.txt gets that WAV (44.1 kHz 16-bit stereo, as converted from the MP3s); every
other number gets 2 s of silence, so the numbering never shifts."""
import os, shutil, sys, wave

FIRST, LAST, LAST_MAX = 2, 80, 84     # UB-1: 81-84 = the Unfinished Business levels' music (optional)

def silence(path, seconds=2):
    w = wave.open(path, "wb")
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(44100)
    w.writeframes(b"\0" * (44100 * 4 * seconds))
    w.close()

def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    mapfile, src, dst = sys.argv[1:]
    files = sorted(f for f in os.listdir(src) if f.lower().endswith(".wav"))
    table = {}
    for n, line in enumerate(open(mapfile), 1):
        line = line.split("#")[0].strip()
        if not line:
            continue
        parts = line.split(None, 1)
        if len(parts) != 2 or not parts[0].isdigit():
            sys.exit("*** %s line %d: expected '<track> <file name start>'" % (mapfile, n))
        t, start = int(parts[0]), parts[1].strip()
        if not FIRST <= t <= LAST_MAX:
            sys.exit("*** %s line %d: track %d outside %d-%d" % (mapfile, n, t, FIRST, LAST_MAX))
        hits = [f for f in files if f.startswith(start)]
        if len(hits) != 1:
            sys.exit("*** %s line %d: '%s' matches %d files in %s" % (mapfile, n, start, len(hits), src))
        table[t] = hits[0]
    os.makedirs(dst, exist_ok=True)
    last = max([LAST] + list(table))            # UB-1: tracks after 80 only when the map lists one
    for f in os.listdir(dst):                   # an earlier build's extra tracks
        if f.startswith("Track") and f.endswith(".wav") and f[5:-4].isdigit() and int(f[5:-4]) > last:
            os.remove(os.path.join(dst, f))
    for t in range(FIRST, last + 1):
        out = os.path.join(dst, "Track%02d.wav" % t)
        if t in table:
            shutil.copyfile(os.path.join(src, table[t]), out)
            print("  track %02d  %s" % (t, table[t]))
        else:
            silence(out)
    print("==> %d CD audio tracks (%d with music, %d silent) in %s"
          % (last - FIRST + 1, len(table), last - FIRST + 1 - len(table), dst))

if __name__ == "__main__":
    main()
