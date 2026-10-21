# Optional floor and ceiling decals

## Random dry edge tiles (2026-10-21)

Graphics > Surface decals also adds narrow, porous grime along floor/wall and
ceiling/wall joints. These use the existing surface color and its darker
palette shade, at two world units per texel. Empty sections, single patches,
left/right caps and connected middles are selected deterministically per level;
floors and ceilings have separate layouts and artwork. Default remains Off,
with no save-format change.

The immutable `surface_edge_tiles.inc` atlas stores8KiB of64x16 binary masks:
two surfaces, eight artwork banks, four connection states. Every connected
bank has identical port texels; disconnected ends taper to empty. The offline
`scripts/generate-surface-edge-tiles.py` generator reproduces this atlas.
Runtime selects texels, not generated coverage or deformed shapes.

Boundary queries reuse the existing level-owned shoreline spatial index.
`SsaSurfaceDecals.edge_scene` borrows that stable scene after initialization;
no extra index/allocation or per-frame mutable cache is introduced. Queries
read live floor/ceiling heights, skip open same-height sector joins and avoid
water and sky. World-origin edge coordinates preserve the pattern across
collinear splits and reversed lines. Intersecting strips combine at corners.
GBA plane overlays, native Software (flat/pitched), Vulkan row shading and
reflections all call the same height-aware sampler. Existing stains remain.

Validation:55,562,082 ASan/UBSan/leak and MSVC checks agree, with61 scene draws.
New fixtures cover atlas ports, variant differences, empty sections, surface
separation, wall splitting/reversal, negative coordinates, live heights,
water exclusion and distance fade. All ten focused CTests and6,487,023 Vulkan
checks pass with zero validation errors. The native game was rebuilt; strip
and real scene captures were inspected. No campaign playthrough was run.


Menu organization (2026-10-21): Surface decals, Water reflections and Godrays
are in Graphics, following Mipmapping at rows88,104,120. Back remains at144.
Extras retains its eight other options, one tile per row. All defaults,
saved choices and rendering behavior are unchanged by this move.

Attached shoreline borders use only base water and its lighter hue, with no
white texels. Traveling waves retain their white foam accents separately.

Water tint update (2026-10-21): all water decal families now share a lighter
original-palette water ink. Ripples pair lighter crests with base-water detail, visible without
reflections; shoreline faces use the tint with base-color trailing lips and
sparse white foam. Glints use tinted arms with white centers. The shore atlas
remains128KiB; the two-ink ripple atlas now uses18KiB of immutable ROM data.
Animation timing, surface classification, reflection exclusions and the Graphics
toggle are retained. Validation passes
55,508,027 surface checks with matching MSVC/GCC results and ASan/UBSan/leak
detection, all ten focused tests, and6,076,743 Vulkan checks with no validation
errors. Native and GBA captures with reflections disabled were inspected.

## Sky-opening light shafts (2026-10-21)

Graphics > Godrays independently enables light shafts in GBA, Software and Vulkan,
including reflected scenes. It appears below Water reflections, defaults Off
for fresh settings/Clear Data, and does not depend on Surface decals.
Only two-sided edges between a sky ceiling and a lower solid ceiling qualify;
closed rooms, equal-height ceilings and sky-to-sky edges do not. The light
polygons are two-sided and visible both inside and outside the sky sector.
This optional extension leaves the original renderer unchanged when disabled;
Surface decals still controls floor/ceiling and water artwork only.

Settings v17 retains16 bytes and the CRC over bytes0..11. Byte0 bit7 extends
the version nibble (byte8 low bits,1 for v17); byte5 bit7 stores Godrays.
The other flag positions are unchanged. Versions1..16 remain readable and
migrate Godrays from their saved Surface decals value, preserving the formerly
coupled effect. New saves store both independently; older executables reject
v17 instead of misreading it. Menu keyboard/mouse, Windows live application,
save/load and migration checks cover the independent toggle.

