# OpenLara CD32X on the 8 MB and 16 MB SRAM carts – design (v2.0)

For the MicroNut99 16MB-PCB-CD32X board (rev 0.5): one or two CY62187EV30, battery-backed,
two 2 MB windows with a page register each. Reference: OpenJazz CD32X (JazzBuildNotes_10062026_05):
a resident block checked at start, an identity + checksum to skip loading, A+B+C to force a
reload.

**Status: design, rev B** (after Fable's review of rev A). The board isn't built yet, so nothing
here has run on hardware.

## 1. The idea
The whole game is 26 MB (levels 20.7, cut-scenes 1.9, movies 3.5), so it doesn't fit even in
16 MB. The extra memory becomes a **level store** instead:
- Every level is loaded from the CD **once** and stays in the cart, across power-off, until its
  room is needed.
- 16 MB holds 7 levels plus Caves; 8 MB holds 3 plus Caves.
- Dying, restarting, loading a save and going back to a recent level never load.
- Three of the four cut-scenes ride along with their level.

**One disc for all three carts.** The game detects 4, 8 or 16 MB at start. On a 4 MB cart it
behaves exactly like v1.3.

## 2. The boot stack is not touched
- Unchanged byte for byte: the boot block, CDLOADER.BIN (`sh2/`), the game's `crt0.s` and
  `src-md/`, and the **68000 listener `hw_md.s`**.
- **The only change: new branches in the Sub-CPU's command handler, `subcpu/main.c`**, built only
  from the listener commands that already exist. APP.BIN changes; the boot path inside it doesn't:
  - CMD 38's boot branch (file 20 = OPENLARA.32X) is unchanged.
  - CMD 46 is unchanged.
  - Nothing new is sent during boot.
- The listener commands used:
  - 33 `chilly_rv_memcpy`: RV = 1, copy, RV = 0. Already used by CMD 46 and CMD 61.
  - 19 `read_32x_16` and 20 `write_32x_16`, through the 0x900000 bank window, as CMD 46/61's
    read-back does.
  - 32 `chilly_memcpy`.
- Saves keep using the existing CMD 58 + 61, as in OpenJazz and v1.3.

## 3. What the hardware gives us (PCB README §2, §10, §12)
- **Pages:** 16 MB = 8 pages of 2 MB. The SH2s and the 68000 still see 4 MB: window 0 = cart
  0–2 MB, window 1 = cart 2–4 MB (SH2 `0x02200000`, 68000 `0x600000`).
- **Page registers:** the 68000 writes the even byte at `0xA130F0` (window 0) / `0xA130F2`
  (window 1); the page is the byte value, 0–7 (D8–D10). Write-only. The 32X manual: set RV = 1
  first.
- **Reset state:** window 0 = page 0, window 1 = page 1, which is exactly the 4 MB cart. A console
  reset reboots from the CD with that state.
- **8 MB board:** with the 8 MB GreenPAK image, pages 4–7 mirror 0–3. With the 16 MB image and no
  U2, pages 4–7 float (undefined reads).
- **After a page change** both SH2s clear their caches.

## 4. Cart layout

| Page | Window | 4 MB (v1.3, unchanged) | 8 MB | 16 MB |
|---|---|---|---|---|
| 0 | 0, always | game 0–0.5 MB, Caves 0.5–2 MB | same | same |
| 1 | 1 (default) | level slot, save area last 64 KB | **slot 1** + save area | **slot 1** + save area |
| 2–3 | 1 | – | **slots 2–3** | **slots 2–3** |
| 4–7 | 1 | – | – | **slots 4–7** |

- **Window 0 never changes:** the program runs from it.
- **Only window 1 switches.** It points at the slot being played. A level always starts at
  `0x02200000`, as in v1.3.
- **Every slot is 1.9375 MB (0x1F0000).** The last 64 KB of each page is reserved:
  - on page 1 it is the save area, as now;
  - on pages 2–7 it holds the slot's tag and the size marker.
- **Pages 2–7 are never touched by the 4 MB-era games.** Their stored levels and tags survive
  playing Jazz, Hurrican or SuperTux. Page 1's slot doesn't, and its tag says so.
- **Saves don't move** (settings `0x3F0000`, Caves record `0x3F0300`, saved game `0x3F0400`, all on
  page 1), so a v1.3 save loads in v2.0.

