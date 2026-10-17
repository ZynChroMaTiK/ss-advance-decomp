# Entity scheduling

`src/entity_dispatch.c` recovers the whole outer entity scheduler `08004E04`
and its preparation call `080083E4`. It dispatches through explicit handler
callbacks, allowing existing native player, enemy and projectile controllers
to run without duplicating them in the scheduler. A recognized handler must
be supplied; unsupported controller implementations must fail explicitly.

At entry the original scheduler increments the word at `0300461C`, clears the
walk counter at `03004358`, and calls `083E4`, which simply clears the word at
`030000B0`. The native projection exposes these as `frame_counter`,
`walk_count` and `path_count`. Increment wraps at 32 bits. Preparation does
not update freeze timers, thaw actors, advance animations or process input.

The complete 128-entry jump table at `08004E68` covers types 64 through E3.
There are 28 handled types and 25 unique handler addresses:

| Hexadecimal type | Original handler |
| --- | --- |
| 64 | 08010D7C |
| 65 | 08010E5C |
| 66 | 08010EF0 |
| 67 | 080111C0 |
| 68 | 08011350 |
| 69 | 080113C4 |
| 6A | 08011090 |
| 6B | 08011120 |
| 96 | 08014C58 |
| C9, CA, CB, CC | 08005CCC |
| CD | 08013978 |
| CE | 080135C4 |
| CF | 08013040 |
| D0 | 0801319C |
| D1 | 08013B40 |
| D2 | 08013350 |
| D3 | 08013FA4 |
| D4 | 080141E8 |
| D5 | 08013DB0 |
| D6 | 08013780 |
| D7 | 08014AB8 |
| D8 | 08014648 |
| E1 | 0801B848 |
| E2 | 080113E4 |
| E3 | 08011424 |

All other 16-bit types take the original no-handler branch. This includes
type zero; a zero payload may still be present in the supplied active list.
`ssa_entity_dispatch_handler()` returns the actual even routine address or
zero, so a runtime can explicitly bind the appropriate recovered controller.

The scheduler reads physical active indices starting at `03004470` and
re-reads the live active count at `03004354` after every iteration. It does
not snapshot the active prefix. Releasing an actor swaps the last slot into
its position; the cursor nevertheless advances, so that moved actor can be
skipped this frame. Appending an actor can cause it to execute later in this
same frame. Allocation followed by releasing the current actor may instead
move the newly allocated child behind the cursor. These effects are preserved.

Before dispatch, `current` becomes the selected body, corresponding to
`0300443C`. After the handler, the original checks the type of the body then
stored in that global, rather than retaining a private pointer to the initial
body. The callback may therefore redirect `current` to another stable slot.
All 100 payload addresses remain stable even when their pool slots are free.

The auxiliary list begins at **03004370**. If the post-handler current body's
type is nonzero, the original reads the **next physical active-list halfword**,
adds one, truncates it to 16 bits, and appends that value. It does not append the
slot just processed. This can read a stale entry beyond the live prefix. When
processing physical entry 99, it reads the adjacent halfword at `03004538`,
outside the 100-entry array. The native `active_tail` explicitly supplies that
word; it must not be inferred from active count. A value FFFF wraps to zero,
even in the middle of the output. `auxiliary_count` tracks writes separately
from that value. On completion the scheduler writes a terminating zero and
leaves later auxiliary bytes stale, then resets `current` to slot zero.

`SsaEntityDispatch` references an existing pool and an array containing all
100 stable entity addresses. No initialization allocation is required. Bind
these fields on a zeroed context. Handlers may perform normal pool allocation
and release, update entity payloads and the three global counters, and redirect
`current`. The binding pointers, payload addresses and scheduler-owned
auxiliary fields must not be changed during a call. Nested scheduler calls
are rejected. An invalid entry argument leaves state unchanged. Native
callback failures retain earlier effects, terminate the partial auxiliary
list, restore `current` to slot zero and release the update guard; the ROM has
no corresponding callback-error convention.

`tests/test_entity_dispatch_rom.py` executes whole `04E04` and `083E4` from
the user's ROM. It independently decodes the table and each branch's Thumb
BL. Controlled handlers route allocation and release through the **actual
original** `05170` and `051C8`, using an execution trampoline; only the BIOS
memory-clear service is replaced. The native handlers use the recovered pool
APIs. Every callback snapshot and final projection compares all 100 entity
types and slot positions, both index arrays, active count, complete auxiliary
storage, current entity and all three counters. Tests cover every 16-bit type,
all capacities, every release position, random dynamic handlers, redirection,
counter overflow, allocation/release within one handler, and callback failure.
Counts and the source hash are recorded in
`native-entity-dispatch-validation.json`. The suite uses UBSan and tests nested
call rejection at every handler boundary. Controller behavior and a complete
game frame remain separate integration concerns.
