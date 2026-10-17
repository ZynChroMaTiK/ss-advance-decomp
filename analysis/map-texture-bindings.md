# Native map texture bindings

`src/map_texture_bindings.c` recovers the complete `0x0800B0C4` binding phase,
including the behavior of its lookup helper `0x0800B0FC`. The original map
loader `0x0800ACC8` calls B0C4 after relocating the decoded map's 16-byte texture
descriptor table. Lookup uses the selected base archive at `WORLD+4`.
The texture archive loader and mip association are separate native code in
`texture_assets.c`; see `texture-assets.md`.

For each map descriptor, B0FC finds the first archive name equal under the
original case-insensitive comparison at `0x080C57F4`. Comparison stops at the
first NUL or eight bytes, folds ASCII uppercase and leaves high bytes unchanged.
A match copies archive width, height and the pixel pointer, then returns the
archive index as a signed 16-bit value. B0C4 immediately overwrites the temporary
pixel pointer with that sign-extended index. A missing name instead writes
index zero and leaves the descriptor's incoming dimensions untouched. Native
code retains the final index, so it does not need a transient GBA pointer.

All 854 original map descriptors initially have zero dimensions. With each
map's original archive family selected, 185 descriptors do not match; examples
include numeric color names, `CEIL3_5` and `FLAT1`. None of those missing entries
is referenced by an original wall side. Every non-absent wall reference on all
16 maps resolves to nonzero dimensions. The missing-name fallback is preserved,
not replaced with invented dimensions or artwork.

## Native ownership and live references

Zero-initialize `SsaMapTextureBindings`, then call
`ssa_map_texture_bindings_init(bindings,map,assets,family)`. The original family
choice is available through `ssa_texture_assets_family(level)`: levels 0..5,
12 and 13 use family zero; levels 6..11, 14 and 15 use family one.
Initialization owns only a copy of the bound descriptors and leaves decoded
map data unchanged. Free the binding before its borrowed map or texture assets;
both addresses and their owned arrays must stay stable while bound. Initialization
is atomic on failure and rejects a populated output.

`ssa_map_texture_record` resolves a map descriptor index.
`ssa_map_texture_image` resolves its bound archive index through the original
three-mip association. The low-level binder preserves signed index behavior
at 32767, 32768 and 65534; the image accessor rejects a negative result before
attempting a host dereference. Actual archives are far smaller.

`ssa_map_side_texture` reads the current signed halfword at side +4, +6 or +8
(upper, middle or lower). The original wall renderer's BE60 routine uses these
fields at BF84, BFE2 and C040 and skips `-1` as absent. The native helper returns
1 with a borrowed bound record, 0 with NULL for that absent sentinel, and -1
for invalid input with the output unchanged. It bounds-checks the side section,
side index, layer and descriptor index. Reading the current side matters because
native switch activation changes the middle-texture index at +6.

There is deliberately no cached sector/side-to-texture array. The decoded map
and descriptor ordering remain stable, while switch changes are visible on the
next lookup. Renderers must use this live accessor or equivalent current map
reads, rather than retaining an old side texture selection.

## Sector appearance is a separate path

Sector bytes +A and +B are not indices into the map texture table. The original
BA04 routine passes +A for floor and +B for ceiling to A888's plane grouping.
AB10 then copies that plane's +6 value to `WORLD+0x708`. A98C reads the byte:
a nonzero value goes to the span-fill routine at relocated RAM address
`0x030005AC`; zero selects sky data at `0x0846285C` or `0x0846DC5C` according to
the level family and goes to the span-copy routine at `0x030005BC`.

These are flat-color/sky selectors. Many original bytes exceed the map texture
count; treating them as texture IDs would be wrong. This module does not add
sector texture binding, a floor rasterizer or a sky renderer. This trace is
evidence for subsequent rendering work, not a claim that that pipeline is
already native.

## Validation

Run `./decomp.ps1 test-map-texture-bindings`. The driver executes
`tests/test_map_texture_bindings_rom.py` and its sanitizer C integration fixture.
`native-map-texture-bindings-validation.json` contains the unchanged ROM SHA-1,
per-map/family descriptor and missing-name counts, totals and source hashes.

The completed run passes:

- 1,059 whole original B0C4 calls, including original B0FC and C57F4 with **no
  instruction hooks**, comparing 10,910 complete 16-byte records.
- Both archive families on every one of the 16 maps, plus 1,024 synthetic cases
  covering missing names, duplicate first-match ties, NUL tails, high bytes,
  unmodified name bytes, arbitrary dimensions and empty arrays.
- Original signed archive-index boundaries 32767, 32768 and 65534.
- 59,778 live side lookups, 5,124 image/mip lookups, 160 live map-reference
  mutations and 324 malformed-input/guard cases in the Python/native suite.
- 59,951 ASan+UBSan assertions over all 16 maps, including 29,889 additional
  side lookups, **8 actual native switch activations** with observed texture
  changes, empty-map handling and 33 additional guards. Script enqueue and
  sound callbacks observe those switch requests; they do not execute scripts
  or play audio in this fixture.

Native builds use warnings as errors. The shared-library oracle checks use
UndefinedBehaviorSanitizer; the standalone fixture uses AddressSanitizer plus
UndefinedBehaviorSanitizer with `-no-pie` and `detect_leaks=0`. No sanitizer
error occurred. Unicorn executes isolated original routines only in tests and
is not a game runtime dependency. No extracted artwork is written to source.

## Exact matching

The requested local decomp.wtf engine confirmed **B0C4 byte-identical: 54 bytes,
27 instructions**, on the fourth manual `compile_and_diff` attempt.
`recovered/map_texture_bind.c` is the matching candidate;
`map-texture-bind-match-attempts.json` retains all attempts. An explicit signed
halfword temporary and the unsigned `i < map->count` initial condition reproduce
the original loop and register allocation. Recovery notes were added through
the real engine. The root workflow owns export and full-ROM rebuild verification.

This completes bounded map wall-texture binding and its original routine proof.
It does not implement visibility, projection, wall sampling, sector drawing,
actor rendering or a playable executable.
