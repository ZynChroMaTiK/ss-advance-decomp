# Native weapon controller

`src/weapon_controller.c` recovers complete `080116E4`, the current/requested
weapon transition `08012160`, and ten cooldown timers `08012200`. The controller
uses the native `SsaInventory`, `SsaWeaponControl`, and sprite object projection
from `weapon_animation.c`. Weapon data records at `0844BC8C` are decoded as data,
including continuous-fire mode, animation period, and range. No original code
executes in the native implementation.

Aim, hitscan attacks, projectile allocation, explosion dispatch, special-effect
allocation/insertion, particles, and audio remain explicit callback operations.
These boundaries are not silent implementations of an unrecovered subsystem.
The controller recovers which operations occur, their original arguments,
ordering, ammunition/statistics changes, and all inline projectile/effect field
writes. The runtime adapter determines which operations can execute natively.

## Original ordering and state

`122AC` first establishes the current source, owner, and weapon globals. For an
active player (record flags `BC & 4`) it runs cooldowns, near-wall interaction
`12230`, acquisition/selection `11604`, firing `116E4`, then lower/raise `12160`.
The parent frame `0244C` runs entities (`04E04`), movers (`1C13C`), VM
(`1C6DC`), pickup respawn (`1B854`), and `0908`, then sprite-object dispatch
`F85C` and finally animation `E460`. Type2 object dispatch calls `F150`, which
calls `122AC`. Weapon firing therefore runs after movers/VM and consumes the
preceding animation completion byte; ticking animation before firing changes
behavior. `F150` skips the entire wrapper while GAME flag1 is set and otherwise
updates object flag1 according to display-local player flag8. This outer gate
is separate from the recovered `116E4` controller. The source globals
owner/weapon clear after the wrapper, while the source pointer remains.

Every firing-controller call aims even when no fire button is held. It first
uses the player's heading and full weapon range. If no target is found, it
retries at heading +512 with half range, then heading -512 with the same half
range. The final heading/range/slope also control attacks. The marker begins at
`69696969`, sets/clears control flag `20000`, then resets to zero. Blocking
fraction starts at `INT_MAX` and persists across retries. The final slope is
written to original player field `E4`.

Held and pressed buttons are separate halfwords `A4` and `A6`, not the movement
or selection flags at `BC`. Motion/protection index `GAME+68` controls local
sounds and local-only attack branches. The display-local player's `BC & 8`
denies starting a shot; it is not the motion index. A local game flag `400`
blocks a held start until released. Ready object Y must be exactly `5000` and
the current cooldown must be exactly zero.

Starting a shot clears flag `1000`, sets `400`, and selects animation weapon+9
(special animation22 for weapon3 with exactly one round). Discrete weapons
apply their attack on the next call, set `1000`, decrement ammo, and increment
the current weapon's `GAME+98+16*i` counter once. Weapon3 consumes a second round
only if the signed remaining count is positive. Finishing animation resets
flags `1400` and selects idle animation current-1, period3.

Continuous weapons 1/4/5 fire while held, ammunition is nonzero, and source
health is nonzero (negative signed health does not stop them). In single-player
their damage is ticks times 1/3/4; multiplayer local damage is 2/3/4. Ammo and
shot counters still advance in remote branches that omit a hitscan request.
Weapon1 does not consume ammunition. Sound-stop behavior preserves the return
value of the preceding sound-play request.

Weapon6 and10 projectile launch distance uses the final blocking fraction when
present, fixed-point multiplication with wrap, and a signed threshold. If the
distance changes from80 units, the returned projectile's travel field clears.
The projectile receives source player index+1 and weapon6/10. Weapon7 sets the
owner but leaves its allocation-provided weapon field unchanged. Weapon8 sets
its cooldown120, writes request `80000009`, and requests radius-zero damage1000
explosion. Weapon9 sets eligible target flag800 before allocation, writes
request `40000009`, then builds a typeE3 effect. Pool exhaustion still consumes
the shot. Its animation/radius depend on `(target.x >> 18) & 3`.

Lowering adds ticks<<11 to signed object Y with 32-bit wrap. When Y is strictly
greater than `DBFF`, it clamps to `DC00`, clears flag100, commits requested to
current, and selects the new idle animation. Raising subtracts the same amount;
at Y<=`5000`, it clamps and clears flag2000. Selection key10 forces lowering.
Cooldowns subtract ticks with 16-bit wrap and clamp only a signed-negative
result; a large wrapped subtraction can therefore become positive.

## Native boundaries

`include/ssa/weapon_controller.h` documents every event argument and lifetime.
Controller state owns no body, inventory, object, or effect allocation. Those
must remain stable throughout callbacks. Reentry into step/switch/cooldowns is
rejected. A callback failure propagates `-1` and releases the update lock;
earlier flag, animation, target, and attack effects remain. A projectile-create
callback returning success without an allocation is an error. The special
effect allocator may return NULL to model the original bounded pool failure.
No native callback may invalidate retained source/target storage.

Native guards reject missing dependencies, current weapons outside1..10,
requested IDs outside1..10 for switching, and truncated ROM rules. These are
explicit safety contracts, not newly claimed original behavior. Controller
source health is a projection of the authoritative player inventory health;
the native runtime must refresh it each frame.

## Verification

`tests/test_weapon_controller_rom.py` compares whole original routines through
Unicorn only in tests. It compares complete256-byte player records,512-byte
GAME records,48-byte objects, and complete116-byte source/target/projectile
records, plus ordered boundary calls and marker/fraction/request globals.
The actual original `FC04` and `116BC` instructions execute; aim, hitscan,
allocation, explosion, audio, particle, and fixed multiplication boundaries
are controlled identically on both sides. All ten weapons and all event kinds
are exercised, including pool-full handling, signed ammo, stat wrap, local and
remote players, ready/start/fire/finish states, and extreme switch/cooldown
arithmetic. Native compilation uses strict C11 and UndefinedBehaviorSanitizer.

Exact current counts and source hash are in
`native-weapon-controller-validation.json`. These comparisons establish
controller behavior; they do not establish a playable weapon renderer or
projectile simulation.

The catalogued48-byte `12200` matching candidate reached score30 (98.7%) in a
bounded16-attempt manual pass through decomp.wtf. All24 instructions align;
remaining differences are register allocation. It was released with notes and
is **not** an exact match. The clean candidate is
`recovered/weapon_cooldowns.c`; sources/results for every attempt are in
`weapon-cooldowns-match-attempts.json`. Matching progress is separate from the
native behavioral validation above.
