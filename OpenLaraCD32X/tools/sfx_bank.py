#!/usr/bin/env python3
"""sfx_bank.py - every sound effect of a level into the level file, for the 32X PWM mixer (SFX-2)
    python3 sfx_bank.py gba/packer/out32x subcpu/ROMS

For each <LEVEL>.PKD in the second folder (the copies build.sh puts on the disc), reads
<LEVEL>.SND from the first folder and APPENDS a sound bank to the .PKD:

    soundData ->   'SFXB' u16 rate u16 count            (big-endian, 4-byte aligned)
                   samples: u32 length, then length bytes of signed 8-bit PCM at `rate`
    soundOffsets-> s32[count]: each sample's record, relative to soundData (-1 = left out)

and points the level header's soundData / soundOffsets at them (file offsets, as every other
pointer in the header: read_PKD() adds the load address).  soundOffsetsCount is already the
number of samples.  Nothing else in the file changes, so a level without a bank still loads.

The level is loaded into the cart with its bank, so each level has its own room limit:
    Caves (LEVEL1, the title)  cart 0x080000 .. 0x1FF000 (the cart test's piece 15 after it)
    every other level          cart 0x200000 .. 0x3F0000 (the save area after it)
The rate is the highest of 8000 / 7000 / 6000 / 5000 Hz that fits; samples longer than 3 s are
cut (with a short fade).  A file that already has a bank is left alone (build.sh copies fresh
.PKD files every build)."""
import io, os, struct, sys, wave

LIMIT = {"LEVEL1.PKD": 0x17F000}            # Caves: 0x080000 + this = 0x1FF000
LIMIT_DEFAULT = 0x1F0000                    # 0x200000 + this = 0x3F0000 (the save area)
MARGIN = 4096
RATES = (8000, 7000, 6000, 5000)
MAX_SECS = 3.0
HDR_SIZE = 172                              # out_32X.h Header
OFS_COUNT = 30                              # u16 soundOffsetsCount
OFS_DATA = 164                              # u32 soundData
OFS_OFFSETS = 168                           # u32 soundOffsets


def read_snd(path):
    d = open(path, "rb").read()
    if d[:4] != b"TSND":
        sys.exit("*** %s: not a TSND file" % path)
    n, ni = struct.unpack_from("<II", d, 4)
    p = 12 + 512 + ni * 8
    samples = []
    for i in range(n):
        ln, = struct.unpack_from("<I", d, p); p += 4
        samples.append(d[p:p + ln]); p += ln
    return samples


def decode(wavbytes):
    try:
        w = wave.open(io.BytesIO(wavbytes), "rb")
    except Exception:
        return None, 0
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


def pcm8(a, src, rate):
    cut = len(a) > int(MAX_SECS * src)
    a = a[: int(MAX_SECS * src)]
    if cut:
        fade = min(len(a), int(0.02 * src))
        for i in range(fade):
            a[len(a) - fade + i] *= 1 - (i + 1) / fade
    return bytes((max(-127, min(127, int(round(v * 127)))) & 0xFF) for v in resample(a, src, rate))


def make_bank(decoded, rate):
    count = len(decoded)
    bank = bytearray(b"SFXB" + struct.pack(">HH", rate, count))
    offsets = []
    for a, src in decoded:
        if not a:
            offsets.append(-1)
            continue
        pcm = pcm8(a, src, rate)
        offsets.append(len(bank))
        bank += struct.pack(">I", len(pcm)) + pcm
        if len(bank) & 1:
            bank += b"\0"
    while len(bank) & 3:
        bank += b"\0"
    return bank, offsets


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1:]
    done = 0
    for name in sorted(os.listdir(dst)):
        if not name.upper().endswith(".PKD"):
            continue
        snd = os.path.join(src, name[:-4] + ".SND")
        pkd = os.path.join(dst, name)
        if not os.path.isfile(snd):
            print("  %-12s no .SND - no bank" % name)
            continue
        d = bytearray(open(pkd, "rb").read())
        count, = struct.unpack_from(">H", d, OFS_COUNT)
        data_ofs, = struct.unpack_from(">I", d, OFS_DATA)
        if data_ofs + 4 <= len(d) and d[data_ofs:data_ofs + 4] == b"SFXB":
            print("  %-12s already has a bank" % name)
            continue
        samples = read_snd(snd)
        if len(samples) != count:
            sys.exit("*** %s: %d samples in the .SND, %d in the level" % (name, len(samples), count))
        decoded = [decode(s) for s in samples]
        limit = LIMIT.get(name.upper(), LIMIT_DEFAULT) - MARGIN
        base = (len(d) + 3) & ~3
        for rate in RATES:
            bank, offsets = make_bank(decoded, rate)
            if base + len(bank) + 4 * count <= limit:
                break
        else:
            print("  %-12s *** no room for a bank (%d bytes free) - PCM packs only" % (name, limit - base))
            continue
        d += b"\0" * (base - len(d))
        d += bank
        table = len(d)
        d += b"".join(struct.pack(">i", o) for o in offsets)
        struct.pack_into(">I", d, OFS_DATA, base)
        struct.pack_into(">I", d, OFS_OFFSETS, table)
        open(pkd, "wb").write(bytes(d))
        print("  %-12s %3d samples at %d Hz, bank %4d KB, file %4d KB of %4d KB" %
              (name, count, rate, (len(bank) + 4 * count) // 1024, len(d) // 1024, (limit + MARGIN) // 1024))
        done += 1
    print("==> %d sound banks" % done)


if __name__ == "__main__":
    main()
