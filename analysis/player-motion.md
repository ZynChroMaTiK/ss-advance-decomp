# Complete native player motion

`src/player_motion.c` now implements the entire `080064B8` control flow using
the earlier native controls/vector math, horizontal collision/sliding and
vertical physics. `ssa_player_motion_step` has one explicit horizontal boundary;
`ssa_player_motion_world_step` supplies the native world implementation.

The original copies six position/velocity words from player to body before
physics, copies them back afterward, and round-trips heading through an 8-bit
body facing. NULL options preserve this quantization. The two copy routines
are `08005E04` and `08005E28`; both now compile byte-identically (36 bytes each).
Matching evidence is recorded separately in `player-sync-match-attempts.json`.

## Multiplayer push and arithmetic

When player count exceeds one and the entity's index matches the game index
at +68, the routine clears its impulse and scans the player array in order.
Each other player's wrapped coordinate difference is shifted to whole units.
The original direction routine estimates distance from the dominant axis and
the sine/cosine table. An estimate below 80 replaces impulse direction and
strength; the **last eligible player wins**, not the nearest or a vector sum.
Original control decay then applies before vector generation.

The magnitude numerator wraps through a signed 32-bit left shift. An axis of
32768 whole units can produce a negative distance and enter the push branch.
The native compatibility path preserves this behavior. It also preserves the
earlier impulse/strafe temporary quirk and heading integration order.

`SsaMotionFrame.local_player` represents game byte +68 used by motion, not the
separate local-view byte at `0300464E`. These may differ. `slot` identifies the
current player's record in the supplied peer array. Counts are bounded to the
original four player records. The array and entity storage must remain stable.

## Host ownership and extensions

`SsaPlayerMotion` composes the existing movement state with position, Z velocity
and body facing. `movement.flags` is authoritative and synchronized with the
vertical player's mirror before/after physics. A world call requires matching
player/body X/Y and registered world ownership; otherwise copying coordinates
would invalidate spatial links. Callback errors can follow controls/body
updates and do not roll them back. The caller must handle errors before using
partially synchronized state again.

Non-NULL options make two requested native changes explicit: jump substeps and
retention of all 16 heading bits for mouse yaw. These are not claimed as ROM
matches. `player_runtime.c` composes them with initialized players, frame
scheduling, camera height and H-toggle view; see `player-runtime.md`.

## Validation

`test-player-motion` passes 8,672 complete original-routine comparisons:
624 boundary/ordered-peer cases, 6,000 randomized cases, and 2,048 consecutive
frames. Only horizontal `08004BDC` is controlled; original math, copy routines
and vertical physics execute. Full player/body bytes, callback entry state,
and untouched peers are compared. Fourteen native guards cover pointers,
peer lists, counts and callback failures.

`test-world` adds 2,048 complete `080064B8` frames on all 16 real maps, with
actual original/native controls, grid collision, wall sliding and vertical
physics. Original sector-pool operations and the special-contact handler remain
recorders; BIOS integer division is serviced by the test oracle. Full player
state, body fields, spatial links and ordered contact calls match. These cases
are included in `native-world-validation.json`, separately from the 8,672
generic-boundary cases in `native-player-motion-validation.json`.

Strict GCC C11 warnings and UndefinedBehaviorSanitizer are enabled. These are
simulation tests, not evidence of rendering, OS input or a complete game loop.
