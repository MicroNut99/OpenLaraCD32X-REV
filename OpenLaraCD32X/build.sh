#!/bin/bash
# OpenLara CD32X - full HARDWARE build (milestone OL-1):
#   engine (OpenLara-src, CD32X=1) -> ROM prep -> SH2 loader -> Sub-CPU/68K -> disc image
#
# CART-1 test builds for the 8 / 16 MB SRAM cart (16MB-PCB-CD32X):
#   CART=8 bash build.sh     the 8 MB cart test    -> subcpu/CDROMPlayer_cart8_music.cue
#   CART=16 bash build.sh    the 16 MB cart test   -> subcpu/CDROMPlayer_cart16_music.cue
#   CART=ED bash build.sh    the same test for a Mega EverDrive PRO as the RAM cart (CART-ED)
#                            -> subcpu/CDROMPlayer_cartED_music.cue; with EDCD=1 below
#   EDCD=1 bash build.sh     also subcpu/EDCD.32X: the disc started from the Mega EverDrive PRO, which stays
#                            (EDCD_X=1: CTRL0 with the EverDrive's X bit; EDCD_W0=<bank>: the game in that bank)
#   EDCD_DIAG=1              with EDCD=1: EDCD.32X with the Sub-CPU's green diagnostic text on (the disc is the
#                            same; only the launcher differs): boot steps, heartbeat, the save commands traced
#   EDCD_SAVE=1              with EDCD=1: the saves on the SD card (EDSAVE-1) - the settings and the 16 save
#                            slots in the EverDrive's bank 31, which its OS keeps as bram.srm; the launcher's
#                            header declares save RAM.  Needed: the EverDrive wipes its PSRAM (banks 0-29,
#                            where the saves were) every time its menu runs.
#                            the cartridge (MODE1-1; the builder is in the CD32X BootCamp: BOOTCAMP=<folder>,
#                            default ../CD32X-BootCamp; its docs/12).  The disc still boots the normal way.
#                            For the EverDrive: CART=ED EDCD=1 bash build.sh
# The test is in the title's options (Detail -> Cart Test), then the game.  Without CART: unchanged.
# Sub-CPU switches (all on by default):
#   STREAM=0 bash build.sh   the Sub-CPU without the drive read-ahead (STREAM-1)
#   SLOTS=0 bash build.sh    the Sub-CPU without the save slot area (SLOTS-1): one save
#   FMVRING=0 bash build.sh  no movie streaming (FMV-2): only the movies that fit the cart
#   CARTW=0 bash build.sh    the Sub-CPU copies to the cart in bytes, as before (CARTW-1)
#   UB=0 bash build.sh       the Sub-CPU without the Unfinished Business level files (UB-1)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
CART="${CART:-}"
case "$CART" in ""|8|16|ED) ;; *) echo "*** CART must be 8, 16 or ED (or not set) - stopping"; exit 1;; esac
ENGINE="$HERE/../OpenLara-src/src/platform/32x"
MARKER="SH2-OPENLARA-CD-OL1-LOADER"

grep -q '^#define CDL_TARGET_OPENLARA 1' "$HERE/sh2/cdloader_sh2.c" \
  || { echo "*** sh2/cdloader_sh2.c is not in OpenLara mode - stopping"; exit 1; }
[ -f "$ENGINE/main.cpp" ] || { echo "*** engine not found at $ENGINE - stopping"; exit 1; }
if ! grep -q 'CD32X' "$ENGINE/main.cpp"; then
  echo "==> installing engine/main.cpp + Makefile into OpenLara-src (git keeps the old ones)"
  cp "$HERE/engine/main.cpp" "$HERE/engine/Makefile" "$ENGINE/"
fi
# the cart test patch is applied to every build (it does nothing without CART=), so the
# speed-up patch below always finds the same engine
if [ ! -f "$ENGINE/carttest.inc" ]; then
  echo "==> applying engine/openlara-cart-test.patch to OpenLara-src (the cart test)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-cart-test.patch"
elif ! grep -q "CART-2" "$ENGINE/carttest.inc"; then
  echo "==> updating the cart test in OpenLara-src (start-up -> options)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-cart-test-1to2.patch"
fi
if ! grep -q "CD32X_LMAP16" "$ENGINE/asm/rasterizeFT.i"; then
  echo "==> applying engine/openlara-opt1.patch to OpenLara-src (OPT-1: faster span loops)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-opt1.patch"
fi
if ! grep -q "OPT-2" "$ENGINE/render.cpp"; then
  echo "==> applying engine/openlara-opt2.patch to OpenLara-src (OPT-2: ordering table skip)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-opt2.patch"
