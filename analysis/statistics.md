# Original completion statistics and checkpoints

The native backend now recovers the complete original `08001E48` completion
accumulator and the `08001CF4` / `08001D9C` checkpoint helpers. All three also
compile byte for byte through the requested local decomp.wtf engine, using
ordinary C in `recovered/level_statistics.c`,
`recovered/statistics_checkpoint_capture.c` and
`recovered/statistics_checkpoint_restore.c`. They add 592 exactly matched bytes.

## Completion accumulator

Original completion calls `1231C` before `1E48`. The former captures weapons
and refreshes their statistics rows; the latter performs these writes:

| GAME destination | Source / operation |
| --- | --- |
| +2F0 | Replace with score at +88 |
| +2F4 | Replace with signed body health at local player body+32 |
| +2F8, +2FC | Replace with signed local player protection C6, C4 |
| +300 | Replace with unsigned level byte at +7C |
| +304 | Add elapsed at +0C |
| +308 | Add all 12 spawned counters at +130, stride12 |
| +30C | Add all 12 kill counters at +138, stride12 |
| +310 | Add all 12 player-hit counters at +134, stride12 |
| +314 | Add all 10 weapon **hit** counters at +9C, stride16 |
| +318 | Add accepted pickups at +31C |

The weapon shot counter at +98 is not added. Pickups are accepted items, not
all map objects or secrets: original `1B99C` increments +31C only after an
accepted ordinary contact. Score replaces the saved score; it is not added
again. Arithmetic wraps at 32 bits without saturation. Health and protection
sign-extend from 16 bits. No input counter is cleared by `1E48`. There is no
once-only latch; a second call adds the same counters again.

`ssa_statistics_complete` in `src/statistics.c` implements this with value-only
`SsaLevelStatistics` and `SsaStatisticsTotals` projections. The caller must
commit once per actual completion. Null arguments reject before writes.

## Checkpoint helpers

`1CF4` copies ten complete 16-byte weapon rows from GAME+90 to +1C0 and twelve
12-byte enemy rows from +130 to +260. `1D9C` copies those rows back and clears
only current accepted pickups at +31C. Both preserve elapsed, score, player
vitality, level and campaign totals. The saved rows are not cleared. Native
capture/restore helpers retain these exact boundaries; restoring statistics is
not player respawn, level initialization or a complete continue workflow.

## Native snapshot adapter and limits

`ssa_native_game_statistics` in `src/native_statistics.c` collects the live
native weapon rows, activation counts, combat counters, elapsed time, score,
accepted pickups and player vitality. It uses canonical inventory health and
protection; body health is a native mirror. It copies values only and preserves
the entire game. It rejects unloaded/error/busy games without changing output,
using the same six ownership/query guards as the existing weapon snapshot.

This snapshot observes ongoing counters; it does not run the mutating `1231C`
password-setting counter reset. The new [completion adapter](completion.md)
preserves original capture/accumulate/checkpoint ordering, the setting policy and
bonus-adjusted score. The frontend now commits campaign-session totals and
checkpoint rows after the visible bonus tally, and carries the resulting score
to the next level and its R entry snapshot. See [pause/tally integration](completion.md).
Disk persistence of campaign totals, continue/death orchestration and high-score/
name entry remain pending. All twelve campaign maps are enabled for manual validation.

## Verification

```powershell
.\decomp.ps1 test-statistics
.\decomp.ps1 test-native-statistics
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-statistics.ps1 -SkipBuild
```

`statistics-validation.json` records 8,192 complete original-ROM comparisons per
routine (24,576 total), plus 512 original save/mutate/restore/twice-accumulate
sequences and six null guards under UBSan. No callee is intercepted. Tests
compare whole GAME/player/body blocks, including every untouched byte; they
cover overflow, signed vitality boundaries, distinct shot/hit counters and all
256 level byte values.

`native-statistics-validation.json` and
`windows-native-statistics-validation.json` agree on 27,699 checks, 48 starts,
3,072 actual native gameplay frames, 288 read-only snapshots and 480 rejection
cases across all 16 maps and three difficulties. Jump, H-view and a quick mouse
click are delivered through the actual PC input API. Directed counter fixtures
check every column, high 32-bit values and canonical health separately from
ordinary gameplay. These fixtures are not campaign-completion evidence. GCC
ASan/UBSan/leak detection and MSVC pass.

All 109 portable CMake libraries and 110 Windows libraries build. The complete
portable build exposed an existing Windows-only profile disk test outside a
platform guard; CMake now includes that target only under `WIN32`. Both full
builds pass after that correction. The final audit checks 565 current source
hashes across the four statistics/build reports and confirms the playable
Windows executable remains unchanged.

Live exact matching is now 78/526 functions and 2,816/87,648 catalogued bytes
(3.21%), after 541 compiler attempts. The imported catalogue is incomplete and
does not represent whole-game coverage. The 78-C export and independent rebuild
retain the original ROM SHA-1 `4ff48d8c18b70a5184758f1917343ae85a70858e`.
