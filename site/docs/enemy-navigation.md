# Shared enemy navigation

`src/enemy_navigation.c` recovers shared navigator `0x0801288C`, reset
`0x08012850`, dual-ray construction `0x0801271C`, and segment append `0x080083F0`.
It uses native ROM-data math tables, never ROM instruction execution at runtime.
These are behavioral ports, not exact matching C candidates.

## Navigation state and ordering

`SsaEnemyNavigation` projects these original entity fields:

| Host field | Original offset |
| --- | --- |
| target | `+20` |
| speed | `+24` |
| desired_facing / move_result | `+40` / `+41` |
| phase / distance / heading | `+44` / `+46` / `+48` |
| waypoint_x / waypoint_y | `+4A` / `+4C` |
| navigation_flags / target_timer | `+50` / `+56` |

The original entity-facing byte `+3F` remains externally owned. Its turn
callback receives desired facing and updates that separate facing byte.
The navigator's heading is a search-angle field, not the facing byte.

Visibility rejection or entity flag `0x800` skips navigation but still calls
the finish routine. Flag `0x400` maintains an alternative nearest target:
subtract ticks with 16-bit wrap, expire immediately for target flag `0x20`,
and refresh when the signed timer is nonpositive or the pointer is null. Refresh
sets 600 before selecting. A null selection falls back to the player. The
player's nearest entity is queried independently to identify a type-CF special
path flag. The original current-entity global is represented by the navigation
view supplied to path callbacks.

Phase zero faces the player even when an alternative target exists. It searches
along the stored heading/distance, tests both candidate and target connections,
and enters phase one only if both path masks are three. Otherwise it resets and
moves. Phase one follows a waypoint and enters phase two when a target path is
fully clear; short distance, or blocked movement with sufficiently small turn
error, restarts the search. Phase two pursues the target, sets attack-ready bit
one for turn errors at most 1023, and uses the path's lateral offsets to create
a detour for masks one or two. Unknown phases still finish.

Reset increments the search angle by `0x1000` with 16-bit wrap. When it wraps
to zero, distance increases by 64; a *signed* distance greater than 767 resets
to 64. Preserve the signed comparison even for unusual high-bit distances.
Movement scaling uses the type-indexed coefficients at `0844BFD4`, stride eight,
and wrapped `(0x8000 - abs(turn_delta)) << 3` before fixed multiplication.
The recovered `093E8` distance also preserves signed division of a wrapped
left-shifted numerator: whole-unit `-32768` coordinates can yield negative
distances.

## Dual rays

`ssa_enemy_path_query` computes a perpendicular to the proposed direction and
offsets both ray origins by 1.5 times the radius. Mode zero offsets their targets
by the same amount; nonzero mode uses four world units at the target instead.
The two original ray calls are `A240` with flags one and predicate `A041`.
Each nonzero ray result contributes bit one or bit two to the output mask.

The CF-nearest flag appends both segment endpoints to the original ten-record
buffer before setting `trace_mode` (original `030056FC`) to one. The native
buffer retains the original full-buffer behavior. Both queries always run;
there is no short circuit. Afterward the origin offsets become `offset_x/y`
(original `03005708/03005704`) and `trace_mode` is cleared. A preexisting mode
value is retained during rays when CF-nearest is false, matching the routine.

The single-ray callback remains an explicit original-query boundary. Ordinary
movement clearance must not silently replace it: it needs `A041` semantics and
the current-entity context. Segment recording is recovered storage behavior;
its final rendering/consumer is outside this module.

## Ownership and errors

Storage must stay stable. Actor/player world queries must be inactive on entry.
Callbacks cannot reenter or change navigation fields, ownership, entity type,
radius or target/player storage. Turn may update the separate facing state;
move can use the world API to update the body; finish can update body flags.
All boundary callbacks are required; there are no silent no-op implementations.
The path context is independently guarded and rejects segment counts above ten.

The public functions return one for completion and minus one for invalid input
or a callback failure. Earlier effects remain on failure. In particular a ray
failure can leave appended segments and enabled trace mode, while prior offsets
and the caller's mask output remain unchanged. Every exit clears the native busy
flag. Callers must decide whether to retry, abandon, or reset after an error.

## Validation

`tests/test_enemy_navigation_rom.py` compiles native C with strict warnings and
UBSan, then compares whole original routines using the user's ROM. Navigation
tests compare all entity bytes and ordered callback snapshots; math and the reset
routine execute their original instructions. Dual-path tests execute original
segment insertion, math and fixed multiplication while controlling only rays.
The BIOS signed-division service is replaced with equivalent host arithmetic.

The suite covers 10,725 original-routine cases: 6,117 navigation/reset cases and
4,608 dual-ray cases. It also verifies 53,548 callback reentry rejections, 31
native guards, and eight callback failure paths. Seeded random inputs include
wrapped positions, negative radii and extreme turn deltas; structured cases
cover every navigation phase, masks, timer/dead-target boundaries, signed reset
thresholds, both ray modes, all result masks and segment counts near capacity.
Results and source hashes are in `native-enemy-navigation-validation.json`.

The module is a navigation building block. The original single wall-ray query
is now implemented in `enemy_ray.c`, and `enemy_queries.c` binds it, targeting
and sector rejection to the CD controller; see `enemy-queries.md`. Other enemy
controllers, combat and the outer dispatcher remain. This is not a completed
game runtime or a playable native executable.
