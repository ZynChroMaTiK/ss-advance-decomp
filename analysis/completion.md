# Original timed completion tally and record pipeline

The native backend now recovers the single-player tally loop inside original
`08002564`, followed by the completion data sequence at `08002B20`:
`1231C` weapon capture, `1E48` totals accumulation, `1CF4` checkpoint capture,
then increment the level. These are available through `completion.c` and
`native_completion.c`. The Windows frontend now runs this tally and commits its
bonus-adjusted record before advancement, chapter panels or the ending.

## Playable pause and completion screens

Esc/P opens Continue/Quit over a frozen level. Quit opens the original localized
Yes/No notice with No selected, then returns to Main. Application exit is the
separate bottom-row Quit on Main. Arrows/WASD, Enter/Space and mouse hover/click
select; Esc cancels the confirmation or resumes from the root. The pause adapter
in `pause.c` binds the existing recovered `mode_control.c`/021E4 with one player,
queues its actual commands/notices and retains their prefix state changes.
It is not another replacement controller. Network pause flags are outside this
single-player presentation owner.

`intermission.c` owns a separate84-slot sprite pool/cache/OAM, borrows frozen
game assets/math, and renders the original E914/E9D4 localized notice glyphs,
ECF8 score digits and EE60-compatible bonus spacing. Pending digits use Y88/120;
score remains at Y8. It uses the original30-entry text blend table. The PC layer
darkens the captured world over30ticks and marks the selected pause row white;
those composition choices are host adaptations, not pixel-equivalence claims.
Draws never tick gameplay or the tally. The high-resolution software scene is
retained; GBA remains240x160. Private OAM copies remove old notices from pause's
backdrop and remove all old HUD sprites from completion's backdrop.

Completion starts automatically from the living native input-state3 boundary.
The host calls the original tally once per60Hz presentation tick. Held mouse
fire/Enter/Space accelerates counting. A fresh click/Enter/Space settles the
current time/ammo bonus in full, including during its initial hold. This PC
policy lives in intermission.c; the recovered completion.c remains unchanged.
The final message stays for180 fully visible ticks (three seconds); a separate
fresh confirm after60 visible ticks can shorten that reading hold. Glyph loading
does not consume the window. Both paths still wait for the actual music player
to finish, including muted playback; missing audio is the explicit silent path.
Tally command16 plays the existing count cue. Music9 is confined to completion.
The original wrapped negative par difference can exist briefly at stage3;
the native display hides it instead of overflowing the original8-digit scratch.

After the music/reading hold finishes, the frontend calls `ssa_native_completion_record`
once, stores totals/checkpoint rows for the campaign session, and puts the final
score into the next-level carry/entry snapshot. Reload does not accumulate the
same counters again. Password/standalone setting-byte policy reaches record
capture; carried campaign starts use setting0. Ammo is retained. Chapter6 and
ending12 routes run after recording; R on the next map restores that bonus score.
Existing progress unlock/save behavior remains separate. High-score lists and
name entry are still pending, and campaign-session totals are not a new disk-save
format.

Audio commands4/5 now suspend/resume existing native music/effect instances and
their envelope/sample/sequence clocks. New menu cues remain independent; quit's
command6 retires the old instances. Synth controls mute the paused music tracks
and restore them on resume. Suspension is immediate in the PC mixer; the original
temporary audio fade envelope and physical MIDI-device timing are not claimed
identical by this change.

Run `decomp.ps1 test-pause`, `test-intermission`, `test-completion` and
`test-native-completion`, then `scripts/test-intermission.ps1 -SkipBuild` after
building Windows. The first uses8192 additional ROM comparisons of adapter
states and ordered events; the full mode controller's existing13092 comparisons
remain separate. Screen tests cover36 map/language tally starts, actual glyph
pixels and repeat draws, frozen source state, late zero-time bonuses, music holds,
pause confirmation and sound suspension. Windows hidden-window tests drive real
Esc/P, keyboard and mouse paths in both renderers, all3languages, six next-level/
chapter/ending routes and bonus-score handoff. They use controlled end boundaries;
they are not campaign playthrough evidence. Previews are written under
`build/intermission-tests/`. See `intermission-validation.json`,
`windows-intermission-validation.json` and `pause-rom-validation.json`.

