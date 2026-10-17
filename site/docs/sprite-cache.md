# Native sprite cache and tile uploads

`sprite_cache.c` recovers DBF4 visibility, DDA8 frame requests, DEF8 aging,
DF80 loading, E0A8/E2EC tile allocation/release and E358 tile transfers. DBDC
composes those stages in their original order. All graphics are copied from
the user's existing sprite assets; no original instructions run in native code.

The owner holds 311 actual image cache entries, 50 request slots, 50 allocated
image slots, 50 free/used tile ranges, ten transfer descriptors and the physical
32 KiB OBJ tile window. Initial free range is tile64 `{160,512}`; weapon images
upload to reserved tile0. Allocation intentionally retains the original range
size, including addresses beyond physical tile511. Writes through the upper
OBJ mirror wrap into the same physical 32 KiB window. OAM receives the original
unmasked tile values. See `display.md` for hardware evidence and limitations
when tile overflow affects sprite priority.

DBF4 uses original owner/type filtering and signed16 bounds. Frame bit8000
mirrors horizontal bounds; bit4000 mirrors vertical bounds. Disabled animation
or frame3FFF uses the origin as a point. Exact240/160 boundary points remain
visible. It updates flag4 and leaves owner-filtered, empty and type2 objects
unchanged. It does not apply guessed affine bounds.

Requests preserve the original previous-frame retention and pending-bit
behavior. The50-slot ring wraps exactly; equal cursors mean empty even after an
overwrite. DF80 consumes at most eight requests against its10000-byte budget.
Oversized requests and allocation exhaustion retain original status effects.
DEF8 removes expired images in the original order and skips the shifted next
entry. E1CC's merge loop intentionally leaves its final adjacent free pair in
some cases; a directed original-ROM test proves `{160,500},{660,12}` after
releasing fifty ten-tile allocations. Replacing that loop with a generic merge
changes original allocator state.

DAF0 display handoff reset clears residency, queue cursors and the first50 image
slots, restores the free range and sets active previous frames to0. Physical
pixels, stale request contents and block tails persist. Initialization starts
owned native storage at zero. Clear/free are rejected during updates.

One explicit native safety extension handles FAE8's initial previous-frameFFFF
sentinel: it retains no old image. The original retention probe indexes16383
in its2000-entry cache and reads unrelated memory; native code never does that.
The initial-state oracle seeds that unrelated byte to0 and states that domain
explicitly. Other out-of-range image IDs fail. Extra backing records preserve
the original bounded stale-tail copies without accessing unrelated allocations.

Prepare and flush are separate, preserving original transfer timing before
display. E358 clamps each transfer to160 tiles. A late failure keeps previously
copied pixels and clears the update guard. Assets and dispatch objects are
borrowed only for calls; the cache owns all remaining storage.

`test-sprite-cache` executes complete original routines with only BIOS/DMA
copies modeled:9952 visibility calls,2048 complete DBDC frames,11147 allocations,
5337 releases,2048 E358 transfer passes and64 DAF0 resets. The test compares
objects, cache metadata, all block/request backing records, budgets and
7769920 uploaded bytes. The standalone ASan/UBSan fixture passes20624 assertions,
2359 frames and11 guards, including mirror-crossing transfers and failure
prefixes. Counts and exact source/test hashes are in
`native-sprite-cache-validation.json`.
