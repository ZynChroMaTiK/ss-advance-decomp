# Native sprite clipping, 2026-10-21

Death sprites and explosion artwork can extend below an actor's logical floor.
The native per-pixel world depth test sliced off those texels. Raising the
sprite or adding a depth bias would move original artwork or expose walls.

## Original rendering rule

The recovered `actor_queue.c` insertion (08001238), `wall_renderer.c` wall
results, and `actor_projection.c` agree on a different rule: actors are painted
back-to-front through wall silhouette clips. Floor and ceiling pixels do not
independently depth-test the sprite. Wall depth endpoints choose front/behind;
when those depths straddle the actor, its XY origin chooses the wall side.
Portal bottom/top silhouettes are gated by the actor body's bottom/top Z,
not the artwork's opaque texel bounds. One-sided and closed walls hide the
whole overlapping interval. The existing explicit `step_adjust` crop remains.
These recovered routines already have ROM differential validation; their GBA
implementation is unchanged by this correction.

## Shared native implementation

`pc_actor_clip.inc` owns reusable clipping scratch. `pc_renderer_pitch.inc`
projects nearer wall intervals into the upright billboard plane, applies the
original body-height silhouette gates, splits at interval endpoints, and clips
against the resulting linear upper/lower bounds. The visible pieces are
triangulated before camera pitch and near/water-plane clipping. This preserves
native resolution, FOV and vertical aim without scaling a 120-column GBA mask.

Software and Vulkan now use those same prepared actor triangles and stable
far-to-near ordering, including reflections. They do not retest those triangles
against floor/ceiling depth. The returned actor depth remains geometric for
later effects. Transparent texels still discard; fixed-screen weapon effects
paint after all world actors. Vulkan retains its two batched pipelines and no
longer needs a sprite depth bias. World geometry still uses normal depth tests.

Native floating-point coverage is not claimed pixel-identical to GBA's integer
rasterizer. GBA's original clipping/rasterization path remains intact.

## Validation

`tests/sprite_clip_fixture.h` runs through both real backends. A grounded sprite
extends below its own floor at three pitch/FOV combinations. It must retain
thousands of covered pixels that the old depth test discarded; lowering all
floors must not expose any additional sprite rows. A sprite behind a real
one-sided wall must remain fully hidden at each tested pitch. Existing Vulkan
priority/transparent-layer and paired scene/reflection checks still run.

- ASan/UBSan PC renderer: 11,803,544 checks, seven sizes, jump and chase views.
- Vulkan: 6,487,023 checks, 28 paired scene draws, zero validation errors;
  hidden frontend launch/resize smoke passes.
- All ten focused CTests pass, including decals, godrays, HUD and mipmaps.
- Windows frontend and 128 native libraries rebuilt. No campaign replay.