fi
if [ ! -f "$ENGINE/sfx.inc" ]; then
  echo "==> applying engine/openlara-sfx.patch to OpenLara-src (SFX-1/2: all sound effects)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-sfx.patch"
fi
if [ ! -f "$ENGINE/../../fixed/traps.h" ]; then
  echo "==> applying engine/openlara-objects.patch to OpenLara-src (OBJECTS-1: traps and objects)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-objects.patch"
fi
if ! grep -q "SLOTS-1" "$ENGINE/main.cpp"; then
  echo "==> applying engine/openlara-content.patch to OpenLara-src (v1.6: save slots, streamed movies, creatures, cues)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-content.patch"
fi
if ! grep -q "UB-1" "$ENGINE/main.cpp"; then
  echo "==> applying engine/openlara-ub.patch to OpenLara-src (v1.7: Unfinished Business, loading pictures, credits)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-ub.patch"
fi
# CART-ED: the EverDrive PRO cart test (does nothing without CART=ED)
if ! grep -q "CART-ED" "$ENGINE/carttest.inc"; then
  echo "==> applying engine/openlara-cart-ed.patch to OpenLara-src (CART-ED: the EverDrive PRO cart test)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-cart-ed.patch"
fi
# EDSAVE-1: the saves through the Sub-CPU when they are in the EverDrive's save bank.  Applied with
# EDCD_SAVE=1, taken out again without it, so a build without EDCD_SAVE is the game as before (saves in the cart)
if [ "${EDCD_SAVE:-0}" = 1 ] && ! grep -q "EDSAVE-1" "$ENGINE/main.cpp"; then
  echo "==> applying engine/openlara-edsave.patch to OpenLara-src (EDSAVE-1: saves on the EverDrive's SD card)"
  git -C "$ENGINE/../../.." apply "$HERE/../engine/openlara-edsave.patch"
elif [ "${EDCD_SAVE:-0}" != 1 ] && grep -q "EDSAVE-1" "$ENGINE/main.cpp"; then
  echo "==> taking engine/openlara-edsave.patch out of OpenLara-src (no EDCD_SAVE: saves in the cart, as before)"
  git -C "$ENGINE/../../.." apply -R "$HERE/../engine/openlara-edsave.patch"
fi
if [ "$CART" = "ED" ]; then echo "==> CART TEST BUILD: Mega EverDrive PRO (CART-ED)";
elif [ -n "$CART" ]; then echo "==> CART TEST BUILD: ${CART} MB"; fi

