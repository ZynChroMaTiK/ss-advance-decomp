# Respawn-point selection

`src/respawn_select.c` recovers the complete `08001B18` selector. It reads the
existing `SsaSpawnMarker` array and fixed-point player positions, and picks the
marker with the greatest nearest-player distance. This is the selector called
by the original multiplayer respawn routine `08005FEC`.

Each marker's score starts at 10,000. Players whose X and Y are both exactly zero
are ignored. Positive ties retain the first marker; if no score is positive,
the original returns zero and leaves the output unchanged. The output's Z
copies the selected marker's Y, not its Z. Respawn subsequently replaces that Z
with the entity's old floor height, so correcting the selector alone would not
fix or improve spawn height behavior.

The original distance helper receives differences of signed whole coordinates,
which can span -65,535 through 65,535. This is wider than the existing
`ssa_direction` halfword API. The selector implements the full-width calculation
using the original ROM direction and sine tables. Shifting the dominant axis
left 16 wraps before signed division: distances can become negative at extreme
coordinate differences. Native code deliberately preserves this behavior.

`ssa_respawn_select` returns 1 for selection, 0 for no positive candidate, and
-1 for invalid pointers or a player count greater than four. Output is unchanged
on 0/-1. Zero players and zero markers are supported, matching the original.
Output may alias a marker. The caller must provide valid arrays and loaded math
tables; this is not a spawn or world insertion operation.

Run `decomp.ps1 test-respawn-select`. The test executes the complete original
selector and its actual direction/trigonometry instructions, servicing only
the BIOS signed-division boundary. It compares return values and complete
output, marker, player, and game buffers. **4,998 ROM comparisons** cover
coordinate extremes, nearest-player selection, ties, caps, co-location,
zero-coordinate skipping, sub-unit positions, seeded random layouts, and
output aliasing. Five native API guards pass with undefined-behavior checks.
See `native-respawn-select-validation.json` for the source hash and evidence.
