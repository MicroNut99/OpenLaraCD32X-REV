#!/usr/bin/env python3
"""make_boot_image.py - SPLASH.BIN -> IMAGE.RAW for the loader's boot splash (OpenLara CD32X)
    python3 make_boot_image.py SPLASH.BIN subcpu/ROMS/IMAGE.RAW
IMAGE.RAW = 224 lines x 320 pixels (one byte each, as the 32X frame buffer holds them from byte
0x200 on: black, the 320 x 180 picture from line 14, black) + the 256-entry palette (512 bytes).
The same picture and colours as the loading screen; the loader draws the red bar itself."""
import sys
W, LINES, PIC_H, TOP, BLACK = 320, 224, 180, 14, 252
if len(sys.argv) != 3:
    sys.exit(__doc__)
d = open(sys.argv[1], "rb").read()
if len(d) != 512 + W * PIC_H:
    sys.exit("*** %s: unexpected size %d" % (sys.argv[1], len(d)))
pal, pix = d[:512], d[512:]
out = bytearray([BLACK]) * (W * LINES)
out[TOP * W:(TOP + PIC_H) * W] = pix
out += pal
open(sys.argv[2], "wb").write(out)
print("==> boot splash: %s (%d bytes)" % (sys.argv[2], len(out)))
