# Dynamic PC render resolution

The playable frontend now renders the world at the window's client size by
default. Resizing or maximizing the window changes the internal framebuffer,
world ray count, geometry edges and texture sampling. This is an additional PC
renderer; the original GBA routines and their reference output remain available.
The frontend requests per-monitor DPI awareness and responds to DPI changes,
avoiding ordinary Windows bitmap enlargement on high-DPI displays.

```powershell
.\play-native.cmd
.\play-native.cmd --resolution 1920x1080
.\play-native.cmd --resolution 3840x2160
.\play-native.cmd --resolution 7680x4320
.\play-native.cmd --resolution auto
.\play-native.cmd --original-renderer
```

Custom sizes render at exactly the requested dimensions and fit into the window
with the same aspect ratio. Auto mode fills the client area. There is no preset
resolution ceiling: sizes are checked for integer/address overflow and allocation
failure. Color/depth storage costs eight bytes per pixel; pitched rendering
adds four bytes per row/column for the flat sky lookup and cached floor topology. Windows
presentation adds a client-size 32-bit offscreen DIB. Old and new allocations
can briefly coexist during resize. Available memory,
software rendering performance, and platform integer/DIB limits still apply.

`SsaPcRenderer` owns dynamic color/depth buffers and per-column intersection
scratch. Its resize operation is atomic: invalid dimensions or allocation
failure retain the prior buffers. The frontend reports a requested allocation
failure and exits cleanly. Free releases all buffers. Rendering is read-only
with respect to gameplay and runs once per presented simulation batch.

The PC frontend now advances simulation separately from presentation, so it
does not draw the original GBA reference world on every simulation/catch-up tick.
The original path remains available explicitly. Windows composes each complete
frame, bars and text into a memory DIB before one finished-image blit. It never
clears the visible client to black between frames. See
[performance and presentation checks](render-performance.md).

Each output column casts a separate ray through decoded original map lines,
walking live sector portals and door heights. Perspective-correct wall samples
use the original highest-resolution texture archive; original sector flat colors
and sky data draw the planes. Per-pixel depth clips original directional actor
sprites against geometry and one another. The highest available actor mip is
selected, with its original direction, flip, origin and player overlay. The
vertical field of view stays consistent as the aspect ratio changes, extending
the horizontal view on wide windows.

HUD and first-person weapon artwork retain their original pixel art. A separate
transparent sprite layer is composed from actual OAM/cache data, then scaled
inside a centered 3:2 safe area. It does not copy or enlarge the low-resolution
world framebuffer. Textures and sprite source art have not been replaced or
AI-upscaled. H, jump, aiming, collision and combat remain simulation behavior.

The original renderer remains the reference for exact GBA behavior. The PC
renderer uses floating-point geometry and new depth/raster rules; it does not
claim byte-for-byte or pixel-for-pixel equivalence to GBA projection quirks.

Vertical mouse aiming now rotates the PC camera with proper perspective,
near-plane clipping, depth and texture interpolation, including upright actor
billboards. Read [vertical aiming](free-look.md) for implementation and checks.

The background is a flat, screen-facing image. Yaw scrolls the original panorama
horizontally; pitch shifts the image vertically with the projected horizon. Rows
and columns stay straight at every view angle.
Level and pitched views use the same sky sampling. Geometry, actors and aiming
retain their perspective projection and depth. The sky lookup is separable:
one column and one row index, using4*(width+height) bytes instead of4*width*height.
At level aim, the PC background fits the actual60-row backdrop into the upper
three quarters of the viewport. Looking up moves that image down; looking down
moves it up. The offset
is focal_length*tan(render_pitch), using the actual presentation camera pitch,
including third-person convergence. Artwork size remains fixed. Each column
repeats its first displayed pixel above the translated image and its last
displayed pixel below, so exposed space never wraps or samples adjacent data.
Fractional coverage rounds upward; even at tiny resolutions the
extension uses the last actually displayed row. Column and row lookups update
in O(width+height) per frame without reallocating on pitch changes. This shared
sampler applies to both level and pitched views. The reference sampler's
120-row readable window overlaps the next family/other data and is retained
only for original GBA behavior, rather than displayed below the PC sky.

Validation:

- ASan/UBSan with leak detection covers 1×1, 127×73, 240×160, 960×640,
  1920×1080, 3840×2160 and 8193×17, resize guards, read-only gameplay state,
  24 jumping/chase frames and real actor pixels. At 960×640, 257,587 pixels
  differ from nearest-neighbor enlargement of the original output.
- Normal-input level completion with unlimited ranged attacks: 4,653 simulation frames,
  28,000 score, seven pickups, 23 health. The new renderer draws77 normal and77 pitched
  1280×720 frames along the entire route without a failure.
- Windows tests verify eight sizes/modes through 65537×1, custom output/snapshot
  dimensions, one world ray per output column, GDI presentation, original-mode
  fallback, four invalid size strings, and live
  automatic resizing from 1264×861 to 1008×729. A 4K frame and first/chase
  views were visually inspected. Physical-device interaction remains untested.

Reports: `pc-renderer-validation.json`,
`pc-renderer-playability-validation.json`, and
`windows-render-resolution-validation.json`.

```powershell
.\decomp.ps1 test-pc-renderer
.\decomp.ps1 test-pc-renderer-playability
powershell -ExecutionPolicy Bypass -File scripts/test-render-resolution.ps1
```
