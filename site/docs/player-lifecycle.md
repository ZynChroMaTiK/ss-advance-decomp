# Player death and respawn

`src/player_lifecycle.c` ports the complete control flow of `08006154` and
`08005FEC` onto the recovered host state. These are behavioral ports, not
byte-exact matching candidates. The original ROM remains unchanged.

`test-player-lifecycle` runs 10,494 differential comparisons: 1,422 death
boundary cases, 6,000 randomized death calls, 1,024 direct respawns and 2,048
consecutive frames. It compares 85 known host fields and their values at every
ordered external callback. The original nested respawn and `08005E04` copy
execute directly. Twenty native guards/failure paths also pass. GCC uses
strict warnings and undefined-behavior checks. See
`native-player-lifecycle-validation.json` for the ROM and source hashes.

The test controls spawn selection, spatial unlink/insert, animation, notices,
mode transitions, sound and effects. It services memcpy and BIOS/DMA zero fills.
These are explicit dependency boundaries, not evidence of a complete game.
Unknown original player bytes are not represented by native structures, so the
comparison is a projection of recovered fields rather than all 256 player bytes.

## Death behavior

Only health **exactly zero** starts death when player flag `0x10` is absent.
Negative health alone does not trigger it. The routine sets player `0x10` and
body `0x20`, requests animation `0x68` with period 8, then either consumes the
single-player continue flag and starts at 500 ticks or emits the normal death
notice/sound and starts at zero. The display-local index is used for the notice;
it is distinct from the movement/protection player index.

A dead player's eye offset above four fixed-point units receives eye velocity
`-2 * ticks`; otherwise body flag `0x40` is set. The death timer is an unsigned
16-bit addition and wraps. Values 301 through 399, inclusive, reset the timer
and request input state 4 in single player, state 3 in multiplayer exit mode,
or execute respawn. Values above 590 trigger mode 10 and notice `0xB4` only in
single player with the continue flag zero. Skipped timer ranges remain skipped;
the port does not replace these tests with broad `>=` thresholds.

## Respawn behavior

The original saves player `+28..A3`, clears the player record, unlinks its body,
and asks `08001B18` for a spawn marker. The native port clears known non-weapon
player fields and preserves the corresponding inventory/control fields. Body
state, including its spatial metadata, is not indiscriminately cleared.

Heading is the marker's low eight bits shifted left eight; X/Y become 16.16.
Marker Z is overwritten by the body's **old floor** before reinsertion. New
floor metadata from insertion does not retroactively change this Z. The body
receives type `0xC9 + player index`, radius 32, height 100, flags `0x98`, health
100 and protection timer 60. Player flags become 4 and eye offset becomes 96.
Player/body coordinates, velocities and facing are copied, then effect 5,
animation `0x65`/4 and insertion occur in that order.

The saved weapon block is restored after insertion. Current/requested weapon
and ownership become 1, first ammo becomes 999, pending count and weapon flags
become zero. Other ammo slots and pending entries remain preserved. The weapon
display object receives X/Y `0x7800`/`0xA000`, flags 2, kind 14 and one-based
player number, then weapon animation 0 with period 3 is requested. Health is an
entity field, so it is not replaced by the saved weapon block; player `+C4/+C6`
stats are cleared rather than restored.

## Host API and integration

`SsaPlayerLifecycle` references a stable `SsaPlayerSpawn`, frame, camera, weapon
control and weapon-display view. Its death timer is separate from the frame's
entity protection timer. `ssa_player_death_step` can invoke native respawn;
`ssa_player_respawn` exposes the same operation directly. Both return 0 on
success and -1 on invalid state or a failed dependency.

Inventory health and movement flags are authoritative; entity-health and
vertical-flag mirrors are refreshed at boundaries. Weapon control flags are
authoritative during these calls and must equal movement weapon flags on entry.
Every external event and the selector is explicit in `SsaPlayerLifecycleOps`.
Callbacks must preserve pointer identities and storage, cannot reenter, and
return nonnegative success or negative failure. Animation callbacks update
animation state, and world callbacks maintain ownership/list invariants.

An error retains earlier effects, potentially an unlinked body; it is not a
transaction. The host's native-only jump latch and cached camera pose need
refresh after respawn. The new `player_gameplay` adapter connects these
boundaries to native spatial lists, spawn selection, and the existing runtime.
Rendering, animation playback, audio, notices and main-loop state transitions
remain outside this module.
