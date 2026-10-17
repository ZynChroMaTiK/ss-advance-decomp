# Native enemy motion and environmental damage

`src/enemy_motion.c` recovers five original helper entry points. These are
behavioral ports, not newly exact-matched functions or a complete enemy AI.

| Address | Native operation |
| --- | --- |
| `08004498` | Directional walking, then vertical physics |
| `08004614` | Turn facing toward desired facing |
| `0800456C` | Set desired facing toward an entity |
| `080045C4` | Table-based distance between entities |
| `0800354C` | Kill an enemy on lethal floor or closing-sector sample |

Walking increments the original `03004358` counter before any query. It reads
speed from original entity `+24`, computes sine/cosine at `(facing+128)<<8`,
multiplies each by speed with signed 16.16 arithmetic, and scales by the game
tick byte. It attempts the resulting XY move through `080043D0`, then always
executes `08004CE0`, even if the move was blocked. `ssa_enemy_walk_world` connects
that boundary directly to the existing native world move operation. It neither
slides along walls nor uses player input acceleration.

Turning preserves unusual original behavior: its returned delta is measured
before the update, exactly opposite headings retain the signed +/-32768 tie,
and equal headings still subtract the tick count from facing. The second
original argument, often passed as `0x10000`, is unused. The native interface
therefore takes the actual tick count explicitly.

Facing and distance subtract coordinates with 32-bit wrap, then take signed
whole units. Distance uses the original angle table and sine/cosine division;
it is not a Euclidean square-root replacement. An absolute whole-unit component
of 32768 produces a negative shifted numerator, and that signed result is
preserved. `080045EC` has the same instructions as the recovered distance body,
but the validation count below specifically executes `080045C4`.

Environmental damage checks sector material byte +10 for FF, or 23 when the
level counter exceeds five. It requires whole-unit Z to equal the sector floor.
It then independently runs the existing native `03464` moving-sector sampler.
Either match sets health to zero. It does not update player statistics or run
death effects. The caller must handle the following enemy death state.

## Ownership and failure behavior

The motion view borrows stable entity storage and stores speed, facing and
desired facing separately from the body. Callers must synchronize those fields
with their authoritative enemy state before and after each operation. It is
safe to project a controller's fields into a temporary motion view.

Walking's callback may move this entity through the world API. It must not
replace/free the entity or reenter the motion view. Native guards reject
reentry, active world queries, ticks outside the original byte range, and
player bodies. A callback failure retains the counter increment and earlier
effects, clears the busy flag and stops before vertical physics. Callbacks
return only 0 (blocked), 1 (moved), or -1 (native failure).

The world adapter requires the entity to belong to its supplied world. Storage
and map layout must remain stable under the existing world's ownership rules.
The hazard sampler borrows the caller's hazard list and map. Invalid inputs
return -1; a late sample error can occur after floor damage has already cleared
health. No routine implements animation, sounds, combat, target selection or
enemy dispatch.

## Validation

Run `.venv-linux/bin/python tests/test_enemy_motion_rom.py` in Ubuntu/WSL.
The native test library uses strict C11 warnings and UBSan with immediate
failure. Unicorn is used only as the independent original-instruction oracle.

The suite passes **88,226 whole-routine ROM comparisons**:

- 65,536 turning cases, covering every facing/desired pair with varied ticks.
- 8,273 facing and 8,273 distance cases, including extreme wrapped coordinates.
- 4,096 walking cases executing real original trigonometry, IWRAM fixed
  multiplication and vertical physics. Only `043D0` is controlled; ordered
  destination/callback state, complete entity bytes and counter are compared.
- 2,048 environmental damage cases over all sixteen decoded maps, executing
  the real original BSP and hazard sampler with floor-material variations.

The BIOS integer-division service is replaced with its arithmetic contract.
No original game helper is replaced in the facing, distance, turning or hazard
oracles. The original ROM is unchanged.

Another **4,096 native world frames** use actual map starts and registered enemy
bodies on all sixteen maps: 3,435 moves succeeded and 661 were blocked. This
checks native integration separately from the controlled-collision ROM oracle.
There are 31 invalid-input/query guards, 4,097 callback reentry checks and one
explicit callback-failure path. Results and source hashes are recorded in
`native-enemy-motion-validation.json`. These checks do not establish rendering,
OS controls, enemy AI or combat completeness.