Each edge emits a simple world-space quad dropping96 world units from the lower
ceiling (twice the previous48). Low rooms shorten it to retain16 units of floor clearance.
Quads use the actual shared map endpoints: independent quarter-unit normal
offsets previously caused cracks or double-lit thin lines at corners. Shared
edges and half-open raster coverage avoid gaps and overlapping corner pixels. The quad
is frustum-clipped and triangulated, then rasterized with perspective-correct
height interpolation. Walls and actors occlude it; depth,
gameplay state and water masks are unchanged. First-person artwork and HUD
remain outside the light pass. The gradient goes from a42% ceiling blend to
clear at the lower edge, quantized to16 nonzero blend steps plus identity.
Every renderer uses the same original-palette lookup table. A fixed8x8 ranked
dither texture blends paired gradient steps (0/2,2/4,...14/16) to soften color
bands over twice the former height:12 world units instead of6 on a96-unit shaft.
The average remains linear, with unchanged peak brightness and transparent end. It uses
two-world-unit texels anchored along the dominant wall axis and world height,
with perspective-correct interpolation and ordered Bayer thresholds. Coverage
is evaluated at the center of the same two-unit world texel as the dither rank;
continuous height must not change shade partway through a texel row. Negative
coordinates use floor division, and shortened shafts clamp texel centers to
their gradient endpoints. Halfway
between each pair of shades it forms an exact checkerboard; other fractions use
the same8x8 tile for finer coverage steps. Texel density matches the original
wall textures, independent of viewport resolution. It consumes no frame state
or gameplay RNG; repeated draws are
identical. Reflection clipping retains the original texture/gradient phase. There is no horizontal brightness envelope, corner
attenuation or distance alpha fade. Same-height samples retain the same average opacity across the whole quad,
including its sides and triangle seam. Dithering changes shade selection only,
without eroding polygon edges or adding corner fades.
There is no godray render-distance cutoff: near-plane clipping, screen bounds
and scene occlusion determine visibility, including beyond1536 world units.
There is no volume integration, camera-sector gate or animated noise. Live
palette brightness preserves black transitions and color fades.

`godrays.c` streams fixed-point polygons and spans without allocation or floating
point:24.8 world/camera coordinates,16.16 trigonometry,4-bit screen subpixels,
and reciprocal-depth interpolation. Canonical endpoint/edge order avoids
double-blending triangle seams and reversal-dependent rounding. The4352-byte
blend table is prepared from the base palette at level load, not during fades.
Output indices then follow the live palette. No gameplay RNG or physics queries.

`godrays_reference.c` uses the original120x120 indexed world canvas, completed
wall opening clips and exact opaque sprite raster coverage. A28,800-byte actor
depth mask stores camera depth in four-unit steps; fixed-screen art uses zero.
Near-boundary sprite occlusion thus has four-unit precision. Including the
palette table, the reference workspace is33,672 bytes. It runs after actors and
before hardware display/HUD composition. This makes the effect available in
the GBA renderer without rendering a second3D scene or changing original sprites.

Software and Vulkan use the same fixed-point projection/rasterizer and lookup
table at their own viewport sizes, with existing scene depth for occlusion.
The native adapter caches nearest palette entries for RGB555 scene samples
(including reflections), and resolves result indices through the live palette.
Light spans run synchronously after scene/reflection completion; the existing
parallel Vulkan depth/statistics/water jobs remain unchanged. Reflection passes
draw the same shafts after reflected world/actor depth is complete and before
water blending, for Software, Vulkan and indexed GBA reflection output. The
mirrored camera supplies position/pitch; polygons clip at the water height
without restarting their gradient. Only consumed reflection rows are blended,
so stale depth/color outside the row interval is never accessed. Reflections
retain their existing half-alpha water blend and decal exclusions.

`tests/test_godrays.py` checks qualification, visibility from both sectors/faces,
wall and partial-depth occlusion, near clipping, line reversal, repeatability,
fixed96-unit height in tall rooms and16-unit floor clearance in low rooms,
palette identity/monotonicity, unbiased dither coverage, periodic world texels,
repeated/reversed/clipped dithered draws, undithered baseline and single coverage
across triangle seams, black fades and real doorway/inside-sky views under
ASan/UBSan with leak detection. GBA fixtures cover solid walls, upper/lower
portal openings, nearer/farther sprites, transparent texels and fixed-screen
weapons; the real GBA doorway changes1127 indexed pixels. Additional fixtures
check perpendicular corner joins at subpixel camera offsets for both holes and
double coverage, reflected-plane clipping without gradient rescaling, actual
reflected pixel changes and the indexed reflection/toggle-off path. The Windows
CTest target is `godrays`; Vulkan checks include an eligible doorway view with
exact serial/parallel color and depth agreement.

