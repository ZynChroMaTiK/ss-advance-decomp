# Technical audit follow-up, 2026-10-20

## Graphics menu and presentation follow-up, 2026-10-22

Graphics toggles now use one setting-field mapping for both displayed values
and activation. Activation, keyboard/pointer availability and disabled styling
share the row descriptor's capability policy. Renderer cycling remains separate;
localized labels, stable IDs, order, layout and saved settings are unchanged.
All51 menu captures remain byte-identical after the refactor.

The frontend only reapplies audio settings when music style, music volume or
sound volume changes. Graphics, display and Extras changes no longer acquire
the audio apply lock, reset identical mixer volumes or resend MIDI volume state.
The existing audio-status snapshot and availability fallback still run. An
instrumented Windows frontend fixture verifies zero audio apply calls through
display/Graphics/Extras changes, then one for music style and one per volume edit.

New menus receive VSync availability before their first draw, including menus
recreated after gameplay. VSync Off/unavailable now avoids the two window-state
queries in each presentation; enabled VSync still checks visible/non-minimized
windows and keeps its existing error recovery. No presentation timing, renderer
quality, gameplay clock or audio balance changes were introduced. These are
bounded reductions in redundant work, not a measured gameplay FPS improvement.

Validation: Windows Release rebuilt; all11 focused CTests;40,064 ASan/UBSan
menu/audio checks;23,864 Windows settings/input/audio/persistence checks;
31,086 input/VSync checks;8,453,272 Vulkan checks with no validation errors and
hidden frontend resize sanity. The Windows Settings pointer fixture was updated
to the current display row. No campaign playthrough or physical input test.
See `frontend-settings-refactor-validation.json` for source/capture hashes.

## Motion-blur pass scheduling and working storage, 2026-10-22

The Vulkan postprocessor now omits camera blur on stationary frames and camera
history resets. Motion-only no-op frames skip the composed RGB upload, compute
submission, fence wait and RGB download; AO/AA still run when selected. Camera
history advances even on skipped frames, so movement resumes from the latest
view rather than accumulating old velocity. Main-scene surface metadata is
still retained on the GPU when motion blur is enabled.

AO, AA and blur share a single ordered dispatch loop and at most two working
image regions. Blur can overwrite the AO result after AA has consumed it;
compute barriers cover both read-after-write and write-after-read dependencies.
The three-effect working range drops from12 to8 bytes per pixel (23.7 to15.8MiB
at1080p). With the backend's power-of-two allocator, a fresh1080p work buffer
needs16MiB instead of32MiB; capacities remain retained across smaller viewports.
Push-constant allocation derives from the C structure size instead of a repeated
literal. A cumulative post-dispatch counter supports regression checks.

Three alternating before/after pairs on the RTX4060 Laptop GPU,30 stationary
frames per each of eight scenes at1920x1080, give median run-average timings:

| Enabled effects | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| Motion blur | 16.829ms | 15.471ms | 8.1% |
| AA + AF + AO + motion blur | 18.225ms | 17.888ms | 1.8% |

These are isolated renderer timings with a stationary camera, not gameplay FPS
or measurements of moving-camera blur. All112 before/after scene captures have
identical pixel/depth/water hashes and actor/wall counts:48 stationary captures
plus64 moving captures covering non-motion masks0..7 at odd resolution641x359.
Moving blur depends on wall-clock exposure and is covered by GPU regressions
rather than cross-process hash comparisons. Details and raw run locations are
in `vulkan-motion-refactor-validation.json`. The benchmark accepts an optional
`stationary` argument after the effect mask.

Validation passes8,453,254 Vulkan checks and30 paired scene draws with no Vulkan
validation errors, plus the hidden muted180-frame frontend resize check and
all11 focused CTests. New checks assert omitted no-op dispatches, preserved
outputs, and resumed blur after cuts for every AA/AF/AO combination. The Windows
Release frontend was rebuilt. Settings, effect quality, Software/GBA rendering
and game simulation are unchanged. No campaign playthrough was run.


