# OPT-1 / OPT-2: faster span loops, RESUME-1: no reload after power-on

The textured span loops draw most of every frame (both SH2s). Two changes, measured in ares
(MicroNut99/ares_android) with a fixed benchmark; the pictures are **bit-identical** to v1.4.

## What changed
- **2-byte lightmap** (`gLightmap16`, 16 KB SDRAM): every lightmap byte stored doubled, so
  one 16-bit load gives the two screen pixels a texel covers. The flat-shaded textured loop
  (`asm/rasterizeFT.i`) drops a zero-extend, a byte swap and an OR per pixel pair, and its
  instructions are reordered so no load waits for the one before.
- **4 pixels per loop** in the textured loop and the Gouraud-textured loop (`rasterizeGT.i`):
  one compare and branch per 4 pixels instead of per 2 (an odd pair is drawn first).
- On by default in the CD32X build; `make CD32X=1 OPT=0` builds v1.4's loops.

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

**Fix in OPT-2:** OPT-1's table had rows 0-31, but the shading gives 1-32 (`transformMesh.i` /
`transformRoom.i`: clamp, then +1). Row 32 (the darkest, fully fogged surfaces far away) read
past the table: those faces came out black or wrong, seen in Palace Midas. The table now has
33 rows. Palace Midas is bit-identical to v1.4 again.

Tried and dropped: reading textures through the cache (the packer marks them cache-through,
and that is right: through the cache the benchmark was 13% slower); skipping a texel that
repeats the last one (only 12-15% repeat).

## OPT-2 (`engine/openlara-opt2.patch`, applied by `build.sh` after OPT-1)
- **Gouraud-textured loop on the 2-byte table too** (`rasterizeGT.i`): per scanline, g is turned
  into its address in `gLightmap16` (rows on 512-byte boundaries), so each pixel pair is one
  16-bit load: no zero-extend, byte swap and OR. 2 instructions fewer per pixel pair.
- **Ordering table:** `flush_ot` skips the empty slots in a tight loop.
- The lightmap fix above.

## All measured (ares, benchmark, picture bit-identical to v1.4 on all three levels)
| Level, cart waits | v1.4 | now | |
|---|---|---|---|
| Caves, 6 | 44.6 ms | 40.6 ms | -9.0% |
| Caves, 10 | 50.7 ms | 46.6 ms | -8.1% |
| Caves, 15 | 58.3 ms | 54.5 ms | -6.5% |
| Lost Valley, 10 | 50.8 ms | 46.7 ms | -8.1% |
| Palace Midas, 10 | 94.7 ms | 90.0 ms | -5.0% |

With the split line balancing itself as in the game (`SPLIT=1`, Caves): 46.1 → 41.8 ms (cart 6),
50.4 → 48.5 ms (10), 57.0 → 53.1 ms (15). The frame time moves in whole screen refreshes, so
these steps are coarse.
Stack: 7.6 KB free on the master in the benchmark (the table is now 17 KB, 512-byte aligned).
The engine file is 258048 bytes (2 KB more than v1.4).

## RESUME-1: the level stays in the cart after power-off
The level slot (cart 2 MB) is battery-backed like the save, so the last level is still there at
the next power-on. After every level load a record goes into the save area: `0x3F0310`
'OLLV', level, file length, checksum, ~checksum (marker last). The first level after power-on
(Current Position, or a level from the passport) checks it: same level, same length as on this
disc, checksum of the cart matches → **no CD load** (ares: Lost Valley 28 s → 3 s, the checksum).
Otherwise (a movie or the cart test's load test used the slot, a half-finished load) it loads
from the CD as before. A+B+C held = always from the CD. Same rules as CAVES-1's Caves record.
Not switched by `OPT`: `OPT=0` builds v1.4's loops with RESUME-1.

## Benchmark build
`make CD32X=1 BENCH=1`: starts straight in Caves, one game-logic step per drawn frame, a scripted
pad, a fixed split line and fixed random seeds; after 150 frames the last picture stays and
COMM12 = BEEF. Every build draws the same 150 frames, so pictures compare bit for bit and the
SH2 clocks per function compare between builds. Emulator use only.
`SPLIT=1`: the split line balances itself as in the game (timing only; pictures then differ).
`BENCHLEVEL=n`: another level (`LevelID`: 4 = Lost Valley, 9 = Palace Midas).