`scripts/test-godrays-arm.py` compiles the core, reference adapter and sprite
coverage raster for ARM7TDMI using Arm GNU14.3, with integer division helpers
only and no floating-point dependencies. See `godrays-arm-validation.json` for
object sizes and stack usage. This is compile evidence, not physical GBA frame
timing or a modified playable ROM. The original ROM remains untouched.

The texel-aligned dither update passes655,734 focused godray checks with ASan/UBSan and leak
detection, including exact checkerboard alternation at half coverage, sub-texel
stability in both axes across reciprocal depths and odd/negative shaft bounds, paired-shade coverage and mean error bounded to one rank over the64-rank texture, negative
coordinates, unchanged depth/occlusion, line reversal and reflection clipping.
All ten Windows focused tests and6,076,743 Vulkan checks pass. ARM7TDMI objects
remain integer-only (core8416 bytes, reference2380 bytes); native and GBA
captures were inspected. Dithering adds64 bytes of constant texture data and
perspective texture coordinates, with no new palette or per-level texture allocation.

## Animated water edges (2026-10-21)

Surface decals enables a32-unit shoreline zone. A broken foam front travels
outward, slows, and dissolves in place together with its lighter water-colored face and base-colored trailing wash.
The wave never returns to the bank. A quiet interval with no traveling crest
precedes the next cycle, preventing a visible reset at the outer edge. A narrow
contact meniscus stays attached to the bank, using only base water and lighter hue
instead of a continuous bright outline. There is no per-pixel random sparkle,
checkerboard dithering or alpha blending in the shoreline art. Open-water
glints and wavelets are unchanged.

Sixteen seam-compatible64x16-texel variants have32 animation frames, advancing
every six game ticks (10FPS,3.2seconds per loop), matching ripple frames.
Glints share this six-tick cadence too; all water animations use10FPS. Their
four-stage growth/shrink sequence now lasts48 ticks, with placement and spawn
eligibility unchanged. A shared timing constant keeps the samplers aligned.
White arcs and wash patches
shrink coherently as the wave dissipates, with no stochastic sparkle or alpha.
Four shape families vary broad arcs, lobes, interrupted crests and quiet stretches;
per-variant curvature, arc breaks and wash width add further differences.
Tiles cover128 world units along shore, at the same2-world-unit texel scale as
other decals. A stable hash of the signed world tile coordinate, canonical
shoreline offset and map seed selects the variant. Neighboring tiles vary their
crest bend, foam lobes, wash width and bank shape without reshuffling over time.
All variants have identical border texels in every frame; interior variation
ramps in smoothly. Animation phase stays shared so differently shaped crests
remain joined. Collinear line splits/reversals do not change variant placement.
`scripts/build-water-foam.py` reproducibly bakes `water_foam.inc`; rendering only
reads its immutable128KiB two-bit ink masks and never generates texture shapes per pixel.
Each64x16 frame needs256 bytes
for two bitplanes encoding clear, white, lighter water, and base water. The
lighter ink is selected once from the original palette near a one-third mix
toward white, requiring brighter channels and preserving blue/green ordering.
It follows live palette fades without introducing new palette entries.
Rows use two32-bit words, avoiding64-bit runtime shifts on ARM7TDMI. The atlas
is const ROM data, not a128KiB per-level RAM allocation. All sine/shape calculations run offline in the generator. Frame/texel selection
uses the existing shared sampler in all three renderers. This is a GBA-sized
artwork budget, not evidence of physical GBA frame timing: shoreline spatial
queries still use the port's host math, and no replacement ROM is produced.
Original-palette colors and the existing game clock are retained. Paused redraws are identical;
distance fading removes it by1536 units. Glints and ripples retain their existing
placement and priority. Foam, like those decals, is excluded from reflection
blending, while the remaining water retains its original reflection behavior.
No new setting or save version is introduced; the existing toggle remains Off
by default and controls this effect in Software, Vulkan and GBA alike.