## GPU-retained AO metadata, 2026-10-22

The world-texel AO implementation previously copied each main-view UV/depth/
material sample into a CPU array and uploaded it again after reflections.
The Vulkan backend now retains those samples directly on the device before
reflection passes reuse the render attachments. Only the composed scene RGB
is uploaded for postprocessing. The ordinary CPU readback needed for decals,
water and statistics remains unchanged.

This removes the scene's16-byte-per-pixel AO array and its per-pixel copy/mask
work. At1920x1080, the array occupied31.6MiB; normal AO postprocessing uploads
fall from39.6MiB to7.9MiB per frame (80% less). Device-local copying preserves
all original float samples. The shader applies the same sky/billboard depth
mask that the CPU previously applied. No sample counts, resolution, filtering,
world-texel snapping, AO radius or visual quality were reduced.

Metadata ownership follows the main renderer and dimensions. Starting a new
pass on that renderer invalidates previous metadata; reflected renderers leave
it intact. Successful postprocessing consumes it. The sample block is aligned
to16 bytes, including odd frame sizes, and the transfer-to-compute barrier
covers retained data and the new color upload. Standalone postprocessing keeps
an explicit host override for differential checks. Scene code no longer owns
or populates backend AO metadata.

Three alternating paired runs,30 frames per scene over8 scenes at1920x1080 on
the RTX4060 Laptop GPU, produced these median-of-run-average render times:

| Effects | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| AO | 22.397ms | 16.956ms | 24.3% |
| AA + AF + AO | 22.771ms | 17.061ms | 25.1% |

These are isolated renderer timings; simulation, audio and presentation are
excluded. All96 before/after scene comparisons retain identical pixel, depth,
water-mask hashes and actor/wall counts. Additional odd-resolution checks cover
all remaining effect masks. Raw runs, method and source hashes are in
`vulkan-ao-transfer-validation.json`.

New regressions compare retained raw GPU samples against the former masked
host-upload route after reflection attachment resize/overwrite, main-target
growth/shrink and with/without AA. Real scene draws assert no host AO array is
allocated. Vulkan passes8,222,890 checks/30 paired draws with zero validation
errors, including hidden muted frontend resize sanity. All11 focused CTests
and31,073 Win32 weapon-wheel/input checks pass. The rebuilt Windows frontend
has no compiler warnings in the affected rebuild. Existing narrow integer/mask
and wide-cursor resource warnings were made explicit without changing behavior.
No campaign playthrough was run.


## Vulkan postprocessing and policy cleanup, 2026-10-22

The revision removes unnecessary work from the new Vulkan effects while
preserving their rendered output:

- FXAA without SSAO reads input colors directly. It no longer clears/uploads
  unused depth or dispatches a full-screen copy before filtering. Upload size
  falls from8 to4 bytes per pixel; compute dispatches fall from2 to1.
- Device-local input/work ranges follow the selected effects. AA alone needs
  two single-image buffers; AO alone needs color+depth input and one output;
  combined AO+AA retains its intermediate image and explicit compute barrier.
  Geometric capacities remain retained across toggles to avoid reallocations.
- SSAO reuses its four loaded neighbor depths for normal reconstruction.
- Named buffer-memory and menu-availability policies replace overloaded numeric
  flags. Upload/readback/device-local ownership and renderer-only row rules are
  explicit. Three menu shadowing/signedness warnings are fixed.

Validation: warning-free Windows Release build; all11 focused CTests;
8,202,211 Vulkan assertions and30 paired scene draws with zero validation
errors, including the hidden muted frontend resize smoke. New post tests cover
AA without depth, exact AA versus neutral-AO+AA output, repeated effect toggles,
1-pixel targets and growing odd sizes. ASan/UBSan audio/settings passes40,028
checks; all51 existing menu captures remain byte-identical. No campaign replay.

Three alternating before/after pairs at1920x1080,40 frames per scene across
8 scenes, and additional641x359 runs cover all8 effect masks. All112 compared
scene outputs retain identical color, depth, water-mask hashes and actor/wall
counts. Raw results and source hashes: `vulkan-post-refactor-validation.json`.

