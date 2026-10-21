# Tomb Raider for Sega CD + 32X (OpenLara CD32X)

Tomb Raider (1996) on the **Sega CD32X**, built on XProger's **OpenLara** 32X engine. **Version 1.4.**
The whole game runs from one CD: each level is loaded from the disc into a 4 MB RAM cart, with
CD music, PCM sound effects, saving, the cut-scenes with their voices and the game's movies.

> **No game data is included.** You need your own copy of Tomb Raider 1 (PC) – the build converts
> it. The music and the loading picture are also yours to supply. See *What you need*.

## Hardware
- Sega Genesis / Mega Drive
- Sega CD / Mega-CD
- Sega 32X
- **A 4MB RAM cart** (battery-backed), in the 32X cartridge slot – required
- a **6-button pad** (recommended; with a 3-button pad Start still opens the inventory, but walk,
  roll and look are missing)

Emulators: Kega Fusion cannot emulate the 4MB RAM cart, so the game does not run there.

## What you need
| What | Where it goes |
|---|---|
| Tomb Raider 1 **PC** data (Steam/GOG: the `DATA` folder with `LEVEL1.PHD` …) | `OpenLaraCD32X/DATA/` → converted by the packer |
| Music: WAV files (44.1 kHz, 16-bit, stereo) | `OpenLaraCD32X/soundtrack_wav/`, assigned in `music_map.txt` |
| A loading picture (any size, 16:9 works best) | `OpenLaraCD32X/splash.jpg` |
| Cut-scene sound: your Tomb Raider 1 **game CD** image (`.nrg`) – the voices are CD audio tracks on it | `tools/nrg_cut_audio.py` → `soundtrack_wav/cut22…cut25` |
| Movies: the game's `FMV` folder (`*.RPL`) and **ffmpeg** | `tools/make_fmv.sh` → `OpenLaraCD32X/fmv/*.ROQ` |
| Chilly Willy's Sega toolchain (`/opt/toolchains/sega`), Python 3 with Pillow, MinGW (for the packer) | your build machine (Linux / WSL) |

## Building
Full instructions: `docs/OPENLARA_CD32X_HANDOFF.md`. In short:
1. Get the engine: clone upstream OpenLara next to `OpenLaraCD32X/` and apply `engine/openlara-cd32x.patch`.
2. Build the packer (`OpenLaraCD32X/gba/packer`) and convert your levels: `packer.exe 32x out32x`.
3. Cut-scene sound and movies (optional – without them the cut-scenes are silent and the
   passport has no movies to play):
   `python3 tools/nrg_cut_audio.py "<Tomb Raider 1.nrg>" soundtrack_wav`
   `bash tools/make_fmv.sh "<Tomb Raider 1>/FMV" fmv`
4. `cd OpenLaraCD32X && bash build.sh`
5. **Burn `OpenLaraCD32X/subcpu/CDROMPlayer_music.cue`** (with its `.bin`). The `.iso` alone has no music.

## Starting the game
Put the RAM cart in the 32X, the disc in the Sega CD, and switch on. The picture appears while
OpenLara is copied into the cart; then each level shows the picture with a **red loading bar**
along the bottom while it loads from the disc.

## Fast start
Caves (Level 1) is also the title menu, so it is needed every time the game starts. It now has
its own place in the RAM cart and stays there – the cart keeps its contents without power.

- **First start:** Caves loads from the CD as usual (red bar). Afterwards a record in the cart
  says it is there: its length and a checksum, written only after a complete copy.
- **Every start after that:** the game checks the record against what is really in the cart
  (same length as Caves on this disc, checksum matches). If it is all there, nothing is loaded
  and the menu comes up almost at once.
