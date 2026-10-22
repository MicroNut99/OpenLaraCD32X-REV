# OpenLara CD32X – Release v1.6

**Tomb Raider (1996) on Sega CD + 32X**, running from one CD with a battery-backed RAM cart in the 32X.
It is built on XProger's OpenLara: the fixed-point engine from its Game Boy Advance port, which XProger
also brought to the 32X.

This document lists everything done to port the game to the CD32X, from the source this work started from
to this release.

> **Made for real hardware, not for emulation.** Every timing, memory and bus decision here was made and
> checked for the console. Emulation was used to measure and test, but this release is not tuned for it,
> and emulators that lack the 4 MB RAM cart (Kega Fusion, for example) cannot run it at all. An
> emulation-only release, tuned for emulators, may follow.

> **No game data is included.** The build converts your own Tomb Raider 1 (PC) data. The music, the
> cut-scene voices (from your own game CD), the movies and the loading picture are also yours to supply.

Source: https://github.com/MicroNut99/OpenLaraCD32X-REV (branch `cart-test-8mb-16mb`)
Boot stack documentation and command ledger: https://github.com/MicroNut99/docs

---

## 1. Hardware

| | |
|---|---|
| Console | Sega Genesis / Mega Drive + Sega CD / Mega-CD + Sega 32X |
| Cart | **4 MB RAM cart** (battery-backed) in the 32X slot – required. 8 MB / 16 MB paged carts: test builds only (section 9) |
| Pad | 6-button pad recommended (3-button: no walk, roll or look) |
| Disc | one CD-R: data track + CD audio tracks (burn the `.cue` with its `.bin`) |

## 2. What the player gets

- All 15 levels from Caves to the Great Pyramid, plus Lara's Home, played straight through or picked
  from the passport.
- **Every enemy**, including Lara's double in Atlantis. Since v1.6 the winged mutants fly, gorillas
  climb, rats swim and walk, and the double falls into the pit and stays there.
- **Every trap and moving object** (new in v1.5): rolling balls, spikes, teeth traps, falling ceilings,
  Damocles swords, Thor's hammer and lightning, rolling blocks, drawbridges, the Natla's Mines cabin
  and boat, earthquakes, lava wedges and lava fountains, the Midas hand, mutant eggs, the centaur
  statue, the Scion, fire, and the enemies' visible projectiles. The rooms that change during a level
  (flipmaps) now change.
- **CD music**: one piece per level (your choice), the secret chime, the title music, and (v1.6,
  optional) the game's own story cues and voice lines from the PlayStation soundtrack.
- **All sound effects** of every level, through the 32X's PWM sound chip, quieter with distance.
- The four in-engine **cut-scenes** with their voices, also playable from the passport.
- **All eleven movies** (FMV, v1.6): each plays at its place in the story; the seven big ones stream
  from the CD while they play. All of them are in the title passport too.
- **Saving** in the RAM cart: **16 save slots** in the passport (v1.6) plus the options, kept with the
  power off.
- **Fast start**: Caves (the title) stays in the cart, and so does the last level played. After power
  on, *Current Position* starts in seconds instead of loading from the CD.
- 10–16 frames per second exploring, 10–13 in fights (console, v1.4 profiler build; v1.5 adds the
  span loop speed-ups of section 4).

## 3. How it runs (the CD32X stack)

| CPU | Job |
|---|---|
| **Sub-CPU** (Sega CD 68000) | reads the CD, plays CD audio and the PCM chip, answers the SH2's commands (`subcpu/main.c`) |
| **Genesis 68000** | command slave of the Sub-CPU: the pad, 32X registers, and **all writes to the cart** (the RV method), `hw_md.s` |
| **Master SH2** | the game, running **from the cart** like a cartridge game |
| **Slave SH2** | draws the bottom half of the screen; plays the sound effects and movie sound through the PWM |

- **Boot:** the Sega CD boots the disc. The CD32X loader (`sh2/cdloader_sh2.c`) shows the boot picture,
  copies the engine (`OPENLARA.32X`) into the cart and starts both SH2s.
