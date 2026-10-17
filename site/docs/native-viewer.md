# Windows native viewer

`platform/win32/viewer.c` builds the console-subsystem Windows executable
`ssa_native_viewer.exe`. It displays the actual native map renderer with native
collision, player movement, jump and the H camera extension. Its window title
explicitly identifies it as an **incomplete native viewer**. It is not the
completed Serious Sam Advance PC port.

The viewer renders original placed objects and their original animation frames.
H shows Sam's original directional body and overlay with idle/walking animation.
The original starting knife and normal HUD now render through native cache,
OAM and display composition. The weapon is idle-only. AI, firing/combat, pickup
execution, scripts, moving-sector gameplay, audio and menus are not integrated.
Score/time retain initialized values. Mouse movement changes yaw only; vertical
mouse look is not implemented. `world-scene.md` describes geometry;
`actor-projection.md` and `actor-queue.md` describe the following actor passes.

## Running

After building the CMake target `ssa_native_viewer` with MSVC x64:

```powershell
.\build\native-windows\Release\ssa_native_viewer.exe .\orig\SSA.gba --level 0
```

The level option accepts0..15. The executable reads the supplied ROM without
modifying it and checks its8MiB size and SHA-1 against
`4ff48d8c18b70a5184758f1917343ae85a70858e`. It embeds no ROM and downloads no
assets. Decoders copy the data they need; the input ROM buffer is released
before the window loop starts. The user must retain their own ROM separately.

Click inside the foreground window to capture controls. WASD moves and strafes,
relative horizontal mouse motion changes heading, Space jumps, and H toggles
the first-person/chase camera. Escape releases the mouse. Click again to resume.
Focus loss, window resizing/menu entry and cancellation release capture and
clear held input. Closing the window exits. Action edges are consumed through
the existing portable `SsaPcInput` accumulator, including short Space/H presses.

The original120x120 indexed world framebuffer now passes through the recovered
normal display sampling: source`(x/2,floor(3*y/4))` becomes240×160. The native
compositor overlays actual OAM with the separate original OBJ palette. The3:2
result is letterboxed in the resizable window. Original RGB555 colors are
expanded by five-bit replication into a top-down32bit BGR DIB, displayed using
nearest-neighbor `COLORONCOLOR` StretchDIBits. No generated textures,
replacement art, filtering or modern lighting is inserted.

## Actual native composition

The viewer loads the existing native math/map/texture/sprite-palette/sky and
renderer tables, all actor images and the original actor animation sequences.
`SsaMapActors` allocates the original01984 placed objects in type/file order,
inserts them into a100-slot world, and runs the recovered00908 animation clock.
The original single-player `ssa_players_spawn` path follows that allocation and
finds the map's actual type1 start. Map objects keep their original preactivation
zero health/dimensions/flags; no gameplay-ready enemy state is invented. It does
not construct a dummy GameRuntime or substitute no-op gameplay callbacks.

Each simulation step samples input, applies full-precision heading, runs
`ssa_player_motion_world_step` with one tick and native jump options, then
`ssa_camera_step` and `ssa_view_pose`. Movement, wall-slide and view queries use
separate workspaces. The displaced chase pose resolves its own BSP sector via
`ssa_render_view_pose`. The actual `ssa_render_view_step` clear/world events
call `ssa_world_scene_clear` and `ssa_world_scene_draw`; other presentation
stages fail if accidentally selected. Sky scroll receives the player's heading
separately from the camera height/position. Footstep events are muted because
this viewer has no audio backend.

After fresh geometry, `ssa_actor_scene_draw` binds actual wall clips and runs
original D2D0/D338 collection, D490/D884 projection,1238 wall occlusion and01828
reverse drawing through the native actor sampler. Source snapshots carry the
canonical animation/frame/facing/owner/flags for every registered body. When
the chase pose is usable, only Sam's snapshot clears flag80 to expose his body;
the underlying body and gameplay flags remain unchanged. First-person and
obstructed fallback poses keep the local body hidden. D338's flag10 second pass
draws the original player overlay. The65/66 idle/walk transition follows the
recovered player-frame predicates and period4 animation clock. Jump moves the
actual body, so its projected height follows the new native jump extension.

`SsaHudScene` initializes original1240C starting loadout before101F4's74 HUD
objects, sharing the original84-slot pool. Its explicitly idle-only step runs
12160 raising/lowering, native scalar HUD controllers, E460 animation, DBDC
cache preparation, FCE4 OAM and E358 uploads. Active notices fail explicitly.
Native display composition uses real palettes and copied physical tile bytes.
Health comes from the spawned entity and armor from its inventory; score/time
remain initialized until the actual game loop supplies them. See `hud-scene.md`.
The H extension hides only the first-person weapon's presentation flag.