`water_edges.inc` builds a spatial index of expanded map-line bounds once per
level. It borrows the stable live map and reads current sector heights, colors
and ceilings during sampling. Adjacent water sectors at the same open height
produce no artificial seam; dry neighbors, height changes, closed door openings
and one-sided walls bound the water. Existing dry-floor exceptions are retained.
Texture coordinates follow the closest shoreline tangent and distance, with
world-origin coordinates that remain continuous across collinear line splits
and reversed line directions. Sampling is read-only for parallel Vulkan row jobs, with no gameplay queries,
gameplay RNG or per-frame allocations. `SsaWaterScene` owns the index and frees
it before map teardown; initialize it to zero and free before reuse.

Validation is recorded in `surface-decals-validation.json` and
`windows-surface-decals-validation.json` (matching MSVC/GCC checks over61 primary
draws, including ASan/UBSan and leak detection). Fixtures cover both edge orientations,
negative grid coordinates,192-tick animation loops and6-tick frame holds,
distinct variant artwork, matching borders for every variant/frame and nonrepeating neighboring tiles,
foam across the32-unit band, outward motion, full dissipation before reset,
continuous bank coverage, the128KiB ROM atlas budget and bounded inter-frame changes including the loop seam,
light/base water details, visibility without reflections in RGB/indexed output, pause stability, distance and dry-side
rejection, live neighboring heights/ceilings, toggle-off behavior, indexed/RGB
agreement, foam palette/toggle behavior and reflection-mask exclusion. The Slave
Compound pool and tile atlas previews are used for visual inspection. Vulkan
checks include exact serial/parallel water output and launch/resize.

Graphics > Surface decals is saved and defaults Off. It adds new native artwork,
not recovered GBA content. Floors now have very subtle connected dirt/stain
tiles; ceilings use a different, lighter water-stain pattern. The old cracks,
gouges and chipped plaster are removed. One toggle controls both. Sky remains
undecorated.

`surface_decals.c` builds four fixed texture banks of49 tiles per surface, each
128x128. They sample at two
world units per texel, matching the base wall textures' sampling density.
Each tile now spans256 world units, twice its earlier width and height, without
changing texel density. The set contains an empty tile plus47 occupied pieces:
single stains, caps, straights, inner/outer corners, edges, junctions and a full
interior tile. Diagonal neighbors connect internal corners without introducing
tile seams; both floor and ceiling layouts include full connecting pieces.
Diagonal support blends into the curved shoulders with a smooth two-axis weight.
The former hard whole-quadrant fill caused rectangular density steps inside
the tile artwork; it is removed. The smooth corner artwork is independent of
placement probabilities and bank selection.
The tile library is fixed across levels. Each map cell selects one of four
texture banks, varying contours, interior gaps and speckles. All banks share
border pixels; differences fade in over16 texels inside a piece, preserving
exact joins for every neighboring bank combination. Fixed cluster stamps, with seeded random
selection and orientation, place stains on a64x64 tile grid. Cluster activation
is now100% for floors and95% for ceilings, increased from90%/70%, so more stain
pieces replace empty cells. These are cluster probabilities, not the fraction of
all tiles covered by stains. Matching
corner/edge/full pieces are chosen from occupied neighbors. There are no ellipse
fields, coordinate warping, bilinear masks or per-pixel generated shapes.
Sampling reads the selected tile directly at its original texel coordinates.
The grid repeats after16384 world units; ceiling placement is less dense.

All tile pixels are binary: unchanged surface or one darker original-palette
shade, targeting6% attenuation. This gives at most two colors per underlying
surface, including feathered edges, noise and distance fading. Soft outlines are
baked into the tiles with unordered full-tile random thresholds. The former
16x16 ranked dither lattice is removed: random speckles allow natural neighboring
clusters instead of favoring alternating pixels. The feather spans
the full edge profile instead of saturating into a fill at low coverage. Wider
profiles soften exposed boundaries. Periodic negative noise cuts irregular clean
patches through the interiors, and interior dithering leaves the underlying
surface visible even in full connecting pieces. There are no solid stain fills.
The latest density adjustment doubles the scale of the erosion detail and lowers
its strength: openings are fewer and larger, with denser remaining stain marks.
The curved shoulders are inset farther from exposed corners for rounder outlines.
No intermediate shade levels or interpolated colors are added. Palette matching
retains fractional targets; the actual darker color depends on the chapter palette.
Matching now requires a genuinely darker, distinct opaque palette color rather
than accepting an unchanged surface color. This makes the artwork visible on
every solid campaign floor/ceiling material, including dark ceilings. A pure
black plane uses the nearest distinct opaque shade if no darker color exists.
Blue-dominant floor colors used for water exclude stains and noise; ceiling
colors remain eligible. Water classification uses the original unfaded palette
so damage flashes and fades cannot change eligibility. Sky remains excluded.