Median-of-run-averages effects time for AA alone fell7.386->6.763ms (8.4%).
This timing includes CPU surface effects, not just postprocessing. Total render
time did not improve reliably: AA17.599->18.440ms, AO18.479->19.406ms,
all effects17.927->18.041ms. The concrete benefits are reduced transfers,
dispatches and requested working memory; these measurements do not establish
an overall FPS increase or an AO speedup. The hybrid CPU/GPU scene round trip
remains a larger architectural cost. No quality/sample-count reductions or
simulation/audio timing changes were made.


## Rendering and menu cache follow-up, 2026-10-21

The audit follow-up implements the measured rendering hot spots without changing
resolution, texture art, simulation timing or sprite priority:

- Floor/ceiling and water sampling share caller-owned, 512-entry world-texel
  scratch. Each software view and synchronous hardware row job starts empty.
  Keys include texel coordinates, plane height, material color and surface kind.
  Only the unfaded near range is cached; distance fading and the reflection
  stencil remain per pixel. No mutable cache is shared by worker threads or
  retained across moving sectors, animation frames or palette changes.
- Sprite wall transforms, near clipping and live portal heights are prepared
  once per view. Each sprite still applies its own origin/height gates and wall
  silhouette clipping. Subject/shadow/overlay submission uses one shared helper;
  original stable painter order and floor-overlapping sprite art are preserved.
- Reflections collect height/row intervals in one stencil scan, in original
  first-appearance order. They reuse shadow preparation and actor ordering, and
  avoid redundant geometry-signature work per reflected height and godray pass.
  Every mirror retains its own camera-dependent clipping preparation.
- Vulkan uses hashed material and texture lookups without sorting submissions.
  Texture entries own a byte copy and expanded palette indices. First use in
  each pass compares current bytes, including mip and generated shadow art;
  stale addresses are integer keys, never dereferenced. Unchanged content stays
  uploaded, changed ranges and the live palette are uploaded, and buffer growth
  copies the complete retained content. Retention resets at the next pass after
  4 million texels or 4,096 entries; entries cannot be evicted mid-submission.
- Settings and Graphics row descriptors now drive label/slider coordinates,
  ranges and GBA-disabled styling/hit testing together. Stable action IDs,
  translations, saved settings and existing positions remain unchanged.

The benchmark now includes eight scenes: start with/without decals, water with/
without reflections, reflected shadows, godrays, and a synthetic 64-sprite scene
with/without shadows. The crowd fixture changes render inputs only; it does not
advance or bypass gameplay. Measurements exclude simulation, audio and display
presentation. Larger GPU scheduling/readback changes remain outside this pass.

Validation and paired timings are recorded in
`render-refactors-validation.json`. Three alternating paired runs, 60 frames per
case at1920x1080 on the RTX4060 Laptop GPU, produced these medians:

| Scene | Before (ms) | After (ms) | Render time reduction |
| --- | ---: | ---: | ---: |
| Start, decals off | 8.165 | 8.061 | 1.3% |
| Start, decals on | 10.442 | 8.569 | 17.9% |
| Pool, decals | 16.512 | 11.266 | 31.8% |
| Pool, reflections | 38.318 | 27.694 | 27.7% |
| Pool, reflections and shadows | 41.051 | 27.736 | 32.4% |
| Godrays | 23.816 | 19.167 | 19.5% |
| 64 sprites | 12.838 | 11.535 | 10.2% |
| 64 sprites and shadows | 12.522 | 11.048 | 11.8% |

The 1.3% plain-start difference is small enough to treat as timing noise. These
are isolated renderer measurements, not gameplay FPS guarantees. Pixel, depth,
water-mask hashes and actor/wall counts match before/after in all eight Vulkan
cases across all three pairs, and in eight480x320 Software cases. All39 menu
captures are byte-identical across three languages.

