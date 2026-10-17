# Level script dispatcher

`src/level_script.c` recovers the complete dispatch routine at `0801C6DC`
(return at `0801C922`), queue insertion `0801C670`, sector entry `0801C624`,
script-state initialization `0801C5A8` and entity-group release `0801C6B0`.
This is a native interpreter of original **data records**, not an ARM emulator.
No instruction bytes are executed. These functions are behavioral ports, not
additional byte-exact matches.

The immutable ROM data view reads the table at `0845280C`. The twelve campaign
maps have 5, 12, 10, 11, 10, 13, 10, 15, 9, 3, 7 and 11 script entries, totaling
116. Maps 12..15 point to the table itself and have no valid script entries.
Native queue insertion rejects IDs outside the corresponding table. Every data
read checks the borrowed ROM extent; the caller must keep that buffer alive.

Each instruction occupies five 32-bit words: opcode, list pointer, argument,
an unused word, and spawn interval. The dispatcher has five active slots and
five words of scratch per slot. It handles **one instruction per slot per
frame**, even when an operation completes immediately.

| Opcode | Recovered action |
| --- | --- |
| 1 | Spawn the next item from an eight-byte XY/type/heading list after its interval; XY=(0,0) ends the list. |
| 2 | Wait until the specified byte-sized live-group counter is zero. |
| 3 | Advance without an effect. |
| 4 | Open mover, via `0801C02C`. |
| 5 | Close mover, via `0801C050`. |
| 6 | Unlock mover, via `0801C0B4`. |
| 7 | Lock mover, via `0801C098`. |
| 8 | Delay argument × 60 ticks, with wrapping 32-bit arithmetic. The first frame initializes without subtracting ticks. |
| 9 | Set global game state to 3 and XOR exit-toggle bit 0. Does not advance, so repeats each frame until the outer game loop handles it. |
| 10 | Remove this slot, copying the last slot and all five scratch words over it. The swapped-in slot is skipped until the next frame. |
| Other | Keep the instruction pointer unchanged. |

Spawning copies the original type table at `0844A3D4`, the list XY and heading,
the instruction's trigger tag, timer 60 and flag `0x80`. It calls animation then
world insertion, reads the resulting floor, sets Z and emits particle type 5.
These callees remain explicit callbacks; the interpreter does not implement
enemy AI, the entity pool, animation playback or particles. The known entity
fields use a host structure, not a GBA memory overlay. Adapters must retain
stable allocated storage and transfer the known fields to their world entity.

If allocation fails, the original still emits the particle and consumes the
spawn record. Its Z comes from uninitialized/incoming `r10`, or the most recent
successful spawn earlier in that frame. The native API explicitly accepts
`fallback_z` for the otherwise undefined first value. Tests set original `r10`
to the same value; this does not establish a canonical original failure Z.

Unless tag bit `0x100` is set, successful spawns increment counter
`(tag & 255) - 1`, wrapping at 255. **Real scripts use group zero**, which writes
the byte before the original counter allocation. `alive_before` represents
this stray byte safely and is preserved across reset, as the original byte is.
Entity release skips tags with low byte zero or bit `0x100`; otherwise it uses
the complete tag minus one. The native release API rejects an out-of-range
index instead of touching unrelated host storage.

Sector entry sets the visited byte on the first visit, including sectors without
an event. Flag `0x100` in sector word `+0x10` queues its low-byte ID. Subsequent
visits do nothing. Native invalid/full-queue entry fails before changing the
visited byte; the original has no capacity or ID guard. Reset clears the queue,
both 256-byte arrays and only the first three words of each scratch record.
It preserves scratch words 3/4 and the game-state/exit fields.

`ssa_level_script_step` returns 0 on a completed frame and -1 on invalid input
or callback failure. Other APIs return 1/0. Callbacks return nonnegative success
or negative failure, except allocator NULL means ordinary pool exhaustion.
Effects before failure are retained. No callbacks may mutate/reset/reenter the
VM; the `updating` flag guards API reentry. No callbacks are optional at step
entry, so a partially implemented host cannot silently omit a gameplay effect.

`tests/test_level_script_rom.py` compares complete queue/scratch/counter/visited
state, game-exit values, ordered callback inputs and every byte of allocated
entity records. Original allocation, animation, insertion, particle and mover
callees are hooked; the dispatcher and its transitions execute unmodified ROM
instructions. Synthetic data records exercise all opcode paths, unknown
opcodes, signed/wrapped timers, mixed queues, allocator exhaustion and stale Z.
All 116 real scripts execute to termination or level exit, with external enemy
deaths represented by clearing live counters between frames. This validates
script sequencing and ROM parsing; it does not simulate enemy combat.

The generated `native-level-script-validation.json` records the current test
counts and source hash. The suite builds with undefined-behavior sanitization.
The original supplied ROM is read only and remains unchanged; modified records
exist only in test buffers for boundary coverage.
