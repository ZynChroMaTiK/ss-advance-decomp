# Native world geometry composition

`world_scene.c` connects the recovered BSP visibility, full solid/portal wall
renderer and floor/ceiling plane renderer. It draws the actual indexed world
pixels from the user's ROM; runtime code executes no ROM instructions.

Initialization owns a stable plane pool, visibility scratch,120 column clips,
wall results,24,000 opening bytes and a framebuffer. It borrows map bindings,
math/renderer tables and decoded sky assets. No native pointers are encoded as
GBA addresses. Borrowed map layout/assets stay alive and stable until free;
sector heights, line flags and switch textures are read live by the components.

The ordinary world clear writes14,400 pixels. An explicit120byte readable prefix
precedes them because original ARM one-pixel columns at row0 can sample the
preceding row. Initialization zeros owned storage; clear preserves the prefix.
This retains a defined original read without pointer underflow or invented wrap.

Drawing resets column clipping, constructs B6B0's front-to-back wall list and
the plane queue, resets secondary coverage through00A84, then follows017CC's
head/next order. Solid flagged records update00E3C coverage before BE60 draws.
Visibility range records are copied into wall command types, avoiding C aliasing
assumptions. BE60's plane splits update working result handles; queued wall
plane references remain unchanged, as in the original. AB10 draws planes last.
Native bounds/queue/reentry errors preserve prior effects and clear the guard.

Secondary coverage preserves00E3C's original compaction copying a spare word at
its old exclusive end. Consumers must stop at the first upper sentinel instead
of treating all retained trailing words as intervals. `test-scene-coverage`
passes7,893 whole00A84 resets and40,268 whole00E3C merges, comparing all live and
spare bytes, plus10 malformed guards.

`test-world-scene-rom` runs original B6B0 ->017CC ->fullBE60 ->AB10/AA4C/A98C and
the actual relocated ARM raster instructions. Only BIOS/DMA services and the
two subsequent D2D0/01828 actor passes are controlled. All128 complete geometry
frames on all16 real maps pass:1,843,200 pixel comparisons,1,280 ordered walls
(532 portals),754 planes and19,259 original ARM columns. Framebuffer prefix,
120 column clips,80 plane records, openings/cursor and plane globals also agree.
Selected headings/eye heights and live ceiling changes are covered. This is
whole world-geometry proof, not proof of the excluded actor passes.

`test-world-scene` separately runs native PC/game fixtures, native jump/H camera
poses and full world geometry over2,048 frames across all16 maps with ASan/UBSan.
The test writes original-palette PNG previews under ignored `build/native-tests/`.
The previews were visually inspected and show actual level walls, ground and
sky. They contain private ROM-derived artwork and are not published assets.

The interactive Windows geometry viewer uses the same native movement/collision,
jump/camera and rendering APIs directly. It does not run dummy full-game stages.
Actor rendering, the third-person player body, HUD/weapons, audio, complete enemy
controllers and full game startup remain required for the requested game port.
