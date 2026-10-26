ifdef $(GENDEV)
ROOTDIR = $(GENDEV)
else
ROOTDIR = /opt/toolchains/sega
endif

LDSCRIPTSDIR = $(ROOTDIR)/ldscripts

BOOTBLOCKDIR = $(ROOTDIR)/bootblocks
BOOTBLOCK = $(BOOTBLOCKDIR)/US_BOOT.BIN
#BOOTBLOCK = $(BOOTBLOCKDIR)/EU_BOOT.BIN
#BOOTBLOCK = $(BOOTBLOCKDIR)/JP_BOOT.BIN

LIBPATH = -L$(ROOTDIR)/m68k-elf/lib -L$(ROOTDIR)/m68k-elf/lib/gcc/m68k-elf/4.6.2 -L$(ROOTDIR)/m68k-elf/m68k-elf/lib
INCPATH = -I. -I../../include -I$(ROOTDIR)/m68k-elf/include -I$(ROOTDIR)/m68k-elf/m68k-elf/include

CCFLAGS = -m68000 -Wall -Ofast -c -fomit-frame-pointer
HWFLAGS = -m68000 -Wall -O1 -c -fomit-frame-pointer
# CART-1: make cd CART=8 / CART=16 - the cart test build (CMD 68 in main.c, which is built with
# HWFLAGS).  Without CART: unchanged.
# CART-ED: make cd CART=ED - the same test for the Mega EverDrive PRO (booted with EDBOOT.BIN):
# CMD 68 writes the EverDrive's CTRL4-7 instead of the 16MB-PCB's page register.
ifdef CART
ifeq ($(CART),ED)
CCFLAGS += -DOL_CART_MB=16 -DOL_CART_ED=1
HWFLAGS += -DOL_CART_MB=16 -DOL_CART_ED=1
else
CCFLAGS += -DOL_CART_MB=$(CART)
HWFLAGS += -DOL_CART_MB=$(CART)
endif
endif
# STREAM-1: the drive read-ahead for CMD 46 / 47 (main.c, OL_STREAM).  Default on; make cd STREAM=0
# (or STREAM=0 bash build.sh) builds main.c exactly as before.
STREAM ?= 1
HWFLAGS += -DOL_STREAM=$(STREAM)
# SLOTS-1: CMD 61 also writes OpenLara's save slot area (cart 0x060000-0x07FFFF). Default on;
# make cd SLOTS=0 builds CMD 61 as before (the game then keeps its single save).
SLOTS ?= 1
HWFLAGS += -DOL_SLOTS=$(SLOTS)
# FMV-2: CMD 46 ring mode, the big movies play while they stream from the CD. Default on;
# make cd FMVRING=0 builds CMD 46 as before (the game then plays only the movies that fit).
FMVRING ?= 1
HWFLAGS += -DOL_FMVRING=$(FMVRING)
# CARTW-1: CMD 46 copies the chunks to the cart in words (hw_md.s command 27), almost 3 times faster
# than bytes (command 33).  Default on; make cd CARTW=0 builds the copy as before.
CARTW ?= 1
HWFLAGS += -DOL_CARTW=$(CARTW)
# UB-1: the Unfinished Business level files (EGYPT/CAT/END/END2.PKD) in CMD 38's list. Default on;
# make cd UB=0 builds the list as before.
UB ?= 1
HWFLAGS += -DOL_UB=$(UB)
LDFLAGS = -T $(LDSCRIPTSDIR)/cd.ld -Wl,-Map=output.map -nostdlib
ASFLAGS = -m68000 --register-prefix-optional
# MODE1-1: make CDROMPlayer.bin MODE1=1 - the listener for a cartridge boot (hw_md.s, OL_MODE1), the
# program the CD32X BootCamp's EDCD.32X loads into the Sega CD itself.  Without MODE1: unchanged.
# build.sh EDCD=1 builds it in a copy of this folder; the disc always gets the normal program.
ifeq ($(MODE1),1)
ASFLAGS += --defsym OL_MODE1=1
# MODE1-2: M1WORDS=1 - the cartridge-boot listener writes the cart in words wherever it can
ifeq ($(M1WORDS),1)
ASFLAGS += --defsym OL_M1WORDS=1
endif
# MODE1-5/6: M1W0BANK=<n> the EverDrive bank window 0 shows (EDCD_W0); M1CTRL0=<v> the launcher's
# CTRL0 without the write-enable bit (0x8000, or 0xC000 with the X bit): the listener turns W on
# only inside a copy into the cart (console, 25 Oct 2026: with W on, the 68000's other writes
# corrupt the cart and the SH2's reads of it)
ifneq ($(M1W0BANK),)
ASFLAGS += --defsym OL_M1W0BANK=$(M1W0BANK)
endif
ifneq ($(M1CTRL0),)
ASFLAGS += --defsym OL_M1CTRL0=$(M1CTRL0)
endif
# MODE1-7 / EDSAVE-1: M1SAVE=1 the listener reaches the EverDrive's save bank (cart 0x800000 = bank 31,
# kept on the SD card); EDSAVE=1 the Sub-CPU keeps OpenLara's settings and save slots there (CMD 61/62)
ifeq ($(M1SAVE),1)
ASFLAGS += --defsym OL_M1SAVE=1
endif
ifeq ($(EDSAVE),1)
HWFLAGS += -DOL_EDSAVE=1
endif
# EDSAVE-D: DIAG=1 - the green diagnostic text on (Genesis display stays on), the save commands traced on row 21
ifeq ($(DIAG),1)
HWFLAGS += -DKOBO_DIAG_TEXT=1 -DKOBO_BOOT_TEXT=1
ASFLAGS += --defsym OL_DISPON=1
endif
ifeq ($(NOVERIFY),1)
HWFLAGS += -DOL_EDSAVE_NOVERIFY=1
endif
endif

