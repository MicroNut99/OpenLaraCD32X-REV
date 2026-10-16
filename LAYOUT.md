# Release layout (mirrors the sega/ folder of the original build machine)

| Folder | What |
|---|---|
| `OpenLaraCD32X/` | the CD32X project: build.sh, SH2 loader (`sh2/`), Sub-CPU + 68000 (`subcpu/`), tools, level converter source |
| `Demo/`, `d32xr-master/` | the ORIGINAL CD32X framework files the project was made from (unmodified, only what their builds need) |
| `include/` (if present) | shared headers the Makefiles include |
| `engine/` | the OpenLara engine changes as a patch (clone upstream next to `OpenLaraCD32X/` as `OpenLara-src`) |
| `docs/`, `licenses/` | handoff document, licences |
