# OPT-1: faster span loops

The textured span loops draw most of every frame (both SH2s). Two changes, measured in ares
(MicroNut99/ares_android) with a fixed benchmark; the pictures are **bit-identical** to v1.4.

## What changed
- **2-byte lightmap** (`gLightmap16`, 16 KB SDRAM): every lightmap byte stored doubled, so
  one 16-bit load gives the two screen pixels a texel covers. The flat-shaded textured loop
  (`asm/rasterizeFT.i`) drops a zero-extend, a byte swap and an OR per pixel pair, and its
  instructions are reordered so no load waits for the one before.
- **4 pixels per loop** in the textured loop and the Gouraud-textured loop (`rasterizeGT.i`):
  one compare and branch per 4 pixels instead of per 2 (an odd pair is drawn first).
- On by default in the CD32X build; `make CD32X=1 OPT=0` builds v1.4's loops. `OPT=0` with
  `CART=16` is byte-identical to the v1.4 cart test build.

## Measured (ares, Caves benchmark: 150 frames of standing, turning, running)
Frame time with the cart at its fastest (6 waits), mid (10) and slowest (15) access time from
the 32X manual: the console is somewhere in that range.

| Cart waits | v1.4 | OPT-1 | |
|---|---|---|---|
| 6 | 44.6 ms | 41.2 ms | -7.7% |
| 10 | 50.7 ms | 47.1 ms | -7.2% |
| 15 | 58.3 ms | 54.7 ms | -6.1% |

Textured span loop: 139 → 117 M SH2 clocks for the benchmark (-16%); all drawing -9%.
Stack: the master's free stack in the benchmark is 9.9 KB (was 26 KB; the table takes 16 KB).

Tried and dropped: reading textures through the cache (the packer marks them cache-through,
and that is right: through the cache the benchmark was 13% slower); skipping a texel that
repeats the last one (only 12-15% repeat).

## Benchmark build
`make CD32X=1 BENCH=1`: starts straight in Caves, one game-logic step per drawn frame, a scripted
pad, a fixed split line and fixed random seeds; after 150 frames the last picture stays and
COMM12 = BEEF. Every build draws the same 150 frames, so pictures compare bit for bit and the
SH2 clocks per function compare between builds. Emulator use only.