## 5. Bundles: what goes into a slot

| Bundle | Contents | Size (cut-scene on a 32 KB boundary) |
|---|---|---|
| Each level (Gym, 2 … 10C, except the three below) | the level | 0.86–1.46 MB |
| Tomb of Qualopec | LEVEL3B + CUT1 | 1.68 MB |
| Tomb of Tihocan | LEVEL7B + CUT2 | 1.70 MB |
| Natla's Mines | LEVEL10A + CUT3 | 1.83 MB |
| Cut-scene Atlantis | CUT4 alone (10B + CUT4 = 2.18 MB, too big) | 0.74 MB |
| Each movie | CORE, MANSION, PYRAMID, ESCAPE | 0.40–1.32 MB |

- The build makes this table and stops if a bundle passes 0x1F0000.
- **Trade-off:** a bundled cut-scene makes the *first* load of its level longer by its size (CUT1:
  0.5 MB). After that it costs nothing.

## 6. Every slot describes itself (no central directory)
A **tag** in the page's own reserved tail, written like the Caves record (cleared first, filled,
marker last):

```
tail + 0x0200 ('OLSL' tag, 32 bytes):
    magic | disc id | bundle id | length | checksum | ~checksum | last-used counter
tail + 0x02F0: size-test marker (section 8)
tail = window 1 + 0x1F0000 = SH2 0x223F0000, i.e. page 1: 0x3F0200 / 0x3F02F0
```

- **Writing a tag uses the existing CMD 61** (piece 0): it writes window 1 + 0x3F0000, so with
  window 1 on page *p* it lands in page *p*'s tail. No new command is needed.
- **On page 1** the tag sits in OpenLara's own part of piece 0. Settings end at 0x3F000A, and the
  Caves record is at 0x3F0300.