An additional faint noise tile is selected for approximately one quarter of
otherwise empty grid cells. Its sparse speckles cover about1.5% of the floor tile
and1% of the ceiling tile, using the same single decal shade. Its borders are
clean and it never replaces a stain tile. Other empty cells remain undecorated.
Each bank contains49 tiles (48 stain/empty pieces plus noise).
All artwork and thresholds are stationary and independent of level/game RNG.
Original palette effects still apply; shading never substitutes transparent/sky index0.
Coordinates include negative world positions and stay fixed as the camera moves.
Stain intensity fades after768 units and disappears beyond1536 units to
limit minification noise. No actors, collision bodies, gameplay timers or RNG
state are allocated or advanced by rendering.

The software renderer samples within its existing floor/ceiling passes. Looking
up/down uses world-space UVs interpolated with reciprocal depth. Depth and
occlusion stay unchanged; this is not a screen-space overlay. The reference GBA
renderer reuses its original visible plane intervals before actors, reconstructing
floor/ceiling world coordinates at the120x120 scene resolution. Original plane
rendering functions are untouched. With the option Off, original rendering is
retained in both paths.

Water uses randomly placed, animated glints instead of stains when Surface decals
is enabled. Four fixed8x8 star sprites pulse over48 game ticks, with independent
position, phase and eligibility per64-world-unit patch and128-tick epoch. The
game clock freezes them on pause; rendering does not consume gameplay RNG.
Glints fade after768 units and disappear at1536.

Water also has non-glint animated decals: expanding broken ripple rings and
short wavelets, using eight banks of twelve24x24 frames in water_ripples.inc.
Four ring variants have irregular expanding contours, broad breaks and a weaker
delayed inner ring. Four wavelet variants have three curved, tapered crests with
unequal lengths and spacing. The masks dissolve into shorter connected arcs,
with empty first/last frames rather than an abrupt disappearance. The generator
`scripts/build-water-ripples.py` bakes all shape math offline. The atlas occupies
18KiB of const ROM data (two masks with one32-bit word per24-texel row); no runtime geometry
generation or alpha blending is needed. Crests are slightly broader to retain
their continuous shapes with the new lighter ink.
They sample at two world units per texel. Independently seeded64-unit patches
choose their position, variant, orientation and phase; two thirds are eligible
per256-tick epoch. Each decal animates for72 game ticks,6 ticks per frame,
then disappears before its next randomized appearance. Shoreline artwork uses
palette white, the original water color and its lighter palette tint. White
foam accents are sparse; the lighter ink carries most of the visible wave.
Glints have white cores and lighter water-colored arms, with no offset shadow.
Open-water ripples and wavelets use both lighter crests and base-water detail,
so their highlights remain visible when reflections are disabled. Positions are
inset so the larger24x24 artwork fits entirely within its32x32-texel occupied
cell; neighboring empty cells do not cut rings in half. The sparse half-empty
tile layout and event frequency are unchanged. Tests check distinct variants,
the18KiB budget, empty texture borders and quiet first/last frames in addition
to existing palette, pause, coverage and reflection-exclusion checks.
These texels are opaque artwork and exclude reflection blending,
unlike transparent texels, which retain the surrounding reflected water.
Both white and lighter palette indices follow live fades. Surface decals
controls both water animation types, with no new setting or save-format change.
Ripples use the same pause-safe game clock and768..1536 distance fade, without
advancing game RNG or entities.