- **Back to the menu, or playing Caves:** never loads Caves again.
- **Something wrong?** Hold **A + B + C** while the game starts: Caves is always reloaded
  from the CD. (Also use this after rebuilding the disc with a changed Caves of exactly the
  same size – the only change the check can't see.)

Cart layout: game 0–0.5 MB, Caves 0.5–2 MB, the level being played 2–3.94 MB, settings and
saves in the last 64 KB. `build.sh` refuses to build if the game or Caves would overlap.

## 8 MB / 16 MB cart test builds
For the 16MB-PCB-CD32X paged SRAM cart: `CART=8 bash build.sh` or `CART=16 bash build.sh` makes
a test disc (`CDROMPlayer_cart8_music.cue` / `_cart16_`) with a cart page test in the title's
options (Detail → Cart Test). `bash build.sh` without `CART` is unchanged. See `docs/CART_TEST.md` and
`docs/SRAM_8MB_16MB_DESIGN.md`.

## Speed-ups and resume (OPT-1, OPT-2, RESUME-1)
Faster texture span loops (5-9% less frame time in ares, pictures bit-identical to v1.4), and
the last level is not loaded from the CD again after a power cycle when it is still in the
cart (Current Position starts in seconds). Both are in every build. See `docs/OPT.md`.

## Controls (6-button pad)
| Button | Action |
|---|---|
| D-pad | move / turn |
| **A** | action – grab, climb, pull switches, pick up |
| **B** | jump |
| **C** | draw / holster weapons |
| **X** | walk (hold) |
| **Y** | roll |
| **Z** | look around (hold) |
| **Start** or **Mode** | inventory (open / close) |

**In the inventory:** left/right turns the ring, up/down changes ring, A selects, B or Start goes back.

**Passport** (in the inventory): **Load Game** (all levels, the cut-scenes, the movies, or your
saved position), **Save Game**, **Exit to Title** – flip pages with left/right.

## Cut-scenes and movies
**Cut-scenes.** The four in-engine cut-scenes play after their levels as in the original, and can
be picked any time from the passport's Load Game list: *Cutscene Qualopec*, *Cutscene Tihocan*,
*Cutscene Natla's Mines*, *Cutscene Atlantis*. Picked from the passport, a cut-scene goes back
to the main menu when it ends. Their sound – the voices and music – is the CD audio from the
original game CD (game tracks 22–25), put on the disc as CD tracks 22–25 by
`tools/nrg_cut_audio.py`. The sound may start a moment after the picture: the drive first has
to seek to the track.

**Movies.** On the title menu the passport's Load Game list also has *Movie Core*, *Movie
Mansion*, *Movie Pyramid* and *Movie Escape* – the game's own FMVs. A movie loads into the RAM
cart's level slot (red loading bar), then plays at 288 × 224 with its sound. **A, B, C or Start**
skips it; afterwards you are back in the passport with the title music.

How it works:
- `tools/make_fmv.sh` converts the PC game's `.RPL` movies with ffmpeg to **RoQ**
  (288 × 112, 15 frames/s, DPCM sound 22050 Hz mono). The player shows every line twice.
- The **main SH2** decodes the pictures straight from the cart with the RoQ decoder from
  Victor Luchits' *Doom 32X: Resurrection* (MIT; decoder by Dr. Tim Ferguson) into the 32X's
  32768-colour mode. The previous picture is kept in fast RAM – borrowed from the 3D
  renderer's face and vertex lists, which aren't used while a movie plays.
- The **second SH2** plays the sound: it decodes the sound chunks from the cart and feeds the
  32X's PWM sound chip by DMA. The PWM is ramped up while the movie loads, so there is no click.
  Volume: `FMV_VOLUME` in `fmv.inc` (16 = full; 7 sounds right on hardware – the converted
  sound is very loud).
- If a movie can't play, the screen flashes for a second: red = no memory, yellow = not on the
  disc or too big, blue = loading failed, magenta = not a RoQ file.
- Only movies up to 1984 KB fit the level slot, so `build.sh` puts just those four on the disc.
  The other seven (CAFE, SNOW, LIFT, VISION, CANYON, PRISON, END) are 6–20 MB; they would need
  streaming from the CD while they play.

## Saving
Saves and settings are stored in the RAM cart (battery-backed), one saved position. Save from the
passport; load it again from the passport's Load Game page ("Current position").
Saving is not available in Lara's Home.

## What is in this version
- All 15 levels from Caves to the Great Pyramid, plus Lara's Home, chosen from the passport or
  played straight through.
- CD music: one piece per level (your choice, in `music_map.txt`) and the secret chime.
- Sound effects on the Sega CD's PCM chip, a set for each level, quieter with distance.
- Every enemy: wolves, bears, bats, raptors, T-Rex, lions, pumas, gorillas, rats, crocodiles,
  mutants, centaurs, mummies, Larson, Pierre, the skater, the cowboy, Mr. T, Natla, the Torso
  and Lara's double.
- The four cut-scenes with their voices, also playable from the passport.
- Four of the game's movies, playable from the passport on the title menu.
- Saving and loading, options (sound, music, controls) remembered.

## Known limitations
- **Gorillas** do not climb ledges; **water rats** do not dive.
- Enemy darts, bolts and bullets hit instantly (no visible projectile); an enemy may occasionally
  shoot through a thin wall.
- Sound effects: one per frame, the 31 most important per level. **Animals and creatures make no
  sound yet** – rats, Lara's double and others (next on the list).
- The original game's short story cues are not played (not on the album).
- Cut-scene sound can start a moment late (the CD drive seeks to the track).
- Movies: only 4 of the 11 (the ones that fit the RAM cart), from the title menu only, not yet
  at their places in the story. *Unfinished Business* is not included.
- About 10–16 frames per second (10–13 in fights) – see **Frame rate** below.

## Frame rate
**Status (v1.4):** **10–16 frames per second while exploring, 10–13 in fights with raptors** –
measured on real hardware with the profiler build (Caves, Lost Valley, raptors, the T-rex). When
this work started it was 7–9 fps everywhere; v1.1–1.3 ran 9–15 exploring, 9–10 in fights.

| Part of a frame | Before the split | With the split |
|---|---|---|
| Reading the pad (Sega CD) | 0 ms | 0 ms |
| Waiting for the screen | 0 ms | 0 ms |
| Game logic (main SH2) | 10–36 ms | 7–26 ms |
| Preparing the scene (main SH2) | 19–27 ms | 9–34 ms |
| Drawing (both SH2s) | 58–111 ms | 43–103 ms |
| … main SH2 waiting for the second one | 16–30 ms | 0–5 ms |
| **Whole frame** | **110–145 ms** | **69–111 ms exploring, 120–166 ms in fights** |

With the overlap (below) on top of the split, fights with raptors take 100–114 ms per frame
(9–10 fps, was 116–166 ms) and exploring 64–110 ms.

### What was done
- **Screen split (the big one).** OpenLara's two SH2s used to draw every other polygon and stop
  after every depth layer to wait for each other, so the picture stayed in the right order.
  Now the main SH2 draws the top of the screen and the second SH2 the bottom. Each one goes
  through the whole scene far-to-near and draws only its own rows, so neither ever waits for
  the other while drawing. Polygons crossing the line between the halves are cut in two.
- **The line between the halves stays still.** The first version moved it a few rows every
  frame to balance the work – and a still picture then flashed: a cut polygon places its
  texture pixels very slightly differently, so pixels near the line changed back and forth.
  Now the line only moves when one SH2 has been clearly behind for 8 frames in a row.
- **Game logic overlaps the drawing.** The main SH2 draws its rows, then goes straight on to
  the next frame's pad reading and game logic while the second SH2 is still drawing. The
  second SH2 puts the finished frame on screen itself – but only once both halves are done,
  so the picture never tears. The inventory stays at a solid 30 fps.
- **No waiting for the screen refresh before preparing the next frame.** A finished frame
  only appears at the next refresh (every 16.7 ms). The main SH2 used to wait for that before
  starting the next frame (3–14 ms per frame once the logic overlapped). Preparing the scene
  doesn't need the screen, so now only clearing and drawing wait for it.
- **Why the line is usually not in the middle:** the 32X SH2s are *master and slave* on one
  shared bus. The master owns the bus; the slave has to ask for it for every memory access,
  so while both draw, the slave stalls and gets less done. The balancing gives the slave fewer
  rows when it falls behind.
- **The game runs from the RAM cart, exactly like a cartridge.** The CD drive is only used to
  load a level and to play music – never while drawing a frame.
- **Nothing else competes during play.** Once the game loop runs, the Sega CD side goes quiet;
  the 68000 "heartbeat" that stalled the renderer was removed; the diagnostic text is off.
- **No reload on a restart.** Dying, choosing Restart or loading a save from the same level
  reuses the level already in the cart (less waiting, not more frames).
- **Profiler build:** `PROF=1 bash build.sh` shows the numbers above on screen (F fps,
  T frame, I input, L logic, W screen wait, X prepare, D draw, S wait for the second SH2;
  times in ms). It uses the master's watchdog timer – the SH2 free-running timer is taken by
  OpenLara's start-up code and can't be used for timing.

### Files changed for the frame rate
All engine changes are in `engine/openlara-cd32x.patch` (apply it to OpenLara `3b268ca`) and
only take effect in the CD build (`make CD32X=1`); the cartridge build is unchanged.

| File | Change | Switch |
|---|---|---|
| `OpenLara-src/src/platform/32x/render.cpp` | **Screen split:** `flush_ot()` – each SH2 walks the whole scene and draws only its own rows; `drawPolyY()` – cuts polygons at the line between the halves; `flush_c()` – passes the line to the second SH2 and moves it only when one SH2 is clearly behind. **Overlap:** `flush_c()` returns after the main SH2's rows; `renderFinish()` waits for the second SH2, balances the line and clears the draw list; the second SH2 puts the finished frame on screen when both halves are done. Explained in the comment blocks at the top of the file. | `CD32X_SPLIT`, `CD32X_OVERLAP` (off: `-DCD32X_NO_SPLIT`, `-DCD32X_NO_OVERLAP`) |
| `OpenLara-src/src/platform/32x/rasterizer.h` | `rasterizeSprite_c()` – sprites clip to the drawing SH2's own rows. | `CD32X_SPLIT` |
| `OpenLara-src/src/platform/32x/main.cpp` | Game loop for the overlap (pad + game logic while the second SH2 draws, no wait for the screen refresh before preparing); the second SH2's clear job waits for the screen swap; the profiler (timer, on-screen numbers). | `CD32X_OVERLAP`, `CD32X_PROF` |
| `OpenLara-src/src/platform/32x/Makefile` | `make CD32X=1 PROF=1` builds the profiler. | `PROF=1` |
| `OpenLaraCD32X/build.sh` | `PROF=1 bash build.sh` makes the profiler disc. | `PROF=1` |
| `OpenLara-src/src/platform/32x/32x.h` | v1.4: the REV switches, `marsIdle()`, `MARS_WAIT()` with a pause, `SDRAM_CODE`, the SH2 divider registers. | `CD32X_IDLE`, `CD32X_FILL`, `CD32X_SDCODE`, `CD32X_HWDIV` (off: `-DCD32X_NO_…`) |
| `OpenLara-src/src/platform/32x/main.cpp` | v1.4: `pageWait()` and the second SH2's idle loop pause; `pageClear()` by VDP auto fill. | `CD32X_IDLE`, `CD32X_FILL` |
| `OpenLara-src/src/platform/32x/render.cpp`, `rasterizer.h` | v1.4: `flush_ot()`, `drawPolyY()`, `rasterizeSprite_c/LineH_c/LineV_c/FillS_c` in SDRAM; `drawPolyY()` with the SH2 divider. | `CD32X_SDCODE`, `CD32X_HWDIV` |
| `OpenLara-src/src/platform/32x/Makefile` | v1.4: `EXTRA="…"` passes switches. | |

### v1.4 – four more speed-ups
Measured on real hardware with the profiler build (v1.4r1, 15 Oct 2026, four videos):

| Where | fps | Whole frame | Logic L | Prepare X | Draw D | Wait S |
|---|---|---|---|---|---|---|
| Caves | 10–15, mostly 12–15 | 64–95 ms | 7–12 | 13–14 | 46–50 | 0–4 |
| Lost Valley, exploring | 11–16 | 55–98 ms, mostly 66–86 | 6–15 | 8–29 | 30–48 | 0–6 |
| Raptor fight | 10–13 | 74–95 ms (v1.3: 100–114) | 10–17 | 20–29 | 37–53 | 0–6 |
| T-rex | 9–12 | 83–111 ms | 6–16 | 18–28 | 37–58 | 0–14 |

Not the exact same spots and moments as the v1.3 numbers, so a rough comparison: fights gain
about 1–2 fps, exploring a little.  The screen clear by the VDP clears fully (no left-over pixels).
Each one has its own switch, so a change can be left out without touching the others:
`make CD32X=1 EXTRA="-DCD32X_NO_FILL"` (several: `EXTRA="-DCD32X_NO_FILL -DCD32X_NO_IDLE"`).
All four are explained in `OpenLara-src/src/platform/32x/32x.h` (REV-1..4).

- **REV-1 Pauses in the wait loops** (`CD32X_IDLE`). The two SH2s share one bus. A loop that
  reads a register as fast as it can (master waiting for the second SH2, the second SH2 waiting
  for its next job, both waiting for the screen swap) takes the bus again and again – and the
  other SH2, which is drawing or running the game logic, waits for it each time. Now every
  such loop pauses about 7 µs between reads, with a pause that runs from the cache and doesn't
  touch the bus (the old pauses counted in memory, which also used the bus).
- **REV-2 Screen clear by the 32X VDP** (`CD32X_FILL`). The second SH2 cleared the picture with
  17,920 writes while the main SH2 prepared the scene – on the same bus. The 32X's auto fill
  clears it inside the VDP, 256 words at a time (as in Sega's own `FrameClear` example in the
  32X hardware manual); the SH2 only starts each fill and waits.
- **REV-3 Drawing code in fast RAM** (`CD32X_SDCODE`). The asm rasterizers were already in the
  32X's SDRAM; the C code that runs for every polygon (`flush_ot`, `drawPolyY`, the sprite,
  line and fill rasterizers, about 4.5 KB) ran from the cart. A cache miss is cheaper in SDRAM.
  The SDRAM image grows by about 4.5 KB (limit 192 KB, now about 21 KB); the main SH2's stack
  room shrinks from about 31 KB to about 26 KB.
