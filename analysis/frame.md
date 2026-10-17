# Original frame wrapper

`src/frame.c` recovers the complete control flow of `0800244C`, through its
return at `024D4`. Its twelve callees remain explicit mandatory stage boundaries;
recovering their order does not imply those systems are all implemented.

| Order | Stage | Outer condition |
| --- | --- | --- |
| 1 | `03028` input poll | Always |
| 2 | Copy processed input `030042F4` to `03003F84`; increment GAME+00 | Always |
| 3 | `05898` input/network distribution | Always |
| 4 | `021E4` player/menu control | Always |
| 5 | Add GAME+07 tick byte to GAME+0C elapsed, clamp | GAME+04 bit0 clear |
| 6 | `04E04` entities | Same single pause decision |
| 7 | `1C13C` moving sectors | Same |
| 8 | `1C6DC` script VM | Same |
| 9 | `1B854` multiplayer pickup respawn | Same |
| 10 | `00908` actor animation | Same |
| 11 | `F85C` sprite controllers | Always |
| 12 | `E460` sprite animation | Always |
| 13 | `DBDC` sprite visibility/cache preparation, then `FCE4` sprite OAM construction | Display-local player BC bit8 clear |
| 14 | Store halfword16 to `[03003F00]+B02` | Always on original return |

The frame counter wraps unsigned32 and advances while paused. Elapsed first wraps
unsigned32 when adding the unsigned tick byte, then clamps values above `0x57E04`
(359940). An overflowing sum can therefore become small instead of saturating.
No tick, input or game-state return code is computed by this wrapper. It does not
test an exit-level flag, wait for a frame clock, or loop. The caller owns those tasks.

The GAME pointer is read after poll for the counter, then reread after control
for pause/time. Callbacks can change that pointer; the two writes can affect
different game records. Once the pause branch is selected, later callbacks cannot
cancel its remaining stages. Render gating likewise reads the display-local
index `0300464E` and the selected player's BC after E460; GAME+68 is a different
index. Both render calls execute after that single gate even if the first changes it.

## Native contract

`SsaFrame` holds host projections, a game pointer, and four separately borrowed
player-flag pointers. `SsaFrameOps.stage` receives an enum whose value is the
original entry address. Every reached stage is required; an unavailable system
returns an error rather than being silently ignored. The platform may replace
polling/distribution with a documented native keyboard/mouse input adapter.
That is a host integration decision, not original GBA polling behavior.

Zero-initialize before use. Callback scalar/pointer changes are permitted, provided
storage remains alive; callbacks cannot alter ops/updating or reenter. Game null
checks occur only at the actual read points, and the local index/pointer check
occurs after sprite animation, so preceding callbacks can repair projections.
Returns0 on completion or-1 on invalid arguments/callback failure. Earlier effects
remain on failure, the guard clears, and the final latch assignment is omitted.
These errors are native integration safeguards, absent from the original void routine.

## Callee boundaries and missing work

`05898` contains multiplayer packet distribution; `021E4` can change pause/menu
and input state. They are not no-ops and are not recovered by this wrapper.
`1B854` is gated internally by player count and map12..15, using its own schedule;
its native implementation belongs to the separate pickup-respawn work.

`00908` advances actor animations in active entity order using table `08449D30`.
It skips body flag0800, writes the current frame even with period0, adds ticks
with byte wrap, and processes loop/hold/event markers. Its marker FF completion
differs from sprite animation's done1. It is now recovered in actor_animation.c;
see [actor-animation.md](actor-animation.md). Replacing it with E460 is incorrect.
F85C's separately recovered dispatcher is documented
in [sprite-dispatch.md](sprite-dispatch.md). E460 then traverses the sprite active
list; its individual object update already exists in weapon_animation.c. It skips
paused type2 objects internally, even though the outer frame always calls it.

DBDC calls DBF4, DDA8, DEF8, DF80 in order. Subsequent asset recovery proves these
perform sprite culling, cache requests, cache aging and image loading; the earlier
world-renderer label was incorrect. FCE4 builds sprite OAM presentation state.
The compatibility enum `SSA_FRAME_RENDER_WORLD` names DBDC but does not establish
a world-renderer implementation. See [sprite-assets.md](sprite-assets.md).
Neither runtime rendering nor the meaning/consumer of the final B02 latch is implemented
by frame.c. No playable native executable or complete game loop is claimed.

## Validation and matching

`tests/test_frame_rom.py` runs the entire original0244C with all twelve callees
controlled identically. It compares complete projections and the ordered state
at every callback, including game-pointer replacement and callback changes to
pause, ticks, time, counter, input, display index, player flags and latch.
All256tick values cover clamp/wrap edges and both pause/render gates.

The UBSan suite passes **10,500 ROM comparisons**, **90,023 callback reentry
checks**, **7 native guards** and **12 callback failure prefixes**. The JSON report
records the ROM/source hashes. These comparisons validate orchestration, not
the controlled callees' internals.

The function was claimed through the requested local decomp.wtf driver. Eight
manual `compile_and_diff` attempts reached **94.1%**, score435, with the same
74instructions; render-gate load scheduling/register allocation still differs.
The claim was released with notes and best source retained by the engine.
This is **not an exact match** and adds no exact matching progress.
