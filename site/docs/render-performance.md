# Rendering performance and frame presentation

October19 rendering update:

- Flat views decode wall endpoints and camera-relative ray numerators once per
  draw, reusing them across screen columns. The scratch storage grows with the
  map and is released with the renderer. Refreshing every draw preserves live
  map edits and reloads at reused addresses.
- Power-of-two texture wrapping uses a mask after the original signed floor
  conversion; other dimensions retain signed modulo wrapping.
- Each water reflection still renders the mirrored 3D scene at full resolution
  every presented frame. It now clears/rasterizes only rows that visible water
  at that height can sample. Projection, scanline interpolation, clipping and
  depth tests remain unchanged. Compositing traverses rows and skips coordinate
  divisions for equal-size software output; GBA retains its original sampling.
- Water glints/ripples share coordinate conversion and reject inactive animation
  phases before computing their randomized sprite placement. Art, placement,
  animation timing, distance fade and reflection ordering are unchanged.

`ssa_renderer_bench orig/SSA.gba` (source `tests/bench_renderer.c`) measures12
frozen render fixtures across Egypt/Rome, flat/downward views, dry/water materials,
960x640 and1920x1080. Water fixtures change floor colors solely to exercise the
effect; they do not run or validate campaign progression. Each case warms up,
then reports the median of three12-frame timings and final color/depth hashes.
Before/after executables run sequentially without concurrent builds. Results,
hash comparisons and source fingerprints are in
`analysis/renderer-optimization-validation.json`. These are local render times,
not whole-game FPS guarantees.

Two paired MSVC runs measured30–34% less render time in the flat dry fixtures
and11–23% less in the water fixtures. Dry pitched views ranged from3% slower
to7% faster, so no consistent gain is claimed there. All12 final color buffers
and primary depth buffers match the preserved baseline exactly. Full-HD water
fixtures still take approximately62–89ms per render on this machine; these
changes do not make full-resolution software reflections a guaranteed60FPS.

The final build passes52,135,255 surface/decal checks in GCC ASan/UBSan/leak
checking and MSVC, plus11,169,540 general renderer checks under sanitizers
(seven framebuffer sizes and24 chase/jump frames). Both software and GBA
Windows startup smokes pass. No campaign playthrough was run.

The original crosshair has replaced the temporary PC glyph. The playable game
now routes the original type18 HUD controller and original sprite art without
suppressing them during vertical aiming. Its placement, target/no-target
animation, and knife/weapon8/death/automap visibility remain the recovered rules.
Mouse pitch and direct weapon aiming remain available.

The former Windows paint path filled the visible client black before drawing
the framebuffer and text. It now composes those into a client-size 32-bit memory
DIB, flushes GDI writes, and copies only the completed image to the window in one
blit. This also covers letterboxing, notices, initial presentation and resizing.
No intermediate black clear reaches the visible client. Repainting always uses
a completed game framebuffer. DIB resources are replaced on resize and released
on shutdown. The frontend presents immediately after a simulation batch, rather
than waiting for another queued paint to show the new frame.

The PC frontend uses `ssa_native_game_tick` plus `prepare_view`, retaining the
60Hz simulation and native HUD/OAM updates while avoiding an unused original
world/actor/display render on each catch-up tick. `ssa_native_game_step` retains
the reference-rendering contract for other callers. The original renderer option
draws the reference once per presented batch.

The pitched renderer now caches static subsector polygons, while reading sector
heights/colors live. A geometry fingerprint invalidates the cache across topology
changes. Its flat sky uses separate row/column lookups, retaining vertical
framing across pitch and panning horizontally with yaw. x64 uses SSE2 square root without a new libm
dependency. Triangle rasterization walks covered scanline spans with incremental
reciprocal depth and texture attributes. HUD expansion fills only opaque native
pixel runs instead of scanning every world pixel for transparent artwork.

Validation includes ASan/UBSan/leak checks, 30,100 depth comparisons against an
independent ray tracer, cache reuse with live height changes, and 18,280 original
HUD pixel comparisons at integer/fractional scale. Normal-input completion through
the split simulation path remains 5,258 frames, 28,000 score and 100 health. The
PC renderer draws 87 level and 87 pitched views along that entire route.

`scripts/test-render-performance.ps1` exercises 601 buffered presentations and
1,803 pixel checks across 600 jumping/H/pitch gameplay frames with live resize.
These verify completed offscreen frames against source pixels; physical monitor
flicker and input delivery are not directly measured. The same script records
local Windows throughput using the preserved previous executable when available.
At 1280x720, the 180-frame aiming workload improved from 6.730s to 1.233s (5.46x).
The horizontal workload improved from 2.824s to 2.447s. These are uncapped hidden
test workloads including startup/simulation/rendering, not a promise of that FPS
on every computer; normal gameplay stays at60 simulation ticks per second.

The standalone `tests/bench_pc_renderer.c` measures fixed-scene render CPU time.
The first six cases preserve the before/after pitch and horizontal-turn sequence;
the updated benchmark additionally measures continuous vertical sweeps. Frozen
baseline and rebuilt executables/reports are local validation artifacts under
`build/native-tests` and `analysis/render-performance-before.json` / `after.json`.
Windows evidence: `analysis/windows-render-performance-validation.json`.

In a separate sequential GCC `-O2` CPU-time run at1280x720, stationary tilted
views improved from 34.9--37.7ms to 3.85--3.99ms; horizontal turning while tilted
improved from 64.7--70.1ms to 5.69--7.88ms. Continuous vertical sweeps, which
rebuild sky directions, measured 14.0--16.2ms after optimization; no preserved
before-sweep measurement is claimed. Each case uses24 draws after a warm-up.