PREFIX = $(ROOTDIR)/m68k-elf/bin/m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJC = $(PREFIX)objcopy

ASMZ80 = $(ROOTDIR)/bin/zasm
FLAGSZ80 = -vb2

DD = dd
RM = rm -rf

TARGET = CDROMPlayer
# ---------------------------------------------------------------------------------------------
# KOBO: the SH2 program is built in ../sh2 as D32XR.32x.  It is copied (renamed) to
# ROMS/$(SH2_PAYLOAD) on EVERY build, files.s builds it into the program from there (relative
# path - the old files.s pointed at the J: folder, so S: builds used the old SH2 program), and
# it is also put on the disc.  Keep KOBOGFX.BIN in ROMS/ too: "make clean" deletes *.bin here.
# If you rename SH2_PAYLOAD, change the .incbin line in files.s to match.
# ---------------------------------------------------------------------------------------------
SH2_BUILD   = ../sh2/CDLOADER.32x
SH2_PAYLOAD = CDLOADER.BIN
# FMV-1: *.ROQ (the movies) on the disc too
IMAGE_FILES = OPENLARA.32X IMAGE.RAW $(SH2_PAYLOAD) $(notdir $(wildcard ROMS/*.PKD)) $(notdir $(wildcard ROMS/SFX*.BIN)) $(notdir $(wildcard ROMS/*.ROQ)) $(notdir $(wildcard ROMS/IMAGE[0-9]*.RAW))      # D32X M1: the CD-boot-patched D32XR ROM image      # IMAGE.RAW = splash (KOBO_SPLASH 1); if missing: harmless note
LIBS = $(LIBPATH) -lc -lgcc -lnosys
OBJS = crt0.o main.o hw_md.o scd.o scd_fs.o cd.o kos.o cdfs.o sub_stubs.o
FILES = files.o

# "cd" is also the name of the temp folder, so make would otherwise say "cd is up to date" and skip the copy step
.PHONY: cd clean FORCE

FORCE:

# always refresh the renamed SH2 program (FORCE), then rebuild files.o from it
ROMS/$(SH2_PAYLOAD): FORCE
	@test -f $(SH2_BUILD) || { echo "*** $(SH2_BUILD) missing - build ../d32xr-master (Makefile.cdloader) first ***"; exit 1; }
	mkdir -p ROMS
	cp -v $(SH2_BUILD) ROMS/$(SH2_PAYLOAD)

files.o: files.s ROMS/$(SH2_PAYLOAD)
	$(AS) $(ASFLAGS) $(INCPATH) files.s -o files.o

all: src-cd32x/SRAMTest32X.bin $(TARGET).bin

src-cd32x/SRAMTest32X.bin:
	make -C src-cd32x

$(TARGET).bin: $(TARGET).elf
	$(OBJC) -O binary $< temp.bin
	$(DD) if=temp.bin of=$@ bs=2048 conv=sync

$(TARGET).elf: $(OBJS) $(FILES)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) $(FILES) -o $(TARGET).elf

%.o80: %.s80
	$(ASMZ80) $(FLAGSZ80) -o $@ $<

main.o: main.c
	$(CC) $(HWFLAGS) $(INCPATH) $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $(INCPATH) $< -o $@

crt.elf: crt.o sub_stubs.o
	$(LD) -Ttext 0x6000 crt.o sub_stubs.o -o crt.elf

crt.bin: crt.elf
	$(OBJC) -O binary crt.elf crt.bin

cd.o: cd.s crt.bin
	$(AS) $(ASFLAGS) $(INCPATH) cd.s -o cd.o

cd: $(TARGET).bin
	rm -rf cd
	mkdir -p cd
	cp $(TARGET).bin cd/APP.BIN
	@# K10: ROMS/ FIRST.  "make clean" only removes lowercase *.bin, so an old Demo/KOBOGFX.BIN
	@# survived and was put on the disc instead of ROMS/KOBOGFX.BIN.
	for f in $(IMAGE_FILES); do if [ -f ROMS/$$f ]; then cp -v ROMS/$$f cd/$$f; elif [ -f $$f ]; then cp -v $$f cd/$$f; echo "note: $$f taken from Demo/, not ROMS/"; else echo "note: $$f not found in ROMS/ or this folder"; fi; done
	@echo "cd folder now holds:"; ls -1 cd
	genisoimage -sysid "SEGA SEGACD" -volid "SRAMTEST" -generic-boot $(BOOTBLOCK) -full-iso9660-filenames -o $(TARGET).iso cd

clean:
	if [ -d src-cd32x ]; then make clean -C src-cd32x; fi
	$(RM) *.o *.o80 *.bin *.elf *.map *.log *.iso cd