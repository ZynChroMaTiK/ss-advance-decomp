# Technical audit follow-up, 2026-10-20

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
   before modifying state and does not reconfigure alternate weapons when the
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
