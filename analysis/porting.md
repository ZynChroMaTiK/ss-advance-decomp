# Native port findings

Vulkan is now an optional Windows hardware scene backend, selectable in Graphics
when available. Walls, floor/ceiling polygons, actors and mirrored scenes use GPU
rasterization; HUD, decals/water composition and presentation retain their CPU
paths. Software/GBA remain available and settings versions1..14 remain readable.
See [vulkan-renderer.md](vulkan-renderer.md) for architecture and focused evidence.
The initial severe Vulkan stall is corrected: dedicated cached readback, bounded
parallel effects, amortized buffer growth and visible-row reflection transfers.
`scripts/bench-vulkan.ps1` measures isolated render time; reflections remain the
heaviest case. Resolution, artwork, simulation and audio timing are unchanged.

Technical audit follow-up: save-read failure handling, locked audio status, a
shared decal atlas, separate scene/HUD link targets, centralized runtime settings,
and focused incremental tests are implemented. The latest render-loop pass
combines GPU sample processing, skips overwritten clears and reuses identical
sky rows, with exact before/after outputs and measured lower render times.
See [technical-refactors.md](technical-refactors.md)
for scope, measurements, and validation; frontend separation remains incremental.

Current milestone: all twelve campaign levels are enabled through `play-native.cmd`.
Extras now offers a saved Original/Classic HUD choice, defaulting to Original.
Classic reconstructs the retained green interface with live health, armor, ammo,
score, saved/live high score, time, unread NETRICSA entries and boss health in both renderers. It adds a transient weapon
selector, health/ammo palette flashes and static white localized notices.
New dossiers blink the retained analyzing sprite in HUD green without backing near the upper
center for2s, then teleport the green/white inbox inward, hold2s and teleport
back. Duplicate entries do not retrigger it.
Classic's other anchors now follow the Serious Engine layout with2px edge
insets, spaced armor/health, separate finite/infinite weapon positions and a
higher selector. The former top-center timer panel now shows max(saved high
score,current score), inset2px vertically; the timer sits immediately below.
Weapons and crosshair keep their existing behavior. See [classic-hud.md](classic-hud.md).
The shared original sprite compositor now accepts mixed priorities, including
transparent-sample priority promotion, instead of rejecting the frame. Both
renderers benefit; see [display.md](display.md) for the bounded hardware-model
evidence and native/Windows checks.
The Main-menu Scores screen now includes original ranking/name entry and saved
records, with keyboard typing, original cycling and record highlighting. Pause
Quit and campaign-ending handoffs offer qualifying scores; see [scores.md](scores.md).
Death now consumes a life and automatically retries from the level-entry snapshot.
Retries skip NETRICSA and the map, starting directly at the normal black-to-level
fade with the ready weapon, HUD and movement available.
The final life runs the original Game Over message/music before the black fade
and Main return. A stale pause bit cannot stall the death controller.
See [run-exit.md](run-exit.md).
The automap now opens with M in both renderers, gated by the saved Automap
On/Off option in Extras (default Off; explicit saved choices retained). Mouse wheel or held
plus/minus zooms; live geometry, markers, contacts, palette and scanlines run in
native C. Tab retains NETRICSA. See [automap.md](automap.md).
Jump now defaults to Off in Extras. Existing explicit saved choices are preserved.
Native NETRICSA now toggles during gameplay with Tab; N opens/closes and Esc returns.
It also precedes every pre-level map view. In-game NETRICSA overlays a frozen
full-resolution game frame; only pre-level NETRICSA uses the animated background.
In-game fades take250ms; pre-level NETRICSA retains its original fade speed.
The in-game blend removes HUD sprites except the weapon
while darkening and tinting the frozen level green; closing reverses the blend.
Original weapon/enemy illustrations, localized descriptions, two scrolling panels,
strategic/tactical records and live/campaign-session statistics are connected.
Enemy dossiers unlock only after a kill. A campaign-session journal tracks
weapon/enemy discoveries and individual read status across levels and retries;
Classic's top-right panel displays the combined unread count.
Every level's starting equipment is familiar: viewable, automatically read,
without a new-message count or notification. Actual in-level discoveries still
alert, and carried unread messages retain their status across level changes.
NETRICSA freezes gameplay. Pre-level briefings use the original music cue;
in-game opening/closing preserves the current music's uninterrupted playback.
See [netricsa.md](netricsa.md).
On2026-10-17 the user explicitly requested manual campaign validation rather
than gating levels on automated playthroughs. The final map, Caesar's Palace,
now participates in normal advancement, passwords and saved level selection.
Existing saves remain compatible; beat map10 to unlock map11, or use
`play-native.cmd --level 11` for a direct standalone start.
The original campaign story controller19458/193D8/1965C and its native screen
now connect Egypt/Rome/ending panels, artwork, text, music and fades to the
frontend. The controller passes50,462 ROM comparisons; text adds1,018 original
row comparisons, with87,854 native screen assertions and three Windows paths.
The final completion tally leads into the ending and returns to Main. Esc/P now
opens the native pause menu. The original time/ammo tally, bonus score carry and
final Sam text/music hold are connected; high-score/name entry remains. See
[completion.md](completion.md) and [campaign-story.md](campaign-story.md).
Original localized HUD notices now connect pickup, boss and death callbacks
to both renderers, including cache readiness, layout and shared lifecycle timers.
The controller passes135,104 ROM comparisons, native integration passes13,650
checks, and normal-input level1 still completes in5,110frames. See
[hud-notices.md](hud-notices.md). The timed completion tally's active notice
service is available; its outer screen/digits/audio/record integration remains.
Rome maps now retain the original golden ankh, including the full20x34 and
distant10x17 images decoded from the ROM. The former laurel override is no
longer applied or linked into the frontend. Its artwork is retained as
[historical material](../assets/rome-laurel/README.md).
Advancement now recovers original player construction and weapon restoration:
100 health, zero protection, earned weapons/ammo and score. R retains the exact
level-entry snapshot. See [campaign-entry.md](campaign-entry.md) for 8,192
original restore comparisons and 512 construction/restore sequences on 16 maps.
Original completion capture `1231C` now runs natively too: another 4,096 ROM
comparisons and 512 capture/restore compositions cover gear and per-weapon
statistics rows. The original completion-statistics accumulator and checkpoint
helpers are now native and byte-matched too. Their read-only native adapter
exposes all live counters; campaign-session totals now feed the completion frontend; original high-score lists
and name entry remain unfinished. See [statistics.md](statistics.md).
The timed time/ammo bonus controller and final record pipeline are now recovered
too: 11,466 original-loop frame comparisons and 410,217 native composition checks
pass. The pause/completion frontend now connects original notices, digits,
voice dismissal and campaign-session totals. Active E914/E9D4
message sprites are now recovered and used during gameplay. See
[completion.md](completion.md). The later frontend integration connects the timed tally and bonus-adjusted carry to
the playable executable. The subsequent targeted cannon fix now detaches player cannonballs
at launch, like rockets and Prion shots. Cannonballs retain original no-gravity
flight and now continue rolling horizontally on floor contact, without adding
airborne gravity. Rockets still explode and Prion shots stop at ceilings.
Six launch/trajectory cases per weapon and12ceiling contacts validate these
paths; current counts/hashes are in weapon-fixes-validation.json. The default
projectile integration and enemy cannon regression also remain covered.
The rebuilt launcher/startup/restart checks pass. Full campaign replays were
not repeated for this correction. See [weapon-fixes.md](weapon-fixes.md).
Prion-converted enemies now continually select the nearest living combatant,
including Sam, other converted enemies and bosses, without player preference.
Facing, ranged aim and enemy melee follow that selection. The focused test passes
1,602 checks on GCC sanitizers and Windows across all ten convertible types;
special weapons retain 32,318 checks and default entity runtime 85,982. The rebuilt
Windows launcher/startup/restart checks pass. See
[projectile-conversion.md](projectile-conversion.md#requested-pc-nearest-target-behavior).
Campaign figures below are historical replays, before this targeting change.
The corrected normal-input chain completes all eleven in 140,712 frames with
1,544,000 score / 45 HP. It includes the latest collision, Prion2x and Time Strike
changes. It supersedes the older 136,552-frame full-vitality-carry checkpoint.
A retained converted-enemy target pointer exposed
an overly strict projectile-adapter guard; cleared/reused stable slots now follow
original1097C behavior, with16 native Gunrilla regressions and two directed ROM
comparisons. Read
[native-game.md](native-game.md) for the current composition, validation and limits.
Earlier sections retain the recovery history and individual subsystem boundaries.
The eleventh-level probes found a Time Strike render failure: an animation change
while frozen retained a frame from a longer sequence. The native view adapter
now supplies a valid frame in its render snapshot, without advancing animation
or changing gameplay. Both renderers and views pass the32318-check special-weapon
suite on GCC sanitizers and MSVC. Praetorian Fort now completes through normal
input with earned equipment and original next-level vitality. A separate Normal
standalone test now completes the final map: D8 defeat, original music changes
and the VM exit in 17,270 frames with 63 HP. GCC sanitizers and MSVC agree on
every standalone field. The final earned-inventory probe dies before the boss,
so that map remains unavailable. See [native-game.md](native-game.md#validation)
and [special-weapons.md](special-weapons.md).
Weapon7 Prion shots now launch independently on the sight line immediately,
including level aim before any pitch input; the old sideways offset is removed.
See [weapon-fixes.md](weapon-fixes.md).
Gunrilla cannonballs now clear their own shooter at launch while retaining
normal impacts against walls, players and other enemies. See
[projectile-runtime.md](projectile-runtime.md) for the scoped collision fix and tests.
The playable PC and reference renderers now restore original0750C/0787C bar
colors, rather than using the static sprite palette for every row. See
[hud-colors.md](hud-colors.md) for original-ROM pixel comparisons.
Startup/restart now restore the original pre-level map view and its music;
simulation, controls and HUD start during the level reveal after the map's black
hold, with the level soundtrack.
See [map-view.md](map-view.md). Original feature parity is the completion target;
the full campaign, remaining menus and other documented gaps remain unfinished.
Normal startup now goes straight to Title, then Main/Single Player/difficulty.
Select Level now appears between New Game and Password, with saved completion
unlocks and original localized campaign names (no multiplayer maps). Language
persists too. Clear Data in Options uses a Yes/No warning, default No, to reset
both. See [profile.md](profile.md) for storage, loadouts and validation.
Options cycles English/German/French in place alongside music and credits;
there is no startup language list. Passwords load the original
weapons/ammunition and difficulty on supported maps. See [menu.md](menu.md) for
coverage and original-ROM proof.
The original fixed BG1 grid now overlays the scrolling menu background from
the first title/main-menu visit, as requested.
Fresh diagnostic level launches now use the ROM's original level-start weapon
and ammo records, like password starts. Existing progress takes precedence:
advancement retains carried weapons/ammo, and R restores the captured entry.
Mouse hover/click now selects menu labels with correct letterbox coordinates;
passwords accept direct keyboard text and editing. The menu soundtrack continues
across Press Start. Shared pointer/text helpers prepare future NETRICSA and score
screens; those screens remain unfinished.
Both boss controllers are now connected to the native dispatcher, animation,
combat, projectiles, script queues and HUD health binding. The sixth map's D7
fight and scripted completion are verified; later boss-map progression remains
unvalidated. Weapons 8/9 and original background/hurt palette effects now bind
to native gameplay and both renderers; see [special-weapons.md](special-weapons.md).
See [enemy-boss.md](enemy-boss.md).
Type-64 effects now use recovered wall/sector clipping and sector-only list
updates; see [sector-clip.md](sector-clip.md). The validated playable campaign
historically covered eleven maps. Admission now includes all twelve by user request.
The seventh/eighth are in the second original resource
family; their real exits complete through native scripts without grants or shortcuts.

## Established input behavior

| ROM address | Behavior | Native implementation |
| --- | --- | --- |
| `0x08003028` | Read active-low KEYINPUT; held/newly pressed edges; reset chord; record/playback selection | `ssa_input_poll` |
| `0x08003150` | Pack buttons into one byte and append while count <= 0xE0E | `ssa_input_record` |
| `0x08003298` | Expand packed buttons; advance repeat count and RLE stream; 0xFF ends playback | `ssa_input_playback` |

These three behavioral ports passed 14,848 differential cases against the supplied
ROM's original instructions. The test includes all 1,024 physical button masks,
simultaneous opposite directions, all 256 packed values, recording bounds,
repeat-count bounds, record/playback priority, state flag gating and reset calls.
It does not establish correct full-game integration, rendering or mouse controls.

The input state starts at `0x030042F0`:

| Offset | Observed use |
| --- | --- |
| `+0x00` | Raw held (low halfword), pressed edges (high halfword) |
| `+0x04` | Processed held/pressed after recording/playback selection |
| `+0x08` | Saved global value during record/playback setup; broader purpose unresolved |
| `+0x0C` | Recording-buffer pointer |
| `+0x10` | Playback-stream pointer |
| `+0x14` | Game-state value, not recovered here |
| `+0x1E` | Recording position |
| `+0x20` | Remaining repeat count |
| `+0x21` | Current packed playback buttons |
| `+0x22` | Reset-related byte cleared when reset chord is absent |
| `+0x23` | Bit 1 records; bit 2 plays back; recording has priority |

`0x03004010` holds a pointer to another object. Bit 0 of its halfword at
offset 4 suspends recording/playback. Its broader game-state meaning is not
established. The native API calls this `input_suspended`.

The original recorder resolves simultaneous left/right to left and up/down to up.
Select and Start are not encoded in recordings. Playback emits the current
buttons before consuming the next run. Raw hardware polling retains the upper
six bits of the inverted halfword; masking them early changes reset detection.
Native pointers deliberately have host width; the native structure is not a
binary-compatible GBA RAM structure.

## Matching status and tool fixes

Live matching is 78/526 functions and 2,816/87,648 catalogued bytes (3.21%),
after 549 compiler attempts (live status checked 18 October 2026). Original statistics helpers `1CF4`, `1D9C` and
`1E48` add 592 exactly matched bytes. Both export and independent rebuild
preserve the original ROM byte for byte.

The manual recorder candidate is 97.5% similar, with the remaining differences
at the initial global-pointer/limit/load scheduling. It is not counted among
the exact matches. Its best source and notes are stored in decomp.wtf.

The matched catalogue contains 2,816 bytes of verified C out of 87,648 imported
function bytes. This denominator excludes undiscovered code. The 8 MiB image
size is mostly data and is not a useful code-completion denominator.

Luvdis needed bounded-stack fixes for speculative paths, conservative handling
of partial BL instructions it cannot model, and buffered output for WSL.
`scripts/test_luvdis_stack.py` verifies large frames, negative stack positions,
ordinary returns and unsupported partial calls. These changes do not modify
the ROM. The initial 525-function catalogue was retained after an aggressive
additional sweep proposed thousands of unverified starts.

The GBA exporter emitted unresolved Thumb-call labels as untyped absolute linker
symbols. GNU ld inserted interworking veneers, overflowing the fixed ROM layout.
The local patch emits typed absolute Thumb function symbols in a separate object.
The exporter also now discovers unresolved canonical address symbols from C
objects, including uncatalogued calls and IWRAM data references. Thumb-call
relocations determine function mode; ordinary ARM calls retain ARM mode. This
fix is recorded in `patches/decomp-wtf-c-externals.patch` and applied by bootstrap
after the existing Thumb-symbol patch. `tests/test_export_c_externals.py` checks
both patches against the pinned upstream and links real ELF objects to verify
symbol types, data addresses and absence of added interworking veneers.
The full rebuilt image is checked against the original. Unmatched code remains assembly
or original data; the rebuild is not evidence of a complete decompilation.

## Next work required for the requested playable port

The input-to-player path and pre-collision control/vector phase have now been
recovered. See `analysis/movement.md` for addresses, original quirks, native API
scope and 106,577 differential cases. This includes heading math and movement
velocities. The static collision and move-commit work described below is now
implemented; the world integration described below adds the timestep and sliding.
The PC event adapter and Win32 backend are now implemented; physical-device
validation and the complete game frontend remain. A diagnostic native viewer
already runs movement, jump, H camera, world geometry and original actors.

Vertical position/gravity and camera height are now also ported; see
`analysis/physics.md`. Those complete routines pass 31,567 ROM comparisons.
The user-requested jump extension is separate native code with 36 behavior
scenarios. All 16 map resources now load natively with original wall setup;
see `analysis/map-assets.md`. BSP lookup, static map queries and clearance/move
commits pass 62,118 ROM comparisons. Another 16 start-position checks and 32
real-map jump scenarios cover original spawn points and player dimensions.
See `analysis/geometry.md` for the static query scope. Native entity collision,
host spatial lists, wall sliding and horizontal timestep integration are now
implemented, with 71,808 additional ROM comparisons. See `analysis/world.md`
and `analysis/sliding.md`. Pickup dispatch and synchronous world removal are now
also recovered; scripted effects, combat and OS controls remain.

1. Expand the function catalogue from actual call targets and validated code
   ranges. Review ARM startup, hand-written math/render routines and code copied
   into IWRAM; do not count speculative asset bytes as functions.
2. Continue into subsequent enemy AI, scripted effects, other gameplay contacts
   and projectile simulation. Map-object construction `08001984`, inventory helpers
   and type-0xE1 dispatch `0801B99C` are now native. The latter still exposes
   script/audio/HUD/network/nearest/destruction boundaries through callbacks.
   See `spawning.md`, `inventory.md`, `pickups.md` and updated `world.md`.
   Queued acquisition and cycle selection are also recovered and verified;
   see `weapon-selection.md`. Requested/current animation handoff and original
   weapon control now live in `weapon_controller.c` and `weapon_runtime.c`.
   Player initialization `05E4C`, complete movement `064B8`, frame wrapper
   `05CCC` and entity timer `12CFC` are now native. `player_runtime.c` joins
   these with native jump and H-camera behavior. Death `06154`, respawn `05FEC`,
   spawn selection `01B18` and hazards `05680` are now recovered and connected
   through `player_gameplay.c`. Their 40,068 ROM comparisons and 10,016 native
   integration frames are documented in `player-lifecycle.md`,
   `respawn-selection.md`, `player-hazards.md` and `player-gameplay.md`.
   Moving sectors, switches and the level-script interpreter are now native:
   another 44,433 ROM comparisons include all 116 real script entries with
   controlled spawn/effect dependencies. `level_runtime.c` connects line/sector
   triggers, mover hazards and script mover commands. Entity updates precede
   movers, which precede the script VM, as in `0800244C`. See `level-runtime.md`.
   Scripted creation now uses native pool/world insertion in `script_spawn.c`;
   all twelve enemy state-zero initializers are recovered, including boss setup.
   The pool and activation paths pass 36,697 and 14,576 ROM comparisons. Native
   script-spawn integration runs all 116 original entries; explicit test defeat
   notifications unblock groups without pretending to simulate combat. See
   `script-spawn.md`, `entity-pool.md` and `enemy-activation.md`.
   Shared navigation, directional walking, hazards and death/corpse handling now
   compose with the type-CD controller in `enemy_cd_runtime.c`. Another 121,260
   ROM comparisons and 59,319 native integration assertions cover this work;
   see `enemy-cd.md`. Its 62-byte outer dispatcher is also byte-identical C.
   Original visibility B18C, nearest selection 036C4 and wall rays A240/A041
   now compose through `enemy_queries.c`, with 263,969 additional ROM comparisons
   and 53,384 native integration assertions. See `enemy-queries.md`. Other enemy
   controllers remain. Damage 037F4, explosion 035A8 and reward 125E0 now compose
   through `combat_runtime.c`, with 155,517 ROM comparisons and 26,435 integration
   assertions. CD contact damage and chain explosions reach real player/enemy
   health and death handling. See `combat-runtime.md`. Weapon animation, all ten
   firing controllers, auto-aim and hitscan now pass303,556 further original ROM
   comparisons; see `weapon-animation.md`, `weapon-controller.md`, `weapon-aim.md`
   and `weapon-hit.md`. `weapon_runtime.c` binds hitscan damage and selection to
   native gameplay. Projectile construction, flight, five controllers, wall
   reflection and attached conversion effects now pass136,192 ROM comparisons.
   `projectile_runtime.c` connects weapons6/7/10 to the shared native pool and
   combat. Type69 converts enemies to target other enemies and drains health;
   the previous freeze/thaw interpretation was incorrect. The CD runtime now
   shares the original drain fields with projectile impact. Projectile68/6A
   splitting/spread and presentation remain. See `projectile-conversion.md` and
   `projectile-runtime.md`. Aimed constructor109F0 is another48-byte exact match.
   Original entity scheduling04E04/083E4 now passes72,420 ROM comparisons;
   `entity_runtime.c` connects players, CD enemies, projectiles and owned pickup
   timers in that order of active slots. Integration passes85,982 assertions on
   all16maps, including jump/H. Other enemy handlers and the complete outer
   game loop remain. Original initial loadouts1240C pass12,979 ROM comparisons;
   native startup passes84,077 assertions over608starts and192shots from the
   original selected weapons. See `entity-runtime.md` and `weapon-start.md`.
3. Continue active text notices, palette/blending effects and remaining sprite
   controllers. Normal weapon/HUD/OAM composition and original240×160 display
   sampling now work natively, following D2D0/01828 actors, wall occlusion,
   solid/portal walls and floor/ceiling/sky. Complete game presentation remains.
   Replace GBA VRAM, DMA,
   interrupt and timing operations with native platform interfaces.
4. Finish platform/mode/level startup integration, asset loading, audio and save handling. Add native
   keyboard bindings, relative mouse yaw, independent strafe and movement.
   Determine whether vertical look is compatible with this renderer before
   promising free-look behavior.
   The0244C frame and F85C sprite dispatchers now pass10,500 and70,344 ROM
   comparisons. Actor00908 animation and multiplayer pickup respawn pass218,240
   and73,734 comparisons. `game_runtime.c` composes these with native entities,
   movers/scripts and weapons, passing997,410 integration assertions with genuine
   two-player shared lifecycle/input, live local hazard flags and shared CD sound
   state. Input distribution, mode control and rendering remain external stages.
   See `game-runtime.md`, `frame.md`, `sprite-dispatch.md`, `actor-animation.md`
   and `pickup-respawn.md`. Actor setter05394 adds a60-byte exact match.
5. Extend the existing Windows native viewer into the actual full game and
   verify menus, combat, progression, sound, save/load and original frame timing.

PC event sampling and frame integration now provide WASD, mouse yaw/fire, Space,
H and E, with314,726 sanitizer assertions on all16maps. The Win32 event backend
passes3,215 MSVC assertions with posted window messages/raw payloads; physical
device delivery remains untested. All90 portable libraries plus the Win32 backend
build as91 native Windows x64 libraries, plus the native viewer.
The viewer is incomplete; no complete game executable exists yet.
The local05898 distributor adds183,988 ROM comparisons, with genuine network
distribution explicitly unsupported. See `pc-input.md` and `win32-input.md`.

All311 original sprites and gameplay palettes now decode; original transfer/OAM
and palette routines validate1,261pieces/6,730tiles. E320 adds a56-byte exact
match. DBDC is sprite visibility/cache preparation, and FCE4 constructs OAM;
the legacy RENDER_WORLD enum does not describe a recovered world renderer.
World texture archives now decode666images/2,567,040bytes, with wholeAFF4/AF18,
65,539name comparisons and55,440 originalcolumn-copy blocks proving layout.
Map wall bindings preserve live switch changes and pass1,059wholeB0C4calls;
B0C4 matches54bytes exactly. Original ARM texturedcolumns pass67,678wholecalls,
including7,992 usingall666loadedimages. Flatcolorfills andskycopy add6,189.
The actual chain is02B88 ->B6B0 ->017CC ->BE60 ->ARM002F4; AB10/A98C handle
flatcolor/skyplanes. Those geometry stages and the original actor passes now
compose natively; HUD and final game integration remain. See `world-scene.md` and
`texture-assets.md`, `map-texture-bindings.md`, `world-raster.md`
and `raster-fill.md`. Sector+A/B arecolor/skyselectors,NOTtextureindices.

Original021E4 pause/menu orchestration is native with13,092wholeROMcomparisons,
60,101reentrychecks and41failureprefixes. UI command8EB0 andspritecreationE914
remainmandatoryevents. GameRuntime CONTROL still delegates toexternal; frontend
mustbindcanonicalflags/sharedinput_state andimplementevents. See `mode-control.md`.

The H-toggle third-person camera extension is now implemented independently in
`src/view.c`; see `analysis/view.md`. It computes a chase pose from recovered
player/eye state, retracts against static geometry and preserves gameplay aim
and movement. Native PC/Win32 event routing now handles H presses/repeats;
the native viewer displays this chase pose and Sam's original animated body.
H clears flag80 only in the rendering source snapshot, with gameplay flags
preserved. Complete third-person game integration remains.

World geometry continuation:218,912visibility ROMcalls;14,322completeBE60solid/
portal calls;23,955plane routines. Connected B6B0/017CC/BE60/AB10 matches128
wholegeometry frames/all16maps/1,843,200pixels, withactorD2D0/01828excluded.
2,048PC/game/jump/Hframes add481,049sanitizerassertions and225guards while
drawingactualgeometry. View routing02B88 adds84,032wholecalls/274,785native
cameraassertions. B220 matches42bytes,A87024bytes;09238 adds32bytes andFB5C68.
The catalogue nowcontains74exact/2152bytes=2.46%.
The74-Cexport and independent rebuild are byte-identical.
See `world-visibility.md`,
`wall-renderer.md`, `plane-renderer.md`, `render-view.md` and `world-scene.md`.

Windowsviewer usesactualsingleplayerspawn/motion/collision/camera/jump directly,
withoutdummyGameRuntime or gameplaycallbacks. Nativeclear/world events render
120x120worldindices,then240x160displaycomposition usingoriginalpalettes.
Inputrawmouseforegoundonly,
clickcapture,Esc/focus/menu/cancelrelease. 3,840hiddenWin32/GDIframes/all16maps
and4CLI/ROMrejections pass; physical-device/visible-window interactionuntested.
Original animated objects and the H body now render; combat,pickup execution,
scripts,movers andaudio remain outside theviewer. Original normalHUD andidle
startingweapon nowrender throughcache/OAM/240×160display; nofirecontrollerbound.
Launchwith
`run-native-viewer.cmd [--level N]`; see `native-viewer.md`.

Actor continuation:142resources/5407imagereferences/2607uniqueimages copy and
decode natively. Whole09238/09258/ARM0638 sampling passes13787 original draw
comparisons; ASan adds37860assertions. NativeD490/D884 projection andD2D0/D338
collection pass17664 isolatedcalls/256collections with9110emittedrecords.
NativeF98/1238/01828 queue/depth/occlusion passes67840ROMcalls plus36564sanitizer
assertions. Full originalB6B0 without actor hooks matches3686400pixels over
256views/all16maps, including128animatedframes,317actorcommands/2584walls.
Only BIOSdivide/CpuSet/DMA are modeled at that boundary. See `actor-assets.md`,
`actor-projection.md`, `actor-queue.md` and `native-actor-scene-validation.json`.

MapActors owns the385 original mapobjects, in01984allocation/world order, with
00908animation and no invented activation/AI state. All16maps/2048animation
frames/49280originalrecords compare; owner lifecycle passes5404sanitizer
assertions and preserves separately owned playerbodies onfree. Windowsviewer
draws18326actorcommands,2762120changedactorpixels and1600visibleSamframes,
using original65/66animation andD338overlay. The portable tests and Windows
viewer exercise rendering independently of unfinished fullgame orchestration.

Normal sprite presentation now composes natively. The84-slot pool and101F4
constructor plus normalHUDcontrollers pass77424 whole-routine comparisons.
FullFCE4/E648/FC3C/FC58 OAM passes6036calls/45603entries/10054affineobjects,
with13183sanitizerassertions. Fullcache/visibility/allocator/transfer/reset adds
30596comparisons/7769920uploadedbytes/20624sanitizerassertions. The recovered
mode1 display usesPA128/PD192 to sample the world at(x/2,floor3y/4) for240×160,
withactualseparatepalettes andphysicalOBJ32Kmirror. Originalregister/map/tile
setup passes448calls;635independentpixelframes coverall311realimages and128
affinecases,plus418281sanitizerassertions. Unsupportedblend/window/mosaic,
mode4automap andpriority/budgetoverflow remain explicit errors.

The diagnosticHUDscene runs real1240Csetting0 before74HUDobjects,then12160
idleweaponraise,scalarHUDcontrollers,E460,DBDC,FCE4,E358andnativecomposition.
Hhidesfirstpersonweaponwithoutchanginggameplayaim/body. Healthcomesfromspawned
entity,armorfrominventory; score/time remaininitialized. Firing/combat arestill
unbound rather than mocked. ActiveDnoticesexplicitlyunsupported. Windowsviewer
passes3840frames/all16maps,3856HUDdraws/2112raisedfirstpersonweapondraws,
21807376changedpresentationpixels. Sourceandtestreports document boundaries.

ExactFB5C sprite-mark adds68bytes/34instructions,secondmanualcompile: live
catalogue74/526,2152/87648bytes(2.46%),517attempts. FC3C83.9%(2attempts) and
074F0 83.2%(5attempts) areNOTexact. The74-Cexportrebuild isbyte-identical.
See `sprite-cache.md`, `sprite-oam.md`, `sprite-pool.md`, `display.md`,
`hud-scene.md` and currentvalidationJSON reports.


## Playable level-one checkpoint, 2026-10-15

`native_game.c` and `platform/win32/game.c` now compose real simulation and
rendering. The new `play-native.cmd` frontend exposes level0 only. The original
viewer is retained separately. Normal difficulty, original loadout, live map
objects, CD/CE/CF AI, real damage/firing, pickup inventory, wave/group gates,
movers, exit detection and native PC restart all run. Space and H remain active.
Audio is muted; original active text notices use simple host text.

The first-level bot uses only normal PC event APIs and read-only observations;
its route planner changes only a private map copy. GCC ASan/UBSan/leak scanning
and MSVC x64 both finish in5258 frames:28000score,8pickups,19spawned,100health,
13pistol/23shotgun shots. All16-map ownership proof adds4817checks. The existing
game-runtime suite passes997410assertions/8220frames after shared pool changes.
There are99Windowslibraries and three executables including the completion check.

Next work is remaining campaign/boss and special-weapon support, original UI,
cross-level inventory carry, remaining palette/display effects and audio output.
Do not claim the complete game is finished or expose unimplemented campaign
paths as supported gameplay. The byte-matching catalogue remains separate from
behavioral native coverage. See native-game.md and current validation reports.

## Native weapon corrections, 2026-10-16

Local minigun/chainsaw/machinegun loop ownership now stops stale firing audio
without cutting music or other sources. The PC game opts into launch-independent
player rockets; the original recovery runtime retains its attachment quirk.
The changed normal-input route exposed an offscreen portal row, now safely
clipped with original sampling preserved. See `weapon-fixes.md` and current
`weapon-fixes-validation.json`, `windows-weapon-fixes-validation.json` and
`campaign-validation.json`. That checkpoint covered five campaign maps; current
current campaign evidence is summarized above. These fixes add no matching catalogue entries.

The earlier110,468-frame consecutive run completed nine maps with earned carry.
The sixth includes real D7 damage/death/projectiles/HUD/music and VM finish;
the seventh completes in17,484frames. The eighth completes in15,023frames, ending
at1,290,000score/78HP. The frontend admits indices0..8. The eighth adds both
recessed switches, actual conversion frames and12,018,400 real-mixer stereo frames.
The ninth completes in13,227frames with106spawns/14pickups, ending at
1,446,000score/53HP. It exercises original weapon9 acquisition and falling E3
effects,20conversion shots/12,991converted actor frames, both renderers and
10,581,600native stereo frames without decoder errors. The input bot waits18
frames clear of an original closing-door hazard, reverses wall-blocked strafes,
and uses jumping and its earned special weapons. Only the admission count and
normal-input test planner changed; no game collision, hazard or inventory bypass.
The bot's cardinal escape steps, ranged spacing and supply strafing are normal
PC input; game collision, enemy health, damage and script rules are unchanged.
This proof uses the current original minigun cadence, firing-animation weapon
handoff and immediate E-button policy, superseding older timing checkpoints.
Boss/post-boss audio loops now use gameplay music2/19/20; focused126730assertions
pass. Later D8 natural progression, interchapter presentation and original feature
parity remain unfinished. See enemy-boss.md and current campaign reports.
