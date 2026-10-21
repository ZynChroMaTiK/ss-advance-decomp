# Vulkan hardware scene backend

The optional Windows backend uses Vulkan1.0 graphics pipelines to rasterize
textured walls and BSP floor/ceiling triangles with depth testing, plus
silhouette-clipped billboard actors with transparent palette-index rejection. It is actual geometry rendering,
not an upload of the software-rendered scene. Vulkan is selectable in Options / Graphics
alongside Software and GBA when a hardware device and pipeline initialize.
`play-native.cmd --vulkan` overrides the saved choice for a launch.

Vulkan is the default for fresh settings and Clear Data, with automatic Software
fallback if initialization or rendering fails. Unavailable Vulkan is hidden from
the renderer choices; cycling offers only Software and GBA. Existing saved
renderer choices remain intact (including Software for version1 settings).
Vulkan uses native projection,
vertical-aim policy, FOV and Native/3:2 aspect settings. GBA remains its separate
fixed-resolution reference mode. Settings version15 retains the16-byte format,
existing renderer values0/1, and all previous choices; byte7 bit7 adds renderer2.
Versions1..14 still decode. Invalid renderer3 is rejected.

## Ownership and rendering

`SsaPcHardware` is a borrowed scene interface, independent of Vulkan and Win32.
The shared scene code prepares near-clipped geometry and current actor frames.
The GPU consumes batched triangles, current palettes and column-major indexed
textures. A reversed D32 depth buffer handles occlusion. Two offscreen color
attachments return RGB and visible material/UV/depth information. Nearest texture
sampling retains the pixel art; hardware float interpolation and edge coverage
can differ slightly from the double-precision software rasterizer.

The scene's existing exact CPU decal/water rules shade the visible plane samples.
Reflections perform a second GPU geometry pass using the mirrored/clipped camera,
then the existing half-alpha water composition. The GPU snapshot finishes before
any reflection reuses its resources. Transparent actors exclude covered water
pixels from the reflection mask. Fixed-screen effects, HUD, menus, NETRICSA,
fades and final GDI window presentation retain their existing composition.
Readback and CPU effects remain synchronization/performance costs: this is a
hybrid hardware renderer, not a completely GPU-resident frame. The performance
correction and measured limits are described below.

The frontend owns one Vulkan context across level changes. Scene and reflection
renderers borrow it; their destruction never destroys the device. Resource growth
and target resize happen only after the previous fence completes. Transfers,
render-pass transitions and host reads use explicit Vulkan barriers. Offscreen
targets do not require a window swapchain and are unaffected by minimize or F11;
the existing presentation path handles windows and letterboxing.

`vulkan-1.dll` is loaded at runtime: PCs without Vulkan can still launch the
software/GBA build. Initialization rejects CPU devices, missing graphics queues,
unsupported formats/features and failed allocations. A runtime render failure
detaches both scene backends, releases the device and redraws in software with a
notice. GPU limits are checked before target allocation. Shader binaries are
compiled into the library; the released executable needs the GPU driver, not
the SDK or loose shader files.

## Build and checks

### Stable sprite priority (2026-10-21)

The recovered GBA queue paints actors far-to-near, with later submissions
winning equal-depth ties. Software and Vulkan now share stable billboard-plane
ordering; source order is retained in groups spanning at most0.001 units.
Sorting uses camera-forward XY distance before pitch, and each secondary layer
follows its base layer. Reflections use the same ordering.

Actor triangles are clipped on the CPU to the original GBA wall/portal
silhouettes before submission. World triangles populate reversed depth first;
the second pipeline paints these prepared sprites without testing or writing
depth. This avoids slicing corpse/explosion artwork at the floor, preserves
wall occlusion and transparent-index discard, and prevents sprite depth fights.
There is no longer a sprite-only depth bias. Geometric depth returned to CPU
effects is unchanged. World and actor batches normally require two draw calls.
See [sprite-clipping.md](sprite-clipping.md) for original rules and regressions.

The regression suite covers18 sprite-priority cases, floor-crossing artwork,
solid wall hiding, pitch/FOV, reflections, late-submitted world primitives and
unbiased returned depth. The latest RTX4060 run passes6,487,023 checks and28
paired scene draws with zero validation errors. All ten focused CTests and the
hidden180-frame Vulkan launch/resize check pass.

Install the Vulkan SDK with glslangValidator for a Vulkan-enabled Windows build.
CMake discovers it through `VULKAN_SDK`; without it, or with `-DSSA_VULKAN=OFF`,
the optional backend compiles as an unavailable stub. No ROM assets are embedded
in shaders or binaries. Core still links `ssa_pc_scene`, never the HUD compositor
or platform Vulkan library.

