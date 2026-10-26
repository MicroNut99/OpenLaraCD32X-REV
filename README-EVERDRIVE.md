# OpenLara CD32X on a Mega EverDrive PRO

Tomb Raider on the Sega CD + 32X needs a 4 MB RAM cart in the 32X slot. This lets a **Mega EverDrive PRO**
take that cart's place, and gives the game **16 MB** to work with instead of 4.

**It runs on the console** (Genesis + Sega CD + 32X, Mega EverDrive PRO, 25 Oct 2026).

## What it is

- **Software only.** No new board, no soldering. A stock Mega EverDrive PRO, the 32X and the Sega CD.
- **`EDCD.32X`:** a small 32X cartridge ROM you start from the EverDrive menu with the disc in the Sega
  CD. It turns the EverDrive into the RAM cart, starts the Sega CD itself (the console's "cartridge
  mode"), and hands over to the disc, which loads the game into the EverDrive and runs it there.
- **The `CART=ED` disc:** OpenLara built for it, with a cart test that checks the EverDrive's memory beyond
  the first 4 MB (Options → Graphics → Cart Test). The same disc also boots the normal way with a RAM cart.

## What it isn't

- **Not a cartridge version of the game.** Levels, music and movies still come from the CD.
- **Not the EverDrive's "Run Mega-CD".** That plays CD images and gives the game a save-RAM cart, not
  this memory. Start `EDCD.32X` instead.
- **Not a RAM cart that keeps anything.** The EverDrive wipes its 16 MB (PSRAM) every time its menu
  runs - measured, 25 Oct 2026. So the saves do not live there: with `EDCD_SAVE=1` they live in the
  EverDrive's battery RAM (its bank 31), which the EverDrive OS keeps on the SD card as
  `MEGA/gamedata/EDCD.32X/bram.srm` (256 KB). Settings and all 16 save slots. The cart's fast-load
  records are wiped with the cart, so every launch loads from the CD.
- **Not using the extra 12 MB in the game yet.** The game runs in the 4 MB cart's layout; only the
  cart test reaches past it. Keeping levels in the extra memory is the next step.
- **Not for the 16MB-PCB-CD32X board.** Different registers: don't run the `CART=ED` disc on that board, or
  its `CART=8` / `CART=16` discs on the EverDrive.

## How it works

1. **The EverDrive's memory can be written.** A ROM whose header says `SEGA SSF` turns on the EverDrive's
   "extended SSF" mapper. One bit in its control register makes its 16 MB writable, and its other
   registers put any 512 KB bank of it into each 512 KB window of the cartridge space.
2. **The EverDrive cannot step aside.** The plan was to let the Sega CD BIOS boot the disc as usual; on the
   console the EverDrive stays the cartridge whatever its registers are told (CD32X BootCamp,
   `EDPROBE`). So the boot goes the other way round:
3. **`EDCD.32X` does what the BIOS and the disc's boot block do** - the 32X's SH2 loader from the cart, the
   Sega CD's own processor started with its BIOS unpacked from the Sega CD's ROM, the disc's program
   loaded - and hands over to the disc's boot stack, built a second time for the cartridge-mode memory map
   (`MODE1=1`). The disc itself is the same; its ROM's 68000 jump table points at a RAM table so it boots
   both ways.
4. **The disc does what it always does**: copies the game into "the cart" and runs it there.
5. **More than 4 MB** is the mapper again: the `CART=ED` test switches banks 4–27 into the cart's upper
   2 MB, four at a time, and checks each one.

### What the console taught (it took a day of probes)

The EverDrive is not a RAM cart in one respect: **with its write-enable bit on, some of the 68000's writes to
other addresses** - work RAM, the VDP, the Sega CD's gate array, which a boot stack writes all the time -
**land in the cart and change it, and the SH2's reads of the cart come back as zeros meanwhile.** So the
launcher keeps that bit off, and the stack's 68000 listener turns it on only inside a copy into the cart,
with the SH2 held off the cart. Two more: the cart's first 512 KB cannot be written by the 68000 directly
with the 32X on (the copy goes through another window), and copies between Word RAM and the cart go through
a buffer in work RAM. All three are in the CD32X BootCamp's `docs/12_CARTRIDGE_BOOT.md`, with the probe
ROMs that found them (`edtest/`, `edw0/`).

## How we know the memory works

The CD32X BootCamp's `edtest` ran on a Genesis + 32X + EverDrive PRO (24 Oct 2026): 27 banks (13.5 MB),
written by the 68000 and read back by the 68000 and both SH2s, two full passes (the second with every bit
flipped), **0 bad words**. Photos and details: CD32X BootCamp, `docs/10_EVERDRIVE_TEST.md`. The cartridge
boot itself: the BootCamp disc and OpenLara, on the console, 25 Oct 2026 (`docs/12`).

## The first of its kind

As far as we know, this is the first time a flash cart's own memory has been the RAM cart of a Sega CD +
32X game. It is set up in software on unmodified hardware, and it gives a CD32X game four times the
memory of the cart it replaces.

## Build and play

From the folder that holds `build.sh`, with the CD32X BootCamp checked out next to it (`BOOTCAMP=<folder>`
otherwise) and the usual toolchain settings:

```bash
CART=ED EDCD=1 EDCD_SAVE=1 bash build.sh
```

`EDCD_SAVE=1` (EDSAVE-1) puts the saves on the SD card, see above; without it they go to the cart and
are lost at the next launch.

| It makes | What to do with it |
|---|---|
| `subcpu/CDROMPlayer_cartED_music.cue` + `.bin` | burn it (data + CD audio) |
| `subcpu/EDCD.32X` | copy it to the EverDrive's SD card |

1. Genesis + Sega CD + 32X, the EverDrive PRO in the 32X slot, the disc in the Sega CD.
2. Start `EDCD.32X` from the EverDrive menu (not "Run Mega-CD").

| You see | Meaning |
|---|---|
| The boot splash, then OpenLara | It works |
| Cyan | the 32X did not start the SH2 loader |
| Blue | no Sega CD found |
| Magenta | no Sub-CPU BIOS found in the Sega CD's ROM |
| Red / yellow | the Sega CD's processor did not start / did not hand over |

`EDCD.32X` belongs to this disc: it carries this disc's Sub-CPU program. Rebuild it when the Sub-CPU side
changes; the disc stays. The disc also boots the normal way with a RAM cart (4 MB layout; the saves then
go to that cart, as the game asks the Sub-CPU where they are).

**How the SD-card saves work (EDSAVE-1):** the SH2 cannot see bank 31, so every read of the save area
goes through the Sub-CPU (CMD 62 brings a 4 KB piece into its buffer, CMD 57 hands out the words);
writes went through it already (CMD 58 + 61). The passport asks every listed slot for its level on
every frame, so those answers are read once and kept (a save forgets them) - without that the Save
page, 16 slots long, brought the game to a crawl on the console. And for every command whose 68000
side holds RV = 1 (a level chunk, CMD 46; the save bank, CMD 61/62) **both SH2s wait in a loop that
lives in SDRAM with their interrupts masked** (EDSAVE-4): the slave is sent there first (`MARS_CMD_PARK`,
acknowledged), the master follows, and nothing touches the cart until the Sub-CPU reports the copy
done. Measured on the console (`edsave3`, 26 Oct 2026): on the EverDrive an SH2's cart access during
RV = 1 is not held until RV = 0, as the 32X manual implies and the stack assumed - it is answered at
once with garbage (the slave read ~100% bad words through every such copy, the master too). Any code
fetch in those milliseconds kills the SH2 a moment later: that was the passport freeze, which came in
the game (cold cache) and not at the title (the poll loop cached), with a PSRAM bank and with window 7
just the same. Confirmed on the console 26 Oct 2026: save, load, relaunch, load. The cartridge-boot listener reaches the bank as cart
`0x800000`-`0x83FFFF` through window 1 (hw_md.s MODE1-7), the way it reaches window 0. The launcher's
header declares save RAM (`RA`), which is what makes the EverDrive OS keep `bram.srm`. Measured with
the CD32X BootCamp's `edsave/EDSAVE1.32X`: three menu round trips, banks 0 and 7 blank each time,
bank 31 intact, `bram.srm` on the card.

**The cart test:** on the title, Graphics (the sunglasses) → Cart Test → A. Expected: `FOUND 16MB`,
`PAGE 1 B04-07 OK` … `PAGE 6 B24-27 OK`, `SAVE AREA UNCHANGED`. Then A loads a level into every page.
More: `docs/CART_TEST.md`.

## Credits

- **krikzz:** the Mega EverDrive PRO and its extended SSF mapper.
- **MicroNut99:** OpenLara CD32X, the CD32X boot stack, the console tests.
