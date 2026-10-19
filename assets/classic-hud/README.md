# Classic HUD symbol reference

`time-strike-reference.png` is the user's exact supplied icon, copied unchanged
from `C:\Users\brian\Desktop\download.png` (SHA256
`051fa06923edaafc8b68af4441426fcd774b25d325fc24fa8076f79e27df46c8`). It is16x24,
with opaque artwork only in the first17 rows. Its frame, device and transparency
are used pixel for pixel; only colors change to the HUD's green ramp. Regenerate
`src/classic_time_strike.inc` with `scripts/import-classic-time-strike.ps1`.
The generated pixels retain original RGB555 colors and alpha; the game applies
the green palette at draw time. There is no runtime PNG dependency.

`biohazard-reference.svg` is the public-domain biohazard symbol by Silsor and
subsequent Wikimedia Commons contributors. Source and public-domain dedication:
[Biohazard symbol.svg](https://commons.wikimedia.org/wiki/File:Biohazard_symbol.svg).

The Prion ammo engraving in `src/classic_hud.c` uses a nine-by-nine coverage mask
of this geometry. Each pixel averages eight-by-eight samples across the SVG's
52-by-52 view box and rounds to 0..9. Coverage below 2 is omitted, 2..3 darkens
the green by 4/9, and 4..9 is black. This hints the small tips and openings while
keeping the standard three crescents and center hole.

Both Prion and Time Strike ammo icons copy ROM sprite88 as their shaded circle
base. Its circular interior expands from11x11 to13x13 with centered sampling,
matching UMT's larger badge while keeping the16x16 frame and centered symbols.
They use the original OBJ green ramp, brighten the circular face for
contrast, and engrave the symbols. The frame, silhouette and source ROM assets
remain intact. The clock mask is original native UI artwork. The SVG is a
design reference; the game uses the embedded masks and has no SVG dependency.

`tommygun-ammo.png` is the user's16x16 edited Tommygun ammo badge. Its exact
palette, pixel layout and alpha are embedded by `scripts/import-classic-tommy-ammo.ps1`
in `src/classic_tommy_ammo.inc`. The dynamic meter is drawn separately. Keep
native dimensions, original OBJ palette and binary alpha when editing, rerun
the importer and rebuild. There is no runtime PNG dependency. Do not overwrite
the user's source image with an export of older artwork.
