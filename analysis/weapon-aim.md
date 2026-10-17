# Native weapon aim

`weapon_aim.c` recovers 0800A4F4 and its A09C predicate, together with dynamic
blockmap entity iteration 9870 and diagonal interception 9B54. Existing native
A240 traversal, wall collection, fixed arithmetic and 9C98 openings are reused.
This implements original automatic vertical aim; mouse pitch is a separate
port extension and is not silently substituted for this behavior.

Initialize a zeroed `SsaWeaponAim` with the stable world and math tables. Its
dedicated wall stamps, intercept array and entity-reference array never replace
movement/camera scratch or `world.blocking_entity`. Free before world teardown.
The original walls-first, entities-second order is preserved within each cell.
Entity order is the actual world block list, including repeated visits to a cell;
walls are stamped once. Capacity permits 64 visits times world entity capacity,
plus all map lines. Entity references are borrowed for the synchronous call.

`ssa_weapon_aim_query` accepts source, independent X/Y start distances, Z offset,
16-bit heading and range. X/Y distances and range lose their fractional bits
before multiplication by original sine/cosine. A240 still nudges starts exactly
on block boundaries and stops after 64 visits. The original debug start-Y global
incorrectly stores endpoint Y; the native context preserves this discrepancy
while tracing from the correct start Y.

Each query resets target and vertical aperture to +/-0xA000. Two-sided walls
clip that aperture only where sector floor/ceiling heights differ. An entity
must carry flag 2 and differ from source. Its clipped vertical interval selects
the slope: ordinary entities crossing slope zero use zero, whereas D7/D8 bosses
use the truncated midpoint. First eligible intercept wins, including stable
ties. Entity diagonal selection uses signed `(dx XOR dy) > 0`, including its
zero case. Positive entity fractions beyond range are retained in collection
and ignored by ordered traversal; unlike walls they are not capped at 65536.

`marker` (03005074) and `traversal.blocking_fraction` (0300505C) intentionally
retain stale values if the original made no write. The weapon controller seeds
them before its sequence of aim retries. Query output slope is zero when no
target exists even if marker remains nonzero.

`ssa_weapon_aim_trace` exposes the same flags-3 collector for hitscan. Wall IDs
are below `map.line_count`; larger IDs index `entities[id-line_count]`. It resets
collection and wall stamps but preserves aim globals. The predicate executes
while the world query lock is held and must not mutate world ownership or
reenter. Gameplay damage/presentation must occur after the synchronous trace.
Invalid calls return -1; earlier workspace effects can remain after an error.

`tests/test_weapon_aim_rom.py` passes 8,192 complete A4F4 comparisons on all 16
maps with original trig, division, DDA, collection and predicate executing
unhooked. It compares target, slope, persistent globals, opening values, trace
geometry, every ordered intercept and final fractions. Cases include live
height edits, cardinal/random angles, fractional/negative starts, zero ranges,
duplicate positions, flags, bosses, and head-first entity list ordering.
Results: 3,086 target acquisitions, 5,106 misses, 29,871 entity intercepts and
3,433 wall intercepts; 144 native guards. UndefinedBehaviorSanitizer is enabled.
Hashes and evidence are in `native-weapon-aim-validation.json`. This behavioral
recovery is not an exact-match candidate or a playable weapon implementation.
