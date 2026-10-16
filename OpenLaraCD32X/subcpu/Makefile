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
LDFLAGS = -T $(LDSCRIPTSDIR)/cd.ld -Wl,-Map=output.map -nostdlib
ASFLAGS = -m68000 --register-prefix-optional

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
IMAGE_FILES = OPENLARA.32X IMAGE.RAW $(SH2_PAYLOAD) $(notdir $(wildcard ROMS/*.PKD)) $(notdir $(wildcard ROMS/SFX*.BIN)) $(notdir $(wildcard ROMS/*.ROQ))      # D32X M1: the CD-boot-patched D32XR ROM image      # IMAGE.RAW = splash (KOBO_SPLASH 1); if missing: harmless note
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