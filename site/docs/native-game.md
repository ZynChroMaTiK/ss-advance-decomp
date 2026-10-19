# Native twelve-level campaign

All twelve original campaign maps are now enabled. This includes normal
map10-to-map11 advancement with earned inventory, final-map passwords and
Select Level after beating map10. No save migration or progress reset is needed.
`play-native.cmd --level 11` starts Caesar's Palace directly with its original
standalone loadout. The user requested manual campaign validation on2026-10-17;
automated playthrough success is no longer an admission gate.
M now opens the recovered in-level automap in either renderer, gated by the
saved Extras -> Automap option (default Off), with wheel or
held plus/minus zoom, original walls/markers/palette and scanline animation.
Pause and Tab NETRICSA return to the map. See [automap.md](automap.md).
Original chapter/ending panels now run natively: Egypt before New Game,
Rome on advancement into map6, and the ending after the final bonus tally.
Finite lives now follow original death timing and automatic retries, preserving
the remaining5/3/2 difficulty allowance across levels; permanent death offers
saved high-score entry. See [death/retry/game over](run-exit.md).
The ending offers high-score entry before Main. Esc/P now
opens Continue/Quit with a Yes/No warning. Completion renders original localized
notice sprites, live score and time/ammo bonus digits; held fire/Enter/Space
accelerates counting. Bonus score commits once before advancement. See
[pause and completion](completion.md) and [saved high scores/name entry](scores.md). Preview the ending with
`play-native.cmd --story 12`. See [campaign-story.md](campaign-story.md).

