# Native enemy activation

`src/enemy_activation.c` recovers the twelve state-zero initializers for entity
types CD through D8. These initialize collision dimensions and health after the
original countdown permits activation. They do not implement enemy AI, attacks,
movement, death, rendering or the outer entity dispatcher. Map construction and
scripted construction remain separate from activation.

| Type | Original initializer | Height | Radius | Health | Additional effect |
| --- | --- | --- | --- | --- | --- |
| CD | 08013940 | 120 | 32 | 30 | None |
| CE | 08013584 | 140 | 64 | 100 | Sound 4D |
| CF | 08013008 | 152 | 48 | 50 | None |
| D0 | 0801315C | 90 | 25 | 40 | Hover offset byte +43 becomes 48 |
| D1 | 08013B08 | 140 | 50 | 100 | None |
| D2 | 08013318 | 170 | 70 | 80 | None |
| D3 | 08013F48 | 210 | 48 | 100 | None |
| D4 | 08014158 | 180 | 56 | 130 | None |
| D5 | 08013D78 | 66 | 32 | 60 | None |
| D6 | 08013748 | 138 | 90 | 150 | None |
| D7 | 0801485C | 186 | 70 | 3000 | Boss setup, animation 58, notice 72 |
| D8 | 080143B0 | 186 | 70 | 3000 | Boss setup, animation 52, notice 73 |

Heights and radii in the table are map units; stored values are 16.16. All
initializers call `08012CFC` first. A positive countdown returns without
activation. Expiration with flag 80 clears that flag, resets the countdown to
30 and also returns without activation. Only a subsequent expiration with flag
80 clear activates the enemy. Signed halfword interpretation and subtraction
wrapping are preserved by the already recovered entity timer.

Activation increments the first word in the type's twelve-byte statistics
record at `GAME+130+(type-CD)*12`, as in `080125BC`. This wraps at 32 bits;
the two other words are untouched. The code ORs entity flag 2, sets the listed
dimensions and health, and enters state 1. Ordinary activation preserves all
other existing flags, animation and timers beyond countdown processing.

Both bosses also set timer +4E to zero, save facing byte +3F as a halfword at
+48, set the global boss entity pointer (`03005710`), and clear global phase
byte (`03005714`). Their original state-entry helpers `080147D4`/`08014328`
set byte +51 to 5 and state +3E to 1, request animation 58/52 with period 10,
then clear flag 1000. They subsequently request music 13 and notice 72/73
with arguments `(id, 0, 48, 0)`. That order is preserved, including the flag
still being set at animation callback entry.

## API and ownership

`SsaEnemyActivation` projects an existing `SsaEntity` plus health, countdown,
state, facing and the two additional boss fields. `SsaEnemyActivationGame`
stores the twelve counters and boss globals. These are host structures, not
original RAM layouts. Body storage remains caller owned and must stay stable
while registered or referenced as the boss.

Call `ssa_enemy_activation_step` only for a state-zero CD..D8 entity. Its
return value is 1 for activation, 0 for waiting and -1 for invalid arguments
or callback failure. This status is a native API convention; the original
initializers return void. The API guards other states rather than accidentally
reinitializing an active enemy. Nonzero map variants therefore must be handled
by the relevant later AI state, not forcibly reset to zero.

CE and both bosses require an event callback. Other types accept NULL ops.
Sound, animation, music and notice effects are explicit callbacks; the native
initializer never silently discards them. The animation callback updates the
caller-owned animation record. It must honor the original setter's ID, period
and counter-reset semantics. Callbacks receive read-only views and must not
mutate/free the activation view, body or game, or reenter activation. Guards
reject reentry and activation during an active world collision query.

Failure preserves countdown, counters, state changes and earlier effects.
For example, a failed boss animation callback leaves flag 1000 unchanged and
does not attempt music or notice. A failed music callback suppresses notice.
The updating guard is cleared on every handled return. This is not rollback.

## Validation

Run `.venv-linux/bin/python tests/test_enemy_activation_rom.py` in Ubuntu/WSL.
The test executes each entire original initializer with the real countdown,
statistics increment, boss state-entry helper and animation setter. Only sound
`08008D5C`, music `08008EB0` and notice `0800E914` are intercepted. Animation
entry is observed but its original instructions execute.

- 14,576 whole original-routine comparisons: 8,064 countdown/flag boundaries,
  6,000 randomized cases and every facing byte for both bosses (512 cases).
- Complete 116-byte entity, 512-byte game state and eight-byte boss-global
  regions compared, with unchanged bytes retained. Native entity pointers are
  mapped to the corresponding oracle addresses.
- Ordered effect arguments and complete state snapshots compared at every
  effect boundary. Tests include counter wrap and flag clearing order.
- 29 argument/lifetime checks, 3,983 callback reentry checks and seven callback
  failure paths. The native build uses warnings-as-errors and UBSan.

`analysis/native-enemy-activation-validation.json` records the source hash and
passing counts. These are behavioral ports; no additional byte-identical C
matches are claimed.
