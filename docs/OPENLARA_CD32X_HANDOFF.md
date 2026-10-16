# OpenLara CD32X – handoff (Tomb Raider on Sega CD + 32X + 4 MB RAM cart)

## What it is
Tomb Raider 1 (OpenLara's 32X engine) running from a **Sega CD disc** on real hardware:
- the CD32X loader copies the engine into MicroNut99's **4 MB RAM cart** and starts both SH2s;
- every level is loaded **from the disc into the cart** when it is needed (21 levels on one disc);
- **CD music** (one album piece per level), **PCM sound effects** (one pack per level),
  **saves** in the battery-backed cart, splash + red loading bar, full passport, all enemies
  except Lara's double.

Proven on the console (Sega CD + 32X + RAM cart, 6-button pad).

## Folder layout (`sega/`)
| Path | Role |
|---|---|
| `OpenLaraCD32X/build.sh` | one command: splash → engine → ROM prep → levels → sound packs → SH2 loader → Sub-CPU → disc check → CD audio |
| `OpenLaraCD32X/sh2/` | the SH2 **loader** (`cdloader_sh2.c`, `CDL_TARGET_OPENLARA 1`): boot splash, copies `OPENLARA.32X` into the cart, starts both SH2s |
| `OpenLaraCD32X/subcpu/` | Sub-CPU program (`main.c`) + 68000 listener (`hw_md.s`) + disc build (`Makefile`, `make_mixed_cd2.py`) |
| `OpenLaraCD32X/tools/` | `prep_openlara_cd.py`, `make_ol_splash.py`, `make_boot_image.py`, `make_music_tracks.py`, `make_sfx_packs.py` |
| `OpenLaraCD32X/engine/` | copies of the engine files the patches wrote (for reference) |
| `OpenLaraCD32X/gba/packer/` | the level converter (`packer.exe`, patched: writes `.PKD` + `.SND`) and its output `out32x/` |
| `OpenLaraCD32X/DATA/` | your Tomb Raider 1 PC data (`.PHD`) |
| `OpenLaraCD32X/soundtrack/`, `soundtrack_wav/` | the album (MP3 → WAV, 44.1 kHz 16-bit stereo) |
| `OpenLaraCD32X/splash.jpg`, `music_map.txt` | the picture and the music assignment – edit freely |
| `OpenLara-src/` | the engine (XProger/OpenLara, git, branch `cd32x`): `src/platform/32x/` and `src/fixed/` |

`sega/Demo`, `sega/d32xr-master`, `sega/tools` are the **clean source** – OpenLara only copies from them.

## Build and burn
```bash
cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/OpenLaraCD32X && bash build.sh 2>&1 | tail -3
```
**Burn `subcpu/CDROMPlayer_music.cue`** (with its `.bin`) – the `.iso` has no music.
After changing level data: rebuild `packer.exe` (MinGW, `gba/packer/build`) and run
`packer.exe 32x out32x` in Windows Command Prompt (WSL cannot start `.exe` files here).

## The disc
- Data track: `APP.BIN` (Sub-CPU + 68000), `CDLOADER.BIN` (SH2 loader), `OPENLARA.32X` (engine,
  trimmed), `IMAGE.RAW` (boot splash), `TITLE.PKD` … `LEVEL10C.PKD`, `SFX00.BIN` … `SFX20.BIN`.
- Audio tracks 2–80: **disc track N = game track N** (`music_map.txt`; unlisted = 2 s of silence):
  2–4 title, 13 secret chime, 60 + level = that level's music (60 menu … 80 Great Pyramid).

## How it runs
| CPU | Does |
|---|---|
| Sub-CPU (`subcpu/main.c`) | reads the CD, plays CD audio + PCM, answers the SH2's commands |
| Genesis 68000 (`hw_md.s`) | command slave of the Sub-CPU: pad, 32X registers, **writes the cart** (RV method) |
| Master SH2 | the game; runs **from the cart** (`0x02000000`) |
| Slave SH2 | OpenLara's renderer helper (jobs in COMM4) |

**SH2 → Sub-CPU commands** (COMM0 = command, COMM2 = parameter, Sub clears COMM0 when done):
| Cmd | Use |
|---|---|
| 38 | open file: COMM2 = 20 engine ROM, 0 `IMAGE.RAW`, **100 + LevelID** level; length → COMM8 |
| 46 | 32 KB chunk → cart (level slot 16 = 2 MB); bit 15 of COMM2 = skip read-back |
| 48 | pad (→ COMM8) every frame; high byte of COMM0 = sound effect (sample index + 1) |
| 45 / 54 | play track (bit 15 = once, else repeat) / stop |
| 51 | file chunk → frame buffer (boot splash) |
| 52 | Genesis display on/off |
| 53 | load `SFX<level>.BIN` into the PCM chip (COMM2 = level) |
| 58 / 61 | save area: put a word into the 4 KB buffer / 68000 copies it to cart `0x3F0000 + n × 4 KB` |

**COMM registers shared with OpenLara:** COMM2 and **COMM6** = renderer master/slave sync,
COMM4 = slave jobs. The 68000 heartbeat that used to write COMM6 is removed (it froze rendering).

**Cart (4 MB):** `0x000000` engine · `0x200000` current level · `0x3F0000` settings, `0x3F0400` saved game.

## Engine changes (`OpenLara-src`, all `#ifdef CD32X`)
| Step | Change |
|---|---|
| OL-1 | CD32X build: pad via CMD 48, Genesis text off |
| OL-2b | `osLoadLevel`: level from disc into the cart; SH2s just pause during the 68000's copy (no parking) |
| OL-3 | splash + red progress bar, no text, no reload of the level already in the cart |
| OL-4/8/10/11/12 | CD music: play/stop, `sndStop` leaves music alone, SH2 remembers what it started, per-level music, cues without music ignored |
| OL-6 | sound effects: packer keeps the sound tables, per-level PCM packs |
| OL-9 | saves: SH2 → Sub-CPU buffer → 68000 → cart (SH2 cannot write the cart) |
| controls | Start opens the inventory (the 32X build had a developer free camera on Start) |
| passport | all levels, scrolling list |
| ENEMIES-1/2 | lion, puma, gorilla, rat, crocodile, mutants, centaur, mummy, Larson, Pierre, skater, cowboy, Mr. T, Natla, Torso |

## Hardware lessons (all seen on the console)
1. **The SH2 can read the cart but not write it.** Writes go through the 68000 (CMD 46/61).
2. **While the 68000 copies with RV = 1, SH2 cart accesses simply wait** – no SDRAM tricks needed.
   (Parking the slave in SDRAM was the cause of many dead discs.)
3. **COMM6 belongs to OpenLara's renderer.** Anything else writing it freezes the game as soon as it draws.
4. **No CD data reads while CD audio plays** – every level load stops the music first.
5. **Never restart a CD track every frame** – it stays silent while the drive spins. The engine asks
   "still playing?" each frame; the answer must not depend on the drive's slow status.
6. **The loader's quick check (3 chunks) is off** – a half-finished copy passed it and crashed later.
7. **Colour 0 is what a cleared screen shows** – keep it black in every palette (the white flash).
8. Diagnostic text costs time even when hidden – the per-pass register reads are compiled out.

## Known limitations
- Lara's double (Atlantis) does not move. Gorillas do not climb. Water rats do not dive.
- Enemy projectiles are instant shots; "can see" ignores walls.
- Sound effects: one per frame, fixed volume, the 31 most important per level.
- Music: the original story cues and cut-scene dialogue are not on the album (skipped).
- FMVs and Unfinished Business are not included.

## Everyday changes
- **Picture:** replace `splash.jpg` → build.
- **Music:** edit `music_map.txt` (`<track>  <file name start>`) → build.
- **Levels / sounds:** rerun the packer (see above) → build.