Graphics > Water reflections is a separate saved toggle, default Off. Both the
software and GBA display modes now use a second render of the3D scene. The old
reflection ray tracer and its world-grid cache are removed. The camera eye is
mirrored across each visible water height, pitch is reversed, and the resulting
scene image is flipped vertically into the visible-water stencil. Geometry
crossing the water plane is clipped in3D before near clipping and projection;
the water plane itself is excluded. The shared polygon scene pass renders current
walls, ceilings and world actor sprites, including their current animation
and vertical offsets. HUD, first-person weapon/flash sprites and recursive
reflections are excluded. This renders an independent scene from the mirrored
eye; it does not flip or reuse the primary framebuffer. One scene pass runs per
distinct visible water height, with no pass when no water is visible.
Reflected backgrounds use the original backdrop artwork with mirrored pitch,
then the common vertical flip and water blend. A previous fade correction
incorrectly replaced the entire reflected sky with flat water color; this is
removed. Only reflection opacity fades with distance, ending at each pixel's
own water material color. Godrays still compose over the reflected backdrop.
A shared regression changes only sky artwork and requires visible reflected
water to respond in Software, Vulkan and indexed GBA output.
The reflection uses50% alpha blending with the original water RGB555 color:
each channel is `(base*(510-fade)+reflected*fade)/510`, rounded to the nearest
integer. At full strength this is an equal mix of the water and reflected
scene; the distance fade reduces reflection opacity toward zero. Both dark
and bright scenery contribute, replacing the previous screen/additive blend.
GBA output selects one nearest opaque original-palette color per reflected
RGB555 value. Reflection dithering is removed entirely: pixel coordinates do
not affect color selection. Exact palette colors remain unchanged; colors
between entries can produce visible bands or lose weak highlights. The former
alternate-color/weight caches and hue-filter search are removed, saving68KiB
per water-render context. The remaining nearest-color cache resets with each
water frame so live palette fades remain correct. Regression fixtures check
uniform output across an8x8 area for every grayscale level in a sparse palette,
the former false purple/green mixtures, and exact colored matches.
Glints and ripple decals bypass blending and retain their palette colors. Reflections
fade after384 units and disappear at768, half the previous reflection range.
The fade endpoint is the water material's own live palette color; reflected sky
is fully removed at the cutoff, with exact original color/index output. Water
decal visibility still extends to1536 units. Static stain artwork retains its
existing two-color limit; the separate water shading layer does not alter it.
The native renderer uses full viewport resolution for reflections, retaining
the viewport aspect ratio and live FOV without downsampling. GBA reflections
remain240x160. GBA output reuses this native
polygon renderer with its original3:2 projection, sampling into the original120x120 visible
plane spans with nearest original-palette colors. Its ordinary original world
and actor passes remain unchanged. Both paths use center-aligned bilinear
reconstruction; the visible-row optimization includes the extra sample row and
clamps at all image edges. FOV, pitch, height clipping and full-resolution water
stencils remain intact. The reflection backend is shared through the
native-core/PC-renderer static-library component rather than duplicating geometry
rendering. Each owner frees its water stencil and private reflection renderer
before map teardown. Buffers and geometry caches persist between frames, and
the second pass does not change gameplay entities or the main depth buffer.

Settings version12 retains the16-byte CRC format and all previous flags. Byte8
bit4 stores Water reflections, bit5 Surface decals, bit6 bobbing, bit7 aspect
and bits0..3 version. Versions1..11 migrate with reflections Off; versions1..10
also default decals Off. Extras has nine logical rows, eight visible at
Y16..128 with a16px stride, title Y0 and Back144. Selection scrolls by one row
to reveal Water reflections; arrow clicks and mouse targets follow the scroll.
Each localized row overlays its own font batch so the original128-glyph limit
cannot clip the final value or Back label in longer languages.

