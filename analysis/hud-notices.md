# Original localized HUD notices

The native frontend now services original E914 notices and the complete E9D4
active message controller. Pickup names, boss introductions and death messages
use their original localized sprite lettering, layout, reveal and expiry timing
in both renderers. Generic English pickup/enemy/death host text is removed.
Language follows menu selection, direct starts, advancement and restart.

`hud_notice.c` owns the message state and controller. The new
`ssa_weapon_animation_resolve` implements E410's immediate frame lookup without
advancing animation. Both `hud_scene.c` and `native_game.c` route type-D sprites
through this controller before ordinary E460/cache/OAM processing.

## State and ordering

| Original field | Meaning |
| --- | --- |
| GAME+69/6A/6B | byte length, non-space/non-newline glyph count, font shift |
| +6E..71 | rectangle X/Y/width/height |
| +73/+74 | text Y, glyph readiness progress |
| +76 | signed notice timer |
| +78/+7A | text ID, value argument |
| +320 | message bytes |
| 03006AB8/03006AE8/03006B18 | 42 glyph X/Y/animation entries |

E914 sets timer120, resets progress and clears flag200. The bounded native
setter retains original literal localized message bytes. Formatted encyclopedia
strings and messages exceeding the original 42-glyph scratch reject before
writes; those screens need their separate formatter. The matching candidate
continues to call the original formatting function. ROM data stays borrowed.

E9D4 subtype0 decrements the timer once per sprite phase, rebuilds layout and
centers each line. Spaces, blank/trailing lines, byte wrapping and ARM shift
behavior are retained. Letters select their original animation and resolve the
frame immediately. Reveal depends on the previous/current cache-frame comparison,
progress count and flag200 latch. Expiry clears flags40/200 and hides glyphs.
Automap flag8 hides them without advancing the timer. Preserve sprite order.

The playable adapter shares one timer with player lifecycle. Death's explicit
600-tick overwrite survives E914's initial120 and decrements once per frame.
HUD flag changes commit between handlers so rectangle/reveal state survives
the next projection. Pickup requests retain Y48/font0; boss/death callbacks
forward all four recovered arguments. The unfinished completion shortcut remains
separate from these messages.

## Verification

```powershell
.\decomp.ps1 test-hud-notice
.\decomp.ps1 test-native-notices
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-native-notices.ps1 -SkipBuild
```

`hud-notice-validation.json` compares 2,625 original E914 calls and 135,104 whole
E9D4 calls, with real FC04/E410 execution. It includes 24 sequences / 3,168
presentation frames across all three languages, 2,048 directed states and six
native guards. Whole GAME storage, glyph scratch and the complete sprite are
compared under UBSan. Only E914's literal formatter is supplied; the cache-transfer
readiness boundary is controlled. This is not a whole active cache/OAM comparison.

The preexisting complete original HUD frame-chain regression still passes
2,048 frames on 16 maps: 172,032 objects, 2MiB OAM, 64MiB sprite transfers and
64 independent original-OAM RGB comparisons, with its inactive-notice coverage.

`native-notices-validation.json` and its Windows counterpart agree on 13,650
checks: 48 starts on 16 map datasets and all three languages, 6,432 frames,
39 actual pickup contacts, 48 ready/expired notices, 48 fatal-combat/death cases
and 144 pairs of rendered frames. Nine presentation fixtures cover otherwise
empty multiplayer map datasets. Single-player multiplayer pickup respawning
stays disabled. Pickup relocation and explicit vulnerability are adapter fixtures,
not campaign-admission evidence. Initial fixture assumptions about NULL
single-player damage sources and one-player multiplayer respawns were corrected;
runtime guards were preserved.

Normal-input level1 still completes in 5,110 frames with 24,000 score, eight
pickups and 100 HP, with all enemies/groups cleared. All portable CMake targets
and 113 Windows libraries/frontend build. Launcher/startup/restart checks pass.
Full campaign replays were not repeated; the final map remains unavailable.

## Matching and next work

The actual decomp.wtf compiler ran seven ordinary-C attempts at E914.
`recovered/hud_notice_set.c` reaches 99.7%, with the same 94 instructions, but
swaps pointer/value registers in five loop-counter instructions. It is **not
exact**. The claim was released with notes; no inline assembly, register pins,
catalogue splits or new exact-match count were added. Live status remains
78/526 functions, 2,816/87,648 catalogued bytes (3.21%), 548 attempts. Discovery
is incomplete; these are not whole-game percentages.

The recovered tally can now use the active notice service. Remaining frontend
work is original pre-tally world capture/fade, bonus-number presentation,
voice-finished service, campaign record lifetime and Enter/R/ending routing.
The frontend does not yet run the timed tally or award its bonus-adjusted record.
See [completion.md](completion.md).