Run `play-native.cmd` from the workspace. It opens the supplied, identity-checked
`orig/SSA.gba` in `build/native-windows/Release/ssa_native_game.exe`.
The executable runs native x64 C; the ROM supplies data, never CPU instructions.
Original localized pickup names, boss introductions and death messages now use
their sprite lettering and original reveal/expiry behavior in both renderers.
Language follows the saved menu option. See [HUD notices](hud-notices.md).
Rome campaign ankhs use the requested golden laurel wreath, with original20x34
and10x17 dimensions, origins and gold palette indices. This is an in-memory
presentation replacement shared by both renderers, with original pickup behavior.
See [artwork and source prompt](../assets/rome-laurel/README.md).
Normal startup now begins with title, main menu, Single Player, New Game and
difficulty. Language cycles in Options instead of a startup selection list.
Language and beaten-level unlocks now persist in save.dat. Single Player inserts
Select Level between New Game and Password, with original localized campaign
names and no multiplayer maps. Level1 is free; completing a level unlocks its
successor on every difficulty. Later unported campaign maps remain unavailable.
Options → Clear Data warns with Yes/No (default No) before resetting progress and
language. See [profile storage and level selection](profile.md).
Enemy crowd movement now allows tangent sliding and gradual separation of
overlapping spawns. See [enemy crowd movement](enemy-crowd.md). The focused
normal-input first-level proof finishes in5,110 frames with24,000 score,
eight pickups,19 spawns and100 health, with no living enemies or open groups.
Prion-converted enemies now choose the nearest living combatant on every behavior
pass, without preferring Sam. Facing, projectile aim and enemy melee use the
selected target. All ten convertible types pass the focused 1,602-check GCC/Windows
test; see [Prion targeting](projectile-conversion.md#requested-pc-nearest-target-behavior).
The campaign replay figures below predate this targeting change.
The updated GCC sanitizer and Windows campaign replays complete maps0..10 consecutively with earned
carry: 140,712 frames, 1,544,000 score and 45 HP. Each advancement now resets
vitality through original player construction, carrying earned gear and score.
This includes crowd sliding, wide
[player clearance](enemy-crowd.md#pc-player-clearance), saved-menu support and
the latest Prion2x/Time Strike changes. Map8 finishes 17,235 frames / 76 HP with 106 spawns;
map9 finishes 10,930 frames / 46 HP with 50 spawns. Both use actual conversion, falling
Time Strike objects, jump/H controls, both renderers and the native audio mixer.
The frontend now admits all twelve maps for manual validation.
Praetorian Fort completes in 17,231 frames / 99 spawns / 9 pickups / 45 HP with
earned equipment and original next-level vitality.
The level11 development probes exposed and led to a repair of
Time Strike's stale animation frame in native render snapshots; see
[special weapons](special-weapons.md#frozen-animation-render-repair). No enemy
health, collision, script, inventory or hazard rules were bypassed in those probes.
Arrows/WASD navigate, Enter/Space
confirms, mouse hover/click selects entries and Esc goes back. Title music
continues across Press Start. Difficulty affects native combat; selected language
also applies to subsequent map screens. `--level 0..10` explicitly bypasses these
menus for diagnostics. See [menu recovery](menu.md) for supported pages and gaps.
Options → Credits displays the original ROM-backed scrolling credits. Up/W pauses,
Down/S speeds up, and confirm or Esc returns to Options; gameplay stays frozen.
Single Player → Password supports direct keyboard typing with Backspace/Delete,
alongside the original eight-character editor and table. Left/Right moves the
cursor; Up/Down changes characters, Enter/click submits,
and Esc returns. Accepted passwords select the original level, difficulty and
weapon/ammo loadout, then the normal fade/map/level sequence. Restart restores that
password entry inventory. Later maps remain explicitly unavailable.
Level starts, restart and advancement display the original pre-level map view.
Click or press Space/Enter to confirm; its fade reaches solid black before gameplay begins.
The frontend presents the final black map frame, holds black for at least a
quarter second, then reveals the level over at least one second. These transition
frames cannot be consumed in a simulation catch-up batch. One bootstrap tick
builds the first level image and the complete HUD is loaded before it appears.
Movement, mouse capture, aiming and HUD updates remain active throughout the reveal.
Simulation catches up at its normal clock while each fade step is presented separately.
Map music stays on that screen; gameplay starts with the level soundtrack.

Normal startup and restart now run one native initialization tick before their
first gameplay draw, building valid HUD/cache/OAM data. The pre-level map does
not tick gameplay. Drawing gameplay before its bootstrap tick rejected
the zeroed initial sprite records and made the launcher exit immediately; the
hidden smoke path had already ticked before drawing and did not expose it.
`scripts/test-native-startup.ps1` tests the actual normal startup, raw-input
registration, initial window paint and restart in both renderer modes through
`play-native.cmd`, including launching from another working directory. The
normal Windows gameplay test now includes these checks. The launcher saves
stderr to a per-session `build/logs/launch-*/stderr.log`, displays failures and
waits on error. A session directory is reserved atomically, because CMD random
sequences can collide when two launches start together. On exit the launcher
also updates `build/logs/last-launch.log` when that file is writable. A running
older launcher holding that shared file cannot block a new launch. Automation
may set `SSA_LAUNCH_NO_PAUSE=1` and an explicit `SSA_LAUNCH_LOG`; tests use private
logs and also verify two simultaneous default launches. Current startup evidence is in
`analysis/windows-native-startup-validation.json`.

Mouse look activates automatically during gameplay and on focus return, without
a separate click-to-capture mode or prototype text overlay. Use WASD to move, mouse movement to turn and
aim vertically, left mouse to fire, Space to jump, H to toggle first/third-person view,
E to use doors, and number keys 1–0 to select an owned weapon. Picked-up weapons
also select through the original acquisition logic. Esc/P opens Pause and frees the cursor;
R reloads Colts when Alternate weapons is enabled. Enter advances after the first
eleven exits and shows the ending after the twelfth exit; otherwise it restarts. Mouse aiming uses
the original vertical auto-aim until the first vertical mouse input. Vertical
input enables direct free aim, including full yaw precision, sector-plane hits
and projectile slope. The PC renderer rotates the camera through up to 80 degrees
up/down; the original crosshair art, placement and target animation are restored
in both views. The optional original
240x160 reference renderer retains horizontal look and original auto-aim.
Third-person PC view now orbits with pitch and converges onto the actual weapon
contact, keeping the centered original crosshair aligned. See [chase aiming](chase-aim.md).
Third-person view hides all fixed-screen weapon muzzle flashes as well as the
first-person weapon. The effects still animate normally; changing H back to
first person restores any active flash. World impacts and projectiles remain visible.
Single-player Sam uses the original white-shirt body overlay (variant3,
resources121–125). Selection is confined to the render snapshot: gameplay
player identity and the recovered animation/asset/projection routines retain
their original behavior. Both PC and reference renderers use the same variant.
Ranged attacks now have no weapon-distance cap, including shotgun side pellets.
Player projectiles have no age expiry; walls, floors, ceilings and targets still
stop them. Melee reach and blast radii retain their original rules. See
[unlimited range](unlimited-range.md).

The native audio backend now plays original ROM effects and level music. M
toggles mute; `--mute` starts silently and `--no-music` retains effects only.
Background mixing keeps audio delivery independent of rendering. Restart clears
active effects and plays the map cue on the map screen; confirming it starts the
level song. See [audio integration](audio.md) and [original map view](map-view.md).
Footsteps use the original alternating cadence while walking or strafing on the
ground, in both camera modes. Jumping, standing still, disabled controls and
pressing into a wall do not emit walking sounds. Independent effect voices
keep the soundtrack running.
Enemy, activation and impact sounds use a new PC spatial mixer: stereo direction
follows Sam's yaw and volume fades with three-dimensional distance. The listener
stays at Sam's eye in both views, so H and vertical mouse aim do not displace it.
Each CD/CE enemy can run its own movement loop; attached enemy sounds follow
their emitter, while one-shot impacts stay at their emission point. Doors have
world positions too. Each door loop ends when that door stops,
without cutting off other moving objects. Pickups, local weapons, footsteps and music
remain centered. This spatial model is a native PC addition.
Positioned sounds now fade continuously with distance, including nearby doors,
using the same falloff as enemy effects. Pickups bypass distance attenuation
and positional panning, using the same local playback path as player sounds.
The last life selects the original game-over music at the recovered GAME OVER
notice, while ordinary deaths retain level music and sound17. The real scripted
exit selects the original level-clear cue once. End-screen transitions clear
old effect loops. Restart restores level music. `--no-music` keeps these
transitions effects-only. The death lifecycle and automatic finite-life handoff
are now connected; see [run-exit.md](run-exit.md).

This is a **twelve-level prototype**, with campaign playthrough validation left
to the user. The completion tally UI, score/name entry, remaining
presentation effects and original menus remain unfinished. The new native audio
renderer is not yet an exact reproduction of the GBA hardware mixer.
Pickup/boss/death notices use original HUD sprites; the completion shortcut
still uses host text. The all-map diagnostic viewer remains available separately through
`run-native-viewer.cmd`; it does not run this gameplay composition.

## Composition and ownership

Type-64 moving effects now use original D000/CE50/053D0 geometry and native
sector-only membership updates. The shared constructor and scheduler no longer
reject this relocation path. See [sector clipping](sector-clip.md) for original
comparisons, bounded-loop guards and controlled integration coverage. This adds
no campaign admission or new original creation-site claim.

`SsaNativeGame` keeps stable owners for map/ROM-backed script data, players,
combat, enemies, projectiles, pickups, frame scheduling and rendering. The caller
retains the ROM until teardown. Actual level/mover initialization precedes map
objects, which precede players, preserving the original allocation order.
`ssa_script_spawn_adopt_plan` moves map bodies into the shared100-slot actor
pool. `ssa_world_replace` preserves block/sector link order while transferring
storage before gameplay borrowers bind. The initial reserved prefix is a
highwater mark; adopted actors in it can die, be collected, and be reused.
External player bodies remain separately owned.

The original 0244C frame order runs real player updates, CD/CE/CF AI, projectile
flight/damage, movers, scripts, pickup respawn, actor animation, weapon/HUD
controllers, sprite animation, cache preparation and OAM construction.
The reference first/third-person world and actor rendering composes with the
original HUD, palettes and weapon sprites at 240×160. The Windows frontend now
defaults to an additional dynamic PC renderer that follows the window size;
`--resolution WIDTHxHEIGHT` selects a fixed size and `--original-renderer`
selects the reference view. See `pc-renderer.md`. H exposes only a rendering snapshot
of Sam's body and hides the first-person weapon. It changes neither hitboxes nor
aim. One shared RNG state is owned by the game; impact selection currently is
its only recovered consumer.

Normal difficulty uses damage.difficulty=1, continue_flag=3, input_state=1 and the
actual per-type speed records at 0844BFD4. No debug loadout, invulnerability,
health grant, artificial group defeat or forced animation completion is used.
The PC shell handles restart and completion explicitly; it does not claim a
recovered full original menu dispatcher. The production frontend exposes levels
0 through 10 (`--level 0..10`); the final campaign map remains unsupported.
The recovered difficulty menu now also selects original easy/hard damage rules
and initial continue values5/3/2. The later finite-life integration now performs
automatic retries and final game-over/high-score handoff. Enter restart remains
available only to the explicit startup/transition diagnostic, not ordinary play.

## Level advancement and restart

Enter after a supported intermediate scripted exit starts the next map with
earned weapons, ammo, the equipped usable gun and total score. Original player
construction starts the new map at 100 health with no protection or pending
acquisition queue. H camera mode and Q weapon history also carry forward.
A direct `--level` diagnostic launch uses
the original ROM level/difficulty starter loadout, as password entry does. This
also applies to explicit `--level 0`; ordinary New Game retains its recovered
zero-setting first-level initialization. Restart restores the current level's entry inventory
and score, including after death. Enter after the last admitted map replays it
from its entry state.

`SsaNativeCarry` is a value-only entry snapshot. Exact capture/apply serve restart;
`carry_next` prepares the original vitality/queue reset after completion.
Native `weapon_progress.c` recovers the original `123C4` weapon restore, with
8,192 whole-routine comparisons and 512 original construction/restore sequences
across all 16 maps. It also recovers `1231C` completion capture: 4,096 whole-ROM
comparisons and 512 capture/restore compositions. `ssa_native_game_weapon_progress`
reads equipped/requested guns and ownership/ammo/shot/hit rows without mutating
the game. The carry path now uses that recovered capture. The separate
`native_statistics.c` adapter snapshots all live completion counters for the
new original accumulator/checkpoint backend; frontend total persistence and
score screens remain unimplemented. See [statistics.md](statistics.md).
The separate `native_completion.c` adapter now composes the verified timed
time/ammo tally and bonus-adjusted capture/accumulate/checkpoint sequence; it
does not replace the Windows frontend's Enter path yet. See
[completion.md](completion.md).
See [campaign-entry.md](campaign-entry.md). This does not
claim recovery of every outer campaign screen or transition. Capture requires
an idle game; apply requires a newly initialized game before its first tick.
Invalid/dead captures and invalid carry values are rejected. New maps retain
their own spawn positions, actor owners, scripts and transient weapon effects.
The frontend initializes each replacement game at its final heap address:
`SsaNativeGame` contains internal pointers and cannot be relocated by copying.
Renderer caches and audio voices reset at the transition. The map view runs
without gameplay ticks; after confirmation and its fade, a bootstrap tick
prepares the first gameplay frame before drawing.

## Validation

`tests/test_native_playability.c` completes level 0 through normal portable PC
input events. Its route planner reads a separate copy of map geometry; possible
unlocked door openings are considered only in that copy. Real doors still need
E and the actual mover update. A separate aim workspace chooses bot inputs.
The bot never writes gameplay bodies, health, ammo, damage or script counters.

The earlier GCC/MSVC checkpoint completed
in 4,977 frames: 28,000 score, 7 pickups, 19 spawned enemies,
69 remaining health, 13 pistol shots and 23 shotgun shots. The quick-click buffer
retains an additional shotgun attack and changes the bot's later combat/route.
The bot continues firing at close threats even when low
on health, and stops optional supply detours after clearing the waves. It still
uses only normal PC inputs. The actual group 3/4
deaths unlock the final gate, and touching the exit pickup queues opcode9.
`analysis/campaign-validation.json` records the current sanitizer proof;
`analysis/windows-native-game-validation.json` records Windows completion and
hidden-window frontend smoke. Physical keyboard/mouse interaction has not been
manually tested. Separate integration checks pass 600 gameplay frames with 390
airborne frames, 360 third-person frames, a pickup and incoming damage, then
120 fresh frames after teardown/restart, with leak detection enabled.
The current completion/build reports bind their sources and executable hashes
to these results. Older broad source audits describe earlier checkpoints.

`tests/test_campaign.c` runs consecutive completions using the shared input bot,
with earned weapons/ammo and score plus original next-level player construction.
The updated chain, including crowd/player clearance and the latest special-weapon
changes, completes the following maps identically on GCC ASan/UBSan/leak detection
and MSVC:

| Map | Frames | Cumulative score | Pickups | Spawned enemies | Health |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 5,110 | 24,000 | 8 | 19 | 100 |
| 2 | 10,584 | 112,000 | 12 | 49 | 100 |
| 3 | 10,118 | 234,000 | 17 | 73 | 95 |
| 4 | 13,923 | 408,000 | 22 | 104 | 100 |
| 5 | 12,997 | 622,000 | 26 | 119 | 97 |
| 6 | 15,530 | 844,000 | 13 | 146 | 84 |
| 7 | 11,026 | 1,022,000 | 11 | 112 | 100 |
| 8 | 16,028 | 1,194,000 | 17 | 102 | 48 |
| 9 | 17,235 | 1,330,000 | 14 | 106 | 76 |
| 10 | 10,930 | 1,386,000 | 6 | 50 | 46 |
| 11 | 17,231 | 1,544,000 | 9 | 99 | 45 |

The final map has a separate Normal standalone completion in
`campaign-standalone-validation.json` and its Windows counterpart: 17,270 frames,
117 spawns, 8 pickups, 160,000 score and 63 health. The D8 boss naturally enters,
takes damage, releases its group on death and reaches the original script exit;
boss/post-boss music each starts once. Nine boss firing events are observed.
No active type-6A boss projectile survives to the post-frame observation in this
route, so this does not establish natural D8 projectile flight. Some optional
group-six enemies remain alive, as permitted by the exit.

The uninterrupted development chain beyond map11 died on the final map at
frame 4,961, before D8. Its actual earned entry had weapons `0x37F`, 96 shotgun
shells, 25 rockets and no automatic/cannon/special ammunition; no fresh starter
grant was substituted. This probe is not completion evidence. The frontend
now admits all twelve maps at the user's request, independently of automated
playthrough results. The next campaign check is the user's manual playthrough.

The later-map bot uses normal vertical mouse input to aim at visible body parts,
backs away from ledges, uses earned special weapons and steers away from
nearby enemies while firing. This run uses the current original-rate minigun,
clocked chainsaw, firing-animation weapon handoff and immediate E-button policy.
The bot chooses its earned shotgun for close threats on earlier maps, holds a
steadier automatic weapon on map7, and keeps more distance on maps6/7. When low
on health it can aim/fire while moving toward a reachable supply pickup. These
are ordinary W/A/S/D and mouse inputs, not health or ammunition grants.
Its dedicated collision scratch checks walking routes without changing gameplay.
Cached planning edges use short moves,
a small clearance margin and current sector heights. Reaching the destination's
grid cell retains the exact final approach needed for pickups and wall switches.
These are test-bot decisions, not changes to original AI, movement or damage.

Praetorian Fort adds a 32-unit planning grid and pit-aware walking probes.
Its first arena retains exact-width stair routes; after a genuine stall in the
southern rooms, the bot requires one unit of extra route clearance,
retains fractional steering and approaches interior points near wall corners.
This avoids planning exact64-unit passages that cannot accept its current
fractional position. Only the private planning map and normal keyboard/mouse
events are affected. The actual player radius, wall sliding, hazards, enemy
health, damage, script groups and ammunition remain unchanged. Its copied-body
planner can consider a low ledge, but only ordinary Space input allows the real
player to cross. Navigation jumps release the bot from the raised secret-room
exit without relocating the gameplay body. Ninth-map inputs keep more distance
and collect reachable armor between waves. Final-map inputs finish an actual
shot before reassessing a brief visibility gap, avoiding repeated cancelled
lower/raise animations. These are input-driver choices only.

The bot now uses its earned cannon ammunition when automatic guns are empty,
alongside real Prion and Time Strike pickups. All 99 original enemy spawns and
the final group-gated exit execute naturally. Optional group-zero enemies
remain alive in the completed earned run, as permitted by the original exit.
An earlier standalone Praetorian Fort checkpoint completed independently of
carry. The current standalone report instead validates the final map.

Complete BFS trees cache the original predecessor/queue order for repeated goals
from the same grid cell. Changes to sector heights, body dimensions, clearance
or the start cell invalidate the tree; exploration still reads current visited
sectors. A30000-frame cached/uncached development comparison produced identical
diagnostic logs and every result field. Both runs failed at the same route, so
that comparison proves cache equivalence rather than level completion. Those
logs remain `eleventh-corner3-earned.*` and `eleventh-cached-earned.*` under
`build/native-campaign/`. The successful uninterrupted chain supersedes them.

The fourth map's opening armor pickup triggers its original script and opens the locked
start door. Script OPEN preserves the lock flag; the test observes actual door
clearance rather than requiring an invented unlock. The real exit can be reached
with two of its three switches activated. The fifth map completes without a wall switch;
its pickup and sector scripts supply the required progression. Optional original
content remains loaded and available.

Earlier fourth/fifth routes included 3,697 airborne, 13,692 third-person and 10,793 pitched
frames, plus308 level/pitched PC draws alongside the reference renderer. Ten
carry guards still reject invalid/late values. No inventory grants, teleports,
forced defeats or script-state shortcuts are used in the completion run.
`campaign-validation.json` binds all linked sources, headers and test includes.
The separate second/third-level drivers remain available; their older reports
record earlier checkpoints. The sixth map naturally spawns and defeats D7 through
the original scripts, then completes through opcode 9. Unlike the earlier maps,
it has no initial exit pickup. The seventh map is the first in the second
resource family, and now completes with112 spawned enemies and its real scripted exit
using only the sixth map's earned carry and its own pickups. It adds3,439 airborne,
8,724 third-person and9,000 pitched frames, with196 PC/pitched draws alongside
reference rendering. It acquires weapon7 from the actual pickup and fires twice,
recording390 converted-enemy controller frames.
The eighth map activates both recessed switches and reaches its original VM exit
in15,023 frames, recording102 enemy spawns,17 pickups,2,115 airborne frames,
7,320 third-person frames and7,557 pitched frames. Both original and PC renderers
run, including168 PC level/pitched draws. The carried conversion gun fires twice,
and410 controller frames now contain a converted enemy during an actual completed
campaign map. The real native mixer plays all12,018,400 stereo frames with zero
decoder errors. These current routes include the corrected Prion launch and
Gunrilla cannonball clearance.

The ninth map completes in13,227 frames with106 original enemy spawns,14 pickups
and53 health. The bot acquires weapon9 and fires20 times, recording448 actual
E3 actor frames in the shared projectile scheduler. It also records12,991 converted
enemy frames,2,382 airborne frames,6,627 third-person frames and4,857 pitched
frames. Both renderers run, including148 PC level/pitched draws. The native mixer
renders10,581,600 stereo frames with zero decoder errors. This is an earlier
nine-map checkpoint; current admission is summarized above.

The test planner recovers from grid/corner stalls by making short ordinary
movements at exact cardinal/diagonal headings, looking for doors and jumping.
Map6 uses3 recoveries and map7 uses28. Collision and original wall sliding remain
unchanged. Recessed switches use a farther80-unit input-planning approach when
the original48-unit stand point is too small/low for Sam. The original use query
spans64 units plus body radius, so it activates them normally from the room.
Both normals are tried; no runtime collision or interaction range is changed.
The eighth, ninth and tenth maps each use two navigation recoveries. The later-map
bot reverses a blocked strafe, jumps more often during fights, uses carried
conversion ammunition against Gunrillas, and avoids wasting shots on converted
allies or repeatedly selecting weapon9 for a target already marked by its effect.
These remain normal PC inputs. A closing door's original radius-sampled crushing
hazard killed the first probe; the bot now waits clear and looks away until the
door finishes closing. It records14 such waiting frames on map9 and11on map10. No game hazard,
collision, enemy health, inventory or script behavior was changed to pass.
Earlier failed/aborted probe
logs are retained in `build/native-campaign/`; they are not completion evidence.
The updated GCC sanitizer/Windows proof is the full 140,712-frame chain,
rather than an isolated granted
loadout or a resumed snapshot. The player's stats, actor bodies and scripts are
never changed by the bot. The checker can write an earned entry snapshot with
`ssa_native_campaign_check.exe 9 build/native-campaign/entry.bin` and run an
isolated development probe using `--resume 9 build/native-campaign/entry.bin`.
Resumed probes are explicitly labeled and are never accepted as the default
consecutive campaign validation.

`scripts/test-native-campaign.ps1` also exercises the actual Windows launcher,
Enter advancement and R restoring entry values with both renderers and a real,
muted audio device. Its explicit end-screen/death fixtures test the frontend;
the independent campaign bot proves real gameplay completion. It additionally
checks original map presentations with no simulation progress and the
map-to-level music handoff on advancement and restart. Physical input
and listening remain untested. The current run covers all ten boundaries and
entry restarts through index10, with40frontend stages,21map views and1470frozen
map ticks per renderer. All21reveals check immediate weapon/HUD display,
movement and entry state; captured black/midpoint/full pixels verify each fade.
Evidence is in `campaign-validation.json`
and `windows-native-campaign-validation.json`.

The ownership test adds 4,817 checks on all 16 maps. The existing combined frame
regression passes 997,410 assertions over 8,220 frames. New enemy, pickup and particle
recovery proofs are documented in `enemy-control.md`, `enemy-ranged.md`,
`pickups.md` and `particle-projectiles.md`. These behavioral ports do not imply
additional byte-matched functions in the decomp.wtf catalogue.

Build/test commands:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1
powershell -ExecutionPolicy Bypass -File scripts/test-native-game.ps1 -SkipBuild
powershell -ExecutionPolicy Bypass -File scripts/test-native-campaign.ps1 -SkipBuild
.\decomp.ps1 test-second-level
.\decomp.ps1 test-third-level
.\decomp.ps1 test-campaign
.\decomp.ps1 test-native-playability
.\decomp.ps1 test-native-game
.\decomp.ps1 test-map-gameplay-ownership
```

Player rockets and cannonballs now fly independently from launch, and continuous local weapon audio
stops when firing ends. The changed rocket combat route accounts for the updated
fourth/fifth-map outcomes above; the normal-input bot is unchanged. It also exposed
a portal raster boundary now clipped safely. See [weapon fixes](weapon-fixes.md).

The Win32 map handoff now puts the weapon at its normal ready position before
building the first reveal image. The weapon fades in together with the world
from the first visible frame; it no longer raises only after the reveal ends.
Third-person hiding and music timing remain unchanged. Gameplay and the HUD now
update during the reveal, with controls available from its beginning.

The current reveal is live: capture and keyboard controls are available immediately,
and input, physics, enemies, weapon animation and HUD advance on the normal
simulation clock while each fade step is presented. Sprite cache transfers are
completed before the first visible image, so the full HUD accompanies the ready
weapon. The outgoing map and black hold still keep gameplay stopped and own
the map soundtrack. The reveal lasts at least one second independently of the
simulation clock. Mouse look stays active throughout the reveal.

Windows transition checks deliver W input and require actual XY movement during
every map reveal in both renderers. Initial HUD residency and entry carry are
checked before live gameplay can change inventory. Black/midpoint/full images
are compared to paired unfaded images from their own simulation frame; the
older frozen-frame comparison is no longer appropriate. These diagnostics are
recorded in windows-native-campaign-validation.json.

The Windows frontend also accepts mouse-wheel weapon selection while gameplay
has mouse capture. Wheel up selects the previous usable weapon and wheel down
selects the next, wrapping through IDs 1–10 and skipping unowned or empty weapons.
The relative selection begins at the requested weapon, so rapid scrolling works
during the original lower/commit/raise animation. Partial high-resolution wheel
deltas accumulate; focus/capture loss clears them. Menus, map view, pause and
death do not accept wheel selection. Number keys retain their existing behavior.

`scripts/test-weapon-wheel.ps1 -SkipBuild` exercises the actual Win32 message
handler on a hidden window and advances native ticks through a completed switch.
Its 158 checks cover rapid/partial/multiple notches, both wrap directions,
Q toggles, key repeat, availability, inactive input and bounded extreme step
counts. It does not test
physical mouse delivery. Results and hashes are recorded in
`windows-weapon-wheel-validation.json`.

Q now selects the last actually equipped weapon, so successive presses toggle
between the last two usable weapons. Requests alone do not alter history;
the native lower/commit stage does. History carries between levels and in the
restart entry state. An unowned or empty previous weapon is ignored. Key repeat,
menus, map view, pause and inactive gameplay cannot trigger Q.

E button activation is independent of weapon stowing. The frontend's
`gameplay.immediate_use` policy activates the original switch, texture, script
and click on the first valid use tick, while the real weapon action/animation
continues. The use key, contact geometry and original one-shot switch latch
still apply. This is separate from the requested firing-completion gate for
weapon changes. See [interaction checks](player-hazards.md).

The frontend enables `finish_fire_before_switch`. Number keys, wheel and Q can
change the pending destination during firing, but lowering waits for completion.
Discrete weapons finish their entire firing/reload sequence. Continuous weapons
coast to the current cycle's end without further attacks, then run the normal
stop/wind-down and handoff. Sticky loop completion flags are reset when this
wait begins. Pause freezes the wait. Selecting the current firing weapon again
cancels the pending switch. Impact effects use the actual firing weapon during
the wait. The default recovery runtime leaves this PC policy off.

`tests/test_weapon_switch.py` and `scripts/test-weapon-switch.ps1 -SkipBuild`
pass 3,295 checks on GCC ASan/UBSan with leak detection and MSVC: all ten
weapons in both views, 506 waiting frames, pause, released fire, empty automatic
ammo, overwritten/cancelled destinations, previous-weapon availability and
campaign carry. Cadence/quick-click checks remain 53,141 and the original
runtime regression remains 133,923. Windows weapon-audio/rocket/Prion checks pass
127,418 assertions including six cannon launch regressions; frontend
perspective/chase/4K/reference smoke remains a separate earlier checkpoint.
The current eleven-map campaign proof above includes this PC handoff policy.