Validation: the static-runtime Windows Release build passes; all11 focused
CTest checks pass; Vulkan passes6,564,616 assertions,30 paired scenes and the
muted frontend resize smoke with zero validation errors. Texture regressions
cover same-address byte changes, palette changes, mip-only changes, changed
image dimensions, repeated material sharing, buffer growth and cache eviction.
ASan/UBSan with leak checks passes surface decals (56,872,820 checks), PC renderer
(11,803,544) and sprite shadows (14,408). Settings/core audio and the hidden,
muted Windows Settings UI pass. No campaign playthrough was run.


## Render-loop optimization, 2026-10-21

The hardware scene finish now gathers depth, actor/wall statistics, decals and
water in one parallel row pass instead of a serial full-frame pass followed by
a second effects pass. Each job owns its rows and per-row counters; counters
are reduced after synchronous completion. Scratch counters grow with the target
height, persist across draws and are released with the renderer's geometry
cache. No gameplay or audio state enters worker jobs.

Hardware frames skip CPU depth clears that GPU readback completely overwrites,
including the consumed reflection rows. Software still initializes depth.
The hardware path also skips the redundant zero-pitch color clear: its sky
pass fills every consumed row before upload. Sky rendering resolves each
distinct texture row once and copies adjacent identical rows. This reuse is
only within a draw, preserving live palette fades, yaw and pitch changes.

Three paired 60-frame runs at1920x1080 on the RTX4060 Laptop GPU measured these
median isolated render times (simulation, audio and presentation excluded):

| Scene | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| Level1 start, decals off | 11.139ms | 8.011ms | 28.08% |
| Level1 start, decals on | 11.976ms | 8.344ms | 30.33% |
| Slave Compound pool, decals | 16.144ms | 12.731ms | 21.14% |
| Pool, decals and reflections | 33.581ms | 24.744ms | 26.32% |

All measured final-frame pixel/depth/water hashes and actor/wall counters match
the baseline exactly across the Vulkan runs; four separate854x480 Software
cases also match. `tests/bench_vulkan.c` reports these hashes outside its timing
interval. Timing variation remains, and these are not end-to-end gameplay FPS.
The backend's `effects_ms` now includes depth/statistics processing too, so
compare total render times rather than that component across implementations.
Raw paired samples and source hashes: [render-optimization-validation.json](render-optimization-validation.json).

Validation: rebuilt Windows frontend and128 libraries; eight focused CTests;
5,846,303 Vulkan checks,18 sprite-order cases and22 paired GPU/software draws
with no validation errors; exact serial/parallel pixels, depth, water masks and
counters with and without effects; muted180-frame default-Vulkan launch/resize;
11,598,404 portable renderer assertions under GCC ASan/UBSan with leak checking.
No campaign playthrough was run.

The audit fixes are implemented without intentional changes to gameplay,
rendered artwork, audio balance, or save formats. The remaining large frontend
state-machine redesign is deliberately staged; the original transition order
and ownership flags still govern the application.

## Changes

1. **Profile errors.** `ssa_win32_profile_store` checks the reread under its file
   lock. A transient read failure retains the pending in-memory change and
   leaves the disk file intact. Missing/corrupt files can be repaired from known
   session data; explicit Clear Data still clears, and another writer's valid
   cleared profile wins over stale session bits. This follows the existing
   score-table writer's policy.
2. **Audio status.** The UI obtains device availability, MIDI error and music
   activity through `ssa_win32_audio_status`, taking the worker's mutex.
   Open/close remain UI-thread lifecycle operations. Music modes, gains, delay,
   voices and sound-effect mixing are unchanged.
3. **Shared decal atlas.** Immutable masks/thresholds are initialized once per
   process. Windows uses InitOnce; portable builds use acquire/release C11
   atomics. Level instances retain their own palette and layout, and never free
   the shared atlas. Sampling coordinates, tile generation and texels are
   unchanged. Tests retain a live instance while another is reinitialized and
   freed, in addition to checking complete atlas hashes and rendered pixels.
