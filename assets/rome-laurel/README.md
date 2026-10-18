# Rome laurel pickup

Requested PC artwork replacing the ankh in Rome maps 6 through 11, 14 and 15
(zero-based, following the original Rome palette family). Egypt maps retain
their decoded artwork. This asset policy does not enable multiplayer gameplay.

- `laurel-20x34.png`: full sprite, original origin (-9, -35).
- `laurel-10x17.png`: distant sprite, original origin (-4, -15).
- `source.png`: transparent artwork from the built-in image generation tool,
  guided by the user's golden laurel reference and the original ankh sprite.
- `prompt.txt`: the generation prompt.
- `manifest.json`: image/resource identity, palette indices and file hashes.

Both final PNGs use the original Rome RGB555 palette at ROM offset `0x454c5c`.
Only the original ankh's ten opaque indices are permitted; index zero remains
transparent. The importer fits the source to the original canvas, filters in
linear light and samples alpha,
maps colors to that palette, and creates the half-size version and column-major
C arrays. Reproduce from the saved source with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/import-laurel-art.ps1
```

`native_pickup_art.c` applies the two arrays to owned decoded resource 63, images
28 and 56, before either renderer borrows the assets. It validates the original
dimensions/origins first. The original ROM, animation, collection trigger, sound,
collision, progression and all other image references remain unchanged. The
compiled game needs no external PNG files at runtime.

Validation initialized all 16 maps and compared every decoded actor image with
the original: only the two image references on each of the eight Rome-family maps
differ (16 replacements; 86,496 unchanged image references). All dimensions/origins and
allowed color indices pass. Four engine captures cover the wreath and original
Egyptian ankh in both renderers. Capture fixtures relocate the pickup and lower
the render camera solely to frame it for inspection. ASan/UBSan and leak checks
pass, as do the existing renderer checks and rebuilt Windows startup/restart.
This does not claim a full campaign replay or completed original-feature parity.
Results and source/capture hashes: [validation report](../../analysis/rome-laurel-validation.json).
