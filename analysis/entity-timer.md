# Shared entity countdown

`src/entity_timer.c` ports the complete original routine at `08012CFC`.
The type-`E1` update wrapper `0801B848` calls it and ignores its result. Multiple
enemy update routines also call it, but their surrounding activation/AI logic
is outside this utility's scope.

`ssa_entity_timer_step(&flags, &timer, ticks)` takes host pointers to the fields
corresponding to entity halfwords `+30` and `+4E`. The original timestep is the
unsigned byte at game state `+7`. The result is:

| Condition after subtracting ticks | Mutation | Return |
| --- | --- | --- |
| Wrapped 16-bit timer is signed-positive | Store the new timer | 0 |
| Timer is zero or signed-negative, flag `80` clear | Set timer to zero | 1 |
| Timer is zero or signed-negative, flag `80` set | Clear flag `80`, set timer to 30 | 0 |

Other flag bits are preserved. Subtraction wraps **before** the signed test;
for example, `8000 - 1` becomes `7FFF` and stays active, while `0 - 1` expires.
A zero timestep still performs the expiration test. Resetting to 30 does not
consume any leftover elapsed time. No visibility/rendering, activation,
destruction or script callback occurs inside this routine. Naming flag `80`
as a complete gameplay state would require examining its other users.

Null or aliased field pointers, and timesteps above 255, return `-1` without
mutation. These are native integration guards, not claimed original behavior.
The caller retains ownership of the fields and is responsible for selecting
which entities update each frame.

`tests/test_entity_timer_rom.py` compiles the native implementation with strict
C11 warnings and undefined-behavior checks. It executes the complete original
countdown and E1 wrapper without hooks, compares the complete entity/game byte
arrays, and compares the countdown's defined return value. The void wrapper's
return register is deliberately not treated as a result. Passing coverage:

- 131,072 countdown cases: all 65,536 timer patterns, with flag `80` both set
  and clear, randomized other flags and varying byte timesteps.
- 12,288 additional cases: every byte timestep across zero, signed-limit and
  subtraction-wrap boundaries, with four flag patterns.
- 4,096 whole-wrapper cases, plus six native pointer/timestep guards.

Total: **147,456 ROM comparisons**. Evidence is recorded in
`native-entity-timer-validation.json`; Unicorn is used only as the test oracle.

The decomp.wtf matching claim for `08012CFC` was released after 20 bounded
attempts, including two compilation errors. Best score is 920 / 80.8%; the
candidate is **not exact**. Both streams have 48 instructions, but candidate
LR saving, register allocation and load scheduling differ. The best candidate
is `recovered/entity_timer.c`; 17 later trial sources/results are recorded in
`entity-timer-match-attempts.json`, and all 20 attempts remain in the decomp.wtf
database. The E1 wrapper was already matched and was left unchanged.
