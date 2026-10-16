#!/usr/bin/env python3
"""
make_mixed_cd.py  -  turn the built ISO + a WAV into a real MIXED-MODE disc image (one .bin + one .cue)

A Sega CD is a mixed-mode disc:
    track 1   MODE 1 data            2048 bytes of data per sector, stored as 2352-byte sectors
                                     (12 sync, 4 header, 2048 data, 4 EDC, 8 zero, 276 P/Q error correction)
    track 2+  Red Book CD audio      16-bit stereo 44.1 kHz PCM, 2352 bytes per sector, 75 sectors per second
Everything sits in ONE .bin, addressed by ONE .cue.  Nothing on the data side is changed: the sectors of your
ISO are copied unchanged into track 1.

The layout follows the working cue of "DOOM CD32X FUSION V3 with IDKFA Soundtrack":  every audio track starts
directly after the previous track, and the cue has only  INDEX 01  lines (no INDEX 00, no PREGAP).  A strict
Red Book disc has a 2 second (150 sector) silent pre-gap in front of the first audio track; if you ever need it
add  --pregap 150  (the gap then goes into the .bin and the cue gets INDEX 00 / INDEX 01 lines).

    python3 make_mixed_cd.py                                              (same as the next line)
    python3 make_mixed_cd.py CDROMPlayer.iso Track02.wav                 -> CDROMPlayer_mixed.bin/.cue
    python3 make_mixed_cd.py CDROMPlayer.iso Track02.wav Track03.wav     (more audio tracks)
    python3 make_mixed_cd.py CDROMPlayer.iso Track02.wav --out MyDisc
    python3 make_mixed_cd.py --mode2                                     data track as MODE2/2352 (Mode 2 Form 1)
                                                                         instead of MODE1/2352 - see below
    python3 make_mixed_cd.py --pregap 150 --out CDROMPlayer_burn         for burning: Red Book 2 s gap before audio

--mode2:  the data track is written as CD-ROM Mode 2 Form 1 sectors (12 sync, 4 header with mode byte 2, 8 byte
subheader 00 00 08 00 twice, 2048 data, 4 EDC over subheader+data, 276 P/Q with the header counted as zero) and the
cue says  TRACK 01 MODE2/2352.  The 2048 data bytes are the same as in the ISO; only the sector wrapper differs.

The WAV must be PCM.  If it is not 16-bit / 44.1 kHz / stereo it is converted when Python has the 'audioop'
module (Python up to 3.12); otherwise convert it first:
    ffmpeg -i in.wav -ar 44100 -ac 2 -sample_fmt s16 Track02.wav
"""
import argparse, os, struct, sys, wave

SECTOR = 2352

# ------------------------------------------------------------------ Mode 1 error detection / correction
ECC_F = [0] * 256
ECC_B = [0] * 256
EDC_T = [0] * 256
for _i in range(256):
    _j = ((_i << 1) ^ (0x11D if _i & 0x80 else 0)) & 0xFF
    ECC_F[_i] = _j
    ECC_B[_i ^ _j] = _i
    _e = _i
    for _ in range(8):
        _e = (_e >> 1) ^ (0xD8018001 if _e & 1 else 0)
    EDC_T[_i] = _e


def edc(buf):
    e = 0
    for b in buf:
        e = (e >> 8) ^ EDC_T[(e ^ b) & 0xFF]
    return e


def ecc_block(s, base, major_count, minor_count, major_mult, minor_inc, dest):
    size = major_count * minor_count
    for major in range(major_count):
        index = (major >> 1) * major_mult + (major & 1)
        a = 0
        b = 0
        for _ in range(minor_count):
            t = s[base + index]
            index += minor_inc
            if index >= size:
                index -= size
            a ^= t
            b ^= t
            a = ECC_F[a]
        a = ECC_B[ECC_F[a] ^ b]
        s[dest + major] = a
        s[dest + major + major_count] = a ^ b


