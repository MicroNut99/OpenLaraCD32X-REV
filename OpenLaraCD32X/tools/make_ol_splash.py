#!/usr/bin/env python3
"""make_ol_splash.py - picture -> SPLASH.BIN for OpenLara CD32X's loading screen
    python3 make_ol_splash.py splash.jpg OpenLara-src/src/platform/32x/data/SPLASH.BIN
SPLASH.BIN = 256 palette entries (32X BGR555, big-endian, 512 bytes) + 320 x 180 pixels (bytes).
Indices 0-251: the picture.  252 black, 253 bar background, 254 bar (red), 255 bar edge.
The picture is scaled to 320 wide and cropped/padded to 180 high (16:9).
If a file VERSION sits next to the picture, its text (e.g. v1.1) is drawn in the bottom-right corner."""
import struct, sys
try:
    from PIL import Image
except ImportError:
    sys.exit("*** Python's PIL is missing:  sudo apt install python3-pil   (or: pip install pillow)")

W, H, NCOL = 320, 180, 252

def bgr555(r, g, b):
    v = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
    return v if v else 0x0400            # 0x0000 is see-through on the 32X (KOBO rule 5)

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    img = Image.open(sys.argv[1]).convert("RGB")
    # scale to 320 wide, then crop/pad the height to 180 (centred)
    h = round(img.height * W / img.width)
    img = img.resize((W, h), Image.LANCZOS)
    canvas = Image.new("RGB", (W, H), (0, 0, 0))
    canvas.paste(img, (0, (H - h) // 2))
    # v1.1: the version (file VERSION next to the picture) small in the bottom-right corner,
    # drawn before the colours are picked so it gets its own clean colour
    import os
    vfile = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), "VERSION")
    if os.path.exists(vfile):
        from PIL import ImageDraw, ImageFont
        ver = open(vfile).read().strip()
        draw = ImageDraw.Draw(canvas)
        font = ImageFont.load_default()
        box = draw.textbbox((0, 0), ver, font=font)
        x, y = W - (box[2] - box[0]) - 4, H - (box[3] - box[1]) - 5
        draw.text((x + 1, y + 1), ver, fill=(0, 0, 0), font=font)          # shadow
        draw.text((x, y), ver, fill=(210, 210, 210), font=font)
        print("==> splash: version %s" % ver)
    q = canvas.quantize(colors=NCOL, method=Image.MEDIANCUT)
    pal = q.getpalette()[:NCOL * 3]
    pal += [0] * (NCOL * 3 - len(pal))
    # OL-7: the darkest colour becomes colour 0 (a cleared screen shows colour 0 - no flash)
    lum = [pal[i * 3] * 3 + pal[i * 3 + 1] * 6 + pal[i * 3 + 2] for i in range(NCOL)]
    d = lum.index(min(lum))
    pix_swap = None
    if d:
        pal[0:3], pal[d * 3:d * 3 + 3] = pal[d * 3:d * 3 + 3], pal[0:3]
        t = bytearray(range(256)); t[0], t[d] = d, 0
        pix_swap = bytes(t)
    out = bytearray()
    for i in range(NCOL):
        out += struct.pack(">H", bgr555(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]))
    out += struct.pack(">H", 0x0400)                        # 252 black
    out += struct.pack(">H", 8)                             # 253 bar background (dark red)
    out += struct.pack(">H", 31)                            # 254 bar (bright red)
    out += struct.pack(">H", 16 | (3 << 5) | (3 << 10))     # 255 bar edge
    pix = q.tobytes()
    if pix_swap:
        pix = pix.translate(pix_swap)
    assert len(pix) == W * H and max(pix) < NCOL
    out += pix
    open(sys.argv[2], "wb").write(out)
    print("==> splash: %s -> %s (%d bytes, %d colours)" % (sys.argv[1], sys.argv[2], len(out), NCOL))

if __name__ == "__main__":
    main()
