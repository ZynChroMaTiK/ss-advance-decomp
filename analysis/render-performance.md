# Rendering performance and frame presentation

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
