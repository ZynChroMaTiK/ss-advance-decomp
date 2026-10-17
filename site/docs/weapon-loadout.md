# Initial weapon loadout

`weapon_loadout.c` recovers complete `0801240C`, called by sprite initialization
wrapper `0800F84C` before its `080101F4` call. It initializes each indexed player
whose original player flags at `+BC` contain bit 4. Those flags are supplied as
`SsaWeaponControl.keys`, independently of held/pressed weapon buttons.

For every active player, it allocates a sprite through the explicit `F9B4`
boundary, replaces the weapon object pointer, clears weapon flags, sets owned
weapons to bit 0 and ammo for weapon 1 to 999, and empties the pending-weapon
count. The sprite receives X=`0x7800`, Y=`0xA000`, type 2, layer 14 and owner
equal to player index plus one. Other sprite fields retain their allocated
values until the subsequent animation setter modifies them. An allocator can
use the existing `ssa_weapon_object_init` defaults.

Original settings byte `+27` selects a loadout. Zero does not read the table and
leaves ammo for weapons 2–10 unchanged, although their ownership bits are reset.
For nonzero settings, nine signed halfwords at `0844BCF0 + setting * 20` provide
ammo for weapons 2–10. Each nonzero entry grants its weapon bit and stores the
signed minimum of the entry and that weapon's signed capacity. Zero entries
clear ammo and do not grant the weapon. Negative entries/capacities retain the
original signed behavior. The tenth halfword in each record is unused here.

The data loader copies the **36** nonzero-setting records through `0844BFD3`.
They form three repetitions of the twelve observed records. The next data
begins at `0844BFD4`; setting 37 or above is rejected natively instead of reading
unrelated data. This API preserves the original setting value and does not
assign a new menu or campaign meaning to it.

Any nonzero settings byte `+26` then overrides ammo for weapons 2–10 to 999 and
grants all ten weapons. Requested weapon starts at 1; original previous-weapon
cycling selects the highest owned weapon with nonzero ammo. The selected ID
becomes current, and native `FC04` sets animation=current−1 with period 3.

One original detail is preserved exactly: **only cooldown[player index] is
cleared**. The apparent ten-iteration loop has a single halfword store after
the loop and an address indexed by player index. The other nine cooldowns
retain their input values. Inactive players retain all fields and allocate no
sprite. Health, pending weapon contents, movement, jump and camera state are
outside this routine and are not reset.

The host API accepts 0–4 indexed player projections. Inventory, control,
cooldown arrays and object storage remain caller-owned and stable. Its shared
loadout state prevents callback reentry. Invalid projections are rejected
before mutation. Allocation failure or a NULL object returns -1 instead of
the original NULL dereference; previously initialized players remain changed.
Callbacks may allocate the requested object but must not mutate projections,
replace/free their pointers or reenter. Runtime integration must also enforce
its existing player/world update ownership rules.

`tests/test_weapon_loadout_rom.py` passes **12,979** complete original routine
comparisons. It covers all 37 settings including zero, counts 0–4, every active
mask, god-mode bytes 0/1/255 and 4,096 signed table/capacity fuzz cases. Only
`F9B4` allocation is controlled: original `12050`, `11FFC` and `FC04` execute.
Ordered allocation snapshots and all four 256-byte player records and 48-byte
sprite records match, including inactive players and untouched fields. The
suite also checks **17** guards, **13,018** callback reentry rejections and **2**
allocation failure paths. Strict compilation and undefined-behavior sanitizer
pass. Counts and source hashes are in `weapon-loadout-validation.json`.

This is native behavior recovery, not a byte-identical C match for `1240C`.

`ssa_weapon_runtimes_start` now binds this initialization to real native player,
combat and weapon runtimes. See `weapon-start.md` for 608 starts, subsequent
hitscan/projectile firing, two-player failure cleanup and jump/H verification.

The PC frontend now distinguishes fresh level entry from existing progress.
Passwords and explicit `--level N` launches select the matching ROM row
(`difficulty*12 + N + 1`), including its capped ammo and initial selected gun.
Ordinary New Game retains original setting0. The host helper
`ssa_native_game_init_campaign` uses a later map's starter row only when no
progress is supplied. A supplied `SsaNativeCarry` takes precedence completely:
weapons, depleted ammo, pending acquisitions, selected/previous weapon, health,
armor, score and H mode carry through. R restores the captured entry unchanged.
No per-level starter grant is added on normal advancement. This follows the
user's explicit fresh-start/carry distinction; it does not claim recovery of
the entire original outer campaign dispatcher.
