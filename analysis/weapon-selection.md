# Weapon acquisition and selection

PC order update (2026-10-22): UMT is last, after cannon. Selection slots are
Chainsaw, Colt (single/dual variants), Double Shotgun, Tommygun, Minigun,
Rocket Launcher, Prion Gun, Time Strike, Cannon, UMT. Keys8/9/0 select the
last three respectively. Mouse wheel, Classic HUD weapon/ammo rows, starting
weapon choice, pickup priority and empty-ammo fallback share this order.
`weapon_order.h` maps selection slots to unchanged internal ROM IDs; inventory,
ammo, loadout contents, saves and Q's previous-weapon identity are unchanged.
Native games enable `SsaWeaponRuntime.pc_weapon_order`, which uses the separate
`ssa_weapon_update_pc`/`ssa_weapon_cycle_pc` adapters. The recovered APIs below
retain original ROM ordering for differential validation.

Validation: Windows build and weapon-wheel checker pass31,073 checks, including
all1,023 nonempty ownership masks, key/wheel events and runtime UMT depletion.
Classic HUD passes385,369 checks under ASan/UBSan with leak detection, and all11
focused Windows regressions pass. Original selection still passes29,176 ROM
comparisons,34 guards and18 pickup integration cases. No campaign playthrough.

`src/weapon_select.c` implements the original pending-weapon and selection
phase, using the native inventory structure and ROM-loaded ammunition limits.
It contains no emulator or platform input dependency.

| Original routine | Native API |
| --- | --- |
| `08011588` | `ssa_weapon_take_request` |
| `08011604` | `ssa_weapon_update` |
| `08011FFC`, `08012038`, `08012050` | `ssa_weapon_cycle` |

The separate `SsaWeaponControl` stores current weapon (original player word 28),
weapon flags (word 30), requested weapon (halfword 78), and selection keys
(halfword BC). `SsaInventory` remains unchanged. Current and requested weapons
are deliberately separate: this phase requests a switch; the unported weapon
animation state machine subsequently commits the current weapon.

Flags `0x2C00` suppress request consumption. Otherwise the request routine
first pops a pending weapon from the end of the inventory stack. If the stack
is empty, it clears and consumes key `0x40` as next-weapon, then key `0x80` as
previous-weapon. If neither key is pending and current ammunition is exactly
zero, it requests previous-weapon automatically. Negative signed ammunition
does not trigger this automatic request.

The update grants the queued weapon's ownership bit and half its ammunition
capacity, rounded down, using the original ammunition helper. It automatically
requests that weapon only when the **entire old ownership mask is numerically
less than the new weapon bit**. This original condition compares neither the
current weapon nor its requested replacement. Duplicate pending grants still
add ammunition, subject to the original capacity rules. Automatic selection or
an explicit cycle request sets flags `0x2100`.

Cycling advances before testing, wraps across sixteen bit positions, and picks
an owned weapon whose signed ammunition is nonzero. It starts from the
requested weapon. Negative ammunition is eligible. A sole usable weapon can
therefore select itself after a full rotation.

## Native validity contracts

Recovered weapon IDs are 1–10. Cycling rejects ownership bits for unsupported
slots 11–16 rather than indexing past the native ammunition array. Queued IDs
outside 1–10 and counts above ten are rejected. A selector with no eligible
weapon returns `-1`; the original would continue looping indefinitely.
Only direction +1 or -1 is accepted. The complete update applies changes to
local copies first, so invalid integration leaves both input structures
unchanged, including an otherwise-consumed key or stack entry. These guards
are native behavior and are tested separately from original valid inputs.

## Verification and integration

`tests/test_weapon_select_rom.py` passes **29,176 ROM comparisons** with no
hooked dependencies. The original helper calls execute their actual ROM
instructions. Tests compare complete 256-byte player records and unchanged
game state, plus the request routine's defined return value:

- 4,700 request operations covering blocked flags, LIFO priority, keys,
  current ammo and signed extrema.
- 16,476 complete acquisition/selection updates, including all 1,024 valid
  ownership masks against each queued weapon, randomized cases and 1,536
  consecutive update frames.
- 4,000 next-weapon and 4,000 previous-weapon selections, including wrapped
  starting values, unavailable weapons and negative ammunition.

Another 34 checks cover native guards and atomic failure. Eighteen native
integration scenarios run the actual pickup dispatcher, enqueue every one of
the nine weapon pickups, then run the actual acquisition update. They verify
removal/count effects, delayed ownership, ammunition grants, duplicates,
requested selection and rejection of later already-owned weapon pickups.
The test library is built as strict C11 with UndefinedBehaviorSanitizer.
Evidence is `native-weapon-selection-validation.json`.

Firing, weapon animation and current-weapon commit, damage, UI rendering,
OS controls and the game loop remain unported. These tests establish native
pickup-to-inventory functionality, not visible or playable combat.
