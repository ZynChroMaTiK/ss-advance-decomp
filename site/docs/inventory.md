# Native inventory and pickup statistics

`src/inventory.c` recovers seven complete original routines. The host structure
contains gameplay fields rather than mirroring GBA pointers or padding.

| Original routine | Native API | Behavior |
| --- | --- | --- |
| `080114B8` | `ssa_inventory_has_weapon` | Test the ownership bit for weapon IDs 1–10 |
| `08011558` | `ssa_inventory_queue_weapon` | Append an unowned weapon ID to the pending weapon stack |
| `080114D4` | `ssa_inventory_add_ammo` | Add signed ammo, subject to ownership/multiplayer and capacity |
| `08011534` | `ssa_inventory_fill_ammo` | Apply an addition of 2,000 to each weapon |
| `0800549C` | `ssa_inventory_add_health` | Update the player's entity health |
| `080054D8` | `ssa_inventory_add_stat_c4` | Update the player statistic at original offset C4 |
| `08005510` | `ssa_inventory_add_stat_c6` | Update the player statistic at original offset C6 |

The stat names C4 and C6 remain deliberately descriptive of their original
locations: their UI meanings have not been established by this recovery.
Health belongs to entity offset 32 in the original, reached through the player's
AC pointer. Native game integration must keep that health synchronized with its
entity representation.

Ownership is player halfword 34; ten signed ammo counts begin at 36. The pending
count is halfword 7A, followed by ten 32-bit weapon IDs at 7C–A3. Queueing a weapon
does **not** set its ownership bit or grant ammo. Thus multiple pickups of the
same unowned weapon can append duplicate entries before the weapon update
consumes the stack. The subsequent acquisition/selection phase is now ported
separately in `src/weapon_select.c`; see `weapon-selection.md`. The native queue
refuses to overwrite the subsequent input fields
when count reaches ten. This guard is a new host safety behavior, not a claim
about safe behavior in the original for malformed/full queues.

Ammo limits are loaded as data from ten 12-byte ROM records beginning at
`0x44BC8C`, halfword offset 6. Their values are 999, 999, 100, 300, 300, 25, 10,
3, 10 and 10. Native code executes no ROM instructions. An unowned weapon can
receive ammo only when the original player-count byte exceeds one. The helper
returns success for an eligible addition even if the requested amount is zero
or negative. Signed 16-bit addition wraps before comparing the result with the
limit, preserving the original arithmetic.

All three statistic helpers truncate the amount to signed 16 bits. An amount of
100 adds directly, without the usual cap or precondition. Other amounts are
applied only when the old signed value is below 100, then capped at 100 after
16-bit wrapping. In particular, the helper itself does not cap a 100-point
health pickup at 200; the pickup dispatcher has separate eligibility gates.

## Verification

Run `tests/test_inventory_rom.py` through the workspace WSL Python environment.
It compiles strict C11 native code with UndefinedBehaviorSanitizer and executes
the original seven routines as independent test oracles, with **no hooked
dependencies**. Entire 256-byte player and 116-byte entity records are compared,
including unchanged bytes; defined return values and unchanged game-state
bytes are also checked.

The suite passes **31,740 ROM comparisons**: 10,240 ownership tests covering all
ten-bit masks and weapon IDs; 2,000 queue operations; 11,600 ammo additions;
1,000 complete fill operations; and 2,300 cases for each statistic helper.
Cases cover ownership, player counts 0/1/2/255, signed extrema, overflow,
truncated arguments, capacities and 100-point exceptions. Another 31 checks
cover native pointer, ID, queue-capacity, player-count and ROM-span guards.
Machine-readable evidence is `native-inventory-validation.json`.

The three original stat helpers (`0800549C`, `080054D8`, `08005510`) also compile
to their exact original bytes using decomp.wtf and the pinned GBA compiler:
172 bytes across three functions. Each matched on the first manual compile
using reconstructed structs and ordinary conditional additions. Sources are
`recovered/inventory_stat_health.c`, `inventory_stat_c4.c` and
`inventory_stat_c6.c`; engine responses are recorded in
`inventory-match-attempts.json`. The native inventory structure and APIs remain
separate from these GBA matching candidates.

This is a native gameplay subsystem, not a playable game. Weapon animation and
current-weapon commit, firing, damage, UI and the complete game loop remain separate
work. The ROM oracle is a test dependency only.