Run `scripts/test-vulkan.ps1 -Validation` to build and check on a hardware GPU
with the Khronos validation layer. Add `-SkipBuild` to reuse the build. It checks
depth occlusion, transparent texels, odd-size and1-pixel targets, resizing,
software pixel comparisons, level/third-person views, FOV extremes, actual pool
reflection, renderer switching, menu availability and settings controls. It
does not run a campaign playthrough. Results and input hashes are recorded in
`vulkan-validation.json`; snapshots and validation logs are under
`build/native-windows/test-output/vulkan`.

The2026-10-20 run on an NVIDIA GeForce RTX4060 Laptop GPU passed4,868,499 checks
over22 paired draws with zero validation errors. Compared images matched
99.13..100% of pixels (4,858,486 of4,868,374 overall), including third person,
FOV60/120, decals and the Slave Compound pool. A hidden, muted180-frame frontend
smoke run completed using Vulkan. Eight focused CTests passed. No campaign
playthrough or subjective control/audio validation was performed.

Synchronization guidance: [Khronos synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html)
and [rendering submission lifecycle](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/02_Rendering_and_presentation.html).

## Performance correction, 2026-10-20

The2026-10-21 follow-up combines depth/statistics/effects processing into one
parallel row pass, removes redundant hardware buffer clears, and copies
repeated sky rows within each draw. Three paired benchmark runs show21..30%
lower median isolated frame time with identical captured pixel/depth/water
hashes. See [technical-refactors.md](technical-refactors.md) for the current
measurements, validation and ownership details. The older timings below remain
the historical result of the first stall fix.

The first implementation selected the first coherent host-visible memory type
for both uploads and downloads. On the RTX4060 Laptop GPU, reading individual
sample components from that uncached mapping caused enormous CPU stalls:
1080p isolated draws took617..2110ms (0.47..1.62fps). Coherent does not mean
cached. See the [Khronos memory allocation rules](https://docs.vulkan.org/spec/latest/chapters/memory.html).

Uploads and readback now use separate persistent buffers. Readback prefers
HOST_CACHED|HOST_COHERENT, then cached non-coherent memory with explicit
invalidation after the transfer barrier and fence. If neither cached type is
available, a bulk copy into ordinary CPU memory precedes sample processing.
Non-coherent readback is never written by the CPU. Buffer capacities grow
geometrically; descriptor ranges still cover only actual data. The GPU stores
zero output alpha directly instead of requiring a CPU mask over every pixel.

Independent RGB decal/water rows run synchronously on a bounded Windows worker
pool (at most seven workers plus the render thread, scaled down on smaller
machines). Jobs only write disjoint pixels; statistics remain on the caller.
All jobs finish before reflection reuse, another frame, or device destruction.
The indexed GBA reflection path remains serial because it updates a shared
palette cache. The portable scene library still depends on no Windows APIs.

Reflections retain full native resolution and projection, but rasterize,
upload, download and shade only rows consumed by visible water. One-to-one
reflection composition skips redundant bilinear taps. Camera/palette setup
runs once per water height, after rejecting already-seen stencil pixels.
No settings, decal patterns, water blending or simulation cadence changed.

Run `scripts/bench-vulkan.ps1 -SkipBuild` for four isolated1080p scenes,60
frames each, with wall-clock transfer/wait/effect timings. `-Software` measures
the reference path; `-Width`, `-Height` and `-Frames` vary the workload.
Validation layers are disabled only for benchmarking. Reports include source
hashes in `performance-vulkan.json`. These are render timings, not a promise
of end-to-end gameplay FPS: audio, simulation and presentation are excluded.
Full-resolution reflected water is still the most expensive case.

Final60-frame local measurements on the RTX4060 Laptop GPU at1920x1080:

| Scene | Effects | Milliseconds/frame | Render FPS |
| --- | --- | ---: | ---: |
| Level1 start | Decals off | 11.710 | 85.40 |
| Level1 start | Decals on | 16.558 | 60.39 |
| Slave Compound pool | Decals, no reflections | 18.849 | 53.05 |
| Slave Compound pool | Decals and reflections | 31.467 | 31.78 |

The separate [before/after comparison](vulkan-performance-comparison.json)
uses identical three-frame camera workloads:616.973/711.717/1271.430/2109.613ms
before versus12.599/14.686/17.790/30.653ms after, respectively. Normal timing
variation and longer camera sweeps explain differences from the60-frame table.

Regression coverage additionally checks partial-row transfers leaving outside
rows intact and exact serial/parallel RGB, depth and water-mask equality over
eight animation phases at an odd target size. The ordinary Vulkan/software
image comparisons, resize/startup smoke check and portable renderer tests remain.
Final checks:4,869,177 Vulkan assertions with zero validation errors; exact
serial/parallel output equality; eight focused CTests;11,169,555 portable
renderer assertions under ASan/UBSan with leak detection; hidden muted180-frame
Vulkan frontend startup/resize check. The Windows playable executable was rebuilt.
