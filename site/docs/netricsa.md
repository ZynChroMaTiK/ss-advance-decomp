# Native NETRICSA

NETRICSA appears before the map on ordinary level starts, advances and manual
restarts. Automatic death retries skip straight to the level reveal.
Chapter introductions lead to NETRICSA, then the map; the ending still returns
to Main. Pre-level NETRICSA keeps the original animated background and music.
Closing it proceeds to the map, with gameplay still at frame0.
Classic HUD announces genuinely new unread weapon/enemy types with retained
analyzing sprite79 (2s, upper center, HUD-green on/off blink, no backing), then a green/white inbox
notification that snaps inward after analysis, holds2s, then snaps back. Its idle position is215,2; the attention
position is108,28, in place of the analysis text (108,40 with a boss bar).
Both inbox blink phases use green sprite65, with its green shading ramp
palette-swapped to neutral shades for white. The white sprite66 is not used;
frame geometry, black outlines and transparency stay identical between phases.
The blink spans the5s sequence. This uses simulation time and pauses with gameplay. Reading
all outstanding entries clears the notification; duplicates and retained
campaign/retry journal entries do not restart it. See [classic-hud.md](classic-hud.md)
for artwork, reference positioning and focused validation.

Opening N/Tab during play instead overlays NETRICSA on a frozen copy of the
current game image. Its frame, artwork and text blend over that image, with
transparent space showing the game. The animated background is omitted. The
RGB888 compositor preserves the current PC renderer resolution, including
letterbox areas, and also supports the240x160 reference renderer. Pointer mapping
accounts for both the game viewport and the NETRICSA canvas. Closing returns to
gameplay, restores capture, and frees the captured buffers. The current level
music continues uninterrupted through overlay opening, navigation and closing;
its track, note state and playback position are not reset or faded.

Manual opening now selects the newest unread weapon/enemy dossier directly.
Reopening after a fully presented entry selects the next unread dossier in
reverse discovery order. A quick close during the fade leaves it unread. With
no unread entries, and for automatic pre-level briefings, the original opening
page is retained.
Options > Extras > In-game NETRICSA can disable this gameplay shortcut. It
defaults to On and persists; pre-level briefings remain enabled either way.

In-game presentation fades take15 ticks/250ms at60Hz through ssa_netricsa_pc_step.
The original controller entry point retains its recovered timing for oracle
comparisons. Closing during fade-in reverses from the current opacity.
In-game opening blends from the full captured frame to a dark green world with
the weapon still visible: health/ammo/boss bars, crosshair and notice sprites
fade away. Closing restores them. The tint follows the same continuous opacity
as the NETRICSA artwork/text, including the viewport margins. Pre-level
NETRICSA retains its animated background and original fade speed through
ssa_netricsa_step.

The frontend captures a second frame with weapon-only OAM built from private
sprite copies; live OAM is restored after rendering. No sprite links, animation
timers or cache state are changed for this capture. Both RGB888 frames remain
owned by the frontend until closing; the compositor supports output aliasing
either frame and uses no animated menu background.

Weapon/enemy item numbers sit only above the upper text panel, starting at
two pixels right and one pixel down from its top-left grid boundary, at
x90/y17, one number per8-pixel grid cell toward the
right. The row stays within the panel's144-pixel width. Both lists share that
alignment; hit regions follow the cells. Numbers use the original green font palette normally,
white on hover and yellow for the active record, which takes precedence over
hover, without any background highlight. Unavailable records are hidden and
non-clickable; visible record numbers keep their stable positions. The numbers
retain their original font, fades and overlay composition. The preview
`build/native-tests/netricsa.bmp.selectors.bmp` shows all three available states.
The Enemies category is dim gray and cannot be hovered or selected when no
enemy records are available. The availability gate includes all twelve enemy
types, including a dossier containing only the final boss. A sparse/disabled
preview is `build/native-tests/netricsa.bmp.disabled.bmp`.

