# v1.6: the missing content

What TR1 had and the CD32X release did not, added in v1.6. Engine: `engine/openlara-content.patch`
(applied by `build.sh` after the objects patch). Sub-CPU: `subcpu/main.c`, each change behind its
own switch (below). No change to `hw_md.s`, the boot block, CDLOADER, `crt0.s` or `src-md/`.

## Save slots in the passport (SLOTS-1)
- **Save:** the passport's save page lists **16 slots**, each with the level saved in it (or `-`).
  The last used slot is picked first. (The Gym has no save page, as before.)
- **Load:** the load page lists the saved slots first, then the levels, cut-scenes and movies.
- The old single save (v1.5 and before) stays loadable as its own entry.
- **Where:** cart `0x060000`-`0x07FFFF` (between the game and Caves), 8 KB a slot, battery-backed
  like the rest of the cart. The game's ROM must end before it: `build.sh` stops if
  `OPENLARA.32X` is over 384 KB (it is about 270 KB).
- **Sub-CPU:** CMD 61 with COMM2 bit 8 writes a piece of the slot area; the reply is `0x101`
  instead of `1`. An older Sub-CPU (or `SLOTS=0`) replies `1`: the game sees that at start and keeps
  the single save.

## Every movie, at its place in the story (FMV-2)
- **All eleven movies play.** The four small ones (CORE, MANSION, PYRAMID, ESCAPE) load whole as
  before. The seven big ones (CAFE, SNOW, LIFT, VISION, CANYON, PRISON, END: 5–20 MB) play
  **while they stream from the CD**, at the full 15 pictures a second.
- **How:** the cart's level slot becomes a ring of 60 × 32 KB plus one spare slot (a copy of the
  first, so a RoQ chunk across the ring's end stays in one piece). CMD 46 in ring mode (COMM2
  bit 14; bit 13 = the spare slot) writes file chunk *c* to slot *c* % 60. The master keeps the CD
  busy without waiting for it and never lets it overwrite what the picture decoder or the sound
  still needs. Before the movie starts, up to 58 chunks (1.8 MB) load with the red bar (about 16 s).
- **The sound is the clock.** The slave plays the sound from a ring of 8 PWM blocks (115 ms) and
  fills it only while the 68000 is not writing the cart (the Sub-CPU sets COMM12 bit 15 around
  each copy), so RV never holds it while a block runs out. Each picture is shown when the sound
  has reached its time, so picture and sound stay together even when the CD is late.
- **At their story places** (as the PC game): CAFE + SNOW when a new game starts (Caves from the
  title), MANSION before Lara's Home, LIFT before St. Francis' Folly, VISION before the City of
  Khamoon, CANYON before Natla's Mines, PYRAMID before Atlantis, PRISON after Atlantis, END after
  the Great Pyramid - then the game goes back to the title. Picking a level in the passport plays
  no movie. All eleven are also in the title passport, in story order.
- A, B, C or Start stops a movie (hold it a moment: while a chunk loads the pad is read between
  chunks). If the CD gives no data for 10 s, the movie ends.
- **Sub-CPU:** `FMVRING=0` builds CMD 46 as before; the game then sees no ring-mode reply
  (`0x100`) and plays only the movies that fit, as in v1.5.

## Faster cart copies (CARTW-1)
CMD 46 copies each chunk to the cart with the 68000's word copy (`hw_md.s` command 27,
15 clocks a byte) instead of the byte copy (command 33, 42 clocks a byte), in 4 KB pieces
(the 68000's interrupts are masked for about 8 ms per piece). The SH2s are locked out of the cart
(RV) for a third of the time, which streaming needs; levels load a little faster too.
`CARTW=0` builds the byte copy as before.

## Creatures
| What | Before | Now |
|---|---|---|
| Winged mutants (Khamoon, Atlantis, the Great Pyramid) | the level's placeholder model (one mesh), stood still | the mutant's model; the winged ones fly (their own height, navigation zones for flyers), the others walk and shoot |
| Gorilla (Tomb of Tihocan) | could not climb | climbs one-block steps |
| Rats (Vilcabamba, Cistern) | one kind only | the swimming rat and the land rat (vole) change into each other at the water's edge |
| Lara's double (Atlantis) | disappeared after the fall | falls into the pit, dies there and stays, as in TR1 |
| Hatched mutants (eggs) | stood still after hatching | start in their standing animation and attack |

## Story cues and voices (CUES-1)
The levels ask for music cues (game tracks 6–21) and voice lines (26–56: Lara's Home hints, Natla,
Larson, Pierre, the cowboy, the skater). The PC CD has none of them; the PlayStation soundtrack set
has all (`track06.flac` … `track56.flac`, numbered by game track):

    PS1MUSIC="<folder with track06.flac ...>" bash build.sh

converts them once (`tools/import_tr1_music.py`, ffmpeg) into `soundtrack_wav/` and adds them to
`music_map.txt` (lines you wrote stay). About 14 minutes more CD audio. The engine plays only the
tracks that are really on the disc (`build.sh` passes them as `CD32X_TRACKS_LO/HI`), so a missing
cue never interrupts the level music with silence.

## Fixes after the review (Fable)
- An old Sub-CPU could pass the ring-mode probe when the slot still held an earlier movie: the
  probe now needs the Sub-CPU's ring-mode reply.
- The END movie played on "Exit to title" in the Great Pyramid: now only when the level is finished.
- After the Great Pyramid the game loaded Caves under the next level's number: it goes to the title.
- A CD stall no longer puts the pictures ahead of the sound; a CD that stops ends the movie (10 s).
- A RoQ chunk longer than the spare slot ends the movie instead of reading past it.
- Lara's double stops after 3 s in the pit even if her animation never reaches its end.

## Tested in emulation
| What | Seen |
|---|---|
| CANYON (20 MB, 2715 pictures, 3 min) streamed | 2704 pictures at **15.0 a second**, sound at 22 060 samples a second (real time), the CD never late, 2 held sound blocks (at the start) |
| Before the sound ring | pictures and sound both about 10 % slow while streaming (the slave held by RV) |
| A small movie (MANSION, loaded whole) | 15.0 pictures a second, sound real time |
| Story places | the end of cut-scene 2 → VISION → the City of Khamoon; the end of the Great Pyramid → END → the title |
| Save slots | save to a slot, the slot list with level names, load it back; with the v1.5 Sub-CPU: the single save |
| Creatures | the winged mutant flies, the ground mutant in Khamoon runs at Lara, the vole on land, the gorilla's AI |
| Sub-CPU | with every switch at 0 its code is identical to v1.5's |
| Not run | the cues and voices (the test discs have no audio tracks), the gorilla's climb, the double's fall |