Interactive timing uses QueryPerformanceCounter with a60Hz accumulator and
one original tick per update. Catch-up is capped at eight steps after a long
stall. This is an explicit diagnostic host timing policy, not a recovered
original frame-timing routine. Renderer errors terminate the viewer with a
console diagnostic; they are not replaced with a placeholder scene.

## Win32 ownership and input

Raw Input registration is foreground-only and targets this viewer window.
The existing game-runtime Win32 backend is not changed or supplied a fake
runtime. WM_INPUT retrieves a real RAWMOUSE record with GetRawInputData,
ignores absolute packets, and forwards relative counts only while captured
and foreground. DefWindowProc runs afterward for foreground input cleanup.
Keyboard messages supply their actual repeat bit to the portable accumulator.

Mouse capture is explicit. ClipCursor uses the current client rectangle mapped
to screen coordinates; SetCapture is verified with GetCapture. Failure releases
the clip. WM_SETCURSOR hides the cursor only in the captured client area,
without changing the process ShowCursor display counter. Release clears input,
releases only capture owned by this window, restores the cursor and removes
the clip. The program removes its Raw Input registration before teardown.

All storage is owned by a stable heap-allocated viewer object. Scene and query
workspaces are freed before map/texture owners. Map actors detach only their own
bodies before releasing storage; player storage stays alive until world teardown.
Allocation, ROM identity and
initialization failures leave cleanup able to free completed earlier stages.
The explicit120byte framebuffer prefix required by the recovered ARM column
correction belongs to `SsaWorldScene` and is retained through clears.

The platform behavior follows Microsoft documentation for
[Raw Input](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input),
[GetRawInputData](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputdata),
[WM_INPUT cleanup](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input),
[registration/removal](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices),
[ClipCursor ownership](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-clipcursor),
[StretchDIBits](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits),
[high-resolution timing](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps),
and [BCrypt hash lifetime](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptcreatehash).

## Hidden smoke mode

```powershell
.\build\native-windows\Release\ssa_native_viewer.exe .\orig\SSA.gba --level 0 --smoke --snapshot .\build\native-tests\viewer-level0.bmp
```

Smoke mode creates an actual hidden HWND, runs240 native movement/render frames,
and calls StretchDIBits on its device context. It exercises WASD, relative yaw,
two jump presses, two H toggles and input clearing through direct calls to the
portable event accumulator. It uses **no SendInput, system input injection,
foreground activation, cursor confinement or Raw Input registration**.
The JSON result reports actual airborne/chase frames, actor commands, changed
actor pixels and visible Sam pixels as well as palette-index diversity. The body
counter rasterizes only Sam's ordered commands with their actual clips into a
separate diagnostic mask; it does not count merely selecting the H camera.
Chase selection must last100 frames; obstruction may
legitimately fall back to first-person for some of those frames. Jump height is
constrained by the actual map, so the test reports rather than invents airtime.

`--snapshot` optionally writes the actual final240×160 DIB as a BMP at the
explicit supplied path, replacing that output file if it already exists. It
does not package or copy the ROM. Such local previews stay in ignored build
directories and are not source assets.
`--snapshot-chase output.bmp` additionally writes smoke frame100 while H is
selected and requires `--smoke`. This is a private original-asset preview.

The CMake target builds with MSVC C11 `/W4 /WX`, Unicode window APIs and
User32/Gdi32/BCrypt. The automated hidden runs validate executable initialization,
native steps, camera changes, drawing and teardown. They do not claim manual
physical-device testing, visible-window usability, or completion of the game.
The root validation driver records executable smoke results and source hashes.

The final hidden smoke run passed **all16 maps and3,840 frames**, including
**1,278 airborne frames** and **1,600 visible Sam frames**. It drew18,326 actor
commands and2,762,120 changed actor pixels, with2,160,300 visible Sam mask pixels.
All385 placed objects were loaded. Its report is
`analysis/native-viewer-validation.json`; the driver is
`scripts/test-native-viewer.ps1`. The level0 final BMP was visually inspected
and shows the original courtyard, textured walls, pyramid and sky; the chase BMP
shows Sam's original body and overlay. The source
hash in that report matches the final viewer source. Physical-device and
visible-window testing remain unperformed.

The presentation continuation adds3856 HUD draws (including initial draws),
2112 raised first-person weapon draws and21807376 changed presentation pixels
to the same3840 simulation-frame test. All screenshots are240×160. The level0
first-person BMP shows the original knife, icons, health bar and glyphs; its
H snapshot shows Sam and the same HUD with the first-person knife hidden.
Display limitations such as active blending, automap mode4 and overloaded OAM
priority interactions are explicit errors, as documented in `display.md`.
