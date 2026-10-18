# Native campaign story controller

`campaign_story.c` recovers the original campaign panel script19458, moving
panel sprites193D8 and input/text-scroll wrapper1965C. The eight records at
0844CFC4 are copied from the user's ROM as data. Original197B0 chooses
record0 at level0 (three Egypt panels), record4 at level6 (one Rome panel),
and record6 at level12 (one ending panel); records3/5/7 are sentinels.

The six script stages retain entrance motion, strict endpoint tests,
wait120+180, scrolling, wait120+300 and exit motion. Position arithmetic wraps32;
sprite coordinates use signed high halfwords and clamp independently for
each of the four64x64 panels. A changed image transfers only after all four
positions update. Text changes reset scroll before requesting its initial rows.
Scrolling retains the original signed high-half clamp and21-row ring updates.

Processed input10000 requests skip command13, marks mode1 and requests sound fade7
only when music is enabled. Its remaining input is cleared for that call.
Up40 pauses scrolling and suppresses skip-wait800000; Down80 accelerates4x.
The script still ages holds while scrolling is paused. Calls during outer
fade modes1/2 continue updating image motion and text, without repeating skip.

The API rejects unsupported starting levels, malformed metadata, invalid state
and callback reentry. Presentation callbacks observe committed prefix effects;
failures preserve those effects and release the update guard. This controller
does not own artwork, text formatting, audio playback or the outer dispatcher.
The new `campaign_screen.c` supplies that presentation composition and now runs
in the Windows frontend. Score/name entry and the original completion tally UI
remain separate unfinished work.

## Native screen and Windows integration

New Game now shows the three original Egypt panels before its map. Advancing
from map5 to map6 shows the Rome panel before the next map. Password/selected
level starts and R restart bypass chapter stories, preserving the original
19268 skip gate. After the final level, Enter shows the ending panel and then
returns to Main; R still replays the level. Completion still uses the existing
Enter shortcut rather than the recovered timed score/tally screen.

All eight original128x128 tile images load from0844CFA4, with the original
palette, scrolling stone background,8x8 font,2px text inset and96px left text
window. The original21-row tile ring, inline palette directives, text alpha
blend, global fade and blinking skip prompt now render natively. All three
languages use their ROM text. Up/W pauses text, Down/S scrolls4x, Right skips
the current hold, and Enter/Space/Esc or a left click skips the story. Gameplay
and its input clock stay frozen until the next map; the native mixer keeps
running independently. Story command11 selects songs71/6E/82 for0/6/12.
The original8E74 argument7 is a sound fade interval, not music command7.

Preview these screens with `play-native.cmd --story 0`, `--story 6` or
`--story 12`. The ending preview returns to Main without awarding completion.
Returning from an ending now allows a fresh New Game; the frontend rebuilds
the original level0 state rather than retaining the final level's game object.
Saved language/unlocks remain intact. The already-running game was preserved
by renaming its locked executable within Release; restart the launcher for
this build.

`decomp.ps1 test-campaign-screen` compares1,018 original18C84/18D70 text rows,
including703 streamed rows over all15 localized story strings, and actual
18DFC line counts. Glyph raster18AB0 executes in the oracle, with only its DMA
clear serviced. Native screen checks pass87,854 assertions over27 screen runs
and43,510 ticks with ASan/UBSan/leak detection. GCC/MSVC counters and all three
captured English images agree byte-for-byte. The Windows script exercises
actual keyboard/mouse skip and scrolling through each screen, observes161
music checks each (including effects-only Rome), freezes the game counter for
285 ticks each and verifies the map/menu destinations with zero audio errors.
The normal launcher also passes New Game/title/story/map/reveal and R restart.
These are presentation checks, not automated campaign playthroughs or a
complete hardware-frame oracle of197B0. Reports are
`campaign-screen-validation.json` and `windows-campaign-screen-validation.json`.
All115 native Windows libraries and the frontend build.

`decomp.ps1 test-campaign-story` passes50,462 complete original1965C calls,
including24 natural/input-controlled sequences and40,592 sequence frames,
8,334 directed calls,61,546 ordered events and19,402 row requests. The oracle
executes original19458/193D8 too. Input polling, image transfers, text rendering
and line counts are controlled boundaries. UBSan,19 rejection guards,
61,554 callback reentry checks and6 service failure paths pass. The report binds
source hashes in `campaign-story-validation.json`. No new exact match was
added; live decomp.wtf remains78/526 functions,2,816/87,648 catalogued bytes
(3.21%),548 attempts. Discovery remains incomplete.

## Campaign admission and manual testing

On2026-10-17 the user requested all campaign maps immediately and explicitly
waived automated level-validation gating. `SSA_NATIVE_CAMPAIGN_LEVELS` is12.
Normal advancement, all campaign passwords and Select Level use this shared
limit. The existing twelve-bit profile needs no migration: map11 unlocks from
map10 completion. Direct launch is `play-native.cmd --level 11`; it uses the
original standalone level-start loadout rather than granting carried equipment.

The previous standalone final-map result remains historical. Development
earned-entry probes this turn still died before D8. A current uninterrupted
replay completed maps0..8 and died on map9 after the recent nearest-target Prion
change; the older eleven-map report predates that change. Temporary bot changes
did not establish a completed route and were reverted. No enemy damage,
inventory, collision or script behavior was changed to force a completion.
At the user's instruction these failures do not prevent enabling the maps.
Further campaign playthrough validation is manual; build/launch checks are
separate from gameplay-parity evidence.