The playable frontend now opens NETRICSA with N or Tab during gameplay, including
terminal gameplay states. Tab toggles it off during play; Esc/N also returns to
the same world. Enter/Space cycles
category/upper/lower focus. Tab also closes pre-level NETRICSA and proceeds to
map view, retaining its original fade speed. Arrows or WASD navigate; Left/Right cycles acquired
weapons or defeated enemies. Mouse hover selects categories; clicking focuses
a panel. Wheel scrolls that panel. Clicking either half of an illustration, or
wheeling over it, cycles records. Hover sounds play when selection changes.
Clicking the localized "Press A to skip" prompt also closes NETRICSA, both
before a level and during gameplay. Its hit region follows the original font
width at x134/y9 and remains active during the prompt's blink-off phase.
The click queues the normal close input, preserving each screen's fade speed,
close sound, music transition and destination.

Weapons/enemies now show numbered selectors above the upper text panel, using
the original small font: weapons1..10, enemies1..12. Click an available number
to open its record and reset both panels' scroll/held-scroll state. The current
record is highlighted; hover highlights another available number. Unavailable
records are hidden and ignore clicks, preserving acquired/defeated filtering.
Keyboard and illustration/wheel cycling remain available. Selectors share the
same fading/compositor and letterbox pointer transform in both screen modes.

The screen uses the original23 illustrations at0844CF48, NETRICSA tiles/map
080AAF38/080B4C20, scrolling menu background080A8368, small font080B7A60 and
original metrics/charmap. It renders native240x160 RGB555 pixels, scaled by the
existing letterbox/presentation service. No ROM instructions execute at runtime.

## Discovery and unread dossiers

The latest user correction makes the Classic top-right panel an unread-message
counter for weapon/enemy dossiers, not lives. SsaNetricsaJournal keeps separate
discovered/read masks for10 weapons and12 enemy types. Weapon ownership creates
an entry, including starting loadouts; enemy spawns/hits do not. Native kill
counters create enemy entries; dead actor bodies also cover drain/hazard deaths
that bypass ordinary damage statistics. Repeated kills/acquisitions never
duplicate an existing dossier. UI availability uses kills or the journal's
known masks across keyboard, illustration/wheel and numbered selectors.

On every level, starting equipment is already familiar: its dossiers remain
browsable, but create neither an unread count nor an analyzing/inbox alert.
This includes standalone password/level-selection loadouts at all difficulties
and alternate-mode starting weapons. Pre-level snapshots acknowledge only
owned starting weapons not already known in the carried journal. The first
simulation tick commits that baseline before movement or pickups, so subsequent
discoveries still alert. Existing unread entries carried from earlier play
remain unread; entering another level does not acknowledge them. Read status
still survives advancement/retry. The earlier first-level Schofield exception
also remains, including its pickups there. Unowned weapons are not unlocked,
and save/settings/score formats are unchanged.

After a successful Windows frame presentation, ssa_netricsa_viewed acknowledges
only the fully visible selected weapon/enemy record. Fade-in, fade-out, briefing
and statistics pages acknowledge nothing. Merely opening NETRICSA does not clear
the counter. The native read service intersects acknowledgements with known
entries, so unavailable dossiers cannot be marked read in advance.

Journal masks are carried in the in-memory campaign snapshot. Retry reloads
keep the current journal while restoring entry equipment/score, so new knowledge
and read status survive a death. Advancing carries the same journal. Fresh
New Game/password/selected-level starts create a new run journal. Existing save,
settings and score-file formats remain unchanged. Statistical rows remain actual
counts; old journal entries never invent kills/ownership in a new map's stats.
The hidden show_all diagnostic still exposes all enemy dossiers for original
controller/text oracle comparisons; ordinary gameplay does not enable it.

## Recovered behavior

Original19C00 builds the records and submits labels;1A2C4 handles category,
focus, scrolling, close and refresh. The outer1A780 screen initializes category4
and focus1, loads artwork, requests music7 (song70), updates the background,
and fades. Native implementations are src/netricsa.c and include/ssa/netricsa.h.

