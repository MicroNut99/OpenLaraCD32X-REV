#!/usr/bin/env python3
"""
prep_openlara_cd.py  -  make an OpenLara 32X ROM ready for the CD32X disc (milestone OL-1)

    python3 prep_openlara_cd.py OpenLara.32x Demo/ROMS/D32XR.32X
    python3 prep_openlara_cd.py --mode1 ...   (build.sh EDCD=1) also step 4 below

What it does:
  1. checks the ROM the same way cdloader_sh2.c does (SEGA at 0x100, 32X header at 0x3C0,
     SH2 entries in SDRAM, SDRAM image small enough) - so a bad ROM is caught on the PC,
     not after a burn (loader error 4DE3);
  2. writes the marker "CDB2" at 0x3BC.  build.sh refuses ROMs without it.  0x3BC is in the
     reserved, unused part of the 68000 jump table (all zeros in OpenLara); the 68000 never
     runs OpenLara's Genesis code in the CD32X build anyway;
  3. writes the result under the name the loader opens: D32XR.32X (file 20);
  4. --mode1 only (MODE1-1, the cartridge boot from a Mega EverDrive PRO with the CD32X BootCamp's
     EDCD.32X, its docs/12): points the 68000 jump table (0x206-0x319, vectors 2-47) at the RAM jump
     table a CD boot's BIOS leaves at 0xFFFD00, and makes the vectors the 32X standard.  There the
     32X sends the 68000's vectors to the cart's table - where this ROM is copied - and the listener
     has its vblank in that RAM table.  A CD boot never uses either, so the disc boots both ways.
"""
import struct
import sys

MARKER_OFS = 0x3BC
MARKER = b"CDB2"

# --mode1: vector n (2..47) -> the Sega CD BIOS's RAM jump table entry (the same in the US, EU and JP
# BIOSes); the vectors the BIOS sends to its ROM go to 0xFFFDA8.  As the BootCamp's edboot/make_edcd.py.
VECTOR_RAM = {3: 0xFFFD7E, 4: 0xFFFD7E, 5: 0xFFFD84, 6: 0xFFFD78, 7: 0xFFFD8A, 8: 0xFFFD9C,
              9: 0xFFFDA2, 10: 0xFFFD90, 11: 0xFFFD96, 26: 0xFFFD12, 28: 0xFFFD0C, 30: 0xFFFD06}
for _t in range(16):
    VECTOR_RAM[32 + _t] = 0xFFFD18 + 6 * _t


def fail(msg):
    sys.exit("ERROR: " + msg)


def main():
    args = sys.argv[1:]
    mode1 = "--mode1" in args
    args = [a for a in args if a != "--mode1"]
    if len(args) != 2:
        sys.exit(__doc__)
    src, dst = args
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

    if mode1:
        for n in range(2, 48):
            o = 0x200 + 6 * (n - 1)
            op = struct.unpack(">H", rom[o:o + 2])[0]
            if op not in (0x4EF9, 0x4EB9) and rom[o:o + 6] != bytes(6):    # jmp, jsr, or reserved
                fail("--mode1: the 68000 jump table entry at 0x%03X is not a jmp/jsr (%04X)" % (o, op))
            rom[o:o + 6] = struct.pack(">HI", 0x4EF9, VECTOR_RAM.get(n, 0xFFFDA8))
        print("mode1       : 68000 jump table -> the RAM jump table at 0xFFFD00")
        std = struct.pack(">I", 0x01000000) + struct.pack(">I", 0x3F0) * 63
        if bytes(rom[0:0x100]) != std:
            rom[0:0x100] = std
            print("mode1       : vectors set to the 32X standard (SSP 0x01000000, all 0x3F0)")

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