`tests/test_surface_decals.c` checks binary tile pixels, the two-color rendering
limit, a fixed library across levels, distinct and randomly selected texture
banks, compatible ports across all bank pairs, and clean exposed edges. Tests
also require diagonal pieces to match their unfilled counterparts along
both internal midlines, guarding against rectangular quadrant fills. Threshold
histograms and neighbor alternation check uniform, unordered speckling. It checks
lower coverage in the broad edge band, rounded exposed corners, porous yet denser
interiors in every8x8 patch, increased stain placement with empty areas remaining,
actual single/corner/edge/full pieces, diagonal-aware
joins, random noise tile frequency and sparse speckle density. It checks direct
stored-tile sampling across the whole layout period, negative coordinates and
integer limits, distance fading, palette validity and sky rejection. Reference
checks distinguish blue floors from identically colored ceilings. Asset-only
checks decode all twelve campaign maps and verify every floor/ceiling material
against the two chapter palettes, including dark shades and water exclusions.
Reference visibility, output guards, stable repeated draws and unchanged depth/gameplay
are checked in both renderers and three pitch views on Egypt/Rome fixtures.
Water checks use render-only blue floor fixtures, verify actual second scene
passes with no reflection rays, multiple water heights, visible reflected actor
pixels and removing actors from render snapshots, half-alpha color mixing, bright glints unaffected by
reflection, animation and distance limits, stable repeated draws and unchanged
depth/inventory. Non-glint checks cover independent animation, shared palette-white
palette colors, the shared toggle, reflection exclusion and distance rejection;
render captures verify stable paused frames and animation advancing with the
game clock. The suite now makes61 primary draws. Settings checks cover the new
Extras row, live application, persistence and legacy save migration.
The2026-10-20 reflection update passes54,072,272 checks with identical MSVC and
GCC ASan/UBSan/leak-check results. Added cases cover native full-viewport
allocation, GBA240x160 allocation, resize growth/shrinkage and wider viewports,
tiny and odd-sized output stencils, edge sampling, sparse-palette quantization,
unchanged exact palette colors, and exact50/50 mixing across every RGB555 channel value.
All eight focused Windows checks pass; sample native/GBA reflection images were
inspected. No campaign playthrough was run.
A synthetic visibility fixture is placed beside actual stains; a starting view
may legitimately be clear. These are render fixtures, not campaign playthrough
validation. See surface-decals-validation.json and
windows-surface-decals-validation.json. Original ROM files are unchanged.

Slave Compound pale pool correction (2026-10-19): map3 sector9 is the large
pool (floor96, palette120, RGB5 27/30/31). Its blue-green difference is only1,
so the saturated-blue heuristic missed it. Sectors38/39 share palette120 but
are smaller stair treads at144/128. The shared material predicate therefore
adds only level3/color120/height96, retaining the original color and geometry.
PC flat/pitched shading, reflection-pass stains, GBA stain exclusion and GBA
water overlays use that predicate. Other levels, same-colored steps and ceilings
retain their existing behavior; the two Extras toggles retain their meanings.
The fixture verifies the original three sector records, real pool rendering at
two pitches, one reflection height, and separate GBA pool/stair visibility spans.
Captures: build/surface-decals/slave-pool-flat-3.bmp and slave-pool-pitched-3.bmp.

Water decal density (2026-10-20): glints and ripples share a seeded64-unit
tile occupancy mask, leaving approximately half the tiles permanently empty.
The remaining tiles retain the existing per-epoch appearance probabilities,
sprite placement and animation timing. This roughly halves overall decal
density and creates persistent clear areas. Empty tiles still show the original
water color or reflection; both renderer paths use the same layout.

Caesar's Palace dry floor correction (2026-10-20): the U.M.T at (-192,-448)
stands in sector43. This floor and its connected sectors44/62 use palette36
at height0. Treat that material as dry on level11, overriding the blue-color
heuristic. The original color and geometry remain intact; glints, ripples and
water reflections no longer apply, and optional ordinary stain decals can.
Other levels retain their palette classification, including Slave Compound's
pale pool exception. The surface fixture verifies the ROM sectors and U.M.T
location, dry software/GBA shading, and unchanged classification in Rome level6.

Extras layout update (2026-10-20): all nine options now fit on one page at
Y16..80 with an8px stride. This supersedes the eight-row scrolling layout
described above; Water reflections no longer requires scrolling.

Forum of Mars dry floor correction (2026-10-20): level6 sector21 uses palette130
at height0. It is dry stone, so this level's material classification overrides
the blue heuristic. Both renderers omit water reflections, glints and ripples;
ordinary optional stain decals apply. Other levels keep their existing rules.
The ROM-backed surface fixture checks the sector record and dry predicate.
