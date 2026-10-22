#!/bin/bash
# make_fmv.sh - the game's movies (.RPL, PC version's FMV folder) -> RoQ for the CD32X player.
#
#   bash tools/make_fmv.sh "<your Tomb Raider 1>/FMV" fmv
#
# 288 x 112 (288 x 108 picture + 2 black rows top and bottom; the player shows every line
# twice = 288 x 224), 15 frames/s, RoQ video + RoQ DPCM sound, 22050 Hz mono.  Needs ffmpeg.
# build.sh puts every fmv/*.ROQ on the disc.  CORE, MANSION, PYRAMID and ESCAPE fit the RAM
# cart's level slot (up to 1984 KB) and load whole; the seven big ones (CAFE, SNOW, LIFT, VISION,
# CANYON, PRISON, END) play while they stream from the CD (FMV-2; FMVRING=0: not played).
set -e
SRC="$1"; OUT="${2:-fmv}"
[ -d "$SRC" ] || { echo "usage: bash tools/make_fmv.sh <FMV folder with *.RPL> [out folder]"; exit 1; }
mkdir -p "$OUT"
for f in "$SRC"/*.RPL "$SRC"/*.rpl; do
  [ -f "$f" ] || continue
  n=$(basename "${f%.*}" | tr a-z A-Z)
  echo "==> $n"
  ffmpeg -v error -y -i "$f" -vf "scale=288:108:flags=lanczos,pad=288:112:0:2" -r 15 \
         -c:v roqvideo -c:a roq_dpcm -ar 22050 -ac 1 "$OUT/$n.ROQ"
done
ls -l "$OUT"/*.ROQ
