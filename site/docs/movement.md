# Recovered player movement

This records the AENP ROM evidence behind `src/movement.c`. The native code is
a subsystem, not a playable port. It does not execute ARM instructions.

## Input path and update order

1. `0x08003028` polls the keypad and writes processed input to `0x030042F4`.
2. Frame routine `0x0800244C` copies that word to `0x03003F84` at `0x08002458`.
3. `0x08005898` distributes input. Its single-player path starting at
   `0x08005C26` walks four 256-byte player slots, assigning the word to `+0xA4`
   for the local slot with flag `+0xBC & 4`. The slot-array pointer is at
   `0x03004648`; the local index is game-state `+0x68`.
4. Entity dispatcher `0x08004E04` calls player update `0x08005CCC` for player
   entity types. That update invokes `0x08006154`, movement `0x080064B8`, then
   camera/eye-height routine `0x080062D4`.
5. The local camera at `0x03004020` receives player X/Y, eye Z and heading in
   `0x08005D48..0x08005D60`.

The game-state pointer is stored at `0x03004010`. Its byte `+7` is the movement
tick increment; `+0x10` is player count. These uses are established by code,
but the complete state structure and frame scheduling remain unrecovered.

## Player fields used by movement

| Offset | Observed meaning |
| --- | --- |
| `0x00`, `0x04`, `0x08` | X, Y, Z, signed 16.16 positions |
| `0x0C`, `0x10`, `0x14` | Entity-compatible velocity components |
| `0x18` | Camera eye Z |
| `0x20` | External impulse magnitude |
| `0x24` | Turn velocity; low 32-bit product with ticks, arithmetic shift by 16 |
| `0x30` | Flags in weapon state; bit `0x100` skips turn acceleration/decay |
| `0xA4` | Held buttons and newly pressed edges |
| `0xAC` | Entity pointer |
| `0xB0`, `0xB4` | Forward and strafe speed, signed 16.16 |
| `0xB8`, `0xBA` | View heading and movement heading, 65536 units per turn |
| `0xBC` | Player flags; bit `0x10` suppresses control acceleration |
| `0xC2` | B-held tick counter |
| `0xD0`, `0xD4` | Camera eye offset and its change per update |
| `0xD8` | Nonzero selects a lower eye position |
| `0xDC` | External impulse heading |
| `0xE0` | Camera bob amplitude |

The native structure uses named fields, not a packed GBA memory overlay.

## Preserved control behavior

Heading integrates the previous turn velocity before that velocity changes.
Left/right accelerate turning, with left priority when both are held. Forward
has priority over backward. L/R select strafe direction; simultaneous shoulders
preserve the previous direction bits unless B is held. Pressed shoulder edges
also influence selection. B changes acceleration and a counter whose release
can set player flag `0x40`. These details should not be simplified before the
remaining action handlers have been recovered.

Forward/strafe speeds normally clamp to +/-8 units. Original B+forward behavior
adds 1/8 unit per update, then snaps to 1 unit only if speed exceeds 8; it does
not simply clamp to 1. Turn deceleration is asymmetric. These oddities pass
the ROM comparisons and are deliberately preserved.

The impulse path at `0x080069B2` replaces the strafe temporaries with scaled
impulse, then adds the unscaled impulse direction. This is what the original
instructions do, including for nonzero strafe. It is not corrected in this port.

## Math and table data

`0x080094E8` produces sine and cosine in signed 16.16. It uses 16384 unsigned
halfwords at ROM offset `0x1C930`; exact quadrant boundaries use +/-65536
directly. `0x080093E8` obtains a heading from a direction table at `0x24930`.
The native direction API covers signed 16-bit axes, as used by movement after
shifting velocity. It does not yet implement that original routine's optional
magnitude output or general 32-bit input behavior.

`ssa_math_tables_load` reads these data tables from the supplied ROM; the caller
must identify the AENP image. It performs bounds checks but is not a ROM identity
validator. No proprietary tables are embedded in the native source.

## Validation and limits

`tests/test_movement_rom.py` compiles native C with undefined-behavior checking
and compares against original Thumb instructions and the original ARM fixed
multiply copied to IWRAM. Only the BIOS integer division call is stubbed using
signed division/remainder semantics. It tests all 65536 trig headings, 4177
direction vectors, and 36864 movement cases including 4096 consecutive frames.
The generated evidence is `analysis/native-movement-validation.json`.

Movement comparisons use one player and stop at `0x08006A3E`, immediately before
entity synchronization and collision. They cover the actual pre-collision
control/vector phase, not position integration or a complete movement tick.
This original phase API accepts an already computed impulse. The later complete
routine in `player_motion.c` now calculates multiplayer pushing and connects
position integration; see `player-motion.md` for its separate validation.

The matching trig candidate is 82.0% (score 1940), not byte-identical. Its
behavioral port is separate from that matching candidate. Twenty bounded
compiler trials were recorded; no new exact matches are claimed.

## Next dependencies

- `0x08005E04` copies player position/velocity to an entity and heading's high
  byte to entity `+0x3F`. `0x08005E28` copies it back, shifting that byte by 8.
  Thus the original full update quantizes heading to 256 steps after collision.
  Both copies are now exact C matches. The new native runtime deliberately
  retains finer heading precision; NULL motion options preserve original behavior.
- `0x08004BDC` clamps horizontal velocity to +/-30, iterates game ticks and
  calls collision/position routine `0x080043D0`; failed movement invokes sliding
  logic including `0x08004A38`. These stages and native world integration are
  now recovered; see `world.md` and `sliding.md`.
- `0x08004CE0` vertical physics and `0x080062D4` camera behavior are now ported
  in `src/physics.c`, along with a separate native jump extension. See
  `analysis/physics.md` for verification and integration limits.
- Renderer, resource loading, game loop, audio, OS keyboard/mouse events and
  the native executable still need implementation.
