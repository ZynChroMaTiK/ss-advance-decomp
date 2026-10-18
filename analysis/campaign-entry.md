# Original inventory restoration at level entry

The original level transition restores weapons and ammunition onto a newly
constructed player. It starts the next level at 100 health, with both protection
fields and the pending acquisition queue cleared. The earlier PC prototype
carried the complete inventory snapshot across advancement, including health
and protection. R restart still needs that exact snapshot behavior.

The original completion flow calls `1231C` at `02B26` to save the weapon record at
player+28, then `1E48` accumulates level statistics. Level setup calls `05E4C` at `02076`,
which clears the four player records and constructs the active body with
health 100. `F84C` calls `1240C` for weapon setup. On the existing-progress path,
`020C8` calls `123C4` with the new player+28 record. That helper restores only:

| Record offset | Restored field |
| --- | --- |
| +00 | Equipped weapon, 32-bit |
| +0C | Owned weapon mask, 16-bit |
| +0E..+20 | Ten ammunition halfwords |
| +50 | Requested weapon, set from equipped weapon |

It then calls FC04 on the existing sprite with animation `(current-1)&FFFF` and
period 3. It does not restore health, protection, pending acquisitions or flags.
An invalid animation leaves the sprite unchanged; the inventory copy still
completes. The native `weapon_progress.c` preserves that behavior, with null
pointers rejected before writes.

`ssa_native_game_carry_next` now prepares the advancement entry after a real
completion. It resets the original vitality/queue fields and captures the
equipped usable weapon rather than an unfinished request. The existing PC
fallback for an empty equipped weapon remains. Earned ownership/ammo, score,
H-view and Q history persist. It rejects unfinished or busy games without
changing the output. `carry_capture`/`carry_apply` remain exact entry snapshots
for R and diagnostic resumes; `init_campaign` applies the supplied entry.

The Windows Enter path and normal-input campaign checker use `carry_next`.
The Windows transition fixture now expects the original reset at advancement,
then verifies that R restores that entry after gameplay has changed it. It also
requests a weapon immediately before its completion fixture: advancement must
restore the still-equipped gun rather than the unfinished request. The older
test's hard-coded requested-gun expectation failed and was corrected; production
advancement retains the recovered equipped-gun behavior.

## Original completion capture and weapon statistics

Native `ssa_weapon_progress_capture` now recovers all of `1231C`, the original
completion-side save. It copies current weapon, ownership, ten ammunition
halfwords and the pending requested weapon into the saved record. Unlike the
restore helper, capture preserves the distinction between equipped and requested.

The original helper also updates ten 16-byte rows at `GAME+0x90`:

| Row offset | Value |
| --- | --- |
| +00 | Ownership bit, as a 32-bit zero or one |
| +04 | Ammunition, sign-extended from the inventory halfword |
| +08 | Shot counter |
| +0C | Hit counter |

Every row receives ownership and ammunition, including unowned weapons and
negative ammunition values. With original `settings+0x1B == 0`, shot/hit counters
remain unchanged. Any nonzero byte clears both counters in every row. No other
saved-record, source-record, settings or game bytes change.

`ssa_native_game_weapon_progress` exposes the ongoing-campaign (setting zero)
projection using actual `weapon_game.shots` and `weapon.hits`. It rejects busy or
invalid game states before changing either output and never mutates the game.
The existing `carry_capture` now obtains its gear through this original native
routine while retaining its exact-entry snapshot policy. This does not add
frontend cross-level total persistence, original end statistics, NETRICSA or score
screens; those outer systems remain unfinished.

`test_weapon_progress_rom.py` compares 8,192 whole original `123C4` calls, including
all 256 record bytes and the real FC04 sprite writes. It also compares 4,096
whole `1231C` calls, all 256 possible setting bytes, the full saved/source/game
records and 512 actual capture/restore compositions. Eight null guards and UBSan
pass. `test_player_spawn_rom.py` adds 512 actual `05E4C`/`1240C`/`123C4` compositions
on all 16 maps. Vitality writes and restoration instructions are never hooked;
only sprite allocation and the existing platform memory services are supplied.
These prove construction/restore behavior, not the entire outer campaign loop.
The password/start suite covers advancement and exact restart separately on
every admitted map and difficulty, plus 33 read-only statistics captures with
all six busy guards and complete game snapshots. Its 3,736 checks pass on GCC
ASan/UBSan/leak detection and MSVC. Reports are `weapon-progress-validation.json`,
`native-player-spawn-validation.json` and `password-start-validation.json`.

The original vitality reset changes later combat and supply choices. The
normal-input eleven-map replay now takes 140,712 frames and ends with
1,544,000 score / 45 health. The earlier 136,552-frame full-vitality-carry run
is a historical checkpoint. The final map has a separate standalone completion;
its earned-inventory campaign path remains unfinished and unavailable.

The decomp.wtf candidate in `recovered/weapon_progress_restore.c` matches all 72
bytes / 36 instructions of `123C4` exactly. Export now rebuilds the original ROM with
75 functions in C; its remaining code/data stays assembly. This is separate
from native behavioral coverage and does not make the whole port complete.

The capture candidate `recovered/weapon_progress_capture.c` reached 86.8% after
five plain-C compiler attempts, with the same 168-byte / 84-instruction length.
Register allocation, literal placement and loop addressing differ. It is **not**
an exact match and its claim was released with notes. Exact matching therefore
was 75 functions / 2,224 of 87,648 catalogued bytes (2.54%) at that checkpoint.
Subsequent statistics helpers add three exact matches; current live progress is
78/526 and 2,816/87,648 bytes (3.21%). See [statistics.md](statistics.md).
