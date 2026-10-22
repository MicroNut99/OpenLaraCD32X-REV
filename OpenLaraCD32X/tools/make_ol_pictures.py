#!/usr/bin/env python3
"""make_ol_pictures.py - the area loading pictures and the end credits for OpenLara CD32X (PICS-1)

    python3 tools/make_ol_pictures.py "<PlayStation DELDATA folder>" "<PC DATA folder>" "<UB DATA folder>" subcpu/ROMS

Any of the three folders may be "-" (or missing): its pictures are left out, and the game then
shows the usual loading picture there, or skips those credit pages.

  PlayStation disc, folder DELDATA (RNC-packed 384 x 256, 15-bit):
    GYMLOAD.RAW  -> IMAGE17.RAW  Lara's Home
    AZTECLOA.RAW -> IMAGE18.RAW  Peru         (Vilcabamba ... Qualopec; Caves keeps the title picture)
    GREEKLOA.RAW -> IMAGE19.RAW  Greece       (St. Francis' Folly ... Tihocan)
    EGYPTLOA.RAW -> IMAGE21.RAW  Egypt        (Khamoon ... Sanctuary; Return to Egypt, Temple of the Cat)
    ATLANLOA.RAW -> IMAGE22.RAW  Atlantis     (Natla's Mines ... the Great Pyramid; Atlantean Stronghold, The Hive)
  PC game, folder DATA (640 x 480 PCX):
    CRED1.PCX -> IMAGE23.RAW,  CRED2.PCX -> IMAGE24.RAW,  CRED3.PCX -> IMAGE25.RAW,  END.PCX -> IMAGE26.RAW
  Unfinished Business, folder DATA:
    CRED0.PCX -> IMAGE27.RAW   (its own credits page, shown first after The Hive)

Each IMAGEnn.RAW: 256 palette entries (32X BGR555, big-endian, 512 bytes) + 320 x 224 pixels, one
byte each (72192 bytes).  Colours 0-251 = the picture (0 = its darkest: a cleared screen shows
colour 0), 252 black, 253-255 the red loading bar.  The Sub-CPU loads file IMAGEnn.RAW to cart
nn x 128 KB (inside the level slot, before the level itself), so no new command is needed.
Needs Pillow."""
import os, struct, sys
try:
    from PIL import Image
except ImportError:
    sys.exit("*** Python's PIL is missing:  sudo apt install python3-pil   (or: pip install pillow)")

W, H, NCOL = 320, 224, 252

AREAS = [("GYMLOAD.RAW", 17), ("AZTECLOA.RAW", 18), ("GREEKLOA.RAW", 19), ("EGYPTLOA.RAW", 21), ("ATLANLOA.RAW", 22)]
CREDITS = [("CRED1.PCX", 23), ("CRED2.PCX", 24), ("CRED3.PCX", 25), ("END.PCX", 26)]
UB_CREDITS = [("CRED0.PCX", 27)]


def rnc_unpack(d):
    """RNC ProPack method 2 (no key) - ported from Lab 313's decompiled ProPack (free to use)."""
    if d[:3] != b"RNC" or d[3] != 2:
        raise ValueError("not RNC method 2")
    size, packed = struct.unpack_from(">II", d, 4)
    src = memoryview(d)[18:18 + packed]
    sp = 0
    bitbuf = bitcnt = 0
    out = bytearray()

    def byte():
        nonlocal sp
        b = src[sp]; sp += 1
        return b

    def bits(n):
        nonlocal bitbuf, bitcnt
        v = 0
        for _ in range(n):
            if not bitcnt:
                bitbuf = byte(); bitcnt = 8
            v = (v << 1) | ((bitbuf >> 7) & 1)
            bitbuf = (bitbuf << 1) & 0xFF
            bitcnt -= 1
        return v

    def offset():
        o = 0
        if bits(1):
            o = bits(1)
            if bits(1):
                o = ((o << 1) | bits(1)) | 4
                if not bits(1):
                    o = (o << 1) | bits(1)
            elif not o:
                o = bits(1) + 2
        return ((o << 8) | byte()) + 1

    def copy(cnt, off):
        for _ in range(cnt):
            out.append(out[-off])

    bits(2)                                   # lock / key flags
    while len(out) < size:
        if not bits(1):
            out.append(byte()); continue
        if bits(1):
            if bits(1):
                if bits(1):
                    cnt = byte() + 8
                    if cnt == 8:
                        bits(1); continue     # end of a chunk
                else:
                    cnt = 3
                copy(cnt, offset())
            else:
                copy(2, byte() + 1)
        else:
            cnt = bits(1) + 4
            if bits(1):
                cnt = ((cnt - 1) << 1) + bits(1)
            if cnt != 9:
                copy(cnt, offset())
            else:
                n = (bits(4) << 2) + 12
                for _ in range(n):
                    out.append(byte())
    return bytes(out[:size])


