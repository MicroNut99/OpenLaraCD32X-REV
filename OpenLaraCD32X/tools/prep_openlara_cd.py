#!/usr/bin/env python3
"""
prep_openlara_cd.py  -  make an OpenLara 32X ROM ready for the CD32X disc (milestone OL-1)

    python3 prep_openlara_cd.py OpenLara.32x Demo/ROMS/D32XR.32X

What it does:
  1. checks the ROM the same way cdloader_sh2.c does (SEGA at 0x100, 32X header at 0x3C0,
     SH2 entries in SDRAM, SDRAM image small enough) - so a bad ROM is caught on the PC,
     not after a burn (loader error 4DE3);
  2. writes the marker "CDB2" at 0x3BC.  build.sh refuses ROMs without it.  0x3BC is in the
     reserved, unused part of the 68000 jump table (all zeros in OpenLara); the 68000 never
     runs OpenLara's Genesis code in the CD32X build anyway;
  3. writes the result under the name the loader opens: D32XR.32X (file 20).
"""
import struct
import sys

MARKER_OFS = 0x3BC
MARKER = b"CDB2"


def fail(msg):
    sys.exit("ERROR: " + msg)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    rom = bytearray(open(src, "rb").read())

    if len(rom) < 0x800:
        fail("%s is too small to be a 32X ROM" % src)
    if len(rom) > 0x400000:
        fail("%s is %d bytes - more than the 4 MB cart" % (src, len(rom)))
    if rom[0x100:0x104] != b"SEGA":
        fail("no 'SEGA' at 0x100 - not a Genesis/32X ROM")

    name = rom[0x3C0:0x3D0].decode("ascii", "replace").rstrip()
    srcofs, dstofs, size, ment, sent, mvbr, svbr = struct.unpack(">7I", rom[0x3D4:0x3F0])
    print("module      : %s" % name)
    print("SDRAM image : %d bytes from ROM 0x%06X to SDRAM +0x%05X" % (size, srcofs, dstofs))
    print("entries     : master 0x%08X  slave 0x%08X" % (ment, sent))
    print("VBRs        : master 0x%08X  slave 0x%08X" % (mvbr, svbr))

    # the same limits cdloader_sh2.c checks (error 4DE3 on the console)
    if size == 0 or size > 0x30000 or dstofs + size > 0x38000:
        fail("SDRAM image size/destination outside the loader's limits")
    if (ment >> 24) != 0x06 or (sent >> 24) != 0x06:
        fail("SH2 entry points are not in SDRAM (0x06xxxxxx)")
    if srcofs + size > len(rom):
        fail("SDRAM image runs past the end of the ROM")

    old = bytes(rom[MARKER_OFS:MARKER_OFS + 4])
    if old == MARKER:
        print("marker      : already present")
    elif old == b"\0\0\0\0":
        rom[MARKER_OFS:MARKER_OFS + 4] = MARKER
        print("marker      : 'CDB2' written at 0x%03X" % MARKER_OFS)
    else:
        fail("bytes at 0x3BC are not free (%s) - unexpected ROM layout" % old.hex())

    # OL-3 trim: drop the zero padding at the end (keeps the SDRAM image, rounded to 2 KB)
    end = len(rom)
    while end > 0x800 and rom[end - 1] == 0:
        end -= 1
    end = max(end, srcofs + size, 0x800)
    end = (end + 2047) & ~2047
    if end < len(rom):
        print("trimmed     : %d -> %d bytes" % (len(rom), end))
        rom = rom[:end]
    open(dst, "wb").write(rom)
    print("wrote       : %s (%d bytes, %d chunks of 32 KB)" % (dst, len(rom), (len(rom) + 32767) // 32768))


if __name__ == "__main__":
    main()
