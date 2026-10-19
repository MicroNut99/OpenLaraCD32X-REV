# 8 MB / 16 MB cart test builds (CART-1)

Test discs for the MicroNut99 16MB-PCB-CD32X (paged SRAM cart). They answer one question before
anything else is built: **does the page register work with the 32X on?** Design:
`docs/SRAM_8MB_16MB_DESIGN.md`; the stack proposal: MicroNut99/docs `CD32X_SRAM_PAGING_PROPOSAL.md`.

## Build
```bash
CART=8  bash build.sh     # -> subcpu/CDROMPlayer_cart8_music.cue
CART=16 bash build.sh     # -> subcpu/CDROMPlayer_cart16_music.cue
bash build.sh             # v1.3, unchanged (no test code in it)
```
The first `CART=` build applies `engine/openlara-cart-test.patch` to `OpenLara-src` (adds
`carttest.inc`). After that the normal build is still v1.3: every addition is behind the switch.

## What changes
| Part | Change | Normal build |
|---|---|---|
| Boot block, `sh2/` (CDLOADER), `crt0.s`, `src-md/`, **`hw_md.s`** | none | – |
| `subcpu/main.c` | CMD 68 (window 1 page), only with `CART=` (`OL_CART_MB`) | identical |
| Engine `main.cpp` + `carttest.inc` | the test screen before the game, only with `CART=` (`CD32X_CART_MB`) | identical (checked: same `main.o`) |

CMD 68 writes the page as one even byte to `0xA130F2` with the listener's existing routine 26
(`safe_cart_xfer`: interrupts off, RV = 1, byte copy, RV = 0).

## On the console
The test runs at start, before the game:
1. **Writing / Reading.** Every page gets its own 4 KB pattern in **piece 15** (cart
   `0x3FF000` of the page: the save area's last piece, free in the ledger, never used by
   OpenLara). The 8 MB build writes pages 1–3 and 6; the 16 MB build writes pages 1–7. Then the
   SH2 reads each page back.
2. The screen:
   ```
   CART TEST  BUILD 16MB
   FOUND 16MB
   PAGE 1 OK
   PAGE 2 OK
   ...
   SAVE AREA UNCHANGED
   ```
   | Line | Meaning |
   |---|---|
   | `OK` (green) | the page holds its own pattern |
   | `SAME AS n` (yellow) | the page shows page n: aliased (a 4 MB cart, or pages 4–7 on the 8 MB image) |
   | `BAD` (red) | no memory there (8 MB board with the 16 MB GreenPAK image, pages 4–7) |
   | `68K-RB` (red) | CMD 61's own 68000 read-back failed for that page |
   | `FOUND` | 4 MB = no paging (every write went to page 1); 8 MB; 16 MB. Green when it matches the build |
   | `CMD 68 FAILED - NO PAGE SWITCH` (red) | the Sub-CPU didn't do the page command (a disc built without `CART=`, or a Sub-CPU mismatch). The test stops there; START plays |
   | `SAVE AREA UNCHANGED` | settings and the saved game (pieces 0–14 of page 1) are untouched: must be green |
3. **A = load test** (optional, about a minute): TITLE.PKD is loaded into every page that showed
   `OK` (on a 4 MB cart: page 1) with the normal level load (CMD 38 + 46), and its checksum shown
   per page (`LOAD xxxxxxxx`). Then every page is read again. All the same and no `X` = loads into
   pages work and stay there. On the 8 MB board, use the 8 MB build for the load test: with the
   16 MB build its pages 1–3 show `SAME AS`, so none is loaded (pages 4–7 would be page 0, the
   game itself, and are never loaded into).
   During loading the red bar of the loading screen is drawn over the bottom line; that's normal.
4. **START** = window 1 back to page 1, and the game starts as normal.

## Expected results
| Cart | 8 MB build | 16 MB build |
|---|---|---|
| 4 MB cart (no mapper) | FOUND 4MB, pages 1–3 SAME AS 6, page 6 OK (it was written last), game plays | FOUND 4MB, pages 1–6 SAME AS 7, page 7 OK (written last), game plays |
| 8 MB, 8 MB GreenPAK image | FOUND 8MB, pages 1 and 3 OK, page 2 SAME AS 6 (page 6 mirrors page 2) | FOUND 8MB, pages 1–3 SAME AS 5–7, pages 4–7 OK (mirrors) |
| 8 MB, 16 MB image (no U2) | FOUND 8MB, pages 1–3 OK, page 6 BAD 68K-RB | FOUND 8MB, pages 4–7 BAD 68K-RB |
| 16 MB | FOUND 16MB (8 MB build: pages 1–3 OK) | FOUND 16MB, pages 1–7 OK |

**Test order:** the 4 MB cart first. It must say FOUND 4MB, SAVE AREA UNCHANGED, and the game
must play and load a save as before. Then the 8 MB board, then the 16 MB board.

On a 4 MB cart the 16 MB build shows `SAME AS 7` for pages 1–6: all writes went to page 1's
piece 15 (unused). On the 8 MB image, the 16 MB build's writes to pages 4–7 land in pages 0–3's
piece 15. Page 0's is cart `0x1FF000`, past the end of Caves (`build.sh` checks that Caves ends
before it in the `CART=` builds), so nothing is harmed.

Send a photo or video of the screen.
