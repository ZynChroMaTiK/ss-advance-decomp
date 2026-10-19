# Development reference and research archive

These are detailed notes from the port’s development, including earlier milestones.
For the current download, European ROM requirement, controls and setup, start with
[the player README](README.md). Older counts and limitations below describe the
work at the time; they are not a current release checklist.

**Windows x64 prerelease: [download v0.8.0-alpha.2](https://github.com/ZynChroMaTiK/ss-advance-decomp/releases/tag/v0.8.0-alpha.2).**
Extract the ZIP to a writable folder, put your own ROM at `orig/SSA.gba`, and run
`play-native.cmd`. The package includes the native executable; no build tools are
needed. ROMs and SoundFont banks are not included. See the
[release notes](releases/v0.8.0-alpha.2.md) for features and known limitations.

[Project website, features and roadmap](https://zynchromatik.github.io/ss-advance-decomp/)
· [3D level browser](https://zynchromatik.github.io/ss-advance-decomp/#levels/1/3d)

## Build from a fresh checkout

Install Visual Studio 2022 Build Tools with the C++ desktop workload and CMake
tools. Clone this repository, open PowerShell in its folder, and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
```

Compilation does not require a ROM. To play, place your own game ROM at
`orig/SSA.gba`, then run `play-native.cmd`. The launcher uses
`build/native-windows/Release/ssa_native_game.exe`; build it first.
All twelve campaign levels are enabled, including Caesar's Palace.
Campaign playthrough validation is manual at the user's request.

Options starts with **Settings**: its first row is **Display mode**, choosing
**Fullscreen** (borderless, default) or **Windowed**. Changes apply immediately
and persist in `settings.dat`; windowed mode restores the previous window size
and position. Then choose **Software / GBA** rendering,
choose **Original / SoundFont / MIDI** music and
adjust **Music volume** and **Sounds volume** with bars and percentages.
The separate Music On/Off option is removed; set Music volume to0% to silence music.
Options lists Settings, Extras, Language, Clear Data, then Credits at the bottom.
**Extras** contains Jump, Third-person, In-game NETRICSA, Alternate weapons,
Automap, HUD, Weapon bobbing, Surface decals and Water reflections. All nine
items fit on one page. HUD defaults to Original; the added visual effects and
automap default to Off. Press M for an enabled automap and F11 for display mode.
R reloads alternate Colts rather than restarting the level.
Jump and Alternate weapons default to Off; Third-person and In-game NETRICSA default to On. All persist with your settings.
With Alternate weapons On, a new campaign starts with one Colt; picking up another unlocks and
equips dual Colts. Each Colt fires every 0.45 seconds; duals alternate hands at 0.225 seconds.
A click fires two alternating shots even when released quickly; holding fire continues the sequence.
Each holds six shots with unlimited reserve ammo. Reload starts when the next gun's turn
finds it empty, allowing 12 alternating shots from full duals. The right Colt reloads
first, with the left starting 0.33 seconds later: 1.07 seconds per gun, 1.40 seconds
for the complete pair. Single Colts reload alone after six shots.
Each reload plays a local mechanical click. Re-equipping a Colt refills its six-shot magazine;
switching from single to dual refills only the left gun that is drawn.
These timing and magazine rules apply only with Alternate weapons On.
That mode also gives the double shotgun a fixed 14-pellet pattern, with 5 base damage
per pellet (70 for a full hit), horizontal spread up to ±6.84° and vertical rows
at +2.58°, +0.86°, −0.86° and −2.58°. It follows free aim and GBA auto-aim.
The alternate double shotgun fires every 1.75 seconds, with its animation frames
spread evenly across that cycle.
Colts retain key **2**: press it again to toggle single/dual once acquired. Mouse-wheel order puts
single immediately before dual. Switching variants draws/stores only the left Colt.
Password/level-select starts after the first level upgrade starting Colts to duals in this mode.
Disabling in-game NETRICSA blocks Tab/N during
gameplay; pre-level briefings still appear.
Disabling Third-person returns to first-person and disables the H view toggle.
GBA rendering keeps the camera level and moves the original crosshair vertically
with auto-aim without following camera bob. Third-person cursor placement accounts for the raised camera.
Use Left/Right or click a bar; preferences persist in `settings.dat`.
For the optional SC-55 music style, run
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/install-soundfont.ps1`.
You can load another GM bank with `play-native.cmd --soundfont "path\bank.sf2"`.
SoundFont is offered only when that file exists and loads successfully. MIDI
uses **Microsoft GS Wavetable Synth** and needs no SF2 file; it is offered when
Windows can open that device. Missing saved modes fall back to Original.
MIDI mode compensates for the GS synth's measured delay by buffering only the
original music samples for 185 ms. Sound effects stay immediate. For a different
audio device, override with `play-native.cmd --midi-delay 0..500` (milliseconds).
SoundFont mode retains the ROM's sampled instruments and replaces only PSG
instruments. Its music uses the volume slider without Original-mode fixed
boosts/cuts; a 15 dB SoundFont trim balances it against the retained samples,
then a shared +3 dB gain raises the whole mix. Final output gains raise both
music modes to full peaks (Original +9.54 dB, SoundFont +6.02 dB), with peak
protection to prevent overload.
See [audio settings and checks](analysis/audio-settings.md).

The repository includes native source, Windows frontend, tests, scripts,
research, recovered C and the complete static website. ROMs, saves, generated
assembly, toolchains, dependency checkouts and local server databases stay local.
See [preserved recovery sources](recovered/snapshot/README.md) for the exact C
export and sanitized candidate/attempt/note history.
GitHub Pages publishes only `site/`; source additions do not change its URL.

Optional original-ROM timing tests use a pinned test-only mGBA checkout:

```sh
git clone https://github.com/mgba-emu/mgba tools/mgba-timing
git -C tools/mgba-timing checkout --detach 26b7884bc25a5933960f3cdcd98bac1ae14d42e2
```

The native game does not link against mGBA. Decompilation tooling is provisioned
separately by `scripts/bootstrap.sh` under WSL/Ubuntu; see the setup below.

## Current prototype

**All twelve campaign levels are available natively on Windows.** Run `play-native.cmd` to use your
supplied `orig/SSA.gba`. This build includes actual enemy AI, weapon firing,
damage/death, pickups, scripted waves, doors, the exit, and restart, with the
original world, actors, HUD and weapon graphics.
New Game includes the original Egypt intro; entering Rome shows its chapter
panel. After the final level's bonus tally, the ending returns to Main.
Up/W pauses the story, Down/S scrolls faster, Right skips a hold, and
Enter/Space/Esc or a click skips the screen. Preview the ending directly with
`play-native.cmd --story 12`. See [campaign screens](analysis/campaign-story.md).
Rome maps use the original golden ankh pickup, including its full and distant
sprites decoded from the ROM.
Time Strike targets now keep a valid rendered frame if their controller changes
animation while frozen, preventing a reference-renderer failure during combat.
Health, armor and boss bars now use the original scanline palette colors,
including the original health-above100 variation.
Player collision now detects large enemies before Sam enters their bodies.
If a scripted spawn already overlaps Sam, movement away can free him while
walls and other enemies still block him.
Startup goes straight to the title screen. Options includes a Language row:
Enter, click or Right cycles English/German/French; Left cycles backward.
Language and beaten levels persist in `save.dat` beside this launcher.
Single Player has New Game, Select Level, Password. Level 1 is always unlocked;
beating a level unlocks its successor on any difficulty. Select Level uses the
original localized campaign names, shows locked levels with padlocks, and omits
multiplayer maps. Egypt and Rome each have a page of six levels, shown without number prefixes.
Use Up/Down to select a level or Left/Right/the arrows to switch chapters.
Caesar's Palace unlocks when Praetorian Fort is beaten, using the existing save.
Selected levels use their original starting weapons and ammo after choosing difficulty.
Options → Clear Data resets progress and language after a Yes/No warning;
No is selected by default, and Esc cancels. New Game keeps existing unlocks.
See [saved data and level selection](analysis/profile.md).
The original background grid appears from the first title/main-menu visit.
Choose Single Player, New Game and a difficulty to begin. Use arrows or WASD,
Enter/Space to confirm, or hover and click a menu entry. Esc goes back.
Mouse selection plays the original cycling sound only when the highlighted item changes.
The title music continues after Press Start without restarting.
Options → Credits now shows the original scrolling credits. Hold Up/W to pause
or Down/S to speed up; Enter/Space/click or Esc returns to Options.
Single Player → Password now restores original starting weapons and ammunition
for all twelve campaign maps. Type the password directly (case-insensitive),
with or without its trailing underscore placeholders;
use Backspace/Delete to correct it, and Enter or a click to submit. Left/Right
moves the cursor; Up/Down retains the original character cycling. Esc goes back.
Passwords select difficulty too.
Fresh `--level` launches use that map's original starting weapons and ammo.
Advancing from a previous level keeps your carried weapons and ammunition.
Original ROM sound effects and level music now play through a native audio mixer.
Pickup names, boss introductions and death messages now use the original
localized sprite lettering, reveal and expiry timing in both renderers.
See [original HUD notices](analysis/hud-notices.md).
NETRICSA appears before the map view on every level start/restart, with its
original animated background. During gameplay, press N or Tab to open it as an
overlay over the paused game, without that animated background. Tab toggles it
on/off during play; Esc or N also returns. It includes original localized weapon/enemy records,
illustrations, strategic/tactical briefings and live statistics. Enter/Space cycles
panel focus; Up/Down selects categories or scrolls text; Left/Right cycles records.
Hover/click categories and panels, wheel to scroll, or click either half of an
illustration to cycle records. The overlay preserves the current renderer's resolution.
Click numbered weapon/enemy indicators above the text panel to view a record;
unavailable records are dimmed and the current selection is highlighted.
During play, NETRICSA fades in/out over250ms, removing the HUD while keeping
the weapon visible and darkening the level with a green tint. Before map view,
NETRICSA keeps the original fade speed; Tab closes it and proceeds to map view.
See [NETRICSA recovery and controls](analysis/netricsa.md).
The overall soundtrack plays at50% of its previous amplitude so effects stand out.
Audio buffers refill on device events, keeping music at normal speed when the
mouse and UI are idle.
Footsteps follow grounded movement in both views without interrupting the music.
Tommy gun fires about 8.53 times per second. Chainsaw uses the measured original
GBA average of about 8.69 attacks per second, with increased 192-unit PC reach.
The minigun also uses its measured original GBA cadence, about 8.53 rounds per
second, with its existing damage per round.
Chainsaw bites accumulate damage between attacks so they also hurt enemies on Hard.
Special weapons 8 and 9 now fire natively: the area blast and targeted falling
effect include their original background-palette flashes. Hurt flashes expire,
allowing later hits to play the hurt sound again.
Time Strike falls at the measured original GBA update rate and retains original
damage, including tougher enemies surviving. Empty space, items, corpses and
temporarily protected enemies consume no Time Strike ammo and play no firing sound.
These attempts still play the weapon's firing gesture. Falling objects move
smoothly between physics updates, preserving their landing and damage timing.
Aim, movement and release remain responsive at 60 Hz.
Continuous weapon loops stop on release, switching, empty ammo, pause or focus loss;
the minigun keeps its wind-down sound. See [weapon fixes](analysis/weapon-fixes.md).
Prion gun shots now start on the aim line and fly independently immediately,
like the corrected rocket launcher, without following later mouse movement.
Cannonballs also fly independently from the instant of launch, so later mouse
movement, walking or jumping cannot steer them.
Prion-affected enemies run their behavior and animation at twice normal speed.
They continually target the nearest living enemy or player, without preferring Sam.
Quick fire clicks stay pending until the first shot, even if released during
weapon startup, raising or reload. Holding fire keeps the same firing rate.
World and enemy sounds pan left/right and fade with three-dimensional distance
from Sam. Doors use the same smooth falloff as enemies, getting quieter as you
move away even at close range. Pickups play locally like player sounds, without
distance attenuation or positional panning. Door loops stop when each door stops; multiple enemies can sound at
once, with moving sounds following their own source.
The U.M.T. pickup cue finishes after its initial sequence instead of leaving a buzzing loop.
The third-person camera keeps the same audio listener as first person.
Before every level start/restart, the original map view plays its start music.
Click or press Space/Enter to leave the map; it fades to solid black, pauses,
then gradually reveals the level with its soundtrack. The weapon is visible from
the start of the reveal. Movement, aiming and the live HUD are active during it.
Death and level completion select their original music cues.
Old effect loops stop on the end screens.
Instrument note tails now follow the ROM's echo settings.
Music and effects use separate voices, so weapons, enemies and footsteps can
play together without cutting off the soundtrack. Player pickups, weapons and
hurt sounds take priority over enemy/world effects when sound pools or output
headroom fill up. Each music track plays one
note at a time; repeated effects can overlap. Sampled instruments and effects
receive a 6 dB PC volume boost to improve their balance against PSG channels.
World, player and enemy effects receive an additional 50% boost with peak protection.
Wave instruments also use the original lower octave and stepped volume conversion.
Single-player Sam uses the original white-shirt variant.
Enemies slide around one another in crowds and gradually separate overlapping
spawns, while keeping walls, doors and the player solid.
Gunrilla cannonballs clear their shooter's collision bounds before normal
collision with that shooter resumes; walls, players and other enemies still block them.

Mouse look activates automatically during gameplay and when returning to the game.
WASD moves, mouse turns, left mouse fires, Space
jumps, H toggles first/third person, E uses doors, and 1–0 selects owned weapons.
Mouse wheel up selects the previous usable weapon; down selects the next.
Cycling wraps around and skips unowned or empty weapons.
Q swaps to the last equipped weapon. Weapon changes wait until the current
firing/reload animation ends; automatic weapons finish the current animation cycle.
Buttons activate immediately on E, without waiting for the weapon to finish lowering.
Esc or P opens the pause menu and releases the mouse. Continue resumes; Quit
asks Yes/No, with No selected initially, then returns to Main. The separate Quit
item at the bottom of Main asks Yes/No before closing the application, with No
selected by default. No or Esc returns to Main with Quit selected.
Arrows/WASD and Enter/Space, or mouse
hover/click, operate it. Esc cancels quit confirmation or resumes gameplay.
R is unbound and does not restart the level.
Level completion now shows the original localized time/ammo bonus tally and
score digits over the frozen level. Click or press Enter/Space to finish the
current bonus count immediately, awarding every remaining point. Holding an
action still counts faster. The final Sam text stays fully visible for three
seconds and waits for completion music; a fresh confirm after one second can
shorten the reading hold once music ends. The original sound7 is a short
synthesized cue, not recorded speech.
Then the next map retains earned weapons, ammo and the bonus-adjusted score,
with100 health and no armor. Chapter and ending panels follow the tally.
See [original campaign entry](analysis/campaign-entry.md).
The native snapshot path also uses the recovered original weapon-progress
capture, with verified per-weapon ownership, signed ammo and shot/hit counters.
The original completion-statistics accumulator and checkpoint helpers are also
recovered and verified. Completion totals and checkpoints now connect to the
frontend's campaign session. High-score/name entry remains unfinished.
See [statistics recovery](analysis/statistics.md).
The native backend now also reproduces the original timed time/ammo bonus tally
and completion-record ordering. Its frontend presentation now uses the original
notice and number sprites; see [pause and tally integration](analysis/completion.md).
Press M to mute/unmute audio. Use `play-native.cmd --no-music` for effects only,
or `play-native.cmd --mute` to start silently. Mouse look turns and aims vertically
through 80 degrees up or down, with perspective-correct world rendering.
The sky stays flat and perpendicular to the view, with horizontal panorama scrolling.
At level aim its artwork fills the top three quarters. Looking up or down shifts
the image vertically, extending each column's top/bottom edge pixel into exposed space.
At level aim, the H camera sits 256 units behind the player with a 64-unit
height bias. It orbits with mouse pitch and points along a common sight ray,
keeping the original crosshair aligned; walls and ceilings retract the camera.
Switching to the chainsaw keeps the same camera angle as ranged weapons.
Weapon muzzle flashes are hidden in third-person view and visible in first person.
Ranged weapons have no distance cutoff; shots stop at obstructions or the map
boundary. Player projectiles continue until impact. Rockets follow their launch
direction immediately, independently of later mouse turns or player movement.
The chainsaw remains a melee weapon with a 192-unit trace, twice the ROM's 96.
The original crosshair artwork and behavior are restored. Rendering now caches
static geometry and presents completed frames offscreen to reduce black flicker.
If launching fails, the launcher keeps the error visible and saves a separate
session log under `build/logs/`. It also updates `build/logs/last-launch.log`
when available. Multiple game sessions can launch concurrently.

Rendering now follows the window size automatically. Resize/maximize for more
world detail, or run `play-native.cmd --resolution 3840x2160` for a fixed size.
Arbitrary dimensions are supported within memory and platform limits; this
renders new world samples. `--original-renderer` restores the 240×160 reference
view. See [render settings](analysis/pc-renderer.md).

The updated normal-input campaign run completes eleven consecutive maps in
140,712 frames, ending with 1,544,000 total score and 45 health. Weapons and ammo
are earned; each next level uses the original 100-health/no-armor construction.
It covers pickup-triggered doors, wall switches, elevated
enemies, the first boss, Prion conversion, Time Strike and the real exits.
GCC ASan/UBSan with leak detection and Windows MSVC agree on every result.
Praetorian Fort completes in 17,231 frames with all 99 original enemy spawns,
scripted waves, pit hazards and the real exit. Both renderers and the native
mixer remain active. The final map separately completes from its original Normal
starter loadout in 17,270 frames, including D8 defeat and the scripted exit.
These are historical automated results. All twelve maps are now enabled at
the user's request, without requiring an automated earned-inventory completion.
To jump directly to the last map for manual testing, run
`play-native.cmd --level 11`. See [campaign details](analysis/native-game.md#validation).
Converted enemies can now fire safely after their previous target's actor slot
has been cleared or reused, preserving the original slot behavior.
See [playable build details](analysis/native-game.md).
Original feature and behavior parity is the completion target, alongside the
explicitly requested PC controls and rendering changes. Full campaign playthrough
verification, score/name entry and remaining
menus/presentation effects are unfinished. Original chapter/ending panels now
run natively; see [campaign story recovery](analysis/campaign-story.md).
Audio fidelity is still being refined;
see [audio integration](analysis/audio.md).
See [special weapons and palette recovery](analysis/special-weapons.md) for the
new integration checks and remaining presentation limits.
The native effect runtime now also supports original type-64 wall/sector
clipping and relocation; see [recovery and checks](analysis/sector-clip.md).

The separate `run-native-viewer.cmd [--level 0..15]` remains a diagnostic map
viewer with idle objects; it is not the playable build.

## Verified progress

- Native first-level gameplay with crowd sliding completes through normal PC input
  in 5,098 frames: 24,000 score, 8 pickups, 19 spawned enemies, 13 pistol and
  21 shotgun shots, with 94 health and no living enemies or open script groups.
  Both GCC with ASan/UBSan/leak detection and MSVC x64 pass identically.
- Level two completes with the actual earned inventory from level one, including
  its wall switch, waves and exit. Windows checks cover Enter advancement,
  level-entry restart, both renderers and audio reset.
- Level three completes from that earned inventory, activating both wall switches,
  fighting its waves and reaching the real exit. The route includes jumping,
  5,397 third-person frames and 118 reference/PC rendering checks.
- Levels four and five complete with earned equipment, including the rocket
  launcher and minigun. Their routes include vertical mouse aiming, jumping,
  H-view toggling, scripted waves and optional switches.
- The original pre-level map view restores artwork, route animation, fonts,
  localized names, passwords, prompt and fades. Gameplay stays frozen while
  map music plays. See [map-view recovery](analysis/map-view.md).
- Original menu recovery includes all three languages, the title prompt,
  animated background/logo, single-player/new-game flow, difficulty and music
  options. Difficulty affects combat and language carries into map screens.
  See [menu recovery and remaining gaps](analysis/menu.md).
- Shared ownership preserves original map-before-player slot and spatial-list
  order. Adopted enemies/pickups can be damaged, released and reused; 4,817
  ownership checks pass across all 16 maps.
- Both original boss controllers now run natively, including volleys, reinforcement
  script requests, damage/death and the boss health gauge binding. ROM comparisons
  and Windows/GCC integration pass. The sixth map now includes its natural D7 boss
  fight, boss music and scripted completion using earned equipment. D8 now has
  a separate standalone completion; its earned-inventory campaign path remains
  unfinished. See [boss recovery](analysis/enemy-boss.md).
- CF/D0/D1/D2 and CE/D3/D4/D5/D6 controllers, native pickup contact binding,
  particles, impacts, enemy target aiming and extended projectile controllers
  are connected to production owners. See `enemy-control.md`, `enemy-ranged.md`,
  `pickups.md` and `particle-projectiles.md` under `analysis/`.

- Uses [decomp.wtf](https://github.com/i12bp8/decomp.wtf), pinned to
  `2b0321db4a505a04c4e9aa47ed914758dd7aaf6c` (0.9.6).
- Identified the supplied European AENP ROM, SHA-1
  `4ff48d8c18b70a5184758f1917343ae85a70858e`.
- Catalogued 526 functions: the initial conservative 525 plus one validated
  entity-collision callback. Discovery is incomplete;
  these are not all functions in the game.
- 78 functions compile to the exact original bytes: 2,816 of 87,648 bytes in
  that catalogue (3.21%). The initial automatic pass also produced 80 near-matches.
- The exported C plus remaining assembly/data rebuilds the entire 8 MiB ROM
  byte for byte. This is a GBA rebuild, not a PC executable.
- Native keypad polling, input recording and input playback are implemented in
  `src/input.c`. They passed 14,848 differential cases against the original ROM.
  They compile and run as native x86-64 C under Ubuntu. All portable native
  libraries now also compile for Windows x64 with the installed MSVC toolchain.
- The native CMake library build and all four upstream GBA regression tests pass.
- Native movement controls and pre-collision velocity generation are recovered,
  along with heading lookup math. They pass 106,577 ROM differential cases,
  including every heading and consecutive movement frames, with C undefined
  behavior checks enabled.
- Native vertical physics and camera-height routines pass 31,567 further ROM
  comparisons. The new jump extension passes 36 behavior scenarios, including
  landing, head collisions, held-button handling and camera following.
- All 16 real map resources load natively from the user's ROM. Bounds-checked
  decompression and map validation pass; 7,632 wall records match original setup.
- Native BSP lookup, static wall collision and clearance/position commits pass
  62,118 ROM comparisons. Player-start recovery is verified on all 16 maps.
  Another 32 jump scenarios use real map geometry, including walking jumps from
  each original player start with the original player radius and height.
- A new native first/third-person camera toggle maps H key events to view mode.
  The chase camera follows jumping and retracts around static walls/ceilings.
  Tests cover 2,048 camera frames on all 16 maps. OS event delivery, player-body
  rendering and visible third-person gameplay still require the runtime.
- Original entity collision, spatial lists, wall sliding and horizontal timestep
  are now connected in a native world layer. They pass 71,808 additional ROM
  comparisons, including 2,048 complete horizontal updates on real maps.
  Another 2,048 native frames combine horizontal motion, jumping and H-view
  updates. Synchronous pickup removal adds 1,024 ROM query comparisons.
- Native inventory helpers and the pickup dispatcher pass 31,740 and 5,420 ROM
  comparisons respectively. Another 64 queries across all 16 maps verify pickup
  refusal, acceptance, inventory updates and removal during world traversal.
  Script, sound, HUD and network effects remain explicit runtime boundaries.
- Native queued-weapon acquisition and cycling pass 29,176 ROM comparisons and
  18 pickup-to-acquisition scenarios. Animation and firing are described below.
- Original map-object construction passes 256 complete routine comparisons,
  including 3,128 entity records and 728 start markers. This recovers ordering,
  placement and pickup IDs; entity AI activation remains separate.
- Player initialization passes 688 whole-ROM comparisons. Complete player
  movement passes 8,672 controlled-boundary cases and 2,048 real-map frames,
  including multiplayer pushing, synchronization and all recovered physics.
- The player-frame wrapper passes 11,656 comparisons; the shared entity timer
  passes 147,456. Its stage adapters connect the recovered player routines.
- A native player runtime connects initialization, movement, jump, camera and
  H-view state. It passes 188,189 assertions across 5,408 frames on all 16 maps
  with address and undefined-behavior sanitizers. It accepts relative yaw with
  full heading precision; physical keyboard/mouse events and rendering still
  require a platform backend.
- Native death/respawn, respawn-point selection, and environmental interactions/
  hazards pass 10,494, 4,998, and 24,576 original-ROM comparisons respectively.
  The gameplay adapter connects them to the runtime, retaining jump and H-view
  behavior through respawn. It passes 211,701 assertions over 10,016 frames on
  all 16 maps, including 32 complete death/respawn cycles, with address and
  undefined-behavior sanitizers. Script and presentation effects remain external.
- Native moving sectors, switches and the level-script interpreter pass another
  44,433 ROM comparisons. The interpreter suite exercises all 116 real script
  entries with controlled spawn/effect boundaries. The level runtime connects
  line activation, switch textures, first-entry sector triggers, closing-door
  hazards and script mover commands to gameplay in the original frame order.
  Integration passes 43,622 assertions over 4,128 player frames on all 16 maps,
  including jump/H-view, with address and undefined-behavior checks.

- The native 100-slot entity pool and all 12 enemy activation paths pass 36,697
  and 14,576 ROM comparisons respectively. Scripted spawning now uses that pool,
  native world insertion and original activation countdowns. Script group defeat
  notifications are separate from slot release, preserving original behavior.
  Integration passes 82,853 assertions across all 116 scripts and all 16 maps,
  with 3,072 player frames covering movement, jump and H-view. This includes
  1,983 frames of horizontal movement while a spawned actor remains registered.
  See `analysis/script-spawn.md` and its validation report.
- Shared enemy navigation, motion/hazards, death handling and the type-CD
  exploding-enemy controller pass another 121,260 ROM comparisons. Native world
  integration passes 59,319 assertions on all 16 maps, including 1,205 enemy
  movement frames, 36 corpse-expiry cycles and 2,048 player jump/H-view frames.
  Combat effects remain explicit boundaries. The 62-byte CD dispatcher also
  matches the original bytes exactly.
  See `analysis/enemy-cd.md` for ordering, integration and remaining scope.
- Native sector rejection, ordered nearest targeting and full navigation wall
  rays pass 263,969 additional ROM comparisons. They now drive the CD controller
  in a native integration with 53,384 passing assertions across all 16 maps,
  including 1,774 enemy movement frames and 4,096 player jump/H-view frames.
  See `analysis/enemy-queries.md`; weapon/entity rays are described below.
- Native damage, explosions and score rewards now drive CD contact attacks and
  chain detonations. Another 155,517 ROM comparisons and 26,435 native integration
  assertions pass, including 32 enemy chain deaths and 16 player deaths across
  all 16 maps. Jump/H-view remain covered.
  See `analysis/combat-runtime.md`.
- Native weapon animation, all ten weapon controllers, automatic aim and hitscan
  geometry pass 303,556 additional ROM comparisons. The animation setter also
  matches all 56 original bytes. Actual shot geometry is distinct from automatic
  aim; both trace walls and dynamic bodies in original order. The weapon runtime
  connects selection, animation handoff, hitscan damage and score to gameplay.
  Projectile simulation is described below; presentation remains a boundary. All 90
  native libraries build. See `analysis/weapon-controller.md`, `weapon-aim.md`,
  `weapon-hit.md`, `weapon-animation.md` and `weapon-runtime.md` in `analysis/`.
  Integration passes 100,466 assertions with address/undefined-behavior checks:
  all five hitscan weapons on all 16 maps, native damage and corpse cleanup,
  switching, wall impacts, failure cleanup, multiplayer credit/respawn, and
  2,048 movement frames retaining jump and H-view. No weapon graphics or audio
  playback are implemented yet.

- Native projectile construction, motion, five controllers, wall reflection and
  attached conversion-effect following pass 136,192 additional ROM comparisons.
  The aimed constructor also matches all 48 original bytes. Weapons6/7/10 now
  allocate into the shared native entity pool and use native collision/combat;
  no ROM instructions execute at runtime. See `analysis/projectile-runtime.md`.
  Integration passes 37,132 ASan/UBSan assertions across all 16 maps, including
  3,270 projectile frames, ten wall reflections, pool exhaustion/reuse,
  penetration, two multiplayer kills and 2,048 jump/H-view frames.
  Weapon 7 converts enemies to retarget other enemies and gradually lose health.
  Its attached effect follows the target. The earlier freeze label was incorrect;
  see `analysis/projectile-conversion.md`. Split/spread projectile
  types68/6A, actor animation playback and remaining enemy controllers remain.

- The original entity scheduler passes 72,420 ROM comparisons, including dynamic
  allocation/release ordering. Its native runtime connects players, CD enemies,
  projectiles and pickup timers, passing 85,982 integration assertions across
  all 16 maps and 2,048 movement/jump/H frames. Unsupported enemy handlers fail
  explicitly. This is the entity phase; the complete game loop remains.
- Original starting weapon loadouts pass 12,979 ROM comparisons. Native startup
  integration passes 84,077 assertions, 608 starts and 192 shots using the
  original selected weapons on all 16 maps, retaining jump and H-view state.
  See `analysis/weapon-start.md` and `analysis/entity-runtime.md`.
- Weapon-7 conversion integration passes 686 assertions with native targeting,
  movement, periodic health loss and attached-effect removal. Canonical drain
  fields now connect projectile impact to the CD controller.
- The original frame and sprite dispatchers pass 10,500 and 70,344 ROM
  comparisons, preserving pause gates, stage order, dynamic sprite lists and
  submission queues. Actor animation passes 218,240 ROM comparisons; its
  setter adds a new 60-byte exact match. Natural animation completion and corpse
  release pass 75,088 integration assertions across all 16 maps.
- Multiplayer pickup respawn passes 73,734 ROM comparisons and 6,314 native
  integration assertions on all four multiplayer maps, covering all 71 markers,
  original schedules, occupancy, timers, pool exhaustion and reuse.
- The combined frame runtime passes 997,410 ASan/UBSan assertions, including
  8,220 single-player/extra frames and 132 two-player frames. It connects native
  entity updates, movers, script spawning, pickup respawn, actor animation,
  weapon controllers and sprite animation. Tests retain jump/H, automatic CD
  binding, natural corpse release and same-frame multiplayer/script input locks.
  Platform input, mode logic and render calls are explicit test boundaries.
  See `analysis/game-runtime.md`; the first-level frontend now supplies these boundaries.

- Native PC events now drive WASD movement/strafe, relative mouse yaw, mouse-left
  firing, Space jumping, H-view toggling and E use. The frame adapter preserves
  active-player gates and remote inputs, consumes paused/disabled/focus-lost
  actions and rejects stale samples. It passes 314,726 sanitizer assertions on
  all 16 maps. The original local input distributor adds 183,988 ROM comparisons;
  genuine network distribution remains an explicit unsupported branch.
- A native Win32 keyboard/raw-mouse backend passes 3,215 MSVC assertions using
  hidden window messages and raw payloads, including focus and registration
  cleanup. Physical-device delivery remains untested. The Windows build produces
  all current portable libraries plus the platform backend: 100 native x64 libraries,
  the diagnostic viewer, playable frontend and completion-check executables.
  See `analysis/pc-input.md` and `analysis/win32-input.md`.
- All 311 original sprite assets and gameplay palettes decode natively:
  1,261 pieces and 6,730 tiles. Validation includes original transfer/OAM/palette
  routines and 2,488 sanitizer assertions. Transfer helper E320 adds a 56-byte
  exact C match. See `analysis/sprite-assets.md`. DBDC prepares sprite visibility
  and caching; its old RENDER_WORLD name was misleading and is retained only as
  a compatibility alias. The actual world renderer is being recovered separately.
- All six original world texture archives load natively: 666 images and three
  resolutions for each family. Column-major storage and original mip joins are
  verified. Map wall-texture binding passes 1,059 complete routine comparisons
  and 59,951 sanitizer assertions, including live switch changes. B0C4 adds a
  54-byte exact match. See `analysis/texture-assets.md` and
  `analysis/map-texture-bindings.md`.
- Native textured wall-column sampling passes 67,678 complete ARM comparisons,
  including 7,992 cases consuming all 666 loaded images. Flat-color fills and
  forward sky-span copies add 6,189 comparisons. These draw original indexed
  pixels; their subsequent full world composition is described below. See
  `analysis/world-raster.md` and `analysis/raster-fill.md`.
- Original pause/menu state control passes 13,092 whole-ROM comparisons with
  its remaining UI command and sprite creation calls explicit. The frontend
  still needs to bind and implement those boundaries. See `analysis/mode-control.md`.
- World BSP visibility/list construction passes 218,912 ROM comparisons;
  full solid/portal walls pass 14,322, and plane allocation/spans/sky pass 23,955.
  Connected world geometry matches 1,843,200 original pixels across 128 views
  on all 16 maps. Another 2,048 PC/game camera frames pass 481,049 sanitizer
  assertions while drawing actual world geometry, including jump/H movement.
  See `analysis/world-scene.md`. B220 and A870 add exact 42/24-byte matches.
- The MSVC Windows viewer passes 3,840 hidden-window frames across all 16 maps,
  with actual native movement, jumping, camera toggling and GDI presentation,
  including 1,600 frames of visible Sam body and 18,326 actor draw commands.
  Four invalid CLI/ROM cases are rejected. Physical-device and visible-window
  interaction remain untested. No emulator runs in the executable.

- All 142 actor descriptors and 5,407 image references decode natively; original
  compressed columns and transparent/mirrored sampling pass 13,787 complete
  ROM draw comparisons. Projection, sector collection and wall occlusion are
  native. The connected world/actor renderer matches 3,686,400 original pixels
  across 256 views, including 128 animated views, on all 16 maps. See
  `analysis/actor-assets.md`, `analysis/actor-projection.md` and `analysis/actor-queue.md`.
- `map_actors.c` owns all 385 original placed objects across the maps and runs
  their recovered animation clock. Their preactivation gameplay state remains
  unchanged; this does not simulate AI or combat. H exposes only the local
  rendering flag snapshot and preserves the player's canonical body flags.
  See `analysis/map-actors.md` and `analysis/actor-scene.md` for ownership and
  composition boundaries.
- Native sprite allocation and the original 74-object single-player HUD setup
  pass 18,576 pool and 480 complete constructor comparisons. HUD health/armor
  bars, icons, numbers and inactive notice handling pass 58,368 controller
  comparisons. Active text notices remain unsupported. See `analysis/sprite-pool.md`.
- Sprite cache, allocator, retention and tile uploads pass 30,596 whole-routine
  comparisons. Native OAM construction passes 6,036 whole FCE4 calls, including
  affine sprites. The display compositor recovers the original 240×160 sampling
  and palettes, with 635 independent pixel frames and 418,281 sanitizer assertions.
  See `analysis/sprite-cache.md`, `analysis/sprite-oam.md` and `analysis/display.md`.
- The native viewer renders the original starting knife and HUD through these
  stages. Its 3,840-frame Windows run includes 3,856 HUD draws and 2,112 raised
  first-person weapon draws. H hides that weapon while showing Sam's body.
  The weapon remains idle; this does not implement firing or complete gameplay.
- Connected presentation also matches 2,048 complete original frame chains and
  64 independent RGB references. Native real-map integration passes 187,759
  sanitizer assertions, including pending/committed weapon selection and H
  weapon hiding. See `analysis/hud-presentation.md`.

The twelve-level frontend now composes these systems; see
`analysis/native-game.md`. Manual campaign verification,
score/name entry, remaining palette effects and audio fidelity still
need work. Earlier module-by-module limitations above describe those individual
test boundaries and the separate diagnostic viewer.

## Use the existing workspace from PowerShell

```powershell
.\decomp.ps1 status
.\decomp.ps1 rebuild
.\decomp.ps1 test-input
.\decomp.ps1 test-movement
.\decomp.ps1 test-physics
.\decomp.ps1 test-maps
.\decomp.ps1 test-geometry
.\decomp.ps1 test-view
.\decomp.ps1 test-world
.\decomp.ps1 test-slide
.\decomp.ps1 test-horizontal
.\decomp.ps1 test-inventory
.\decomp.ps1 test-pickup
.\decomp.ps1 test-spawn
.\decomp.ps1 test-weapon-select
.\decomp.ps1 test-player-spawn
.\decomp.ps1 test-player-motion
.\decomp.ps1 test-player-frame
.\decomp.ps1 test-player-runtime
.\decomp.ps1 test-player-lifecycle
.\decomp.ps1 test-respawn-select
.\decomp.ps1 test-player-hazards
.\decomp.ps1 test-player-gameplay
.\decomp.ps1 test-sector-movers
.\decomp.ps1 test-switches
.\decomp.ps1 test-level-script
.\decomp.ps1 test-level-runtime
.\decomp.ps1 test-entity-pool
.\decomp.ps1 test-enemy-activation
.\decomp.ps1 test-script-spawn
.\decomp.ps1 test-enemy-motion
.\decomp.ps1 test-enemy-navigation
.\decomp.ps1 test-enemy-lifecycle
.\decomp.ps1 test-enemy-cd
.\decomp.ps1 test-enemy-cd-runtime
.\decomp.ps1 test-enemy-targeting
.\decomp.ps1 test-ray-traversal
.\decomp.ps1 test-ray-intercepts
.\decomp.ps1 test-enemy-ray
.\decomp.ps1 test-enemy-queries
.\decomp.ps1 test-combat-damage
.\decomp.ps1 test-combat-explosion
.\decomp.ps1 test-combat-rewards
.\decomp.ps1 test-combat-combined
.\decomp.ps1 test-combat-runtime
.\decomp.ps1 test-weapon-animation
.\decomp.ps1 test-weapon-controller
.\decomp.ps1 test-weapon-aim
.\decomp.ps1 test-weapon-hit
.\decomp.ps1 test-weapon-runtime
.\decomp.ps1 test-projectile-create
.\decomp.ps1 test-projectile-motion
.\decomp.ps1 test-projectile-controller
.\decomp.ps1 test-projectile-bounce
.\decomp.ps1 test-projectile-effect
.\decomp.ps1 test-projectile-runtime
.\decomp.ps1 test-projectile-conversion
.\decomp.ps1 test-entity-dispatch
.\decomp.ps1 test-entity-runtime
.\decomp.ps1 test-weapon-loadout
.\decomp.ps1 test-weapon-start
.\decomp.ps1 test-frame
.\decomp.ps1 test-sprite-dispatch
.\decomp.ps1 test-actor-animation
.\decomp.ps1 test-actor-animation-runtime
.\decomp.ps1 test-pickup-respawn
.\decomp.ps1 test-pickup-respawn-runtime
.\decomp.ps1 test-game-runtime
.\decomp.ps1 test-input-dispatch
.\decomp.ps1 test-pc-input
.\decomp.ps1 test-sprite-assets
.\decomp.ps1 test-texture-assets
.\decomp.ps1 test-map-texture-bindings
.\decomp.ps1 test-world-raster
.\decomp.ps1 test-raster-fill
.\decomp.ps1 test-mode-control
.\decomp.ps1 test-render-view
.\decomp.ps1 test-plane-renderer
.\decomp.ps1 test-world-visibility
.\decomp.ps1 test-wall-renderer
.\decomp.ps1 test-scene-coverage
.\decomp.ps1 test-world-scene
.\decomp.ps1 test-world-scene-rom
.\decomp.ps1 test-map-actors
.\decomp.ps1 test-actor-assets
.\decomp.ps1 test-actor-projection
.\decomp.ps1 test-actor-queue
.\decomp.ps1 test-actor-scene-rom
.\decomp.ps1 test-actor-scene
.\decomp.ps1 test-sprite-cache
.\decomp.ps1 test-sprite-pool
.\decomp.ps1 test-sprite-oam
.\decomp.ps1 test-display
.\decomp.ps1 test-hud-scene
.\decomp.ps1 test-hud-scene-rom
.\decomp.ps1 test-native-viewer
.\decomp.ps1 test-win32-input
.\decomp.ps1 test-entity-timer
.\decomp.ps1 gui
```

The GUI command prints the authenticated localhost URL. The analysis database
and matched sources are under `decomp/serious-sam-3/`; the generated rebuild tree
is `decomp/serious-sam-3/source/`. `rebuild` checks that existing tree.

If PowerShell disables scripts, run a command in a process with a temporary
policy override: `powershell -NoProfile -ExecutionPolicy Bypass -File .\decomp.ps1 test-geometry`.

```powershell
# Retry remaining machine drafts after improving shared types or discovery:
.\decomp.ps1 autopilot

# Regenerate the source tree from the database (backs up source edits first):
.\decomp.ps1 export

# Agent tools are also callable without modifying global MCP settings:
.\decomp.ps1 tool -ToolName get_function -ToolArgs '{"function":"sub_08003028"}'
```

Re-running autopilot alone does not solve the remaining reverse engineering.
Its drafts, claims, notes and best attempts persist in the database.

## Reproduce the setup

Requires Ubuntu in WSL, Git, GCC, and `uv`. In Ubuntu, from this repository:

```bash
bash scripts/bootstrap.sh
.venv-linux/bin/python scripts/decomp.py import --own-copy
.venv-linux/bin/python scripts/decomp.py autopilot
.venv-linux/bin/python scripts/decomp.py export
```

Place your own ROM at `orig/SSA.gba`. The ownership flag confirms that copy is
legally obtained. ROMs and generated ROM-derived assembly stay out of Git.
The importer deliberately uses conservative discovery; broad speculative scans
of this ROM proposed thousands of unverified starts in asset data.

Build the portable native libraries under Ubuntu:

```bash
.venv-linux/bin/cmake -S . -B build/native
.venv-linux/bin/cmake --build build/native
```

Build all native Windows libraries with the installed Visual Studio 2022
BuildTools, using its bundled CMake:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
# Also run the hidden-window input test:
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -TestInput
```

Output is `build/native-windows/Release/`, including the playable
`ssa_native_game.exe`, static libraries, diagnostic viewer and validation tools.

## Source and evidence

| Path | Purpose |
| --- | --- |
| `src/pc_input.c`, `src/pc_input_runtime.c`, `analysis/pc-input.md` | PC key/mouse event sampling and native frame control integration |
| `platform/win32/input_win32.c`, `analysis/win32-input.md` | Native HWND keyboard/focus/raw-mouse backend and MSVC tests |
| `src/input_dispatch.c`, `analysis/input-dispatch.md` | Verified original local/bypass input distributor; explicit network-required result |
| `src/sprite_assets.c`, `analysis/sprite-assets.md` | All 311 original sprite resources, static raster decoding and palettes |
| `src/frame.c`, `src/game_runtime.c`, `analysis/game-runtime.md` | Original frame order composed with native gameplay stages |
| `src/sprite_dispatch.c`, `analysis/sprite-dispatch.md` | Original dynamic sprite order and native submission byte queue |
| `src/actor_animation.c`, `src/actor_animation_runtime.c`, `analysis/actor-animation.md` | Actor sequences, natural completion and shared-pool animation stage |
| `src/pickup_respawn.c`, `src/pickup_respawn_runtime.c`, `analysis/pickup-respawn.md` | Original multiplayer pickup schedules and native pool/world binding |
| `src/entity_dispatch.c`, `src/entity_runtime.c`, `analysis/entity-runtime.md` | Original active-slot scheduler and native entity phase |
| `src/weapon_loadout.c`, `analysis/weapon-start.md` | Original initial loadouts and native weapon runtime startup |
| `analysis/projectile-conversion.md` | Weapon-7 alternate targeting and periodic health drain correction |
| `src/weapon_runtime.c`, `analysis/weapon-runtime.md` | Native weapon frame, actual aiming/hitscan, combat and animation handoff |
| `src/weapon_controller.c`, `src/weapon_animation.c` | Ten original weapon state machines and all 233 sprite animation sequences |
| `src/weapon_aim.c`, `src/weapon_hit.c` | Original wall/entity auto-aim and shot geometry with independent trace storage |
| `src/combat_runtime.c`, `analysis/combat-runtime.md` | Native damage/reward binding, CD contact attacks and chain explosions |
| `src/combat_damage.c`, `src/combat_explosion.c`, `src/combat_rewards.c` | Original damage, radial targeting/attenuation and kill score |
| `src/enemy_queries.c`, `analysis/enemy-queries.md` | Native visibility/targeting/navigation rays bound to the enemy controller |
| `src/enemy_ray.c`, `src/ray_intercepts.c`, `src/ray_traversal.c` | Full original wall navigation queries with independent scratch |
| `src/enemy_targeting.c`, `analysis/enemy-targeting.md` | Ordered nearest enemy/pickup and sector rejection |
| `src/enemy_cd.c`, `src/enemy_cd_runtime.c`, `analysis/enemy-cd.md` | Type-CD behavior and native world/script lifecycle integration |
| `src/enemy_navigation.c`, `analysis/enemy-navigation.md` | Original navigation phases and dual-ray geometry |
| `src/enemy_motion.c`, `analysis/enemy-motion.md` | Original turning, facing, walking, distance and environmental damage |
| `src/enemy_lifecycle.c`, `analysis/enemy-lifecycle.md` | Shared enemy death entry, corpse expiry and periodic health drain |
| `src/script_spawn.c`, `analysis/script-spawn.md` | Native scripted entity insertion, activation and explicit group release |
| `src/entity_pool.c`, `analysis/entity-pool.md` | Original 100-slot pool, 36,697 ROM comparisons |
| `src/enemy_activation.c`, `analysis/enemy-activation.md` | All 12 enemy initializers and boss setup, 14,576 ROM comparisons |
| `src/level_runtime.c`, `analysis/level-runtime.md` | Native movers/switches/script integration and world update ordering |
| `src/sector_movers.c`, `analysis/sector-movers.md` | Original moving sectors and door hazards, 20,735 ROM comparisons |
| `src/switches.c`, `analysis/switches.md` | Switch initialization, texture changes and script enqueueing, 6,582 ROM comparisons |
| `src/level_script.c`, `analysis/level-script.md` | Level-script interpreter, queue and sector triggers, 17,116 ROM comparisons |
| `src/player_gameplay.c`, `analysis/player-gameplay.md` | Native runtime death/respawn and hazard integration, 10,016 tested real-map frames |
| `src/player_lifecycle.c`, `analysis/player-lifecycle.md` | Original death and respawn control flow, 10,494 ROM comparisons |
| `src/respawn_select.c`, `analysis/respawn-selection.md` | Original farthest-from-nearest-player spawn selection, 4,998 ROM comparisons |
| `src/player_hazards.c`, `analysis/player-hazards.md` | Interaction sweep, lethal floors and sampled hazard sectors, 24,576 ROM comparisons |
| `src/input.c`, `include/ssa/input.h` | Native behavioral port of three input routines |
| `src/movement.c`, `include/ssa/movement.h` | Native controls, pre-collision vectors and ROM table math |
| `analysis/movement.md` | Input-to-player trace, verified fields and remaining movement dependencies |
| `analysis/native-movement-validation.json` | 106,577 math/movement differential cases |
| `src/physics.c`, `include/ssa/physics.h` | Native vertical physics, camera and new jump extension |
| `analysis/physics.md` | Jump behavior, validation scope and collision dependencies |
| `analysis/native-physics-validation.json` | 31,567 physics/camera comparisons and 36 jump scenarios |
| `src/map_assets.c`, `include/ssa/map_assets.h` | Native decompression and loading of 16 actual maps |
| `analysis/map-assets.md` | Recovered map sections, preprocessing and validation scope |
| `analysis/native-map-assets-validation.json` | Map hashes/counts, 7,632 wall comparisons, 164 malformed cases |
| `src/geometry.c`, `include/ssa/geometry.h` | BSP lookup, static collision, move commits and player starts |
| `analysis/geometry.md` | Collision behavior, integration API and remaining dependencies |
| `analysis/native-geometry-validation.json` | 62,118 ROM comparisons, 16 start checks, 32 real-map jump scenarios |
| `src/view.c`, `include/ssa/view.h` | New H-toggle chase camera, obstruction handling and renderer-facing pose |
| `analysis/view.md` | Third-person behavior, tests and runtime integration requirements |
| `src/world.c`, `include/ssa/world.h` | Dynamic entity collision, host spatial lists and integrated horizontal movement |
| `src/horizontal.c`, `include/ssa/horizontal.h` | Recovered horizontal timestep and callback error handling |
| `src/slide.c`, `include/ssa/slide.h` | Original wall tracing, velocity projection and sliding |
| `analysis/world.md`, `analysis/sliding.md` | Behavior, ownership, test scope and remaining gameplay dependencies |
| `analysis/native-world-validation.json` | 28,720 world ROM comparisons, including complete player motion |
| `analysis/native-horizontal-validation.json` | 16,760 timestep comparisons and 12 API guards |
| `analysis/native-sliding-validation.json` | 29,400 trace/projection/sliding comparisons and 64 guards |
| `src/inventory.c`, `include/ssa/inventory.h` | Original weapon ownership/queue, ammo and stat helpers |
| `src/pickup.c`, `include/ssa/pickup.h` | Pickup dispatcher with native inventory and explicit effect callbacks |
| `src/spawn.c`, `include/ssa/spawn.h` | Original map-object construction and start-marker recovery |
| `src/weapon_select.c`, `include/ssa/weapon_select.h` | Queued acquisition, ammo grants and weapon-cycle requests |
| `src/player_spawn.c`, `src/player_motion.c`, `src/player_frame.c` | Original player initialization, complete motion and frame scheduling |
| `src/player_runtime.c`, `include/ssa/player_runtime.h` | Native integration with jump, precise yaw and H-camera pose |
| `src/entity_timer.c`, `include/ssa/entity_timer.h` | Shared original entity countdown and visibility flag behavior |
| `analysis/inventory.md`, `analysis/pickups.md`, `analysis/spawning.md` | Behavior, ownership, validation and remaining runtime work |
| `recovered/input_record.c` | Matching candidate at 0x08003150; 97.5%, not byte-identical |
| `decomp/serious-sam-3/source/src/` | The exported verified matching C functions |
| `analysis/rom.json` | ROM identification and hashes |
| `analysis/rebuild-validation.json` | Full-ROM rebuild verification |
| `analysis/native-input-validation.json` | Native differential test results |
| `analysis/porting.md` | Established addresses, limitations and next work |
| `src/texture_assets.c`, `analysis/texture-assets.md` | Six original world archives, mip joins and proven column-major storage |
| `src/map_texture_bindings.c`, `analysis/map-texture-bindings.md` | Original name binding and live mutable wall references; B0C4 exact match |
| `src/world_raster.c`, `analysis/world-raster.md` | Original textured ARM columns, 67,678 full routine comparisons |
| `src/raster_fill.c`, `analysis/raster-fill.md` | Original flat-color rows/columns and forward sky copies |
| `src/mode_control.c`, `analysis/mode-control.md` | Original pause/menu state changes with explicit remaining UI boundaries |
| `src/render_view.c`, `analysis/render-view.md` | Original display-local camera routing and native eye/chase pose adapter |
| `src/world_visibility.c`, `analysis/world-visibility.md` | Original BSP visibility, clipping and ordered wall/plane queues |
| `src/wall_renderer.c`, `analysis/wall-renderer.md` | Full solid/portal projection, texture sampling and sprite clip results |
| `src/plane_renderer.c`, `analysis/plane-renderer.md` | Original plane allocation, row spans and flat-color/sky pixels |
| `src/world_scene.c`, `analysis/world-scene.md` | Connected verified world geometry and explicit framebuffer prefix |
| `src/map_actors.c`, `analysis/native-map-actors-validation.json` | Owned original map objects and animation clock |
| `src/actor_assets.c`, `analysis/actor-assets.md` | Original actor image decoding and transparent sampling |
| `src/actor_projection.c`, `analysis/actor-projection.md` | Original actor/player directional projection and sector collection |
| `src/actor_queue.c`, `analysis/actor-queue.md` | Wall occlusion, mixed depth ordering and actual actor drawing |
| `src/actor_scene.c`, `analysis/native-actor-scene-validation.json` | Whole connected world/actor pixel comparison |
| `platform/win32/viewer.c`, `analysis/native-viewer.md` | Native Windows movement, jump, animated actors and H Sam body |
| `src/sprite_cache.c`, `analysis/sprite-cache.md` | Original visibility, cache retention, allocation and real tile uploads |
| `src/sprite_oam.c`, `analysis/sprite-oam.md` | Complete original sprite ordering and raw affine OAM construction |
| `src/sprite_pool.c`, `src/hud_sprites.c`, `analysis/sprite-pool.md` | Original sprite pool, HUD allocation and normal controllers |
| `src/display.c`, `analysis/display.md` | Recovered 240×160 world sampling and native OAM/palette composition |
| `src/hud_scene.c`, `analysis/hud-scene.md` | Diagnostic starting weapon and HUD connected to native presentation |
| `patches/` | Reproducible fixes to decomp.wtf |
| `scripts/patch_luvdis.py` | Local analyzer fixes, tested with Thumb instructions |

The tests use Unicorn to execute isolated original functions as an oracle.
Unicorn is a development dependency; the native library contains no emulator.