Current results:143825 ASan/UBSan/leak checks and69917 tally frames, plus6015
Windows checks over12pause paths and6completion routes; GCC/MSVC core counters
agree. The original tally still passes11466 ROM frame comparisons, and its record
adapter passes410217 checks/9216records. Existing audio regression passes654862
checks across131songs without decoder errors. All121Windows libraries/frontend
build. Startup/restart/concurrent launches pass both renderers. Portable and
Windows pause/bonus/Sam previews were visually checked. Current frontend SHA256:
`bd3c2cefd14237c5f499fb0adcf2fa3f532033475fd0ca103f6c2824c96c165d`.
No campaign playthrough, ROM modification, process termination or new exact
match was performed. Live decomp.wtf status is78/526 functions,
2816/87648catalogued bytes(3.21%),549attempts; the catalogue is incomplete.

## Bonus and timing behavior

The original loop increments GAME+00 once per presentation tick while gameplay
elapsed time stays frozen. It reads the twelve original par times at `0844A610`:
120, 300, 300, 420, 360, 360, 300, 360, 360, 360, 360, 360 seconds.

| Phase | Original behavior |
| --- | --- |
| Time bonus | Positive `par - elapsed/60` becomes `1000 * (difficulty+1)` points per remaining second |
| Time counting | Wait 60 ticks, then transfer 100 points per tick, or 1000 while fire is held |
| Ammo bonus | Sum signed ammunition for weapons3..10; positive first scaling becomes `100 * sum * (difficulty+1)^2` |
| Ammo counting | Wait 60 ticks, then use the same 100/1000-point transfer |
| Short wait | 30 ticks after ammo counting; held fire can skip it |
| Final message | Submit original localized text ID `B9+level` and sound7, then wait for music-player completion |

All arithmetic uses the original wrapped 32-bit values and signed branch
decisions. The ammo positivity test occurs after the first multiplication by
`10*(difficulty+1)`, before the second; do not replace it with a saturating
formula. Ammo is not consumed. Weapons1 and2 never contribute. Score wraps.

Normal time completion goes directly to ammo setup. The original stage2 short
wait exists in the jump table but is not entered by the ordinary recovered
path; the native controller preserves and tests it. Counting emits original
command16 only on frame-counter multiples of8 and only for nonfinal transfers.
The final message cannot dismiss on its first submission tick. Thereafter a
fresh fire press can bypass the remaining 60-tick hold, but **music must be
finished**. Without a fresh press, it waits for that hold to reach zero too.
Held fire and pressed fire are separate inputs.

### Final Sam text and audio: corrected interpretation

Original02906 requests sound7 through08D5C. Song7 uses ROM player1 and consists
of two PSG tracks, not a recorded Sam sample. Its native sequence lasts36ticks
(about0.6seconds). All twelve levels use that same cue; localized text IDsB9..C4
provide the different Sam lines. There is no per-line speech lookup here.

Original02932 instead reads signed03006994, the status of player0 at03006990.
The ROM player table at083E99A0 (file offset3E99A0) has entry0 that
points to03006990, while entry1 points to030069D0. Player0 owns music, including
completion command9 (Egypt song78/Rome7F). The old host incorrectly checked
effect instances of song7. screen_completion_music_finished now reads player0
under the mixer lock. The native tests render both real completion tunes and
the cue, confirming that music continues after the cue ends and the cue uses
no PCM speech sample. Windows tests distinguish their statuses independently.

The recovered controller's60-tick hold is preserved. The three-second readable
window and direct bonus skip are explicit PC presentation changes requested
by the user, not claims of exact original timing. Completion music still starts
at tally entry, not at the final line, and is not restarted there.

