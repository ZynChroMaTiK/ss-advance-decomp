# CE and D3–D6 enemy controllers

`enemy_ranged.c` implements complete dispatchers `135C4` (CE), `13FA4` (D3),
`141E8` (D4), `13DB0` (D5) and `13780` (D6). The module name groups this recovery;
CE attacks by contact and D5 burrows. It uses the shared `enemy_control.h` state
and event contract. State-zero initialization, navigation, damage reactions,
drain, corpse expiry and native math use the existing recovered modules.

CE navigates, applies contact damage, computes contact knockback, then processes
hurt/death. Contact helper `12638` returns one after a successful damage request;
its jump-table branches reach the assignment at `126C4`. Knockback uses the
player body's position, while the hurt-facing calculation uses the player
record's separate XY. The impulse is navigation speed times the original cosine
table, and is emitted only for the original front-facing half-angle threshold.
The consumer must set the player's impulse heading and impulse. CE's sound loop
uses `03004674`, separate from CD's loop source: start at signed nonzero distance
at most 399, stop above 400, or when navigation/state changes. The postlude still
observes cleared state after a destruction request.

D3 fires projectile 66 immediately after the native attack gate, with forward
offset `500000`, side zero, speed `640000` and facing-derived heading. D6 does
the same with projectile 67 at speed `500000`. D4 enters its attack animation
first; marker `FA` clears the marker and emits projectile 66 at speed `5A0000`.
Only marker `FE` resumes walking. These are actual `1097C` requests; the shared
event's mode is one. Creating, moving and impacting those projectiles belongs
to the projectile runtime, not this controller module.

D4's special branch above 128 units over the player selects the player as target,
turns through `14518`/`04614`, updates the attack-alignment bit using absolute
pre-turn delta at most 1999, and runs the hazard helper. Otherwise it navigates
normally. Preserve signed wrapped subtraction for the altitude test.

D5 sets speed to one fixed-point unit before navigation, and jumps with vertical
velocity `50000` when phase two, signed distance at most 255, and the current
whole-unit Z equals floor. Its direct player-body distance uses reversed operands
relative to ordinary enemy-distance calls. Beyond unsigned distance 767 it starts
burrowing and clears flag two. Bury, hidden, emerge and hurt states perform the
original zero-speed walk/gravity call. Hidden distance at most 511 starts emerge;
completed emerge restores flag two. Damage reaction still runs after initiating
burrow in the active frame.

All callbacks preserve stable storage. Animation events observe `animation_done`
already zero. Destruction is deferred until the controller returns, with
`destroy_requested` checked even on errors. Failure preserves earlier state and
events. Null/mismatched views, unsupported type, reentry, active world queries
and ticks above 255 fail explicitly. No default presentation/combat callbacks
pretend to succeed.

## Validation

`tests/test_enemy_ranged_rom.py` compares complete original dispatchers, including
shared activation, contact, drain, table math, hurt/death/corpse, attack gate and
sound logic. Original navigation `1288C` and world calls `04498`/`0354C` are
controlled without mutation on both sides. `A4F4` supplies a controlled selected
entity; combat, group notification, destruction and projectile creation are
ordered effect boundaries. Animation setter `5394` executes in the original
oracle, and its metadata is compared with the host callback projection.

11,994 complete comparisons pass: 2,608 CE, 2,349 each D3/D4/D6 and 2,339 D5.
This includes 134 knockbacks, 89 projectile requests, 8,109 callback reentry
checks and 11 callback-failure paths. Directed cases cover every state and
animation marker, timer signed-wrap boundaries, aim acceptance, contact headings,
burrow distances and sound thresholds.

The strict-C11 ASan/UBSan fixture `test_enemy_ranged.c` adds 20,480 native
controller steps, 180,884 assertions, 81,920 input/reentry guards, 9,253 events and
19 injected callback failures. Navigation and world effects are controlled in
this sanitizer fixture too; real-world composition remains the generic enemy
runtime's responsibility. Source and test hashes are recorded in
`native-enemy-ranged-validation.json`. These are native behavior proofs, not
new byte-matched functions or a claim of complete gameplay integration.

A bounded decomp.wtf attempt for CE hurt-resume `13438` used the actual byte
matcher and original toolchain. Two manual candidates reached 89.7% and 76.5%;
neither is exact. The better unsigned-field candidate remains in
`recovered/enemy_ce_resume.c`. Its pointer register allocation and halfword/literal
load ordering differ from the original. Attempt results are recorded in
`enemy-ce-resume-match-attempts.json`; this adds no exact-match progress.
