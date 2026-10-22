# Engine changes

`openlara-cd32x.patch` = all CD32X changes to OpenLara (XProger), made against
upstream commit `3b268ca` (https://github.com/XProger/OpenLara). Source files only.

From the root of this repository:

```bash
git clone https://github.com/XProger/OpenLara OpenLara-src
cd OpenLara-src
git checkout -b cd32x 3b268ca
git apply ../engine/openlara-cd32x.patch
```

`OpenLara-src` then sits next to `OpenLaraCD32X/` (build.sh expects `../OpenLara-src`).
The CD32X changes are switched on by `make CD32X=1` - the normal 32X cartridge build is unchanged.

Files changed:
```
 src/fixed/camera.h            |   17 +
 src/fixed/common.h            |   12 +
 src/fixed/enemy.h             | 1758 ++++++++++++++++++++++++++++++++++++-----
 src/fixed/inventory.h         |   70 +-
 src/fixed/item.h              |    2 +-
 src/fixed/lang/en.h           |   12 +
 src/fixed/lara.h              |    5 +
 src/platform/32x/32x.h        |   59 ++
 src/platform/32x/Makefile     |   17 +-
 src/platform/32x/asm/data.s   |   29 +-
 src/platform/32x/main.cpp     |  800 ++++++++++++++++++-
 src/platform/32x/rasterizer.h |   27 +-
 src/platform/32x/render.cpp   |  566 +++++++++++++
 src/platform/32x/sound.cpp    |  154 ++++
 14 files changed, 3316 insertions(+), 212 deletions(-)
```

New files (not in the stat above): `src/platform/32x/fmv.inc`, `roq.h`, `roq_read.c`.

Later patches, applied in this order by `OpenLaraCD32X/build.sh` (each only once): `openlara-cart-test.patch`
(+ `-1to2`), `openlara-opt1.patch`, `openlara-opt2.patch`, `openlara-sfx.patch`, `openlara-objects.patch`
(OBJECTS-1, `docs/OBJECTS.md`).
