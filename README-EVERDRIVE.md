# OpenLara CD32X on a Mega EverDrive PRO

Tomb Raider on the Sega CD + 32X needs a 4 MB RAM cart in the 32X slot. This lets a **Mega EverDrive PRO**
take that cart's place, and gives the game **16 MB** to work with instead of 4.

## What it is

- **Software only.** No new board, no soldering. A stock Mega EverDrive PRO, the 32X and the Sega CD.
- **`EDBOOT.BIN`:** a small program you start from the EverDrive menu. It turns the EverDrive into the
  RAM cart, then starts the Sega CD, which boots the OpenLara disc as usual.
- **The `CART=ED` disc:** OpenLara with a cart test that checks the EverDrive's memory beyond the first
  4 MB (Options → Graphics → Cart Test).

## What it isn't

- **Not a cartridge version of the game.** Levels, music and movies still come from the CD.
- **Not the EverDrive's "Run Mega-CD".** That plays CD images and gives the game a save-RAM cart, not
  this memory.
- **Not battery-backed.** The RAM cart keeps saves without power; expect the EverDrive to lose them at
  power-off.
- **Not using the extra 12 MB in the game yet.** The game runs in the 4 MB cart's layout; only the
  cart test reaches past it. Keeping levels in the extra memory is the next step.
- **Not for the 16MB-PCB-CD32X board.** Different registers: don't run the `CART=ED` disc on that board, or
  its `CART=8` / `CART=16` discs on the EverDrive.

## How it works

1. **The EverDrive's memory can be written.** A ROM whose header says `SEGA SSF` turns on the EverDrive's
   "extended SSF" mapper. One bit in its control register makes its 16 MB writable, and its other
   registers put any 512 KB bank of it into each 512 KB window of the cartridge space.
2. **`EDBOOT.BIN` sets that up**: banks 0–7 in the eight windows, which is exactly the 4 MB RAM cart, with
   writing on.
3. **Then it lets go of the cartridge line.** Another bit tells the EverDrive to stop telling the console
   "a cartridge is here". The console then sees only the Sega CD, so the Sega CD's BIOS starts and boots
   the disc. The EverDrive's memory is now where a RAM cart would be.
4. **The disc does what it always does.** Its boot stack copies the game into "the cart" and runs it
   there. It never knows the cart is an EverDrive.
5. **More than 4 MB** is the mapper again: the `CART=ED` test switches banks 4–27 into the cart's upper
   2 MB, four at a time, and checks each one.

## How we know the memory works

The CD32X BootCamp's `edtest` ran on a Genesis + 32X + EverDrive PRO (24 Oct 2026): 27 banks (13.5 MB),
written by the 68000 and read back by the 68000 and both SH2s, two full passes (the second with every bit
flipped), **0 bad words**. Photos and details: CD32X BootCamp, `docs/10_EVERDRIVE_TEST.md`.

**Not yet tried on a console:** step 3, `EDBOOT.BIN` handing the console to the Sega CD. In ares with its
EverDrive PRO cart, EDBOOT boots a CD32X disc through the Sega CD BIOS, and the OpenLara cart test shows
`FOUND 16MB`.

## The first of its kind

As far as we know, this is the first time a flash cart's own memory has been the RAM cart of a Sega CD +
32X game. It is set up in software on unmodified hardware, and it gives a CD32X game four times the
memory of the cart it replaces.

## Build and play

```bash
CART=ED bash build.sh      # -> subcpu/CDROMPlayer_cartED_music.cue  (burn it)
```

1. `EDBOOT.BIN` comes from the CD32X BootCamp (`edboot/`, `bash edboot/build.sh`). Copy it to the
   EverDrive's SD card.
2. Genesis + Sega CD + 32X, the EverDrive PRO in the 32X slot, the disc in the Sega CD.
3. Start `EDBOOT.BIN` from the EverDrive menu.

| You see | Meaning |
|---|---|
| The Sega CD logo, then OpenLara | It works |
| A red screen | The EverDrive did not let go of the cartridge line |
| A blue screen | No Sega CD found |

`EDBOOT.BIN` boots the normal OpenLara disc too, in the 4 MB layout.

**The cart test:** on the title, Graphics (the sunglasses) → Cart Test → A. Expected: `FOUND 16MB`,
`PAGE 1 B04-07 OK` … `PAGE 6 B24-27 OK`, `SAVE AREA UNCHANGED`. Then A loads a level into every page.
More: `docs/CART_TEST.md`.

## Credits

- **krikzz:** the Mega EverDrive PRO and its extended SSF mapper.
- **MicroNut99:** OpenLara CD32X, the CD32X boot stack, the console tests.
