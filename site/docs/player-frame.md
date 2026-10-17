# Original player-frame wrapper

`src/player_frame.c` implements the complete control flow of `08005CCC` through
`08005E02`, including the original `08005C7C` and `08005CA4` movement-animation
predicates. The host structures are field views, not GBA RAM overlays or a
complete player/game representation.

`ssa_player_frame_step` performs these operations in original order:

1. In multiplayer, if entity flag `0x80` is set, subtract the game tick byte
   from entity timer `+4E`, wrapping to 16 bits. When its signed interpretation
   is nonpositive, clear `0x80` unless this is the game state's protected-player
   index, then set entity flag `2`.
2. Call death processing (`08006154`), movement (`080064B8`), and camera height
   (`080062D4`) in that order.
3. For the local player, copy cached player X/Y, camera eye Z, heading and
   entity sector into the global local-view state.
4. Call hazards (`08005680`), then the sector script (`0801C624`) using the
   current signed sector ID.
5. If the same entity timer `+4E` is signed-positive, subtract ticks again.
   On expiry, set it to zero and clear entity flag `1`.
6. Switch entity animation state `+3E` from idle `0` to moving `1` when any
   input bit in `0x3C0` is held and player flag `0x10` is clear. Request animation
   `0x66`, duration `4`. Switch moving to idle under the inverse button condition,
   with the same flag restriction, requesting animation `0x65`, duration `4`.
   Other animation state bytes remain unchanged.

The two countdowns intentionally share a timer and can subtract twice in one
frame. The early expiry does not clamp the timer to zero. These details survive
the differential tests, including negative signed values and unsigned wrap.
The protected-player index at game `+68` and local-player index at global
`0300464E` are separate values. Tests vary both independently.

## Native integration

`SsaPlayerFrame` references a stable `SsaEntity` and `SsaMovement`; it holds the
cached X/Y and eye Z used by the wrapper, plus timer and animation state. Stage
adapters must synchronize recovered movement/player/camera state into these
views before returning. For example, movement runs before the camera callback,
and the local-view snapshot must see the camera callback's resulting eye Z.
The local-view snapshot deliberately precedes hazards and sector scripting.

All stage, sector, and animation callbacks are required. They return a
nonnegative value for success or a negative value to abort. Native errors stop
later callbacks and preserve earlier effects; the original has no corresponding
error API. Callbacks may update state, but may not replace/free the referenced
objects or reenter the wrapper. `sector` is read from `SsaEntity.sector`; its
original signed 16-bit meaning is retained at the callback/view boundary.

This recovers scheduling and state transitions. It does not implement death,
damage, hazards, scripts, animation playback, or a complete game loop. Separate
native movement and camera implementations can be connected by adapters; the
test below intentionally controls these dependencies to isolate the wrapper.

## Verification

Run `.venv-linux/bin/python tests/test_player_frame_rom.py` in Ubuntu/WSL.
The test compiles native C with strict warnings and undefined-behavior checking.
It executes the complete original `08005CCC` while hooking only its six explicit
dependency entry points, and executes the original animation predicates without
hooks. Comparisons include full 256-byte player, 116-byte entity, game, local-view
buffers, and ordered dependency inputs. Controlled dependencies mutate timer,
flags, player position, heading, sector, animation state, and game values to
verify the wrapper's reads occur at the correct time.

The passing report in `native-player-frame-validation.json` contains:

- 1,512 boundary frame cases.
- 4,000 randomized frame cases.
- 2,048 consecutive frame cases.
- 4,096 isolated predicate cases, covering every 10-bit button pattern with
  flag `0x10` both clear and set.
- 14 native API checks, including required callbacks and fail-fast behavior at
  every dependency boundary.

Total: **11,656 original-routine comparisons**. These comparisons validate the
wrapper with controlled dependencies, not a rendered or fully playable frame.