Categories are statistics, enemies, weapons, strategic data and tactical data.
The upper/lower panels use18/15 BG tiles and7/9 row scroll bounds. Text keeps
ROM Latin1 accents, palette directives,2px inset and original31-byte streamed
line clipping. Records reference84..93 weapon lore,116..127 enemy lore,
214+map strategic text,202+map tactical text, and a random227..236 tip.
Tactical lower text uses the original136 record with its additional newline.
Formatted stats include chainsaw damage, infinite pistol ammunition, the U.M.T.
format without accuracy, ordinary weapon accuracy, enemy encounters/hits/kills,
game totals and player status. Zero shots yields0accuracy.

Original category ordering, clamped category movement, record wrap/skips and
independent held one-pixel scrolling remain. Changing category can call record
dispatch twice in the same original frame; simultaneous category/record keys
retain that behavior. The original first11-enemy sum gates the enemy category.
An empty weapon list rejects initialization instead of hanging in the original
unavailable-record scan. Blank rows beyond record end are cleared natively.

German enemy-stat record129 contains four numeric directives, although the
original caller supplies three arguments. The native version safely repeats
the attack count in its duplicated attacks row and puts kills in the final row.
That undefined original variadic read is excluded from exact text comparison.

## Frontend ownership

NETRICSA owns a value snapshot, without world pointers. Native statistics come
from ssa_native_game_statistics. Prior completed-map counters accumulate in
the frontend only when advancing; R retains those prior-map counters and
restarts the current map. Fresh New Game/password/level-selection starts clear
session totals. Current health, armor, score, owned weapons and ammo remain
authoritative. Lives uses the native lifecycle's continue count. Session totals
are not a new save format, score table, completion tally or continue workflow.

The game does not tick while NETRICSA owns presentation. Relative movement and
held buttons are cleared on entry; focus loss clears NETRICSA held scroll.
Closing restores the previous capture preference. Only pre-level NETRICSA
requests music7 and its closing music fade. The in-game overlay issues UI
sound cues without resetting the mixer or replacing the current music. Both renderers use
the same NETRICSA canvas. The borrowed ROM and screen address must stay stable;
embedded formatted-text pointers are repaired after initialization's copy.

The tip seed currently comes from a native frame-based LCG projection. The
original global random-state coupling and full outer hardware/OAM screen chain
are not established as byte/pixel-identical. This is working native presentation,
not a claim that the complete game port has original feature parity.

## Verification

Run .\decomp.ps1 test-netricsa and
scripts/test-netricsa.ps1 -SkipBuild after the Windows build.

The portable suite passes19,561 ASan/UBSan/leak assertions over36 map/language
starts and5,292 presentation ticks. It visits all weapon/enemy records, tests
independent panel focus/scroll, illustration clicks/wheel, unavailable records,
bounded text, fades and blank output. A generated bitmap was visually inspected.
Pixel checks compare normal/hover/active number glyphs to the original green,
white and yellow palettes at the panel-aligned positions, and verify untouched
background around glyphs, hidden unavailable numbers and disabled category text.
MSVC runs the same fixture.
Overlay checks use an854x480 patterned background and a distinct HUD frame:
fade endpoints and midpoint, source preservation, dark green viewport margins,
HUD removal, weapon retention and in-place composition are checked. Changing
background scroll has no effect. PC fades complete in15 ticks in each direction;
early closing reverses smoothly. Original controller tests retain their timing.

The Unicorn test executes original CPU routines only as a test oracle. It compares
2,684 localized text rows from18D70,174 record builds from19C00 (including both
statistics panels), and405 whole1A2C4 navigation/scroll calls with original19C00
still reached. Sprite label submission, DMA and sprintf are controlled boundaries;
pixel services are intercepted only in the separate controller comparisons.
Twelve German undefined-format cases are explicitly counted and excluded.