echo "==> splash";    python3 "$HERE/tools/make_ol_splash.py" "$HERE/splash.jpg" "$ENGINE/data/SPLASH.BIN"
python3 "$HERE/tools/make_boot_image.py" "$ENGINE/data/SPLASH.BIN" "$HERE/subcpu/ROMS/IMAGE.RAW"
# CUES-1: PS1MUSIC="<folder with track06.flac ...>" bash build.sh adds the game's own cues and
# voice lines (tools/import_tr1_music.py: converts them, adds them to music_map.txt once)
if [ -n "${PS1MUSIC:-}" ]; then echo "==> cues and voices"; python3 "$HERE/tools/import_tr1_music.py" "$PS1MUSIC" "$HERE/soundtrack_wav" "$HERE/music_map.txt"; fi
# CUES-1: the game tracks that have real audio on the disc (music_map.txt), so the engine plays
# the cues and voice lines that are there and ignores the others
TRACKBITS=$(python3 -c "
import re,sys
lo=hi=x=0
for l in open(sys.argv[1]):
    m=re.match(r'\s*(\d+)\s+\S',l.split('#')[0])
    if m:
        t=int(m.group(1))
        if t<32: lo|=1<<t
        elif t<64: hi|=1<<(t-32)
        elif t<96: x|=1<<(t-64)
print('-DCD32X_TRACKS_LO=0x%08Xu -DCD32X_TRACKS_HI=0x%08Xu -DCD32X_TRACKS_X=0x%08Xu'%(lo,hi,x))" "$HERE/music_map.txt")
echo "==> engine";      cd "$ENGINE" && rm -rf build && make CD32X=1 ${PROF:+PROF=1} ${CART:+CART=$CART} EXTRA="${EXTRA:-} $TRACKBITS"
PREP_MODE1=""; if [ "${EDCD:-0}" = 1 ]; then PREP_MODE1="--mode1"; fi   # MODE1-1 (EDCD=1, below)
echo "==> ROM prep";    python3 "$HERE/tools/prep_openlara_cd.py" $PREP_MODE1 "$ENGINE/OpenLara.32x" "$HERE/subcpu/ROMS/OPENLARA.32X"
echo "==> levels";      N=$(ls "$HERE"/gba/packer/out32x/*.PKD 2>/dev/null | wc -l); [ "$N" -ge 21 ] || { echo "*** only $N levels in gba/packer/out32x (21 expected) - stopping"; exit 1; }; cp "$HERE"/gba/packer/out32x/*.PKD "$HERE/subcpu/ROMS/"
# SFX-2: every level's sound effects appended to its .PKD for the 32X PWM mixer (SFX=pcm: not)
if [ "${SFX:-}" != "pcm" ]; then echo "==> sound banks"; python3 "$HERE/tools/sfx_bank.py" "$HERE/gba/packer/out32x" "$HERE/subcpu/ROMS"; fi
echo "==> sound packs"; rm -f "$HERE"/subcpu/ROMS/SFX*.BIN; python3 "$HERE/tools/make_sfx_packs.py" "$HERE/gba/packer/out32x" "$HERE/subcpu/ROMS"
# PICS-1: the area loading pictures (PlayStation DELDATA *LOA.RAW) and the end credits (PC CRED1-3.PCX,
# END.PCX; Unfinished Business CRED0.PCX) -> subcpu/ROMS/IMAGE17-27.RAW.  Folders: pictures/ (put
# the .RAW files and CRED0.PCX there) and the packer's TR1_PC/DATA.  Missing ones are left out.
echo "==> pictures";    python3 "$HERE/tools/make_ol_pictures.py" "${PS1PICS:-$HERE/pictures}" "$HERE/gba/packer/TR1_PC/DATA" "${UBPICS:-$HERE/pictures}" "$HERE/subcpu/ROMS"
echo "==> movies";      rm -f "$HERE"/subcpu/ROMS/*.ROQ   # FMV-2: all of them (the big ones stream); FMVRING=0: those that fit (1984 KB)
for f in "$HERE"/fmv/*.ROQ; do
  if [ -f "$f" ] && { [ "${FMVRING:-1}" != "0" ] || [ "$(stat -c %s "$f")" -le 2031616 ]; }; then cp "$f" "$HERE/subcpu/ROMS/"; echo "    $(basename "$f")"; fi
done
echo "==> SH2 loader";  cd "$HERE/sh2"    && make -f Makefile.cdloader clean && make -f Makefile.cdloader
echo "==> Sub-CPU";     cd "$HERE/subcpu" && make clean && make cd ${CART:+CART=$CART} ${STREAM:+STREAM=$STREAM} ${SLOTS:+SLOTS=$SLOTS} ${FMVRING:+FMVRING=$FMVRING} ${CARTW:+CARTW=$CARTW} ${UB:+UB=$UB}

ISO="$HERE/subcpu/CDROMPlayer.iso"
[ -f "$ISO" ] || { echo "*** $ISO was not made - stopping"; exit 1; }
grep -a -q "$MARKER" "$ISO" || { echo "*** the disc does NOT contain the OpenLara loader - stopping"; exit 1; }
[ -f "$HERE/subcpu/cd/OPENLARA.32X" ] || { echo "*** OPENLARA.32X is not on the disc - stopping"; exit 1; }
[ -f "$HERE/subcpu/cd/IMAGE.RAW" ] || { echo "*** IMAGE.RAW (boot splash) is not on the disc - stopping"; exit 1; }
NPKD=$(ls "$HERE"/subcpu/cd/*.PKD 2>/dev/null | wc -l); [ "$NPKD" -ge 21 ] || { echo "*** only $NPKD levels on the disc (21 expected) - stopping"; exit 1; }
if [ -f "$HERE/subcpu/cd/OPENLARA.32X" ]; then
  NAME="$(dd if="$HERE/subcpu/cd/OPENLARA.32X" bs=1 skip=$((0x3C0)) count=8 2>/dev/null)"
  [ "$NAME" = "OpenLara" ] || { echo "*** subcpu/cd/OPENLARA.32X is not OpenLara ($NAME) - stopping"; exit 1; }
fi
# CAVES-1: the game (cart 0) must end before Caves (0x080000), Caves before the level slot (2 MB)
SZ=$(stat -c %s "$HERE/subcpu/cd/OPENLARA.32X"); [ "$SZ" -le 393216 ] || { echo "*** OPENLARA.32X is $SZ bytes - over 384 KB, it would overwrite the save slots (cart 0x060000) - stopping"; exit 1; }
SZ=$(stat -c %s "$HERE/subcpu/cd/LEVEL1.PKD");   [ "$SZ" -le 1572864 ] || { echo "*** LEVEL1.PKD is $SZ bytes - over 1.5 MB, it would reach the level slot - stopping"; exit 1; }
# CART-1: the 16 MB test on an 8 MB-image board writes cart 0x1FF000 (page 0, piece 15): Caves must end before it
if [ -n "$CART" ]; then SZ=$(stat -c %s "$HERE/subcpu/cd/LEVEL1.PKD"); [ "$SZ" -le 1568768 ] || { echo "*** LEVEL1.PKD is $SZ bytes - the cart test would reach its end - stopping"; exit 1; }; fi
echo "==> disc checked: OpenLara loader + OpenLara ROM"
echo "==> CD audio";  python3 "$HERE/tools/make_music_tracks.py" "$HERE/music_map.txt" "$HERE/soundtrack_wav" "$HERE/soundtrack_cd"
cd "$HERE/subcpu" && python3 make_mixed_cd2.py CDROMPlayer.iso $(ls "$HERE"/soundtrack_cd/Track*.wav | sort) --out "CDROMPlayer${CART:+_cart$CART}_music"
echo "==> BURN THIS: $HERE/subcpu/CDROMPlayer${CART:+_cart$CART}_music.cue  (data + music)"
if [ "$CART" = "ED" ]; then echo "==> this is the EverDrive PRO CART TEST disc: start EDCD.32X (EDCD=1) from the EverDrive menu with it in the Sega CD; the test is in Options -> Detail -> Cart Test";
elif [ -n "$CART" ]; then echo "==> this is the ${CART} MB CART TEST disc: the test is in Options -> Detail -> Cart Test"; fi
echo "==> done (the .iso alone has no music - burn the _music.cue above)"

# EDCD=1 (MODE1-1): EDCD.32X, made by the CD32X BootCamp's edboot/build_edcd.sh around this disc's Sub-CPU
# program built a second time for the cartridge boot (MODE1=1, in a copy of subcpu/: the disc keeps the
# normal program)
if [ "${EDCD:-0}" = 1 ]; then
  BC="${BOOTCAMP:-$HERE/../CD32X-BootCamp}"
  [ -f "$BC/edboot/build_edcd.sh" ] || { echo "*** EDCD=1 needs the CD32X BootCamp next to this folder (or BOOTCAMP=<folder>): not at $BC"; exit 1; }
  echo "==> EDCD.32X: the disc started from the EverDrive PRO (cartridge boot)"
  T=$(mktemp -d); mkdir -p "$T/subcpu/ROMS" "$T/sh2"
  find "$HERE/subcpu" -maxdepth 1 -type f -exec cp {} "$T/subcpu/" \;
  cp "$HERE/sh2/CDLOADER.32x" "$T/sh2/"
  ( cd "$T/subcpu" && make clean > /dev/null && make CDROMPlayer.bin MODE1=1 M1CTRL0=$([ "${EDCD_X:-0}" = 1 ] && echo 0xC000 || echo 0x8000) ${EDCD_W0:+M1W0BANK=$EDCD_W0} ${EDCD_SAVE:+M1SAVE=$EDCD_SAVE EDSAVE=$EDCD_SAVE} ${EDCD_DIAG:+DIAG=$EDCD_DIAG} ${EDCD_NOVERIFY:+NOVERIFY=$EDCD_NOVERIFY} ${M1WORDS:+M1WORDS=$M1WORDS} ${CART:+CART=$CART} ${STREAM:+STREAM=$STREAM} ${SLOTS:+SLOTS=$SLOTS} ${FMVRING:+FMVRING=$FMVRING} ${CARTW:+CARTW=$CARTW} ${UB:+UB=$UB} > "$T/mode1.log" 2>&1 ) \
    || { tail -20 "$T/mode1.log"; echo "*** the MODE1 Sub-CPU program did not build"; exit 1; }
  bash "$BC/edboot/build_edcd.sh" "$T/subcpu/CDROMPlayer.bin" "$HERE/sh2/CDLOADER.32x" "$HERE/subcpu/EDCD.32X"
  rm -rf "$T"
  echo "==> EDCD.32X: $HERE/subcpu/EDCD.32X - copy it to the EverDrive's SD card and start it from the menu"
  echo "    with the disc above in the Sega CD (not \"Run Mega-CD\")"
fi