- **Commands:** the SH2 asks the Sub-CPU for everything through the 32X COMM registers (COMM0 = command,
  COMM2 = parameter). The numbers are recorded in the command ledger
  (`CD32X_COMMAND_LEDGER_ADDITIONS.md`) and explained in `CD32X_COMMAND_PROGRAMMING_GUIDE.md`.
  OpenLara uses 38 open file, 45/54 music play/stop, 46/47 file chunk to cart / compare, 48 pad (+
  PCM effect), 51/52 boot picture and display, 53 PCM pack, 58/61 save area, and 68 cart page (test
  builds only).
- **Cart layout (4 MB):** engine `0x000000`, Caves (title) `0x080000`, the level being played
  `0x200000`, save area `0x3F0000` (settings, saved game, and from v1.5 the load records in save
  piece 14, `0x3FE000`).

### Rules learned on the console
1. The SH2 can read the cart but **not write it**: every cart write goes through the 68000.
2. While the 68000 copies with RV = 1, SH2 cart accesses simply wait. No parking tricks are needed
   (parking the slave in SDRAM caused many dead discs).
3. COMM6 belongs to OpenLara's renderer. Anything else writing it freezes the game.
4. **No CD data reads while CD audio plays**: every level load stops the music first.
5. Never restart a CD track every frame: it stays silent while the drive spins.
6. Colour 0 is what a cleared screen shows, so keep it black in every palette.
7. Diagnostic text costs time even when hidden, so it is compiled out.

## 4. Everything added, by area

### 4.1 Loading the game from CD (OL-1 … OL-3, CAVES-1, RESUME-1, STREAM-1)
- **OL-1/2:** CD32X build of the engine (`make CD32X=1`); each level loads from the disc into the
  cart's level slot (2 MB) when needed: 21 level files on one disc.
- **OL-3:** the loading picture with a red progress bar; a level already in the cart is not loaded
  again (restart, death, loading a save of the same level).
- **CAVES-1:** Caves, which is also the title menu, has its own place in the cart (`0x080000`).
  After the first full copy, a record with its length and checksum makes every later start skip the
  load. Hold A + B + C at start to force a reload.
- **RESUME-1:** the same for the level slot, so after power-on the last level resumes from the cart
  (emulation: Lost Valley 28 s → 3 s).
- **STREAM-1 (v1.5, Sub-CPU, switch `STREAM`, on by default):** the drive reads on through a whole
  level file instead of stopping and seeking for every 32 KB chunk. Sectors that arrive while the 68000
  copies to the cart are kept in a 64 KB ring. The same code runs on the console in OpenJazz CD32X R7
  and Hurrican CD32X P2b (about 2.5× faster loads there). `STREAM=0 bash build.sh` builds the old reads.
  In emulation: the first level load 40 s → 17 s.
- **CARTW-1 (v1.6, Sub-CPU, switch `CARTW`):** command 46 copies each chunk to the cart with the
  68000's word copy (`hw_md.s` command 27, 15 clocks a byte) instead of the byte copy (command 33,
  42 clocks a byte), in 4 KB pieces. RV is on for a third of the time.

### 4.2 Frame rate
- **Screen split:** the two SH2s draw separate halves of the screen and never wait for each other
  mid-frame. Polygons crossing the line are cut. The line moves only when one SH2 has been behind
  for 8 frames (no flicker along it).
- **Overlap:** the master starts the next frame's pad read and game logic while the slave finishes
  drawing. The slave shows the frame when both halves are done (no tearing).
- **No wait for the screen refresh** before preparing the next frame.
- **REV-1** pauses in the SH2 wait loops (the shared bus), **REV-2** screen clear by the 32X VDP's
  auto fill, **REV-3** per-polygon drawing code in SDRAM, **REV-4** the SH2's hardware divider for
  cut polygons.
- **OPT-1 / OPT-2 (v1.5):** a 2-byte lightmap table and 4 pixels per loop in the textured and
  Gouraud-textured span loops (−7 to −9 % frame time in emulation, pictures bit-identical). Also a
  fix for the darkest fog row (black faces in Palace Midas).