Windows checks exercise Tab open/close, Enter panel focus, keyboard navigation, real window messages for
mouse selection/wheel, music7, snapshot values, a frozen game counter and resumed
simulation on maps0/6 with both renderers. They use muted output and do not prove
physical device input or listening. Campaign playthroughs remain manual, as
requested. Results/hashes are in netricsa-validation.json,
netricsa-rom-validation.json and windows-netricsa-validation.json.
Each Windows path passes127 pre-level briefing ticks before the map and110
in-game overlay ticks,5 pointer checks,1 live snapshot check and70 music checks,
with zero decoder errors; gameplay resumes for3 ticks. The PC case uses854x480
and the reference case240x160. Egypt/Rome introduction checks now require
NETRICSA before the map; the ending still returns to Main. Actual Windows
briefing and overlay captures were visually inspected: the briefing retains
the animated background, while the overlay shows the dark green frozen world
and weapon through transparent regions with other HUD sprites removed.
Weapon-only OAM retains6 pieces in the PC case and14 in the reference case;
19,251 and2,508 pixels respectively differ from the original full-HUD capture.
The initial Windows fixture assumed every map had an enemy record and a long
tactical briefing. It now respects the original encounter gate and scrolls a
known weapon description. A locked executable was preserved under a separate
running-netricsa filename so the open session could continue during rebuilding.
All119 portable/Windows libraries and the Windows frontend build successfully.

One ordinary-C decomp.wtf dispatcher1A27C attempt has the same instructions,
but fails linking its uncatalogued19C00 callee. It is not exact; the claim was
released with notes. No catalogue split, ROM mutation or new matching claim was
made for this screen.

Newest unread entry selection (2026-10-19): SsaNetricsaJournal retains a bounded
22-ID discovery history, oldest first (weapons1..10, enemies11..22), across
campaign carry/retry. Known/read masks remain authoritative. Read-only snapshots
append newly observed types to a copy; ordinary tick/read commits retain order.
Simultaneous discoveries use a stable weapon-then-enemy ID tie order. Repeated
types do not move forward. History validation rejects invalid IDs, duplicates,
unknown IDs and counts>22; an omitted/incomplete history is filled deterministically.
This is campaign-session state, not a new on-disk save format.

ssa_native_game_netricsa_latest returns the newest unread ID without mutation,
zero when none. Manual begin_netricsa supplies it through snapshot.opening_entry;
ssa_netricsa_init resolves the category/record, original artwork and texts before
the first fade frame, with focus on the upper description and reset scrolling.
All22 available IDs work in all three languages. Automatic pre-level NETRICSA
continues to start on the briefing page. The existing fully-visible successful
presentation acknowledgment is retained: early-close/fading records remain
unread; presenting an entry advances the next opening; no automatic page changes
occur while reading. No-unread manual opening uses the original default page.

Validation: native NETRICSA ASan/UBSan/leaks=1 and MSVC20,950checks,36 screen
runs/5,292frames, plus2,684 ROM row comparisons,174 record calls and405 controller
calls. The differential ctypes snapshot was updated for opening_entry.
Native journal/HUD sanitizer and MSVC170,303checks/403draws pass. Actual weapon
acquisition and enemy combat kills verify non-ID chronology, snapshot purity,
carry retention, newest/next/none, and malformed histories. Windows560 checks
exercise mixed weapon/enemy reopen order, early close without acknowledgment,
actual present/read, next entry, drained inbox/default opening and retry in both
renderers; the same suite retains768 supplied Time Strike PNG checks.
Full test-netricsa Windows routes pass: original127-tick pre-level briefing,
110 gameplay-overlay ticks, mouse controls, frozen/resumed simulation,70 music
checks and0 audio errors each in software854x480 and reference240x160.
Reports: netricsa-validation.json, windows-netricsa-validation.json,
classic-hud-validation.json and windows-classic-hud-validation.json.
Static frontend/125libraries rebuilt; SHA256:
fbd14808aeb4492e03ebc71b6a34d5f41b127803986e4a1fc481817cbc1e488d.
No full campaign replay, ROM changes or new matching claims.