def bcd(v):
    return ((v // 10) << 4) | (v % 10)


def msf(sectors):
    return sectors // (75 * 60), (sectors // 75) % 60, sectors % 75


def mode1_sector(lba, data):
    """lba = sector number counted from the start of the disc image (track 1 starts at 0)"""
    s = bytearray(SECTOR)
    s[1:11] = b"\xff" * 10                                # sync: 00 FF*10 00
    m, sec, f = msf(lba + 150)                            # header time = LBA + 2 s
    s[12], s[13], s[14], s[15] = bcd(m), bcd(sec), bcd(f), 1
    s[16:16 + 2048] = data
    s[0x810:0x814] = struct.pack("<I", edc(s[0:0x810]))
    ecc_block(s, 0x0C, 86, 24, 2, 86, 0x81C)              # P parity
    ecc_block(s, 0x0C, 52, 43, 86, 88, 0x8C8)             # Q parity (covers the P bytes too)
    return bytes(s)


def mode2f1_sector(lba, data):
    """Mode 2 Form 1 (the sector type of CD-ROM XA data): same 2048 data bytes, different wrapper"""
    s = bytearray(SECTOR)
    s[1:11] = b"\xff" * 10
    m, sec, f = msf(lba + 150)
    s[12], s[13], s[14], s[15] = bcd(m), bcd(sec), bcd(f), 2
    s[16:20] = b"\x00\x00\x08\x00"                      # subheader: file 0, channel 0, submode 08 = data, coding 0
    s[20:24] = b"\x00\x00\x08\x00"                      # ... stored twice
    s[24:24 + 2048] = data
    s[0x818:0x81C] = struct.pack("<I", edc(s[0x10:0x818]))
    hdr = bytes(s[12:16])
    s[12:16] = b"\x00\x00\x00\x00"                      # P/Q are computed with the header counted as zero
    ecc_block(s, 0x0C, 86, 24, 2, 86, 0x81C)
    ecc_block(s, 0x0C, 52, 43, 86, 88, 0x8C8)
    s[12:16] = hdr
    return bytes(s)


# ------------------------------------------------------------------ audio
def wav_to_pcm(path):
    try:
        w = wave.open(path, "rb")
    except Exception as e:
        sys.exit("%s: not a plain PCM WAV (%s)\n  convert it:  ffmpeg -i in.wav -ar 44100 -ac 2 -sample_fmt s16 %s" % (path, e, path))
    ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    data = w.readframes(n)
    w.close()
    if (ch, width, rate) != (2, 2, 44100):
        try:
            import audioop
        except ImportError:
            sys.exit("%s is %d ch / %d bit / %d Hz and this Python has no audioop.\n  convert it:  ffmpeg -i in.wav -ar 44100 -ac 2 -sample_fmt s16 out.wav" % (path, ch, width * 8, rate))
        if width != 2:
            data = audioop.lin2lin(data, width, 2)
        if ch == 1:
            data = audioop.tostereo(data, 2, 1, 1)
        elif ch != 2:
            sys.exit("%s has %d channels; use 1 or 2" % (path, ch))
        if rate != 44100:
            data, _ = audioop.ratecv(data, 2, 2, rate, 44100, None)
        print("  converted %s (%d ch, %d bit, %d Hz) to 16 bit stereo 44.1 kHz" % (path, ch, width * 8, rate))
    return data


def cue_time(sectors):
    m, s, f = msf(sectors)
    return "%02d:%02d:%02d" % (m, s, f)


def main():
    ap = argparse.ArgumentParser(description="ISO + WAV -> mixed-mode BIN/CUE")
    ap.add_argument("iso", nargs="?", default="CDROMPlayer.iso")
    ap.add_argument("wavs", nargs="*", help="audio tracks in order = track 2, 3, ...  (default Track02.wav)")
    ap.add_argument("--out", default=None, help="output name without extension (default <iso name>_mixed)")
    ap.add_argument("--mode2", action="store_true", help="write the data track as MODE2/2352 (Mode 2 Form 1) instead of MODE1/2352")
    ap.add_argument("--pregap", type=int, default=150, help="silent sectors in front of each audio track (default 0, like the working Doom CD32X cue; Red Book says 150)")
    a = ap.parse_args()
    if not a.wavs:
        a.wavs = ["Track02.wav"]
    for f in [a.iso] + a.wavs:
        if not os.path.isfile(f):
            sys.exit("cannot find %s in %s\n  usage: python3 make_mixed_cd.py [iso] [wav ...]" % (f, os.getcwd()))

    iso = open(a.iso, "rb").read()
    if len(iso) % 2048:
        sys.exit("%s: size is not a multiple of 2048 - this is not a plain 2048-byte-sector ISO" % a.iso)
    nsec = len(iso) // 2048
    out = a.out or (os.path.splitext(a.iso)[0] + ("_mixed_mode2" if a.mode2 else "_mixed"))
    make_sector = mode2f1_sector if a.mode2 else mode1_sector
    binname = os.path.basename(out) + ".bin"

    print("data track: %d sectors (%d bytes), written as %s" % (nsec, len(iso), "MODE2/2352 (Mode 2 Form 1)" if a.mode2 else "MODE1/2352"))
    tracks = []                                   # (start sector of INDEX 01, pcm bytes)
    pos = nsec
    with open(out + ".bin", "wb") as fb:
        for lba in range(nsec):
            fb.write(make_sector(lba, iso[lba * 2048:(lba + 1) * 2048]))
        cue = ['FILE "%s" BINARY' % binname, "  TRACK 01 %s" % ("MODE2/2352" if a.mode2 else "MODE1/2352"), "    INDEX 01 00:00:00"]
        for k, wp in enumerate(a.wavs):
            pcm = wav_to_pcm(wp)
            pad = (-len(pcm)) % SECTOR
            pcm += b"\x00" * pad
            asec = len(pcm) // SECTOR
            if a.pregap:
                fb.write(b"\x00" * (a.pregap * SECTOR))    # silent pre-gap
            fb.write(pcm)
            cue.append("  TRACK %02d AUDIO" % (k + 2))
            if a.pregap:
                cue.append("    INDEX 00 %s" % cue_time(pos))
            cue.append("    INDEX 01 %s" % cue_time(pos + a.pregap))
            print("audio track %d: %s  %d sectors = %.1f s   INDEX 01 at %s" % (k + 2, wp, asec, asec / 75.0, cue_time(pos + a.pregap)))
            pos += a.pregap + asec
    with open(out + ".cue", "w", newline="") as fc:
        fc.write("\r\n".join(cue) + "\r\n")
    print("wrote %s.bin (%d bytes) and %s.cue" % (out, os.path.getsize(out + ".bin"), out))
    print("Load the .cue (not the .bin) in the emulator.")


if __name__ == "__main__":
    main()