4. **Renderer dependency.** `ssa_pc_scene` owns geometry, world/actor drawing,
   allocation and mirrored-scene rendering. `ssa_pc_renderer` composes the HUD
   over that scene. Core depends on the scene backend, which calls no native
   lifecycle or HUD functions; it no longer links back to the PC compositor.
   The backend still reads the prepared `SsaNativeGame` structure. A narrower
   prepared-view data API can be introduced separately if that coupling becomes
   a maintenance problem. The missing portable `libm` dependency is explicit.
5. **Frontend organization.** `game_screens.inc` contains screen entry/exit and
   NETRICSA controller code; `game_diagnostics.inc` contains scripted audio,
   menu and story inputs plus the NETRICSA pointer diagnostic. App ownership and
   presentation-boundary assertions remain in `game.c`. This is a first
   separation of responsibilities, not a claim that every diagnostic or screen
   flag has been removed from the loop.
6. **Settings.** One field catalogue generates native storage, validation and
   comparison. The version-12, 16-byte disk encoder stays explicit. Shared
   runtime application is used for live changes and level setup; it validates
   before modifying state and does not reconfigure enhanced weapons when the
   mode is unchanged. Display/audio/input policy remains in the platform layer.
7. **Focused test builds.** CTest registers eight non-playthrough checks against
   production libraries. The three portable rendering/HUD Python runners share
   a content-addressed GCC sanitizer object/archive cache instead of rebuilding
   every C source independently. Toolchain flags, headers, includes and source
   content participate in cache keys. Failed builds never publish cache entries;
   an interprocess lock protects concurrent builders. Other historical ROM
   differential runners retain their specialized build paths.

## Evidence

- Full Windows Release build passed, including the frontend with `/W4 /WX`.
- Eight CTest checks passed: settings fields, runtime settings, profile I/O,
  audio status, surface decals, PC renderer, classic HUD, frontend intermission.
- Profile I/O: 21 assertions covering sharing violations, retained progress,
  retry, concurrent clear, corrupt-file repair and failed replacement.
- Audio snapshot stress: 50,000 reads with a concurrent writer, no audio device.
- GCC ASan/UBSan with leak checking passed surface decals (52,224,732 checks),
  PC renderer (11,169,540), and classic HUD (382,453). MSVC agrees with the
  surface suite.
- The 12 pre/post renderer benchmark cases have identical pixel/depth hashes;
  all 25 saved decal screenshots are byte-identical.
- Windows Settings UI passed 23,656 checks, muted, including all three music
  modes. The audio/settings codec suite passed 39,044 checks over 26 sequences.
- Muted normal startup/menu/restart, GBA startup/audio/restart, and Egypt story
  handoff passed. No automated campaign playthrough was run.

Measured x64 sizes: `SsaNativeGame` decreased from 8,451,768 to 1,767,112 bytes;
per-level decals decreased from 6,701,680 to 17,016 bytes. The process still owns
one 6,684,672-byte atlas. The first atlas bake measured 45.6 ms during this run;
subsequent level initialization measured 0.258 and 0.249 ms, versus approximately
38–40 ms for each initialization before sharing. These are initialization and
memory measurements, not a claim of higher rendering FPS.

Reproduce focused Windows checks:

```powershell
.\scripts\test-focused.ps1
# Reuse an existing build:
.\scripts\test-focused.ps1 -SkipBuild
```

CTest writes artifacts under `build/native-windows/test-output` and JUnit output
to `build/native-windows/focused-tests.xml`. It never runs campaign completion or
the audible MIDI synchronization diagnostic. WSL sanitizer entry points remain
`tests/test_surface_decals.py`, `tests/test_pc_renderer.py` and
`tests/test_classic_hud.py`. CMake also exposes `SSA_SANITIZERS` for GCC/Clang;
that CMake configuration was not run here because WSL has no CMake installed.

The machine-readable summary is `technical-refactors-validation.json`. Existing
feature-specific validation files retain their own scope. Manual control feel,
audio listening, and full campaign parity were not evaluated by this refactor.
