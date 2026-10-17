# Native moving sectors and line activation

`src/sector_movers.c` implements original routines `0801BF14`, `0801C02C`,
`0801C050`, `0801C098`, `0801C0B4`, `0801C0D4`, `0801C13C`, `0801C2B4`,
`0801C2E0`, and `0801C328` behavior. These are behavioral native ports, not
new byte-exact compiler matches. The final `0801C3B4` call in the initializer
belongs to the separate switch initializer.

Initialization reads 25 records of 20 bytes from the per-level ROM pointer table
at `0844DD4C`, using the active count at `0844DD3C`. Records remain byte-preserved
except the original initialization changes. Each record has sector index +0,
lower and upper floor limits +8/+10, unsigned speed +12, flags +14 and a copy
of sector +18 at record +16. Other fields remain opaque. Floor movers (flag
0x100) start at the lower/upper limit according to flag1. Ceiling movers infer
flag 1 by whether the current ceiling differs from floor. Initialization resets
the active hazard count while preserving its inactive storage.

The public API uses the same `SsaMapAssets` storage that collision reads, so
height changes become visible without copying the map. Map storage and layout
must stay stable. Native guards reject invalid sector indices, truncated tables,
out-of-range records/lines, unsupported ticks and callback reentry before mutation.
Initialize the state to zero before its first use. No allocations are owned by
the mover state. Initialize switches separately after movers to preserve the
original setup order.

Commands OPEN/CLOSE set the direction and pending-sound bit 8 and emit sound
0x67, even if already moving that way. LOCK/UNLOCK change flag 4. Locking prevents
line activation; it does not prevent scripts issuing OPEN/CLOSE or stop updates.
Line activation selects the first unlocked mover matching either signed line
sector at +12/+14, sets the opening direction, and emits sound only when the
previous direction was closing. It does not activate all matching movers.

Every update adds/subtracts `unsigned16 speed * unsigned8 ticks` to the relevant
height, wrapping to 16 bits before signed comparison. Ceiling movers target
floor or floor+224; the latter comparison uses the full integer sum, even above
32767. Floor movers target their signed lower/upper bound. Completion requires
strictly crossing the endpoint: equality does not clamp or emit sound. Crossing
clamps the height, and if bit 8 was set, clears it and emits sound 0x68. Direction
remains set and subsequent updates still perform arithmetic and clamp. Zero
ticks can still clamp an already out-of-range height.

CLOSE on ceiling movers appends the sector to an eight-slot hazard list.
Duplicates are intentional, and a full list silently ignores additions.
Completion of closing removes the first matching entry by replacing it with the
last entry, but only when bit 8 was set. Removal compares the low 16 bits;
`hazard_contains` compares full 32-bit values, matching the original distinction.
This list is available to the existing player hazard adapter.

The optional sound callback returns negative on failure. All mutators return 1
on success or 0 on input/callback failure. A callback failure retains effects
already applied (for example an unclamped overshoot and cleared bit 8), releases
the reentry guard, and skips subsequent effects. Callbacks must not mutate this
state or map and cannot reenter mover APIs. The native error behavior is an
extension; the original sound routine does not report these failures.

`tests/test_sector_movers_rom.py` passes **20,735 original-routine comparisons**:
32 initializations covering all 16 maps and both copy paths, 2,504 commands,
1,408 line activations, 12,695 updates and 4,096 hazard lookups. Updates include
4,096 consecutive actual-map frames, 8,192 randomized wrapped-state frames,
strict endpoint/signed-range boundaries and duplicate/capacity hazard cases.
It compares the entire 500-byte record array, decoded map, eight-word list,
counts and ordered sound events. The oracle services allocation, DMA and BIOS
CpuSet, observes sound and isolates the separately ported switch initializer;
all mover math/list behavior executes original instructions. Native compilation
uses strict warnings and UBSan. Another 20 input/error guards and 10,068 callback
reentry checks pass. Results and source hash are in
`analysis/native-sector-movers-validation.json`.

This does not supply rendering, enemy height adjustment, a frame scheduler,
audio playback or a playable executable. The root gameplay/script adapter must
order mover updates and bind sound/script events into those later systems.
