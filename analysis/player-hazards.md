# Player interactions and lethal floor checks

The PC frontend now enables `SsaPlayerGameplay.immediate_use`. Type-2 buttons
activate on the first valid E press without waiting for the weapon's fully
stowed `DC00` action. `ssa_player_hazards_step_immediate_use` bypasses only that
eligibility check: it retains the actual weapon action, use-key requirement,
display-local flag8 gate, contact range/order, callbacks, latches and hazards.
The original entry point and default gameplay option remain unchanged.

`tests/test_immediate_use.py` and `scripts/test-immediate-use.ps1 -SkipBuild`
pass1,765 checks /96 scenarios on GCC ASan/UBSan/leaks and MSVC: all eight real
buttons, both views, ready/partially lowered/stowed/firing weapons, original-mode
denial and no-use denial. A single down/up E pulse activates on its first tick,
before stowing completes. Original texture change, sound41 once and repeat latch
are verified. Controlled player placement and test invulnerability are fixtures,
not campaign progression proof. Reports:`immediate-use-validation.json` and
`windows-immediate-use-validation.json`. Original hazard recovery still passes
24,576 ROM comparisons. Weapon handoff still passes3,295 Windows checks.

`src/player_hazards.c` recovers the complete `08005680` stage and its two
dependencies, `080055BC` and `08005548`. It also recovers the uncatalogued
`08003464` sector sampler and the contact-list behavior of `0800429C`.
The already matched 20-byte wrapper remains an exact match; this native work
does not add another byte-matched function to the catalogue.

## Recovered behavior

Interactions run before hazards. The weapon record's word at `+10` is compared
with `0xDC00` once, before any callback. This is a weapon action value, not a
weapon ID. Interactions are skipped when the **display-local player's** flag
`8` is set, even when the player being processed is a different player. That
gate does not skip the later hazard checks.

Otherwise, the original eight-bit entity facing produces a point 64 map units
ahead. The swept radius rectangle collects intersecting special lines in block
X-then-Y order, with duplicate lines suppressed. This is a different query from
movement: the original `097EC` iterator ignores the wall visitor's return and
**continues after solid or closed walls**. The native dedicated query preserves
that behavior and exposes its ordered special-line result. Movement collision
scratch cannot be reused while contacts remain in use.

Type-1 lines invoke activation `0801C0D4`. Type-2 lines invoke switch activation
`0801C518` only when the player's `keys & 0x10000` is set and the earlier weapon
comparison succeeded. The button value is read again for each contact, while
weapon eligibility remains latched. Those two script routines remain explicit
callbacks; this module does not implement moving-sector state, switch animation,
or their audio effects.

The hazard stage then reads the current entity sector. Sector byte `+0A` equal
to `255`, or equal to `35` when the separate level-state halfword at `+10` is
greater than `5`, kills a player whose signed integer Z equals that sector's
floor. The fractional Z is discarded, so a Z less than one unit above the
floor still qualifies. Death clears entity health and player stats `+C4/+C6`.
The host API writes the authoritative `SsaInventory.health/stat_c4/stat_c6`;
the runtime adapter must mirror health into its other views.

The second lethal check samples a sector from the player's radius once per
frame, then tests membership in the active hazard-sector list. Original
`GAME.frame_counter & 15` selects these offsets:

| Frame nibble | X offset | Y offset |
| --- | --- | --- |
| 0, 9 | 0 | 0 |
| 1, 10 | -radius | -radius |
| 2, 11 | +radius | -radius |
| 3, 12 | -radius | +radius |
| 4, 13 | +radius | +radius |
| 5, 14 | -half-radius | -half-radius |
| 6, 15 | +half-radius | -half-radius |
| 7 | -half-radius | +half-radius |
| 8 | +half-radius | +half-radius |

Half-radius division truncates toward zero. Coordinates wrap at 32 bits before
the original BSP lookup. An empty hazard list skips the lookup entirely. This
check is independent of Z, so a jump does not avoid an active scripted lethal
sector. A jump can avoid the floor-height check once the integer Z changes.

## Native API and ownership

`SsaPlayerHazards` references the existing entity, movement and inventory, plus
the facing byte and selected weapon action word. `SsaPlayerHazardGame` supplies
the frame counter, active signed sector list, level counter and independent
display-local flags. These are recovered views, not GBA RAM structure overlays.

`ssa_player_hazards_step` composes interactions and both hazard checks.
`ssa_player_hazards_apply`, `ssa_player_hazard_sample` and
`ssa_player_interaction_query` expose the original substeps. Query scratch is
allocated/freed with the existing map-collision API. Its map, layout, backing
storage and player pointers must remain stable. Script callbacks may change
gameplay fields and map contents but may not replace pointers, free storage,
change map structural metadata, touch scratch or reenter this routine.

Invalid native inputs, invalid geometry and callback errors return `-1`.
Successful steps/queries return zero; the sampler returns zero or one.
Callback errors stop later effects and preserve earlier changes. The native
API does not promise rollback or emulate out-of-bounds original RAM reads.
The interaction query's only promised output is the ordered contact list;
original unused scratch heights and wall-query globals are not host state.

## Verification

Run `.venv-linux/bin/python tests/test_player_hazards_rom.py` under Ubuntu/WSL.
The strict-warning native build enables undefined-behavior checking. Unicorn
executes the original whole routines with only `1C0D4` and `1C518` controlled;
all original trigonometry, BSP, block traversal, wall-contact logic and hazard
membership code execute unchanged. Tests compare complete 256-byte player and
116-byte entity records plus ordered callback inputs and contact lists.

The generated `native-player-hazards-validation.json` reports:

- 8,192 whole-stage comparisons on all 16 actual maps.
- 12,288 isolated sector-sampler comparisons, including signed radius halves
  and wrapped 32-bit coordinates.
- 4,096 independent contact-query comparisons covering all 256 facing bytes
  on each map.
- 4,601 ordered callback states, including callbacks changing button edges
  and weapon records to check reload/latching behavior.
- 96 native guard/failure checks, including callback failure and rejected
  player pointer replacement.

Total: **24,576 original-routine comparisons**. Some cases retain real geometry
while marking eight nearby walls as triggers to exercise multiple contacts.
An initial test that marked every wall caused original contact-array overflow
into adjacent globals; the revised fixtures stay within the original ten-slot
storage. This was an oracle fixture failure, not hidden passing evidence.

The module recovers interaction dispatch and hazard decisions. It does not
provide the unresolved script effects, combat, enemy AI, renderer or OS input.
`level_runtime.c` now connects its line callbacks to recovered native mover and
switch activation and supplies the live closing-door hazard list. See
`level-runtime.md` for integration ordering and remaining effect boundaries.
