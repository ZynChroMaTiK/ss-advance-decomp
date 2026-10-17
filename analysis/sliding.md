# Original wall sliding

`src/slide.c` ports the original horizontal wall-sliding routine at `08004A38`
and its wall-trace dependencies. It uses decoded map geometry and the original
trigonometric tables through native C. No ROM instructions execute at runtime.

## Recovered behavior

| Original address | Native operation |
| --- | --- |
| `0800970C`, ARM `08000430` copied to `030003A4` | Signed 16.16 division, retaining original 32-stage quotient behavior |
| `080099F8` | Fixed-point trace-side classification with sign shortcut |
| `0800995C` | Wall/trace intersection fraction |
| `080098CC` | Wall-side test using the second vertex as origin |
| `08009A88`, `080099C8` | Wall intersection collection |
| `0800A240`, `0800975C` | Wall-only blockmap trace and ordered line visitation |
| `08009C00`, `08004894` | Stable nearest-first interception and slide blocking policy |
| `08004930`, magnitude branch of `080093E8` | Wall-parallel velocity projection |
| `08004A38` | Three-corner traces, projected retries and axis fallback |

The tracer retains the original 64-cell traversal limit. A start exactly on a
128-unit block boundary moves one map unit along that coordinate before tracing;
the destination remains unchanged. Walls are visited once in original blockmap
order. The algorithm switches its side-test method when either trace component
exceeds 16 units in magnitude. Only fractions in `[0, 65536]` are retained.
Equal-fraction interceptions retain insertion order when processed.

The original generic tracer also supports entities. This implementation covers
its `flags=1` wall-only path, which is the path used by sliding. Entity blocking
is still applied by the supplied move callback. The trace workspace is separate
from the gameplay query workspace and does not overwrite pending contact lists.

Sliding traces the leading corner and the two adjacent corners. One-sided walls
block only from the original facing side. Two-sided openings block if the body
does not fit, its top would hit the ceiling, or the rise exceeds 48 units.
The earliest blocking fraction across the three traces controls the move.

The body first moves up to 1/32 of a tick before the wall, when that distance is
positive. The remaining velocity is projected along the wall. Axis-aligned
walls clear one component directly. Diagonal projection first reduces velocity
components to signed whole units, then uses the original direction, magnitude
and trigonometric calculations. This quantization is original behavior, not a
smooth floating-point projection. Two projected attempts are allowed; subsequent
failure tries Y-only, then X-only movement. These fallback attempts do not clear
velocity components themselves.

The source uses explicit wraparound arithmetic. Division preserves even the
original divide-by-zero and overflowing-quotient results; it is not replaced
with a saturated or floating-point divide. This matters to trace boundary cases.

## Integration

Zero-initialize `SsaSlide`, call `ssa_slide_init` with a loaded map, and retain
the map and its layout until `ssa_slide_free`. Sector heights may change.
`ssa_slide_move` receives a body, radius, math tables and a callback:

```c
int try_move(void *context, SsaBody *body, int32_t x, int32_t y);
```

The callback must perform the actual collision check and position commit and
return 1 for success, 0 for blocked, or -1 for invalid geometry. The world layer
can therefore supply entity collision and spatial bookkeeping without a library
dependency cycle. Errors stop further movement but do not roll back earlier
committed moves. The caller must apply the original horizontal timestep policy,
including its velocity clamp, before entering this routine.

This does not implement sector actions, doors, damage, rendering or OS controls.
It is a native behavior port, not a byte-identical compiler match.

## Validation

`tests/test_slide_rom.py` builds with GCC warnings-as-errors and undefined-behavior
checks and compares against the supplied ROM:

- 5,000 signed fixed-division cases, including zero divisors and integer limits.
- 5,000 trace-side cases, including axis-aligned and full-range integer inputs.
- 5,000 intercept-fraction cases.
- 4,000 wall-projection cases over all sixteen maps, including integer limits.
- 4,000 complete wall traces over all sixteen maps, checking ordered contacts,
  exact block boundaries, short/long traces and out-of-map cells.
- 6,400 complete `08004A38` slide cases over all sixteen maps. These compare the
  whole entity byte image, every proposed move in order, velocity at each move,
  and the number/order of spatial unlink/relink calls.
- 64 native invalid-input/workspace guards.

Total: **29,400 ROM comparisons and 64 native guard checks**. Branch counters
verify projected movement, partial movement before projection, retry and axis
fallback all execute. The full-slide oracle has an empty dynamic entity grid;
only spatial unlink/relink functions are hooked. Original fixed multiplication,
fixed division and trigonometric code execute directly; the BIOS integer-division
wrapper is serviced by the test harness. Dynamic-world integration is validated
separately by that layer.

Evidence and source hashes are in `analysis/native-sliding-validation.json`.
