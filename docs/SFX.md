# SFX-1 / SFX-2: the missing sound effects

**Why they were missing:** a level uses 70–94 different sounds (83–110 samples, 400–630 KB at 8 kHz).
The Sega CD's PCM chip holds 64 KB, so each level's pack had room for about 20 samples, and only
15–17 of the level's sounds could play at all: mostly Lara's own. On top of that, only the loudest
effect of each frame was sent to the Sega CD.

## SFX-2: every effect through the 32X PWM (the default)
- **The samples:** `tools/sfx_bank.py` appends a bank to each level file and points the level header
  at it.
  - The header fields used are `soundData` / `soundOffsets`, which were unused before.
  - The bank is loaded into the cart together with the level, by the normal level load.
  - **No new command, no boot stack change**, and RESUME-1's cart check covers it.
  - The bank holds **all** samples of the level, 8-bit, 8000 Hz. Where the cart room after the level is
    smaller, the rate drops: Caves 6000, Tomb of Tihocan 7000, The Great Pyramid 6000. Nothing is cut
    (the longest sample, 7.7 s, plays whole).
  - The banks are 200–500 KB.
- **The mixer:** `sfx.inc` runs on the slave SH2, in the DMA 1 interrupt.
  - DMA 1 feeds the PWM from two blocks of up to 316 words (the movie player's buffers).
  - Up to 8 sounds at once, with distance volume and pitch. The engine's modes work as on the GBA
    (UNIQUE: no restart; REPLAY: restart).
  - The PWM runs at 20–24 kHz: each mixed sample is written 3–4 times.
  - The samples are read cache-through from the cart, so the renderer keeps its cache.
  - The slave runs at interrupt level 3 instead of 13. Only the DMA interrupt (priority 4) gets
    through: the slave's 32X interrupts stay off.
- **Movies:** the mixer stops while a movie plays, and starts again afterwards.
- **Volume:** set by `SFX_SHIFT` in `sfx.inc`. It is matched to the PCM effects' level in emulation,
  not yet on the console.

## SFX-1: the Sega CD PCM path (the fallback)
- Up to **3 effects per frame**, the loudest first.
  - The first goes with the pad request, the others with one more command 48 each. That command
    already exists.
- The packs (`tools/make_sfx_packs.py`):
  - one variant per sound, and the other variants play it;
  - 6 kHz, at most 0.75 s;
  - about **26** sounds per level instead of 17 (the Sub-CPU driver allows 31).
- This path is used when a level has no bank: `SFX=pcm bash build.sh`, or an older disc.

## Measured in emulation
| | Before | SFX-2 |
|---|---|---|
| Sounds that play, Lost Valley | 15 of 73 | all 73 |
| Effects per frame | 1 | all, up to 8 at once |
| Caves benchmark frame time (cart 10) | 46.7 ms | 46.7 ms (mixer about 3% of the slave) |
| Lost Valley load from the CD | 31.9 s | 41.3 s (+29%, the 443 KB bank) |
| Resume after power-off (RESUME-1) | ~3 s | ~3 s |

Checked:
- the pictures are bit-identical (Caves benchmark);
- effects on the title ring, in Caves and in Lost Valley (gunshots, footsteps, jump, draw);
- a movie with the mixer on, and the effects back after it;
- the PCM fallback with the new packs.

**Not yet on the console.** Things to listen for there:
- clicks when a level or a movie starts;
- the volume against the CD music;
- crackle in busy scenes.

## Build
`bash build.sh` (or `CART=16 bash build.sh`). The new step `==> sound banks` lists each level's
rate and size. `SFX=pcm bash build.sh` builds without banks.