- **REV-4 The SH2's own divider** (`CD32X_HWDIV`) for polygons cut at the line between the
  halves (`drawPolyY`): one divisor per cut edge instead of four or five calls to the C
  library's division routine (which is in the cart). Same results as C division (D32XR uses
  the divider the same way); if it weren't, it would show as pixels shifting along the line.

Checked on the console: the screen clears fully (REV-2); no seam along the line between the halves (REV-4).

### Checked and ruled out
- **32-bit cart reads.** The cart slot is 16 bits wide and the 32X sets the wait time for every
  cart access, so a cart that can answer 32-bit reads can't make the console read it faster.
  The game already reads 32 bits at a time where it can.
- **The controller read.** The profiler shows it costs nothing.

### What's left
Preparing the scene (9–34 ms) still runs on the main SH2 while the second SH2 waits. Letting
the second SH2 draw one frame while the main SH2 prepares the next would need the scene lists
twice (about 72 KB); only about 30 KB of the 32X's fast RAM is free, so it doesn't fit as is.

## Versions
The version is in the file `VERSION` and is shown small in the corner of the loading picture.

| Version | What changed |
|---|---|
| 1.0 | The whole game from CD: all levels, CD music, PCM sound effects, saving in the RAM cart, all enemies, full passport, loading screen. |
| 1.1 | Faster: the two SH2s draw separate halves of the screen, game logic runs while the second SH2 is still drawing, and the next frame is prepared without waiting for the screen refresh (fights 7–8 → 9–10 fps, exploring 9–15 fps). Profiler build (`PROF=1`). Version shown on the loading picture. |
| 1.2 | Fast start: Caves (the title menu) stays in the RAM cart and is not loaded again – hold A+B+C at start to reload it. |
| 1.4 | Faster (fights 9–10 → 10–13 fps, exploring 10–16 fps): pauses in the wait loops so the two SH2s don't block each other on the bus, screen clear by the 32X VDP, the per-polygon drawing code in SDRAM, the SH2's divider for cut polygons. |
| 1.3 | Cut-scenes in the passport, with their voices from the game CD (and fixed: shorter cut-scenes ended at once, a cut-scene could jump to the next level instead of the menu). The game's movies: RoQ player with PWM sound, four movies in the passport. Lara's double, sound effects quieter with distance. |

## Credits
- **OpenLara** – XProger and contributors: https://github.com/XProger/OpenLara
- **Doom 32X: Resurrection** – Victor Luchits (viciious): the RoQ movie decoder (`roq_read.c`,
  `roq.h`, MIT) and the way its player feeds the PWM; it is also how we knew the 32X can play FMV
- **RoQ decoder** – Dr. Tim Ferguson (2001)
- **Sega CD / 32X boot and command framework** – from the Kobo Deluxe CD32X project, built on
  Chilly Willy's Sega CD and 32X examples and toolchain – the CD32X loader started as its CD-boot loader
- **4MB 32X RAM cart** Victor Luchits, Joseph Fenton, Leo Oliveira, Tiido priimägi and Eric Witt. 
- OpenLara port – MicroNut99
- **Tomb Raider** – Core Design / Eidos (now Crystal Dynamics / Embracer). Tomb Raider is their
  trademark; this project contains none of their data and is not affiliated with them.

Licences: see `licenses/` and each upstream project's own licence.
