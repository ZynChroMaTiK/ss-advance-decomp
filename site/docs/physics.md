# Native vertical physics, camera and jumping

The user requested a new jump feature in addition to the ongoing native PC
port. `src/physics.c` now implements original vertical and camera behavior,
plus an explicitly separate native jump extension. This is library code;
there is still no playable game executable or OS keyboard/mouse integration.

## Original vertical routine: 0x08004CE0

`ssa_vertical_step` ports the entire routine, including entity and player state
changes and its return value. Host structures are declared in
`include/ssa/physics.h`; they are not binary overlays on GBA memory.

| GBA entity offset | Host field | Representation |
| --- | --- | --- |
| `+0x08` | `SsaBody.z` | Signed 16.16 |
| `+0x14` | `velocity_z` | Signed 16.16 units per game tick |
| `+0x2C` | `height` | Signed 16.16 |
| `+0x30` | `flags` | `0x10`: player-linked body; `0x100`: skip gravity |
| `+0x43` | `hover_offset` | Signed byte, nonzero selects hover acceleration |
| `+0x53` | `step_adjust` | Signed byte used by camera adjustment |
| `+0x54` | Player association | Index into 256-byte player array in ROM; host passes pointer |
| `+0x70`, `+0x72` | `ceiling`, `floor` | Signed integer map heights |

Player `+0xBC` maps to `SsaPlayerVertical.flags`; `+0xD0/+0xD4` map to
`eye_offset/eye_velocity`; `+0xD8` maps to `lowered`.

The routine updates player floor-contact bit `0x20` before integrating Z.
It adds `velocity_z * ticks`, clamps against the integer floor, changes velocity
for gravity/hovering, then clamps body height against the ceiling. Normal gravity
subtracts 1/4 unit per tick when velocity is nonzero and 1/2 unit when it is zero.
The floor clamp returns zero; otherwise the function returns one. This return
is not a fully current grounded flag, especially after ceiling handling.

The port preserves wraparound arithmetic, integer-height comparisons, the
pre-integration floor flag and the original unshifted eye-offset adjustment.
The matching C candidate is 69.2% (score 4425), not byte-identical; native behavior
is independently tested. It does not increase the exact-match count.

## Camera routine: 0x080062D4

`ssa_camera_step` ports camera bob, eye-offset recovery, lowered eye position,
step adjustment and floor/ceiling clamps. It reads entity horizontal velocity
and the frame counter. Its output is `SsaCamera.eye_z` and bob amplitude.
The original player fields are `+0x18` and `+0xE0` respectively.

`SsaCameraFrame.previous_bob` replaces global `0x03006AA4`. The caller supplies
whether this is the local player and whether footsteps are muted (originally
the setting at `[0x0300572C]+0x2C`). The function returns sound event 13 or 14
at the corresponding bob sign crossings, or zero for no event. Actual audio
playback is not implemented here. Original math tables are loaded through
`ssa_math_tables_load` from the user's identified ROM.

Some original camera calculations mix integer and fixed-point units. They are
preserved and tested here, including the `entity.z <= entity.floor` step test
and the assignments after ceiling/floor comparisons. These deserve explicit
review during native game integration, not silent correction in recovered code.

## New native jump behavior

Initialize an `SsaJump` with `ssa_jump_init`. Each simulated update calls
`ssa_player_vertical_step` with the current held jump action, tick count and
whether controls are enabled. The intended PC binding is Space; an OS event
backend has not been added, so no keyboard binding is active yet.

- A fresh press launches only a player body exactly on its floor with no upward
  velocity, enough headroom, enabled controls and no player control-lock flag.
- Hovering and gravity-disabled bodies do not jump. No midair jump, input buffer,
  coyote time or automatic jump on landing is implemented.
- Holding the action does not retrigger. A press consumed while disabled or
  airborne does not become a jump after landing or re-enabling controls.
- Default launch speed is 5 units per game tick. With original gravity and open
  headroom the tested apex is 52.5 units above the floor. It is tunable via
  `launch_velocity`; nonpositive values disable jumping and values above 30
  units per tick clamp to 30.
- Vertical simulation uses one-tick substeps. This deliberately differs from
  the original batched update and makes one update of N ticks equivalent to N
  one-tick updates with the same held input and unchanged map heights.
- Exact fixed-point ceiling contact stops upward motion. The current floor flag
  is refreshed after physics. These are intentional native changes.
- Zero ticks leave state and the input latch untouched. Horizontal position and
  velocity are preserved. Camera updating remains a separate call so the
  caller can order it after horizontal and vertical movement.

Floor/ceiling values must come from the eventual native collision query. The
jump routine does not manufacture level geometry or bypass collision. A test
walks a complete jump while calling the recovered camera routine and verifies
that eye height follows body Z through takeoff and landing.

## Validation

Run `.\decomp.ps1 test-physics` from PowerShell. It builds native C with GCC
warnings-as-errors and undefined-behavior checks, then executes:

- 22,471 full original vertical routine comparisons, including all changed and
  unchanged bytes in the entity and four player slots. No calls are mocked.
- 9,096 camera comparisons. Original trigonometry and IWRAM multiply code execute
  in the test oracle; only audio playback is replaced by an event recorder.
- 36 native jump scenarios covering complete arcs, release/repress, rejected
  midair inputs, ceiling contact, pause, disabled controls, speed configuration,
  batched-tick equivalence, negative floors and camera following.

Evidence and source hashes are in `analysis/native-physics-validation.json`.
Unicorn is used only for tests, never by the native libraries.

## Horizontal collision trace

Update: static map queries and clearance/move commits are now native and
verified on all sixteen maps; see `analysis/geometry.md` and `test-geometry`.
Jumping is additionally tested while walking from every original player start,
with the original 32-unit radius and 100-unit height. Dynamic entity collision,
wall sliding and horizontal timestep integration are now implemented separately
in the world layer; see `analysis/world.md` and `analysis/sliding.md`.

`0x08004BDC` iterates horizontal movement using `0x080043D0`, with sliding via
`0x08004A38` or independent axis attempts. `0x080043D0` queries geometry through
`0x08003E5C`, checks body clearance and a 48-unit maximum step, removes/reinserts
the moved entity via `0x0800A65C`/`0x0800A5D0`, then updates its floor and ceiling.

The query at `0x08003E5C` builds a swept bounding box using entity radius
`+0x28`, calls map position lookup `0x0800B370`, and reads an 8-byte record from
the map's `+0x2C` array. Its halfword at `+4` indexes a 20-byte record in the map's
`+0x24` array, whose first two signed halfwords provide initial floor/ceiling.
It then visits spatial cells via `0x08009870` (callback `0x08003D7C`) and
`0x0800975C` (callback `0x08003C6C`). Cell coordinate shifts are 23 bits from
16.16 world coordinates, indicating 128-unit cells. The wall callback and map
loader are now native. Entity collision and host spatial lists are also now
available through the world layer; special gameplay handlers remain dependencies.

Observed query globals: `0x0300434C` floor, `0x0300432C` ceiling,
`0x03004444` initial floor and `0x03004328` collision classification. The
classification can affect return/slide behavior; it must not be reduced to a
boolean without inspecting its producers.
