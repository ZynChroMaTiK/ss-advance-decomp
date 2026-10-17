# Portable PC input and native frame integration

`src/pc_input.c` is a new native keyboard/mouse event accumulator.
`src/pc_input_runtime.c` connects its samples to the recovered native frame
sequence through the existing external POLL and DISTRIBUTE stages. These are
PC extensions, not recovered or byte-matched GBA routines. The adapter uses
the separately recovered local branch of `0x08005898`; its ROM differential
evidence belongs to `analysis/input-dispatch.md`.

The portable event layer accepts case-insensitive W/S/A/D, Space, H and E,
left mouse button changes, both relative mouse axes and focus changes.
At heading zero, W/S produce positive/negative Y movement; A/D produce
negative/positive X strafe without turning. Opposing movement keys cancel.
Space drives the existing native jump extension. H toggles the existing camera
extension once per fresh press; key repeat does not toggle it. E supplies the
original use edge `0x10000`, not the original movement modifier bit 2.

Mouse-left produces distinct held and pressed weapon inputs. A press and
release between samples retains one frame of both inputs. This matters because
the recovered weapon controller checks held fire before starting its animation.
The integration verifies a short click through the actual weapon-10 controller:
the first frame starts firing, the second creates the projectile and increments
the shot counter even though the button is already released. A held mouse
button also fires the native startup weapon on every map.

Horizontal mouse sensitivity is a positive integer number of heading units per
count; the tests use 64 as the ordinary value. Full heading range is 65,536.
Accumulation wraps modulo 32 bits without C signed overflow, and the native
player heading subsequently wraps modulo 16 bits. Boundary tests cover
`INT32_MIN`, `INT32_MAX`, positive/negative counts and large sensitivities.
Vertical counts use the same sensitivity with normal mouse orientation and
saturating signed accumulation. The player clamps pitch to +/-80 degrees;
disabled input and focus loss clear pending pitch. See [free look](free-look.md)
for the perspective renderer and direct weapon aiming integration.

## Control gates and frame contract

Initialize the adapter with stable input and game runtime addresses and an
explicit PC recipient index. This index corresponds to original GAME+68 and is
independent of the display-local player. Free the adapter before either borrowed
object. Calls are single-threaded. Events may be delivered outside a frame or
inside external POLL; sampling and distribution belong only in their matching
external stages. Events delivered after POLL has sampled are retained for the
next frame. Duplicate sampling/distribution in one accepted frame is rejected.
The native game step serial permits another poll after a failed frame, including
a failure before the original GAME counter increment. Distribution also checks
that the sample belongs to the current accepted step; skipping POLL after an
earlier failed frame cannot reuse its sample.

Pause, global input mode 3 and an explicit host disable consume pending action
edges and yaw. Held jump, H, use and fire must be released before becoming
active again. Held movement may resume. Disabled/paused jump presses do not
become new jumps on resume. Focus loss clears held and pending controls, while
preserving the selected camera mode. Focus gain synthesizes no actions. A
normal Space repeat retains its held state; an isolated repeat after losing
state cannot synthesize a new action edge.

The original player active bit (`BC & 4`) also gates PC input. Inactive players
retain all caller-supplied input fields, including yaw, jump and weapon inputs;
the adapter does not force a PC sample into a slot the original local branch
would skip. Focus loss or explicit disable after sampling immediately clears
the cached sample and processed poll word, even if focus/control is restored
before distribution. Distribution checks effective policy again, so clearing
the active bit between the two stages cannot apply the cached actions.

The input distributor preserves the original local/bypass gate: a single-player
game, original GAME flag `0x100`, or a nonzero exit flag. Genuine multiplayer
without that branch returns `SSA_INPUT_DISPATCH_NETWORK_REQUIRED` (`-2`) before
changing player inputs. The outer frame returns `-1`, with the precise reason
retained in `last_error`. The adapter does not implement transport or fabricate
remote player input.

## Integration evidence

Run `tests/test_pc_input.py` through the project Linux toolchain, or the project
`test-pc-input` driver. The runner builds all native sources with GCC C11,
warnings as errors, AddressSanitizer and UndefinedBehaviorSanitizer. It uses
`-no-pie` and `ASAN_OPTIONS=detect_leaks=0`. The generated
`analysis/native-pc-input-validation.json` records exact counts, the unchanged
ROM SHA-1 and source hashes.

The completed sanitizer run passed 314,726 assertions: 134 accumulator samples,
2,504 successful single-player native frames, 65 successful two-player frames,
2,022 moving and 1,751 airborne map frames, 1,536 third-person map frames,
17 startup/short-click shots, 8 direction checks, 33 gate checks, 35,071 input
guards and 96,504 inherited frame reentry checks. The explicit missing-network
case, two controlled POLL failures and a stale-sample distribution rejection
are additional rejected frames. No
sanitizer error occurred.

The fixture composes real decoded maps, player movement/collision/gravity,
jump/camera, world lists, combat, scripts, actor animation, weapon controllers
and animation, projectile allocation and motion, and the combined frame
runtime. It drives 128 keyboard/mouse frames on each of all 16 maps, including
native startup firing, independent strafe, airborne movement and H-camera
changes. Separate cases check direction signs at heading zero, opposite-key
cancellation, short click and jump pulses, H repeat and toggle parity, pause,
mode 3, explicit disable, focus loss, mouse overflow, wrong-stage calls,
duplicate poll/distribute and failed-POLL recovery. POLL callback cases verify
both events delivered before sampling and deferred events after sampling.
Targeted cases invalidate sampled fire/H/jump/yaw by losing focus or disabling
controls, including loss and regain before DISTRIBUTE. They also clear the
active bit after sampling, preserve a preexisting inactive slot's full input,
reject a stale sample when a new frame skips POLL, and guard an invalid player
count, null player binding and missing weapon game state.

A genuine two-player fixture uses two player bodies, gameplay bindings, weapon
runtimes and native entity updates. Player 0 is display-local while player 1 is
the PC recipient. Under the explicit original bypass flag, 64 frames verify
that player 0's supplied movement/yaw/jump/fire values are unchanged by PC
distribution, while player 1 receives W, mouse yaw, H and held fire. A frame
without the bypass verifies the explicit missing-network error and unchanged
bodies/inputs; a nonzero exit flag then permits the original local branch.
The fixture's remote inputs are test data, not network packets.

These tests deliver portable events directly. They do not establish operating
system message delivery, mouse capture, device registration, a renderer, audio
playback or a playable executable. Render and presentation callbacks observe
requests; they do not implement rendering or silently complete unsupported
gameplay operations. Windows backend evidence, if present, is separate.
