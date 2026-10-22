# OBJECTS-1: every TR1 trap and moving object

The fixed-point engine (XProger's GBA port) had the doors, switches, pickups, push blocks and
enemies, but none of TR1's traps and moving objects: they stood still or were missing. OBJECTS-1
ports them all from the original game's behaviour. `engine/openlara-objects.patch`, applied by
`build.sh` after the SFX patch. Main new file: `src/fixed/traps.h`.

## What was added
| Where | What |
|---|---|
| Traps | rolling ball (runs Lara over: boulder death), spikes (damage when moving, death when falling on them), teeth trap, falling ceiling, Damocles sword (falls near Lara, spins), Thor's hammer (crushes; its head raises the floor), lightning, lava wedge, Midas hand (gold death; the lead bar turns into the gold bar), flame emitters and fire (Lara catches fire; lava floors burn her), lava fountain |
| Moving things | rolling block (the floor follows it), drawbridge, the Natla's Mines cabin (three levers, then the room flips) and boat, earthquake |
| Creatures | mutant eggs hatch their mutant / centaur / Torso, the centaur statue releases its centaur, the Scion holder and target (Atlantis end) |
| Projectiles | the mutants', centaurs' and Natla's shots fly and can miss (`Enemy::fire`); levels without the projectile model keep the instant shot |
| Rooms | **flipmaps**: the trigger commands FLIP / FLIP_ON / FLIP_OFF swap each room with its other version (flooded rooms, the cabin's fall, the Thor room, …) |
| Effects | the trigger and animation effects: camera shake (`FLOOR_SHAKE`), earthquake, flood and stair sounds, explosions, Lara turning, the hands-free and level-end effects |
| Saving | the save keeps the flipmaps and every item made during play (a trailer after the items: flipmaps, item count, `OX`); older saves still load |
| Also | the skater is drawn with his skateboard; `soundStop` works on the CD32X; a room never overruns the face list |

## Fixes after the review (Fable)
- A trap or object that starts hidden (bit 8 of its level flags) is shown and started by its trigger,
  as TR1 does for every object that is not an enemy. Before, falling ceilings and similar traps stayed
  hidden: no drawing, no collision, no damage.
- Lara burns only on the lava itself, not on a bridge or a collapsing tile over it.
- A rolling block keeps its raised floor after a load (an explicit flag, saved with the item).
- The cabin clears its one-shot flag, as TR1 does, so every lever works.
- Push blocks that start hidden (the Thor room's falling block) appear when triggered.
- Closed doors stay closed in both versions of a flipmap room.
- The changeable copies of room floors can no longer run past their buffer.
- An item that removes another one during the same frame (a flame emitter its flame) is safe.
- At most 8 lava particles at once (Atlantis uses almost all 256 item slots).
- The save never runs past its buffer; the save area keeps the saved game in pieces 0-2.
- A hatched enemy starts in its standing animation (the mutant stood still for good).

## Tested in emulation
Each object was tested in its level with a test build (Lara put next to the object, the object
triggered; health and the object's state read every frame; pictures every second).

| Object | Level | Seen in emulation |
|---|---|---|
| Rolling ball | Tomb of Qualopec | rolls, runs Lara over (boulder death) |
| Spikes | Tomb of Qualopec | 15 damage per step while running over them |
| Falling ceiling | Tomb of Qualopec | appears when triggered, falls, kills Lara standing under it |
| Rolling block | Tomb of Qualopec | rolls two blocks and stops |
| Teeth trap | Palace Midas | snaps shut, 400 damage per bite |
| Midas hand | Palace Midas | Lara standing on it dies (the gold death) |
| Flame emitter | Palace Midas | the fire appears; burning damage not reached with the test's placement |
| Damocles sword | St. Francis' Folly | falls when Lara is near, 100 damage per hit |
| Thor's hammer | St. Francis' Folly | strikes and kills Lara in its path |
| Lightning | St. Francis' Folly | strikes for 400, kills after a few strikes |
| Centaur statue | Tomb of Tihocan | breaks open, the centaur attacks |
| Drawbridge | Obelisk of Khamoon | lowers |
| Cabin | Natla's Mines | one lever: one step down |
| Boat | Natla's Mines | moves along its path |
| Lava fountain | Natla's Mines | particles fly; Lara on the lava catches fire and dies |
| Mutant egg | Atlantis | hatches (explosion); the mutant comes out and runs at Lara |
| Lava wedge | Atlantis | slides and kills Lara |
| Scion | Atlantis | shot five times, explodes and is gone |
| Lara's double | Atlantis | mirrors Lara through the room centre, step for step |
| SAVE-2 | – | records written to save piece 14, piece 0 untouched; the next boot resumes the level from the cart (level ready at frame 2803 instead of 5163) |
| Every level | Caves … the Great Pyramid (15 levels) | each loads from the CD into the cart, Lara stands in it, the picture is right, no crash |

The test hook (`testobj.inc`, `CD32X_TESTOBJ`) is not part of the patch.
