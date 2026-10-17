# Original world framebuffer fills

`src/raster_fill.c` ports three complete original ARM routines:

| ROM | IWRAM copy | Operation |
| --- | --- | --- |
| 08000338 | 030002B4 | Solid column, destination stride120 |
| 08000348 | 030005AC | Solid contiguous row |
| 08000358 | 030005BC | Forward contiguous byte copy for sky spans |

07014 installs these routines. A98C calls the row fill with the plane's nonzero
palette index; a zero plane selector chooses sky image data and calls the copy.
Floor and ceiling sector bytes+A/+B therefore are color/sky selectors rather
than map texture descriptor indices. These routines write indexed bytes; an
index0 explicitly passed to a fill is written normally. Visibility, span
generation and sky coordinates belong to the surrounding renderer.

The native API accepts caller storage, capacity, offset and count. Columns
retain the original fixed120 stride. The whole destination/source ranges are
validated before writes. A count0 is rejected because the original ARM loops
would wrap their counters and overwrite memory. The sky operation deliberately
copies forward one byte at a time: overlapping source/destination ranges retain
the original propagation behavior rather than host memmove semantics.

`test-raster-fill` executes all three original ARM routines without any hooks.
All6,189 complete framebuffer comparisons pass with undefined-behavior checks:
2,563 row fills,2,563 column fills and1,063 sky copies, including819 overlapping
spans. All256 palette indices, exact destination ends, stride preservation,
random sky spans and22 invalid-bound cases are covered. Full buffer comparison
checks that bytes outside the target remain intact. The report records source
hashes in `raster-fill-validation.json`.

These are native raster building blocks, not a complete scene renderer or
playable build. They do not choose visible walls/planes, project actors, apply
palette effects or present a framebuffer in a window.