- **Disc id:** a checksum over the build's file table (names and lengths), built into
  OPENLARA.32X. A tag with a different disc id is an empty slot (Jazz's "identity mark").
- **Checksum timing:**
  - **computed right after the load**, before the tag is written (loads skip the read-back,
    CMD 46 bit 15);
  - **checked once per power-on**, when the slot is first used.
- **A torn write** (switched off mid-load) costs that one slot, never the others.
- **A+B+C at start:** every tag is ignored and Caves is reloaded, as now.

## 7. Choosing a slot
1. **The bundle is in a slot and its tag checks out:** switch window 1 to it. No loading.
2. **Otherwise pick a slot to load into:** an empty slot. Failing that, the least recently used
   one, but never:
   - the slot being played;
   - the saved game's level's slot, so *Load Game* stays instant (on 8 MB only while another slot
     can go).
3. **Load it** with the red bar, check the checksum, write the tag, and keep window 1 on it.

On a 4 MB cart there is one slot (page 1), which is v1.3's `gCartLevel` logic.

## 8. Sub-CPU: new branches in `main.c`

| CMD | Name | In | Reply | Built from |
|---|---|---|---|---|
| 68 | cart page | COMM2 = window-1 page | COMM12 = page, 0 = refused | page byte → Word RAM, `switch_banks`, listener 33 copies 1 byte to `0xA130F2` with RV = 1 |
| 69 | cart size | – | COMM12 = 4, 8 or 16 | the test below |
| 38 (level / movie branches only) | open file | COMM8L = offset in window 1 for this file (0 = slot start) | as now | the slot rule `cur_img`, plus the offset |

- **CMD 68 refuses** page 0 (the program and Caves) and pages > 3 once 8 MB has been detected.
- **The offset rides on CMD 38** (Fable M4). A CMD 68 offset would be cleared by the CMD 38 that
  precedes every load. CMD 38's branch for file 20, the boot load, is unchanged.
- **The level branch checks the end:** a file whose offset + length passes 0x1F0000 is refused.
  Today nothing stops a long file from running into the save area.
- **Size test (CMD 69).** Every write uses listener 33. Every read uses listener 20 (bank select
  0xA15104) and 19 through 0x900000, as CMD 46's read-back does. M = tail + 0x02F0, OpenLara's own
  bytes on every page.
  1. Page 1: write A at M.
  2. Page 2: write B at M, then ~B at M + 2 (so a floating bus can't echo B).
  3. Page 1: read M. B → **4 MB** (no mapper: the page write did nothing).
  4. Page 6: write C at M, ~C at M + 2, read M. Not C → **8 MB** (16 MB image, no U2).
  5. Page 2: read M. C → **8 MB** (8 MB image, page 6 = page 2).
  6. Otherwise **16 MB**.
  7. Window 1 back to page 1.
- **The ledger:** register CMD 68/69 and OpenLara's use of piece 0 (settings, tag, marker, Caves
  record, the save up to 8 KB) once they've run on the console.

## 9. Engine (`engine/openlara-cd32x.patch`)
- **`cartInit()` at start:** CMD 69 → `gCartMB`. On 8 and 16 MB, read each slot's tag once
  (page by page).
- **`osLoadLevel()`:** Caves as now; everything else goes through section 7. Cut-scenes and movies
  check whether their level's bundle holds them first.
- **`cartPage(p)`:** `renderFinish()` (overlap build), `MARS_WAIT()`, CMD 68, `CacheClear` on both
  SH2s (the slave through `MARS_CMD_CACHE`). Skipped when `p` is already selected.
- **The save area is on page 1, and every access is wrapped** (Fable M2): `saveAreaOpen()` =
  `cartPage(1)`; `saveAreaClose()` = `cartPage(level page)`.
  - **Callers:** `osSaveSettings` (options ring, mid-game), `osCheckSave`, `osSaveGame`,
    `osLoadGame`, `osLoadSettings`, and the Caves record.
  - **Reads too:** `cartRead` and `cartRead32` read `0x223F0000+` directly, and `cartWrite` reads
    the piece before changing it.
  - **`osCheckSave`'s answer is cached** (at boot and after each save), so the passport doesn't
    switch pages each time it rebuilds its list.
  - **Naming:** the existing `csEnd()` (flush the piece) keeps its name; the new pair gets new
    names.
- **Version on the loading picture:** `v2.0 16MB` (8MB / 4MB), so a video shows what was
  detected.

## 10. Steps (each tested on the console before the next)
1. **Cart bring-up disc:** CMD 68/69 only, and a test screen:
   - the detected size;
   - each page's tail written and read back (through CMD 61 and the SH2's window 1);
   - on 8 MB, the alias.

   This proves the page register with the 32X on (RV = 1), the PCB's open point. Nothing else is
   worth building before it passes.
2. **Slots:** tags, LRU, page switching, wrapped save area. Test:
   - play 4 levels;
   - switch off, then on;
   - *Load Game*;
   - go back to each level: no loading;
   - play Jazz, come back: pages 2–7 still there.
3. **Bundles:** cut-scenes with their levels.

## 11. Looked at and not used
- **Loading ahead during play (Jazz's idea):** every 32 KB copy holds both SH2s (RV = 1 while the
  program runs from the cart), and the CD is busy with music or cut-scene voices. It would show as
  stutters.
- **Movies across several pages (rev A):** the master reads the picture and the slave the sound
  from different places in the file. Stepping window 1 would move both. Dropped.
- **Paging window 0, or moving the saves to page 0:** the program runs from page 0, and other games
  load over it.
- **A central directory on page 1 (rev A):** one Jazz session wipes page 1's save area. Per-page
  tags survive.

## 12. Open points
- **The page register with the 32X on:** step 1 answers it.
- **Load speed** (Fable N5, the Jazz notes): most of each load is the per-32 KB-chunk overhead.
  Bigger chunks would need another Sub-CPU command: a separate decision, not part of v2.0.
- **How long a level loads now:** measure it on the console, to state what the store saves.