def ps1_picture(path):
    d = rnc_unpack(open(path, "rb").read())
    if len(d) != 384 * 256 * 2:
        raise ValueError("%d bytes, not 384 x 256 x 2" % len(d))
    px = struct.unpack("<%dH" % (384 * 256), d)
    img = Image.new("RGB", (384, 256))
    img.putdata([((p & 31) * 255 // 31, ((p >> 5) & 31) * 255 // 31, ((p >> 10) & 31) * 255 // 31) for p in px])
    return img


def bgr555(r, g, b):
    v = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
    return v if v else 0x0400            # 0x0000 is see-through on the 32X


def write_picture(img, out):
    """the same colour rules as make_ol_splash.py, full screen 320 x 224"""
    img = img.convert("RGB").resize((W, H), Image.LANCZOS)
    q = img.quantize(colors=NCOL, method=Image.MEDIANCUT)
    pal = q.getpalette()[:NCOL * 3]
    pal += [0] * (NCOL * 3 - len(pal))
    lum = [pal[i * 3] * 3 + pal[i * 3 + 1] * 6 + pal[i * 3 + 2] for i in range(NCOL)]
    d = lum.index(min(lum))
    pix = q.tobytes()
    if d:
        pal[0:3], pal[d * 3:d * 3 + 3] = pal[d * 3:d * 3 + 3], pal[0:3]
        t = bytearray(range(256)); t[0], t[d] = d, 0
        pix = pix.translate(bytes(t))
    b = bytearray()
    for i in range(NCOL):
        b += struct.pack(">H", bgr555(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]))
    b += struct.pack(">H", 0x0400)                          # 252 black
    b += struct.pack(">H", 8)                               # 253 bar background (dark red)
    b += struct.pack(">H", 31)                              # 254 bar (bright red)
    b += struct.pack(">H", 16 | (3 << 5) | (3 << 10))       # 255 bar edge
    assert len(pix) == W * H and max(pix) < NCOL
    b += pix
    open(out, "wb").write(b)


def find(folder, name):
    if not folder or folder == "-" or not os.path.isdir(folder):
        return None
    for f in os.listdir(folder):
        if f.upper() == name:
            return os.path.join(folder, f)
    return None


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    ps1, pc, ub, dst = sys.argv[1:]
    os.makedirs(dst, exist_ok=True)
    for f in os.listdir(dst):                   # pictures from an earlier build
        u = f.upper()
        if u.startswith("IMAGE") and u.endswith(".RAW") and u[5:-4].isdigit():
            os.remove(os.path.join(dst, f))
    done = 0
    for folder, table, kind in ((ps1, AREAS, "ps1"), (pc, CREDITS, "pcx"), (ub, UB_CREDITS, "pcx")):
        for name, n in table:
            src = find(folder, name)
            if not src:
                print("  %-13s not found - left out" % name)
                continue
            img = ps1_picture(src) if kind == "ps1" else Image.open(src)
            out = os.path.join(dst, "IMAGE%d.RAW" % n)
            write_picture(img, out)
            print("  %-13s -> %s" % (name, os.path.basename(out)))
            done += 1
    print("==> %d pictures in %s" % (done, dst))


if __name__ == "__main__":
    main()
