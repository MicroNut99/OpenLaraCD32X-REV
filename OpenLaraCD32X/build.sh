#!/bin/bash
# OpenLara CD32X - full HARDWARE build (milestone OL-1):
#   engine (OpenLara-src, CD32X=1) -> ROM prep -> SH2 loader -> Sub-CPU/68K -> disc image
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ENGINE="$HERE/../OpenLara-src/src/platform/32x"
MARKER="SH2-OPENLARA-CD-OL1-LOADER"

grep -q '^#define CDL_TARGET_OPENLARA 1' "$HERE/sh2/cdloader_sh2.c" \
  || { echo "*** sh2/cdloader_sh2.c is not in OpenLara mode - stopping"; exit 1; }
[ -f "$ENGINE/main.cpp" ] || { echo "*** engine not found at $ENGINE - stopping"; exit 1; }
if ! grep -q 'CD32X' "$ENGINE/main.cpp"; then
  echo "==> installing engine/main.cpp + Makefile into OpenLara-src (git keeps the old ones)"
  cp "$HERE/engine/main.cpp" "$HERE/engine/Makefile" "$ENGINE/"
fi

echo "==> splash";    python3 "$HERE/tools/make_ol_splash.py" "$HERE/splash.jpg" "$ENGINE/data/SPLASH.BIN"
python3 "$HERE/tools/make_boot_image.py" "$ENGINE/data/SPLASH.BIN" "$HERE/subcpu/ROMS/IMAGE.RAW"
echo "==> engine";      cd "$ENGINE" && rm -rf build && make CD32X=1 ${PROF:+PROF=1}
echo "==> ROM prep";    python3 "$HERE/tools/prep_openlara_cd.py" "$ENGINE/OpenLara.32x" "$HERE/subcpu/ROMS/OPENLARA.32X"
echo "==> levels";      N=$(ls "$HERE"/gba/packer/out32x/*.PKD 2>/dev/null | wc -l); [ "$N" -ge 21 ] || { echo "*** only $N levels in gba/packer/out32x (21 expected) - stopping"; exit 1; }; cp "$HERE"/gba/packer/out32x/*.PKD "$HERE/subcpu/ROMS/"
echo "==> sound packs"; rm -f "$HERE"/subcpu/ROMS/SFX*.BIN; python3 "$HERE/tools/make_sfx_packs.py" "$HERE/gba/packer/out32x" "$HERE/subcpu/ROMS"
echo "==> movies";      rm -f "$HERE"/subcpu/ROMS/*.ROQ   # FMV-1: movies that fit the level slot (1984 KB)
for f in "$HERE"/fmv/*.ROQ; do
  if [ -f "$f" ] && [ "$(stat -c %s "$f")" -le 2031616 ]; then cp "$f" "$HERE/subcpu/ROMS/"; echo "    $(basename "$f")"; fi
done
echo "==> SH2 loader";  cd "$HERE/sh2"    && make -f Makefile.cdloader clean && make -f Makefile.cdloader
echo "==> Sub-CPU";     cd "$HERE/subcpu" && make clean && make cd

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
SZ=$(stat -c %s "$HERE/subcpu/cd/OPENLARA.32X"); [ "$SZ" -lt 524288 ] || { echo "*** OPENLARA.32X is $SZ bytes - over 512 KB, it would overwrite Caves in the cart - stopping"; exit 1; }
SZ=$(stat -c %s "$HERE/subcpu/cd/LEVEL1.PKD");   [ "$SZ" -le 1572864 ] || { echo "*** LEVEL1.PKD is $SZ bytes - over 1.5 MB, it would reach the level slot - stopping"; exit 1; }
echo "==> disc checked: OpenLara loader + OpenLara ROM"
echo "==> CD audio";  python3 "$HERE/tools/make_music_tracks.py" "$HERE/music_map.txt" "$HERE/soundtrack_wav" "$HERE/soundtrack_cd"
cd "$HERE/subcpu" && python3 make_mixed_cd2.py CDROMPlayer.iso $(ls "$HERE"/soundtrack_cd/Track*.wav | sort) --out CDROMPlayer_music
echo "==> BURN THIS: $HERE/subcpu/CDROMPlayer_music.cue  (data + music)"
echo "==> done (the .iso alone has no music - burn the _music.cue above)"
