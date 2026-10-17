# Wall intercept collection

`src/ray_intercepts.c` recovers the wall-only part of original blockmap iterator
`0800975C` and intercept callback `08009A88`. It supplies cell collection to
`ray_traversal.c`; wall blocking is evaluated afterward, using the current map
sector heights. This module does not collect entity intersections or establish
visibility by itself.

The geometry arithmetic is shared with the existing verified sliding port:
`ssa_trace_divide` (`0800970C`), `ssa_trace_intercept` (`0800995C`) and
`ssa_trace_side` (`080099F8`). The small-ray branch reproduces `080098CC`, using
the wall's **second** vertex as the side-test origin. When any ray delta is
strictly outside `[-0x100000, 0x100000]`, the two wall vertices are instead
tested against the ray. The vector passed to intersection math (`080099C8`)
uses the first vertex and the preprocessed signed 16-bit wall deltas.

Only fractions satisfying the original unsigned comparison `fraction <= 65536`
are appended. This includes both endpoints and excludes negative fractions.
Records retain blockmap encounter order. The original append has no bounds
check; native storage returns an error before overflowing the supplied capacity.
The original iterator's per-wall validcount stamp is represented by a separate
visited byte array. It is marked before callback processing, including failures.
Out-of-grid cells succeed without touching visits or records. Repeated cells
within one traversal do not append the same wall again.

Zero-initialize `SsaRayWallCollector`, call `ssa_ray_walls_init` once, then call
`ssa_ray_walls_begin` before every traversal. The begin operation only clears
visited marks; traversal resets its own output records. Pass the collector as
context for `ssa_ray_walls_cell`, or use `ssa_ray_walls_line` for a single wall.
Keep map storage and layout stable, and free the collector before the map.
Reinitializing populated storage and same-collector reentry are rejected.
Errors may retain earlier records and visited marks; begin a new traversal
before retrying. Sector heights may change between queries without rebuilding
this collector, since collection reads vertices, wall deltas and blockmap only.

`tests/test_ray_intercepts_rom.py` compiles with UndefinedBehaviorSanitizer and
compares the real original routines with the native module across all 16 maps:

- 9,600 direct wall callback comparisons, including exact threshold deltas and
  rays starting at vertices.
- 6,400 complete blockmap iterator comparisons plus 6,400 repeated-cell
  comparisons with original validcount retained.
- 480 additional native duplicate-line checks and 256 native guard checks,
  including insufficient capacity, reentry, repeated initialization, truncated
  maps, invalid vertices/line IDs, bad cell offsets and impossible dimensions.

There are **22,400 ROM comparisons**, covering 5,490 appended intercept records.
The original copied ARM multiplication and division routines execute unchanged
in the test oracle. No geometry or iterator callback is stubbed. Results and
source hashes are in `native-ray-intercepts-validation.json`. These are native
behavioral implementations, not new exact-match claims. The original ROM is
unchanged; Unicorn is used only by the test process.
