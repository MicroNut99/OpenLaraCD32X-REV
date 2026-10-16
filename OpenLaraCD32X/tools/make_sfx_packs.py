#!/usr/bin/env python3
"""make_sfx_packs.py - Tomb Raider sound effects -> Sega CD PCM packs (OpenLara CD32X, OL-6)
    python3 make_sfx_packs.py gba/packer/out32x subcpu/ROMS
Reads every <LEVEL>.SND the (patched) packer writes next to <LEVEL>.PKD and writes SFXnn.BIN
(nn = the engine's LevelID), one pack per level, loaded into the PCM chip during that level's load.

<LEVEL>.SND (little-endian, from out_32X.h):  'TSND' u32 samples u32 infos  i16 soundMap[256]
    infos x (u16 index, u16 volume, u16 chance, u16 flags)   samples x (u32 length, WAV file)

SFXnn.BIN = Kobo's KSFX pack (big-endian) + a sample map:
    0x000 'KSFX' u16 count u16 image_bytes (0 = 65536)
    0x008 count x (u8 start_page, u8 volume, u16 fd, u16 loop_address, u16 length)
    0x100 PCM image (RF5C164: 8-bit sign-magnitude, 0xFF = loop marker), max 64 KB
    after the image: u8 map[256] - engine sample index -> pack effect (0 = not in the pack)
Why a selection: a level has far more sound than the chip's 64 KB.  Samples are taken in the
order of the lowest sound id that uses them (Lara's own sounds have the lowest ids), at 8 kHz,
each at most 1 s, until 31 effects or 64 KB are used."""
import io, os, struct, sys, wave

LEVELS = ["TITLE", "GYM", "LEVEL1", "LEVEL2", "LEVEL3A", "LEVEL3B", "CUT1", "LEVEL4", "LEVEL5",
          "LEVEL6", "LEVEL7A", "LEVEL7B", "CUT2", "LEVEL8A", "LEVEL8B", "LEVEL8C", "LEVEL10A",
          "CUT3", "LEVEL10B", "CUT4", "LEVEL10C"]          # = enum LevelID order
RATE, MAX_SECS, MAX_FX, RAM, PAGE, TAIL = 8000, 1.0, 31, 65536, 256, 16

def read_snd(path):
    d = open(path, "rb").read()
    if d[:4] != b"TSND":
        sys.exit("*** %s: not a TSND file" % path)
    n, ni = struct.unpack_from("<II", d, 4)
    smap = struct.unpack_from("<256h", d, 12)
    p = 12 + 512
    infos = [struct.unpack_from("<4H", d, p + i * 8) for i in range(ni)]
    p += ni * 8
    samples = []
    for i in range(n):
        ln, = struct.unpack_from("<I", d, p); p += 4
        samples.append(d[p:p + ln]); p += ln
    return smap, infos, samples

def decode(wavbytes):
    w = wave.open(io.BytesIO(wavbytes), "rb")
    ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    if width == 1:
        a = [(b - 128) / 128.0 for b in raw]
    elif width == 2:
        a = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    else:
        return None, rate
    if ch == 2:
        a = [(a[i] + a[i + 1]) / 2 for i in range(0, len(a) - 1, 2)]
    return a, rate

def resample(a, src, dst):
    n = max(1, int(round(len(a) * dst / src)))
    out = []
    for i in range(n):
        x = i * (len(a) - 1) / max(1, n - 1)
        j = int(x); f = x - j
        out.append(a[j] * (1 - f) + a[min(j + 1, len(a) - 1)] * f)
    return out

def sm8(v):
    m = min(126, int(round(abs(v) * 127)))       # 127 would give the 0xFF loop marker
    return (0x80 | m) if v >= 0 else m

def build(smap, infos, samples):
    order = []                                    # sample indices, most important first
    for sid in range(256):
        a = smap[sid]
        if a < 0 or a >= len(infos):
            continue
        idx, vol, chance, flags = infos[a]
        count = max(1, (flags >> 2) & 15)
        for k in range(count):
            s = idx + k
            if s < len(samples) and s < 255 and s not in [o[0] for o in order]:
                order.append((s, vol))
    image, table, smp_map = bytearray(), [], [0] * 256
    for s, vol in order:
        if len(table) >= MAX_FX:
            break
        a, src = decode(samples[s])
        if not a:
            continue
        a = a[: int(MAX_SECS * src)]
        fade = min(len(a), int(0.02 * src))
        for i in range(fade):
            a[len(a) - fade + i] *= 1 - (i + 1) / fade
        pcm = bytes(sm8(v) for v in resample(a, src, RATE))
        start = (len(image) + PAGE - 1) // PAGE * PAGE
        if start + len(pcm) + TAIL + 1 > RAM:
            continue                              # does not fit any more - try smaller ones
        image += bytes([0x80]) * (start - len(image))
        image += pcm
        loop = len(image)
        image += bytes([0x80]) * TAIL + b"\xff"
        table.append((start // PAGE, min(255, max(32, vol >> 7)), int(round(RATE * 2048 / 32552)), loop, len(pcm)))
        smp_map[s] = len(table)
    return image, table, smp_map

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1:]
    made = 0
    for lid, name in enumerate(LEVELS):
        path = os.path.join(src, name + ".SND")
        if not os.path.isfile(path):
            continue
        image, table, smp_map = build(*read_snd(path))
        hdr = bytearray(b"KSFX") + struct.pack(">HH", len(table), len(image) & 0xFFFF)
        for e in table:
            hdr += struct.pack(">BBHHH", *e)
        hdr += bytes(256 - len(hdr))
        out = os.path.join(dst, "SFX%02d.BIN" % lid)
        open(out, "wb").write(bytes(hdr) + bytes(image) + bytes(smp_map))
        print("  %-9s -> %s  %2d effects, %5d bytes of 65536" % (name, os.path.basename(out), len(table), len(image)))
        made += 1
    if not made:
        sys.exit("*** no .SND files in %s - re-run the patched packer first" % src)
    print("==> %d sound packs" % made)

if __name__ == "__main__":
    main()