- **Face limit check (v1.5):** a room never overruns the face list.
- **Profiler build:** `PROF=1` shows fps and per-stage times on screen.

### 4.3 Sound and music
- **CD music (OL-4/8/10/11/12):** play/stop through the Sub-CPU; the SH2 remembers what it started (the
  drive's status is too slow to ask every frame); per-level music from `music_map.txt` (disc track N
  = game track N).
- **MUSIC-1 (v1.5):** Options → Sound → Music turns the CD audio off and on (it was saved but did nothing).
- **SFX-1 (v1.5):** Sega CD PCM effects, up to 3 per frame (was 1), better packs (about 26 sounds per
  level). This is the fallback path.
- **SFX-2 (v1.5, default):** **every** sample of every level, appended to the level file and mixed on
  the slave SH2 into the 32X PWM (up to 8 at once, with distance volume and pitch). Before, only
  15–17 of a level's 70–94 sounds could play. Volume `SFX_GAIN` 2, as tested on the console.
- **SFX-4 (v1.5):** a level with its own bank no longer loads the PCM pack (one CD read less per level).
- **SETTINGS-1 (v1.5):** the Music and Sound FX switches respond at once. The settings are written to
  the cart when the inventory closes, or on the title when its ring is idle (each write is a 4 KB
  transfer that used to freeze the menu and swallow presses).
- Effects are quieter with distance (SFXVOL-1); `soundStop` works on the CD32X (v1.5).

### 4.4 Movies and cut-scenes
- **Cut-scenes:** the four in-engine cut-scenes with their voices (CD audio taken from your own game CD
  by `tools/nrg_cut_audio.py`), in the passport too.
- **Movies (FMV-1):** a RoQ player. The master SH2 decodes the pictures from the cart (decoder from
  *Doom 32X: Resurrection*, MIT) and the slave SH2 plays the DPCM sound through the PWM. 288 × 112 at
  15 frames/s, lines doubled. Four movies fit the level slot.
- **FMV-2 (v1.6):** **all eleven movies.** The big ones (5–20 MB) stream: the level slot becomes a ring
  of 60 × 32 KB plus a spare slot (command 46 ring mode, Sub-CPU switch `FMVRING`); the master keeps the
  CD busy and never lets it overwrite what the decoder or the sound still needs; 1.8 MB load first
  (red bar). The slave plays the sound from a ring of PWM blocks and reads the cart only while the
  68000 is not writing it (the Sub-CPU flags its copy in COMM12 bit 15), so the sound has no gaps and
  is the pictures' clock. Each movie plays at its place in the story (CAFE + SNOW before Caves,
  MANSION before Lara's Home, LIFT before the Folly, VISION before Khamoon, CANYON before Natla's
  Mines, PYRAMID before Atlantis, PRISON after Atlantis, END after the Great Pyramid, then the title).
  CANYON (3 min) streams at 15.0 pictures a second with the sound in real time (emulation).

### 4.5 Gameplay
- **ENEMIES-1/2:** lion, puma, gorilla, rat, crocodile, mutants, centaur, mummy, Larson, Pierre, the
  skater (now drawn with his skateboard, v1.5), the cowboy, Mr. T, Natla, the Torso.
- **DOPPEL-1:** Lara's double in Atlantis mirrors Lara through the room centre, copies her animation,
  hurts Lara when shot, and falls into the pit to open the way on. (It was an empty class before.)
  v1.6: she dies in the pit and her body stays, as in TR1.
