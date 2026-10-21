# Sprite shadows, 2026-10-21

Graphics > Sprite shadows independently enables size-scaled oval sprites for
visible players, enemies/bosses/corpses and E1 pickup items. Default is Off,
including Clear Data and migration from older saves. Shadows do not depend on
Surface decals. Settings v19 stores the toggle in byte10 bit7, retaining the
16-byte CRC record and v1..18 migration. Existing preferences are preserved.

Graphics keeps display/renderer/aspect/mip rows at24/40/56/72; visual toggles
occupy88/96/104/112, with Back at144. Mouse, keyboard and localized labels
use the same positions. Extras is unchanged.

## Sprite composition

The earlier floor-projected sampling pass has been removed. Shadows are now
ordinary transparent upright billboards, centered at their subject's ground
position and flattened into an oval. Each shadow is submitted immediately before
its subject's base sprite, followed by any player-color overlay. The subject
always covers its own shadow. They use painter ordering and wall silhouettes,
without floor polygons, world-depth testing or a depth bias that can flicker.

The stored32x32 four-level mask in `sprite_shadow_texture.inc` supplies a32x8
column-major indexed image. Each subject has distinct, stable texture storage
through the draw, including GPU submission and finish. Transparent zero leaves
the scene untouched; three shades use the original palette at11/22/33% darkening
of the receiving floor color. Palette fades remain live. Overlapping shadow
sprites follow subject order rather than accumulating darkening.

Native collision radius sets the size. Zero-radius pickups derive size from
the original resource's first image and original mip scale. The requested
25% enlargement uses radius7.5..120 world units before airborne adjustment
and integer rounding. Rising subjects shrink/lighten their shadow, disappearing at256
units above the live sector floor. Corpses retain shadows while their sprites
are visible. Hidden first-person player bodies do not submit a main-view shadow;
the exposed third-person/reflected body does. Projectiles, muzzle flashes,
particles, ceilings, sky and water are excluded.

GBA attaches a draw callback to the original actor queue. It draws the oval
immediately before the matching base projection, without consuming any of the
32 actor slots or changing original record layouts. Its expanded horizontal
bounds get independent wall clips using the original inverse-depth, segment-side,
body-height and signed opening rules. Fixed-screen and secondary body layers do
not duplicate shadows. Existing queue/scene APIs retain their original behavior
when no callback is supplied.

Software and Vulkan use the existing transparent actor material and shared wall
silhouette clipping. Reflections use the same billboard submission. Preparing
up to100 sprites uses bounded draw-local storage, no heap allocations, gameplay
changes or RNG. This works in GBA rendering mode; it is not a newly benchmarked
ARM hardware build.

## Validation

`tests/test_sprite_shadows.py` covers classification, enlarged sizes, airborne
behavior, transparent artwork, closed/open GBA wall clips, actual GBA On/Off
composition, repeated-draw stability and native On/Off restoration, under
ASan/UBSan with leak checking. Current counts/hashes are recorded in
[sprite-shadows-validation.json](sprite-shadows-validation.json).

The shared `sprite_shadow_fixture.h` runs against both Software and Vulkan:
a low subject sprite overlaps its broad shadow at three pitches/FOV choices,
and every subject pixel must survive unchanged. Disabling shadows must restore
the baseline exactly. Vulkan validation passes30 paired scene draws with zero
errors. The native capture was inspected: the pickup has a flat oval below it,
with its own pixels in front. The original actor-queue regression and focused
native tests also run. No campaign playthrough is part of this change.