Original message IDs are B6 (time), B7 (time zero/ammo), B8 (both zero), and
B9..C4 for the twelve final Sam messages. The pending-number Y coordinate is
GAME+75, initially88 and later120; it is separate from the notice text's Y40 at
GAME+73. The controller retains notice-timer/flag prefix effects of E914 and
uses the original blend coefficients at `0844A5D4`, selecting flash29 down to0.
The caller still owns actual notice text formatting, character animation,
draws and audio services. No silent production presentation stubs are provided.

## Native composition

`ssa_native_completion_init` requires an idle, completed, living game in
original input mode3. It copies the current score, frame counter, elapsed time,
ammunition and difficulty into the tally. The source game remains frozen and
unchanged. Call `ssa_completion_step` at the original 60Hz presentation cadence,
with explicit held/pressed input and original voice-finished status. Device or
render rate must not alter the tally's timing.

After the tally finishes, `ssa_native_completion_record` builds a value-only
`SsaCompletionRecord` from caller-owned prior totals. It captures actual equipped
and requested weapons separately, ownership and ammunition; refreshes weapon
statistics with the original setting byte; accumulates the **bonus-adjusted**
score and level counters; saves the checkpoint rows; and supplies next-level
index. Setting0 retains weapon shots/hits; any nonzero byte clears both before
accumulation and checkpoint capture.

First success marks the tally recorded; duplicate and premature attempts fail
before output changes. It rejects busy/error/source changes, including score,
counter, elapsed, bonus ammunition and completion state. Null failures preserve
output and the recorded flag. Canonical inventory health/protection supplies
the native projection of original body/player vitality. Source game and prior
totals remain unchanged, including source weapon counters. The resulting
record is safe to retain after game teardown; it contains no borrowed pointers.

This remains an explicit composition API. It does not itself update `SsaNativeCarry`, mutate the live game score,
write save.dat, grant equipment, unlock/admit levels, run next-level player
construction, or route chapters/endings. All twelve maps are now admitted at
the user's request for manual validation. The active E914/E9D4 service supplies gameplay's original localized
pickup/boss/death messages; see [hud-notices.md](hud-notices.md). The original
pre-tally world capture, tally-specific notice calls, on-screen bonus digits and
final audio dismissal are now connected by the host described above. That host
replaces the old direct Enter transition with the timed sequence; it never commits
the earlier pre-bonus score or instantly awards the bonus instead of counting.

## Verification

```powershell
.\decomp.ps1 test-completion
.\decomp.ps1 test-native-completion
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-completion.ps1 -SkipBuild
```

`completion-validation.json` compares 7,370 consecutive original loop frames on
all12 campaign maps / all3 difficulties, plus 4,096 directed loop states. It
starts the actual original02564 and controls world capture, drawing/animation,
VBlank, polling and audio boundaries. The original E914 notice setter executes;
only its formatting service is supplied. Tests compare ordered events and their
prefix states, counter/score/pending/flags/timer/position, every stage/hold/wait,
blend values, source ammo and elapsed. They cover overflow, negative ammo,
late voice completion, fresh versus held fire and otherwise unreachable stage2.
UBSan, 13 invalid-input guards, 865 callback reentry checks and3 callback failure
paths pass. This is a tally-loop comparison, **not** whole-function/UI parity.

`native-completion-validation.json` and its Windows counterpart agree on
410,217 checks: 36 map/difficulty starts, 1,152 actual gameplay frames with
jump/H input, 29,148 tally frames, 9,216 completion records covering every
setting byte, 36 chained records, 10,152 rejection cases and3,254 events.
GCC ASan/UBSan/leak detection and MSVC pass. Directed exit/counter fixtures
validate adapter wiring; they are not normal-input campaign-completion proof
and do not justify admission of the final map.

At the historical backend-only milestone,111 portable and112 Windows native libraries built. The existing Windows
statistics regression also passed, and the playable executable then remained
SHA256 `498e3e0071607a4c2a7a9b69aa71be1740d19c78babb8db1f3dfb35df0638504`.
No matching claim, imported catalogue entry or exact match was added for02564.
Live decomp.wtf status remains78/526 functions and2,816/87,648 catalogued bytes
(3.21%),541 attempts; discovery remains incomplete. Its78-C rebuild is unchanged.
