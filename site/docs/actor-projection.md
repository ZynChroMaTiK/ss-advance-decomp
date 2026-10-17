# Native actor collection and projection

`src/actor_projection.c` recovers the complete `0800D2D0`, `0800D338`,
`0800D490` and `0800D884` rendering routines. It uses the actual actor resources
loaded by `actor_assets.c`, original direction/reciprocal data, the host world's
sector lists, and the native mixed actor/wall queue. ROM instructions execute
only in the differential tests.

`SsaActorSource` borrows a stable entity and supplies its animation, displayed
frame, facing, owner and render flags. `flags` is an explicit snapshot of entity
`+30`, allowing the requested third-person extension to expose the local player
without modifying its collision/gameplay flags. That extension remains separate
from original behavior.

`ssa_actor_project` implements one complete billboard or fixed-screen projection;
`ssa_actor_collect` implements the visible-sector traversal and both original
player passes. The root `actor_scene` adapter composes collection, mixed-list
insertion and final actor drawing after native world geometry.

## Original contracts

`D2D0` resets the actor clip-word cursor, copies the order/clip backing pointers,
then walks sectors in ascending index order. A set world visibility bit selects
a sector. `D338` follows that sector's current linked-list order, rather than
the active entity-pool order. Native collection uses the corresponding stable
`world.sectors` and `entity.sector_next` links; each visited body must have exactly
one source mapping.

Entity flag `2000` selects `D884`; ordinary actors use `D490`. Flag `10` adds a
second `D490` pass with animation temporarily increased by `5 * (owner + 1)`.
The original restores the animation afterward. The native version uses a local
source copy, so the caller's metadata stays unchanged.

`D490` preserves these gates and calculations:

- Animation zero/FFFF and displayed frame FFFF/FFFE reject the actor. Body flag
  `80` hides it; flag `40` hides it when game flag1 is set. `D884` deliberately
  does not apply those two body-flag tests.
- At most 32 projection records are allocated. This cap is separate from the
  mixed queue's 32 actor records. Projection allocation happens before `F98`;
  failure to allocate an actor does not undo the projection record.
- Camera-relative XY rotates through the original signed 64-bit dot product,
  retaining the wrapped middle 32 bits. Negative depth rejects the actor.
  Integer depth indexes the real reciprocal table. The original fixed multiply
  by `0055B05A` and subsequent shifts produce the queue's depth/scale value.
- Resource mip bits 1,2,4,8,16 are considered in that order, with thresholds
  `5FFF`, `3FFF`, `2FFF`, `1FFF`. Their sample scale factors are `AAAA`,
  `10000`, `15555`, `20000`, `40000`. Only present bits advance the mip index.
- The heading is entity facing shifted left eight; flag10 adds half a turn.
  Relative direction uses original `93E8`, rounds by `0800`, folds directions
  beyond eight and sets horizontal flip. Resource direction count reduces that
  angular selection. One-direction resources clear flipping.
- Image index is displayed frame +1 + direction times the original frame-stride
  byte + mip times frame-stride times direction count. This uses the actual
  `0890` stride, including resources whose count is not evenly divisible.
- Real image signed origins and unsigned dimensions determine projected size.
  Horizontal flip changes the origin to `-(origin + width)`. Vertical scale has
  the original factor1.5. Positive floor/step-adjust overlap shortens the image
  before vertical scaling.
- Rejection uses signed projected coordinates and the 120-pixel screen.
  Horizontal clipping retains original X/width and also emits the clipped
  left/width. Output stores only the low halfwords, exactly as the original.

`D884` selects displayed frame +1, uses fixed horizontal scale one-half and
vertical scale three-quarters, centers at60, sets world XY to zero, and queues
depth `10000`. It still retains source Z/height and resource identity.

The shared `SsaActorProjection` is the exact 36-byte scalar record in
`actor_queue.h`. Its reserved leading word is never cleared by projection.
Original `+1C` is the actor resource/animation ID; `+0C` holds the image index
with horizontal flip in bit31. `+10` is the 12-bit image scale. Queue allocation,
wall depth/occlusion, clip-word construction and final reverse draw order are
implemented by the separate native `actor_queue` module.

## Ownership and failures

Keep maps, world lists, camera, assets, math tables, entity addresses and source
arrays stable throughout collection. Source/entity/assets storage cannot alias
queue output. Collection rejects active world queries, nested projector/queue
updates, mismatched maps, missing/duplicate source mappings and broken/cyclic
sector lists. Original record-cap rejection is successful no-draw behavior;
invalid resources or native backing bounds are explicit errors. Earlier queue
effects survive a later error, and the projector guard always clears.

Run `queue_begin` once after a fresh geometry pass. Collection's reset of the
clip cursor does not reset projection or actor counts. The root `actor_scene`
adapter performs this ordering and binds the full backing capacity of wall
opening clips, including the original inclusive endpoint reads.

## Validation

`tests/test_actor_projection_rom.py` builds strict C11 with undefined-behavior
sanitization and compares:

- 15,456 complete original `D490` and 2,208 complete `D884` executions, using
  all 138 image-bearing actor resources, original frame strides, reciprocal
  tables, image origins/dimensions and direction math. Only `1238` insertion
  is controlled in these isolated cases; original `F98` allocation executes.
- 256 complete `D2D0`/`D338` collection passes on all 16 real maps, executing
  original/native `1238` with an explicit empty wall list. Half use controlled
  multi-actor metadata at original starts; half use all 385 original `01984`
  single-player map objects, with their actual positions/resources/facing.
- All 36 projection bytes, 12 actor-record bytes, counts, insertion calls,
  complete clip backing, list head/tail and source preservation. There are
  9,110 emitted projection records and 18 distinct guard cases repeated across
  256 collection runs (4,608 guard assertions).

`tests/test_actor_scene_rom.py` then executes the complete original chain:

```
B6B0 -> BSP/017CC -> BE60/AB10 -> D2D0/D338/D490/D884
      -> 1238 -> 01828 -> 09238/09258 -> ARM0638
```

No actor or renderer call is hooked out. BIOS division/CpuSet and DMA copies are
the modeled hardware boundaries. The original self-modifying wall-column code
is synchronized with Unicorn's instruction cache without replacing its math or
pixels. Native code uses the root `ssa_actor_scene_init/draw/free` adapter.

The full oracle passes 256 frames across all 16 maps, comparing 3,686,400 world
pixels, the 120-byte predecessor row, every projection/actor/clip/order record,
wall columns, plane records and opening data. It covers 317 actor draw commands,
2,584 wall commands (1,088 portals), and 128 consecutive animation frames with
both native and original `00908` state updates. Original map objects retain
their actual pre-AI construction state; these tests do not invent enemy AI
activation, health, attacks or movement.

Camera inputs use eight bounded views at original player starts, repeated while
animation advances. This is not a claim that the original's small scratch area
is safe for every possible camera. During development a level6 variant needed
741 opening bytes, crossing the original720-byte space and overwriting its own
cursor at `03004C70`. The native renderer's explicit 24,000-byte backing avoids
that corruption. Such undefined original-state views are outside the equality
oracle. The evidence and all tested source hashes are recorded in
`native-actor-projection-validation.json` and `native-actor-scene-validation.json`.
Native framebuffer index artifacts are written under `build/native-tests/`
only after equality succeeds.

The actual decomp.wtf engine claimed `sub_0800D490`, recorded these recovery
findings, and released the claim. This host implementation is behavioral
recovery, not an exact compiler match. No new exact match is claimed here.
The rendered actor pass does not implement the remaining game/HUD/audio/combat
integration or establish a complete playable game port.
