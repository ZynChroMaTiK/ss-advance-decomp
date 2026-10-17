# Native wall ray traversal

`src/ray_traversal.c` recovers the flags=1 wall traversal of `0800A240`
and ordered intercept processing `08009C00`. The former was previously embedded
in the slide-specific query; this API exposes cell collection and ordered
predicates for enemy navigation. Original fixed division `0800970C` is reused
through the already verified `ssa_trace_divide`.

The query nudges either starting coordinate by one 16.16 world unit when it lies
exactly on a block boundary relative to the supplied blockmap origin. The ray's
delta is measured from that adjusted start. Cells are 128 world units. Stepping
uses the original wrapped arithmetic, fixed multiplication and division,
X-before-Y comparison, and a maximum of 64 cell visits. A nonadvancing step can
visit the same cell repeatedly; do not replace the loop with a conventional DDA
that handles ties or rounding differently. Off-map cells are passed to the
collector, which decides whether they contain anything.

`ssa_ray_traverse_intercepts` repeatedly scans for the signed lowest fraction,
preserves insertion order for ties, and includes fractions equal to the maximum.
An accepted entry is consumed by replacing its fraction with `INT32_MAX`.
Rejection saves the fraction in `blocking_fraction` and leaves that entry
unchanged. Clear queries preserve the prior blocking fraction. This deliberately
models the original destructive list rather than sorting a copy. The native
API guards the original undefined situation where maximum is `INT32_MAX` and
every remaining entry has that sentinel value.

Use stable caller-owned intercept storage and a zero-initialized
`SsaRayTraversal`; capacity can match the loaded map's line count. Trace resets
the count, not the caller's line-visited flags. The collector may append entries
within capacity and returns 1 to continue, 0 to block, or a negative value on
error. A predicate may inspect state but must not mutate/reenter its workspace.
Both entry points reject reentry. Errors preserve earlier effects and release
the busy flag. There is no entity collection or generic weapon ray behavior.

`tests/test_ray_traversal_rom.py` executes complete original routines with
controlled cell contents and predicates. It compares every visited cell, ray
field, intercept fraction, callback order, result and blocking fraction:

- 6,000 original `9C00` comparisons, including signed extremes, stable ties,
  cutoff boundaries, empty lists and early rejection.
- 5,000 complete `A240` comparisons, including block-boundary nudges, degenerate
  rays, wrapped coordinates, no advancement, the 64-cell limit and collection
  early exits.
- 214,169 callback reentry checks, four native guards and two callback errors.

The test builds native C with UBSan and executes the original fixed arithmetic
in Unicorn. Unicorn is test-only. Source hashes and counts are recorded in
`native-ray-traversal-validation.json`. Full real-map collection and the enemy
wall predicate are tested separately in `enemy-ray.md`.
