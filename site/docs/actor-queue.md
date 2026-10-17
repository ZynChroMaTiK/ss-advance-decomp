# Mixed actor queue, wall clipping and final drawing

`src/actor_queue.c` recovers complete `08001238` actor insertion/clipping and
`08001828` final actor drawing. `08000F98` allocates the original 32 actor slots;
`0800B24C` selects the actor's side of a real map segment through native BSP
math. The final pass directly calls native `ssa_actor_raster_draw`, which uses
the actual decoded actor resources. There is no successful placeholder draw.

The shared header defines the exact scalar 36-byte `SsaActorProjection` and
12-byte `SsaActorRecord` layouts consumed by projection and raster modules.
Projection is recovered separately in `actor_projection.c`; resource decoding
and the `09238/09258/ARM0638` sampler are in `actor_assets.c` and `actor_raster.c`.
This module does not invent actor animation frames or infer entity ownership.

## Original ordering and clipping

The mixed queue uses `8000|index` for actors, plain indices for 60-byte wall
records, and `FFFF` as the end marker. Wall records retain their own stable host
projection in `SsaWorldVisibility`. Actor insertion walks from the head:

- A wall outside the actor's horizontal interval is skipped.
- Actor depth below both wall endpoint depths clips against that wall; above
  both does not. The inclusive intermediate range calls original `B24C`, using
  the line's two vertices and XOR with the segment's side.
- Fully closed wall clips write top 120 and bottom -1. Other flags first apply
  actor bottom/top world-height tests, then merge **signed bytes**: minimum
  bottom and maximum top. Value -2 is normalized back to the unbounded value.
- At an existing actor, the new record stops when its depth is greater than or
  equal to that actor's. Equal-depth actors insert before the older record.
  `01828` gathers actor indices in forward mixed-list order, then draws that
  array backwards, so later equal-depth actors draw last.

Several awkward memory details are observable and preserved:

1. `1238` initializes and emits through `left+clipped_width+1`, inclusive.
   Thus it writes `clipped_width+2` top/bottom pairs, while advancing the shared
   word cursor by only `2*clipped_width`. The final four words overlap the next
   actor's allocation. The sampler consumes exactly `clipped_width` pairs.
2. Original temporary bottom[120] and top[120] arrays are adjacent on the stack,
   followed by the actor's fixed-point world coordinates. Right-edge columns
   120/121 alias those bytes. A bounded 248-byte native scratch array preserves
   these writes and their ordering, instead of accessing past a C array.
3. Wall clipping includes its projected right endpoint, even though BE60 copies
   opening bytes only up to, but excluding, that endpoint. The next/stale backing
   byte must remain readable. `ssa_actor_queue_bind_clips` exposes remaining
   opening-buffer capacity and uses the real 121-byte closed-clip ROM windows
   copied by `SsaActorAssets`; it never fabricates the extra byte.
4. Original sprite image indexing discards both bit31 (mirror) and bit30 through
   its left shift. Normal D490 frames are at most 3000; the sampler also retains
   the original bit30 alias for malformed input.

## Integration and ownership

After fresh world visibility construction and wall drawing:

1. Call `ssa_actor_queue_begin` with the stable visibility workspace.
2. Bind current wall results through `ssa_actor_queue_bind_clips`, passing the
   actual renderer, opening storage and actor assets.
3. Native D2D0/D338/D490/D884 projection fills this queue and calls insertion.
4. Call `ssa_actor_queue_draw` with the 14,400-byte world framebuffer.

`begin` resets projection/actor/draw counts and the clip cursor, preserving
record and backing storage. It requires a fresh wall-only queue. Insertion
mutates visibility head/tail and wall links; finish actor drawing before the
next visibility build. All map, plane/render output, opening, image and queue
storage must remain stable. Inputs cannot alias output pixels. The native
sampler does not need framebuffer bytes beyond the 120-by-120 image.

The 32-slot allocator returns an index or -1. Other queue operations return 1
or -1. Malformed indices, cycles, duplicate insertion, invalid geometry,
insufficient clip backing, capacity overflow and nested updates fail explicitly.
Earlier pixels or queue effects remain after a later failure, matching the
composition's prefix-effect contract. No callbacks run inside this module.

## Validation

`tests/test_actor_queue_rom.py` compiles the implementation with undefined
behavior checks and executes original routines on all 16 decoded maps:

- 33,792 whole `F98` calls, including all capacity failures.
- 32,768 whole `1238` insertions, comparing native geometric side tests, mixed
  links, scalar records and 47,723,672 clip words across consecutive insertions.
- 1,024 complete `01828` passes with the `09238` boundary observed for exact
  reverse call ordering and offsets.
- 256 further complete `01828` passes execute original `09238`, `09258` and
  ARM0638 against real ROM image packets. All 14,400 output bytes match native
  output for 8,192 actor draws, including overlapping sprites and mirrored images.

The queue inputs in this differential suite are controlled wall-result and
projection records, with geometric side queries against actual maps. This is
not a claim that these particular projections came from D490 or that the wall
pixels came from BE60. Complete world/projection composition has its own test.

The same driver runs `tests/test_actor_queue.c` under ASan and UBSan:
36,564 assertions, 432 guards, 1,024 native frames and another 8,192 actual sprite
draws. Tests include closed/opening backing views, malformed lists, duplicate
insertion, all slot limits and a later bad-image failure preserving an earlier
actor's real pixels. Sanitizers use `-no-pie`, with leak scanning disabled.
Metrics and hashes are in `native-actor-queue-validation.json`.

The actual decomp.wtf matcher was also used for F98. Five manual attempts reached
80.3%, **not an exact match**. The best body has the original register usage and
branch shape, but agbcc retains an LR prologue/epilogue absent from the original,
shifting literal placement. The claim was released with these notes. The best
candidate, bounded matching script and attempt history are retained; no naked
attribute or inline assembly was substituted to conceal the difference.