- **Creatures (v1.6):** the levels' placeholder mutant models get the mutant's model; the winged
  mutants fly (their own height, flyers' navigation zones); gorillas climb one-block steps; the
  swimming rat and the land rat change into each other at the water's edge; hatched enemies start
  in their standing animation.
- **CUES-1 (v1.6):** the levels' music cues (game tracks 6–21) and voice lines (26–56) play when they
  are on the disc: `PS1MUSIC="<folder>" bash build.sh` converts them from the PlayStation soundtrack
  set (`tools/import_tr1_music.py`). The engine knows which tracks the disc has, so a missing cue never
  stops the level music.
- **OBJECTS-1 (v1.5):** all missing TR1 traps and objects, ported from the original game's behaviour
  (new file `src/fixed/traps.h`):
  - traps: rolling ball, spikes, teeth trap, falling ceiling, Damocles sword, Thor's hammer and
    lightning, lava wedge, the Midas hand (gold death; the lead bar becomes gold), fire and flame
    emitters (Lara catches fire), the lava fountain;
  - moving things: rolling block (the floor follows it), drawbridge, the Natla's Mines cabin (three
    levers, then the room flips) and boat, earthquake;
  - creatures: mutant eggs hatch their mutant, centaur statue, the Scion holder and target;
  - **visible projectiles** for the mutants', centaurs' and Natla's shots;
  - **flipmaps** (rooms that change: flooded rooms, the cabin's fall, …) and the trigger effects
    (camera shake, flood and earthquake sounds, explosions, Lara turning, …);
  - the save keeps the flipmaps and every item made during play;
  - fixes found by the review: objects that start hidden appear when triggered; burning only on the
    lava itself, not on a bridge over it; a rolled block keeps its raised floor after a load; closed doors
    stay closed in both versions of a flipmap room; one item removing another mid-frame is safe.
- **Controls:** Start (or Mode) opens the inventory; the passport lists all levels, the cut-scenes and
  the movies.

### 4.6 Saving (OL-9, SAVE-2)
- Saves and settings live in the RAM cart's last 64 KB. The SH2 fills a 4 KB piece, the Sub-CPU
  takes it word by word (command 58), and the 68000 writes it to the cart and checks it (command 61).
- **SAVE-2 (v1.5):** the Caves and level records moved out of the piece that holds the settings and the
  saved game, so a level load never rewrites your save. A failed record write is retried. The first
  start with v1.5 loads Caves once more.
- **SLOTS-1 (v1.6, Sub-CPU switch `SLOTS`):** 16 save slots at cart `0x060000`–`0x07FFFF` (8 KB each,
  between the game and Caves). The save page lists them with their levels; the load page lists the
  saved ones first; a v1.5 save stays loadable. Command 61 with COMM2 bit 8 writes the slot area and
  replies `0x101`; with an older Sub-CPU the game keeps the single save. The game's ROM must stay
  under 384 KB (`build.sh` checks; it is about 270 KB).

## 5. What was added to the source

Starting point: XProger/OpenLara, commit `3b268ca` (its 32X build), plus Chilly Willy's Sega CD / 32X
toolchain and the Kobo Deluxe CD32X boot framework.

| Part | Files | Notes |
|---|---|---|
| Engine, CD32X platform | `src/platform/32x/main.cpp`, `render.cpp`, `rasterizer.h`, `sound.cpp`, `32x.h`, `Makefile`, `asm/*.i`, `asm/data.s`; new `fmv.inc`, `roq.h`, `roq_read.c`, `sfx.inc`, `carttest.inc`, `bench.inc` | all behind `CD32X` (`make CD32X=1`); the cartridge build is unchanged |
| Engine, game | `src/fixed/enemy.h`, `inventory.h`, `item.h`, `room.h`, `game.h`, `camera.h`, `common.h`, `object.h`, `lara.h`, `lang/en.h`; new `traps.h` | enemies, double, traps and objects, flipmaps, effects, save slots |
| Patches (applied in order by `build.sh`) | `engine/openlara-cd32x.patch`, `openlara-cart-test.patch` (+ `-1to2`), `openlara-opt1.patch`, `openlara-opt2.patch`, `openlara-sfx.patch`, `openlara-objects.patch`, `openlara-content.patch` (v1.6) | |
| SH2 loader | `OpenLaraCD32X/sh2/cdloader_sh2.c` | boot picture, engine copy, SH2 start |
| Sub-CPU | `OpenLaraCD32X/subcpu/main.c`, `Makefile` | the commands; CMD 68 with `CART=`; STREAM-1, SLOTS-1, FMV-2, CARTW-1 with `STREAM`, `SLOTS`, `FMVRING`, `CARTW` (all off = v1.5's code) |
| Level converter | `gba/packer` (patched) | `.PKD` level files + `.SND` sound tables |
| Tools | `prep_openlara_cd.py`, `make_ol_splash.py`, `make_boot_image.py`, `make_music_tracks.py`, `make_sfx_packs.py`, `sfx_bank.py`, `nrg_cut_audio.py`, `make_fmv.sh`, `make_mixed_cd2.py`, `import_tr1_music.py` (v1.6) | disc, music, sound and movie preparation |

Never changed (the boot stack rule): the boot block, `hw_md.s`, `crt0.s`, `src-md/`, the loader's
start-up code. Sub-CPU changes only behind a build switch, each recorded in the ledger.

### Build switches
| Switch | Default | What |
|---|---|---|
| `CART=8` / `CART=16` | off | 8 / 16 MB paged cart test build (section 9) |
| `STREAM=0` | 1 | Sub-CPU without the drive read-ahead |
| `SLOTS=0` | 1 | Sub-CPU without the save slot area (one save) |
| `FMVRING=0` | 1 | Sub-CPU without movie streaming (only the movies that fit) |
| `CARTW=0` | 1 | Sub-CPU copies to the cart in bytes |
| `PS1MUSIC="<folder>"` | | add the story cues and voices from the PlayStation soundtrack set |
| `SFX=pcm` | PWM | no sound banks: the Sega CD PCM effects only |
| `PROF=1` | off | on-screen profiler |
| `OPT=0` (engine) | 1 | the v1.4 span loops |
| `EXTRA="-DCD32X_NO_SPLIT …"` | | turn off one speed-up: `NO_SPLIT`, `NO_OVERLAP`, `NO_IDLE`, `NO_FILL`, `NO_SDCODE`, `NO_HWDIV` |

## 6. Build and burn

```bash
cd OpenLaraCD32X && bash build.sh            # or: CART=16 bash build.sh
```
Burn `subcpu/CDROMPlayer_music.cue` (or `_cart16_music.cue`) with its `.bin`; the `.iso` alone has no music.
Full steps (packer, music, cut-scene voices, movies): the repository's `README.md` and
`docs/OPENLARA_CD32X_HANDOFF.md`.

## 7. Tested

| What | Where |
|---|---|
| v1.0–v1.4: every level, music, saves, cut-scenes, movies, frame rate | **console** |
| OPT-1/2, RESUME-1, SFX-1/2, MUSIC-1 | **console** (21 Oct 2026) and emulation |
| OBJECTS-1, the review fixes, Lara's double, SETTINGS-1, SFX-4, SAVE-2 | emulation – see the results table below; not yet on the console |
| STREAM-1 | emulation (first level load 40 s → 17 s); the same code is on the console in OpenJazz and Hurrican |
| v1.6: SLOTS-1, FMV-2, CARTW-1, creatures, story movies | emulation – see `docs/CONTENT.md`; not yet on the console |
| CUES-1 | compiled; not run (the test discs have no audio tracks) |

| Object | Level | Seen in emulation |
|---|---|---|
| Rolling ball | Tomb of Qualopec | rolls, runs Lara over (boulder death) |
| Spikes | Tomb of Qualopec | 15 damage per step while running over them |
| Falling ceiling | Tomb of Qualopec | appears when triggered, falls, kills Lara standing under it |
| Rolling block | Tomb of Qualopec | rolls two blocks and stops |
| Teeth trap | Palace Midas | snaps shut, 400 damage per bite |
| Midas hand | Palace Midas | Lara standing on it dies (the gold death) |
| Flame emitter | Palace Midas | the fire appears; burning damage not reached with the test's placement |
| Damocles sword | St. Francis' Folly | falls when Lara is near, 100 damage per hit |
| Thor's hammer | St. Francis' Folly | strikes and kills Lara in its path |
| Lightning | St. Francis' Folly | strikes for 400, kills after a few strikes |
| Centaur statue | Tomb of Tihocan | breaks open, the centaur attacks |
| Drawbridge | Obelisk of Khamoon | lowers |
| Cabin | Natla's Mines | one lever: one step down |
| Boat | Natla's Mines | moves along its path |
| Lava fountain | Natla's Mines | particles fly; Lara on the lava catches fire and dies |
| Mutant egg | Atlantis | hatches (explosion); the mutant comes out and runs at Lara |
| Lava wedge | Atlantis | slides and kills Lara |
| Scion | Atlantis | shot five times, explodes and is gone |
| Lara's double | Atlantis | mirrors Lara through the room centre, step for step |
| SAVE-2 | – | records written to save piece 14, piece 0 untouched; the next boot resumes the level from the cart (level ready at frame 2803 instead of 5163) |
| Every level | Caves … the Great Pyramid (15 levels) | each loads from the CD into the cart, Lara stands in it, the picture is right, no crash |

## 8. Known limitations
- The big movies wait about 16 s for their first 1.8 MB before they start; a movie's skip button must
  be held a moment while a chunk loads.
- *Unfinished Business* is not included (its levels were never converted).
- The story cues and voice lines need the PlayStation soundtrack set (`PS1MUSIC=`).
- The Midas gold look (Lara turning to gold) is not drawn; the death itself works.
- Cut-scene sound may start a moment late (the drive seeks to the track).
- Some enemy shots (darts, bolts, bullets of human enemies) still hit at once without a visible projectile.

## 9. The 8 MB / 16 MB cart test (CART-1/2)
`CART=8` / `CART=16` builds add a cart page test to the title's options (Detail → Cart Test) for
MicroNut99's 16MB-PCB-CD32X paged SRAM cart. They use one Sub-CPU command, 68 (window 1 page),
and save piece 15 as scratch. The normal build has no test code.

## 10. Versions
| Version | |
|---|---|
| 1.0 | The whole game from CD: all levels, CD music, PCM effects, saving in the RAM cart, enemies, passport, loading screen |
| 1.1 | Screen split, overlap, no refresh wait; profiler build |
| 1.2 | Fast start: Caves stays in the cart |
| 1.3 | Cut-scenes with voices, movies, Lara's double, effects quieter with distance |
| 1.4 | REV-1…4: bus pauses, VDP clear, SDRAM drawing code, hardware divider |
| 1.5 | OPT-1/2 span loops, RESUME-1, all sound effects (PWM), Music switch, every TR1 trap and object, flipmaps, visible projectiles, STREAM-1 faster loading, save area and settings fixes, 8/16 MB cart test builds |
| **1.6** | 16 save slots, all eleven movies (streamed, at their story places), word copies to the cart, flying mutants, climbing gorillas, swimming and land rats, the double stays in the pit, story cues and voices; fixes from the review |

## 11. Credits
- **XProger** (Timur Gagiev) and contributors – **OpenLara**, and its **Game Boy Advance port**: the
  fixed-point engine this game runs on, which he also brought to the 32X.
  https://github.com/XProger/OpenLara
- **The D32XR team** – *Doom 32X: Resurrection* (Victor Luchits / viciious and contributors): the RoQ
  movie decoder (`roq_read.c`, `roq.h`, MIT), the way its player feeds the PWM, and the 32X know-how
  that showed what the machine can do.
- **Dr. Tim Ferguson** – the original RoQ decoder (2001).
- **Chilly Willy** – the Sega CD / 32X toolchain and examples the boot stack is built on.
- **Kobo Deluxe CD32X** – the Sega CD / 32X boot and command framework the CD32X loader started from.
- **4 MB 32X RAM cart** – Victor Luchits, Joseph Fenton, Leo Oliveira, Tiido Priimägi and Eric Witt.
- **CD32X port: MicroNut99** – https://github.com/MicroNut99/docs
- **Tomb Raider** – Core Design / Eidos (now Crystal Dynamics / Embracer). Tomb Raider is their trademark;
  this project contains none of their data and is not affiliated with them.