New-entry sound cue (2026-10-19): netricsa_store now starts retained NETRICSA UI
sound0x0B in the same branch that sets hud_netricsa_ticks=300. One centered,
non-looping effect per new notification batch, for either a weapon or enemy.
Use the ordinary local sound service/source1 (legacy play fallback), never the
music-command path. It follows sound volume and the existing protected local
SFX priority, and works with music disabled. No new media is embedded.
Redraws, pulse/blink phases, pause, reading, repeat discoveries and familiar
starter/carried entries do not replay it. Sound is not emitted from getters or
draw functions. Existing per-type unread and reverse discovery order are unchanged.

Focused native ASan/UBSan/leaks=1 and MSVC171,315 checks/403 draws pass. The
notification fixture binds both legacy/source-aware audio sinks and verifies
one cue at the300-tick onset, no repeated cue on redraw/continued analysis/read,
no mutation/reset of the active music player, a centered source1, and an enemy
kill's new-entry cue. It renders48,000 actual ROM-cue frames: nonzero signal,
zero decoder errors, self-completion, plus silence with sound volume0 and music
disabled. Windows560 menu/overlay/reopening checks and768 supplied-PNG pixel
comparisons pass. No physical listening or full campaign playback was performed.
The Windows HUD checker now links ssa_audio for these actual mixer checks.
Static125libraries and frontend rebuilt; executable SHA256:
b521f80abf30ba3e97104cd3672da2c84b181c20580d32aae9045376e9f3ece9.
See classic-hud-validation.json and windows-classic-hud-validation.json. ROM
unchanged; no new exact matching claim or full launcher rerun for this audio hook.

Notification correction (2026-10-19): supersedes the earlier single-cue policy.
Classic HUD plays retained cue0x0B at analysis onset and once more75 active
simulation ticks later (1.25 seconds). Original HUD emits neither play.
A focused real-mixer diagnostic found the cue completes by tick71 (1.183s)
at24000Hz, so75 ticks separates the complete sounds without overlap.
The small hud_netricsa_repeat latch accompanies the existing notification
clock. Pause freezes timing. Reading all unread messages, death/completion,
or switching to Original clears the latch; switching back cannot resurrect it.
The ordinary protected centered source1/legacy SFX path, sound slider, music,
notification visuals, discovery and read ordering remain unchanged.

Validation: native ASan/UBSan/leaks=1 and MSVC each pass335944 checks over405
counted draws/two renderer paths. Both legacy and source-aware sinks verify
exactly two Classic plays, no third, no Original plays, canceled repeats on
read/HUD change, paused countdown, centered source and unchanged music player.
The real mixer confirms completion before the75-tick repeat, audible output
and sound-volume0 silence. Windows frontend560 checks and768 supplied icon
pixels also pass. Static125libraries/frontend rebuilt; executable SHA256:
1f96bcf3c60678f312ee2b96e86e771136a1238d25b6e203331639b99ff8b2b8
See classic-hud-validation.json and windows-classic-hud-validation.json.
No physical listening or full campaign replay was performed; ROM unchanged.

Back-to-back cue correction (2026-10-19): overrides the previous75-tick gap.
The earlier duration measurement observed sequencer completion, not sound
completion. The actual0x0B burst ends about0.1075s into its1.18s ROM sequence;
most of the remaining sequence is silent. New CUE_TWICE source commands queue
one repeat owned by the audio mixer. The second copy starts on the output
sample immediately after the first copy's last active voice, replacing only
its finished instance and ignoring the sequence's trailing rest. A pair has
one centered local instance, never two overlapping copies, and cannot loop.
Music and other effects keep ordinary playback and their independent buses.
CANCEL_REPEAT removes only a pending copy, leaving current playback intact.
Stop/reset/retire remove the queue. Audio pause freezes both the sound and queue.
Windows forwards these commands under its existing audio mutex; there is no
simulation-clock delay in its pair. Notification startup and cancellation are
centralized in native_audio.inc. Legacy sinks without source service approximate
the audible length at7 simulation ticks (frame precision); Windows does not use
that compatibility fallback. Original HUD remains silent; analysis visuals and
the unread journal remain unchanged.
Validation: ASan/UBSan/leaks=1 and MSVC357231 checks/409 draws/two paths pass.
At24000/48000Hz, each first burst matches ordinary playback sample for sample;
the exact next sample matches a manually restarted cue, followed by bit-identical
output and exactly two starts. Cancellation, stop, reset, audio pause/resume,
legacy fallback, Original silence, music preservation and visuals pass.
Windows560 checks plus768 supplied icon pixels pass. Full MSVC audio regression
also passes660573 checks/131 audible songs/35640000 catalog frames/zero decoder
errors or clipped catalog samples. Its stale Game Over fixture was corrected
to use the final life and the recovered pre-terminal music request; no death
behavior was changed. Static125libraries/frontend rebuilt; SHA256:
6aa4beab75a8d8c699d2d313b9b550a309aec6245ac5ccc6d2a52bffd7dd6582
See netricsa-cue-validation.json and current Classic HUD validation reports.
ROM unchanged; no physical listening or full campaign replay was performed.

