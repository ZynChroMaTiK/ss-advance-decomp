# Native type-64 sector clipping

`src/sector_clip.c` recovers original `D000` wall selection, `CE50` correction
and `053D0` relocation geometry. The shared projectile runtime and original
entity dispatcher now update native type-64 objects through that backend.
Construction requests use the existing native shared-pool constructor. This
closes the previously rejected relocation callback; it does not establish new
campaign coverage or identify additional original type-64 creation sites.

The wall selector scans the current sector's tagged line list, preserving its
order and strict best-distance ties. It uses the original trig tables, signed
64-bit dot product, integer segment quotient, wrapped absolute value and signed
distance. Live heights govern two-sided walls: step equality at48 is allowed;
either opening below100 units forces correction. Correction retains the extra
3-unit bias and original BSP update order. Consecutive repeated tagged walls
terminate the correction loop.

`053D0` adds velocity once without multiplying by elapsed ticks, uses a fixed
32-unit radius, then clamps Z to the destination floor/ceiling without subtracting
body height. The outer type-64 controller already adds velocity times ticks,
so combined displacement retains `velocity * (ticks + 1)`. This path emits no
invented impact, damage or destruction.

Only a nonzero correction unlinks the old sector node before committing XYZ
and reinserts at the new sector's head after clamping. Body floor, ceiling,
step adjustment and block-grid links stay unchanged. Zero correction can still
change geometry sector while retaining the old list node. `ssa_world_sector_move`
supports that distinction; remove/replace locate the actual registered head
when necessary. Release, replacement and teardown cannot leave stale heads.
Ordinary player movement is unchanged; live owners never move in memory.

Scratch is call-local and does not overwrite movement, camera or ray workspaces.
Invalid geometry leaves pure output unchanged; the runtime retains earlier
controller effects on failure. Synthetic original corner cases can exceed1024
iterations. Native calls bound that work and return an explicit atomic error.
Those cases count as guards, not terminating-ROM parity comparisons.

Validation:

- `decomp.ps1 test-sector-clip`: all16maps,4096 wall selections,4011 corrections
  and3941 whole relocations agree with the original:12,048 comparisons. Includes
  1840 ordered unlink/relink pairs,2048 live-height fixtures,48 invalid-input
  guards and240 iteration-limit cases. Only BIOS division and sector-list
  allocation hooks are controlled; original math/BSP execute in the test oracle.
- `decomp.ps1 test-sector-clip-runtime`:2212 GCC ASan/UBSan/leak checks,272 type-64
  frames through native construction/scheduling/animation on all16maps, including
  zero ticks. Sixteen list cases verify retained old heads, replacement, relink,
  block-grid preservation, query guards and release. These are controlled
  fixtures, not campaign acquisition.
- `scripts/test-sector-clip.ps1 -SkipBuild` reproduces2212 checks on MSVC. The
  original world suite and37,132-assertion projectile suite also still pass.

Reports: `sector-clip-rom-validation.json`, `sector-clip-runtime-validation.json`
and`windows-sector-clip-validation.json`. The native game executes no ROM code.

One real decomp.wtf compile of `recovered/sector_relocate.c` reached85.3%, with
scheduling, clamp-store and literal placement differences. It is **not exact**;
the claim was released with notes. Live matching remains74/526 functions,
2152/87648 catalogued bytes(2.46%),523 attempts. Discovery is incomplete.