Equal-length gap correction (2026-10-19): supersedes the back-to-back policy.
The first burst is measured in output frames. After its voices finish, the
mixer inserts that exact number of silent frames, then starts the second copy.
For retained0x0B, both burst and gap are about0.108s; its1.18s sequence duration
still does not define audible duration. CUE_TWICE remains finite (two total).
A gap belongs to its protected local source/player, with pending instances
reserved in player_busy. Gap frames advance on the audio clock, stop while
paused, and honor existing repeat cancellation, stop, reset and HUD/read gates.
Original HUD stays silent. The legacy sink rounds the two duration intervals
to13 game ticks; Windows uses mixer measurements with sample precision.
Validation for equal cue/gap duration: ASan/UBSan/leaks=1 and MSVC390355checks/
409draws/two paths pass. Real24/48kHz output matches the first ordinary burst,
then exactly that many silent samples, then a manually restarted second burst
bit for bit. Exactly two starts, no overlap/third cue. Gap pause/resume and
cancellation during silence are included, with earlier stop/reset/HUD/read and
music checks retained. Windows560 checks+768 supplied icon pixels pass.
Full MSVC audio regression660573 checks/131 audible songs/zero decoder errors
or clipped catalog samples also passes. Static125libraries/frontend rebuilt;
SHA256 7278284217a42b848edce0d50912f6ba3c91d0a807a695ea5d50b7e68119a315.
Current report: netricsa-cue-validation.json. No physical listening or campaign
replay was performed; ROM unchanged.

Half-length gap correction (2026-10-19): supersedes the equal-length gap above.
CUE_TWICE now waits half the measured first audible burst's output frames,
rounded up to a single frame, before the second play. This is about54ms of
silence after the roughly108ms cue. Trailing sequence rests remain excluded.
Legacy sinks approximate sound plus gap with10 game ticks. Classic-only
notification, Original HUD silence, pause, cancellation and two-play limit
are unchanged. The24/48kHz fixture checks the shorter silence and compares
both bursts with ordinary playback sample for sample.
Validation: GCC ASan/UBSan/leaks and MSVC382477 checks/409 draws/two paths
agree; Windows560 checks/two renderers and768 supplied icon pixels pass.
Static125libraries/frontend rebuild passes. See netricsa-cue-validation.json
and current Classic HUD reports. No physical listening or campaign replay.

In-game music continuity (2026-10-19): the overlay now keeps the current level
music running across opening, navigation, closing and reopening. The frontend
does not reset the mixer, select NETRICSA music or fade audio for this overlay.
Its visual fade is unchanged. Pre-level briefings retain music7 and the
original transition to map music. UI sound cues remain independent.
Validation:1666 Windows Classic frontend checks include four overlay cycles,
comparing music player0, music state and master gain with uninterrupted
reference playback while both mixers advance. test-netricsa.ps1 passes20950
native checks and both live renderer routes (initial briefing, overlay and
resume;70 level-music checks each, zero audio errors).125libraries/frontend
rebuilt. See netricsa-music-continuity-validation.json and the current Windows
NETRICSA report. No physical listening or broad sanitizer/campaign replay.
