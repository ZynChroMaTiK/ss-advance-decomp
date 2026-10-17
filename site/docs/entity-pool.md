# Native entity slot pool

`src/entity_pool.c` recovers the index bookkeeping in original initialization
`08005238`, allocation `08005170` and release `080051C8`. The native pool owns
100 indices; the caller owns 100 stable payload records. It does not execute ROM
instructions or include an emulator runtime.

The original stores its payload base at `03004644`, free-stack cursor at
`0300433C`, free count at `03004348`, active-list end at `03004454`, and active
count at `03004354`. The free index array begins at `03004540`; active indices
begin at `03004470`. Each original payload is 116 bytes, with its slot index at
`+38` and its active-list position at `+3A`.

Initialization allocates 11,600 payload bytes, clears a separate global frame
counter (`0300461C`), clears all 100 payloads, fills the free array with indices
0 through 99, and starts with no active slots. The host API handles only the
pool. Payload clearing and frame-counter initialization remain caller work.
Native initialization additionally zeroes stale active-array bytes that the
original leaves unchanged; those bytes have no effect until allocated.

Allocation takes `free_slots[active_count]`, appends that index to the active
list, stores its active-list position, and increments the count. It does not
clear payload bytes. A full pool returns no allocation without mutation.

Release pushes the slot onto the free stack and decrements active count. If it
was not the final active entry, the final index replaces its position and that
other slot's active position is updated. The stale final active entry is not
cleared. The original then clears the released payload through `080052B0`.
This produces immediate reuse of the most recently freed slot, not ascending
index allocation after releases. Active traversal order therefore changes when
a non-final slot is removed.

`051C8` does **not** remove world links, update script-group counts, or issue
audio/effects callbacks. The caller must unlink registered bodies before
release, then clear the payload after successful release. Script-group defeat
notification is separate gameplay work; equating every release with a defeat
would change original behavior. Moving an index never moves its payload.

## API and guards

`SsaEntityPool` exposes `free_slots`, `active_slots`, `positions`,
`active_count`, and native-only `occupied` bits. Read the active prefix for
iteration; free count is `100 - active_count`. Callers must not mutate these
arrays. `ssa_entity_pool_init` returns 1 or -1 for a null pointer.
`ssa_entity_pool_allocate` returns 1 with an output slot, 0 for exhaustion,
or -1 for invalid arguments/state. The output must not alias the pool.
`ssa_entity_pool_release` returns 1 or -1 for invalid state/slot. Failures do
not mutate state, and failed allocation leaves its output unchanged.

Validation checks the live active prefix and free suffix for valid, unique
indices, active positions, occupied bits, and the original 100-slot limit.
It rejects double releases and inconsistent metadata before mutation. These
are native guards; original invalid inputs can corrupt memory. No callbacks,
payload allocation, group updates, or world queries occur inside this API.

## Verification

`tests/test_entity_pool_rom.py` passes **36,697 original-routine comparisons**:
110 initialization calls, 20,277 allocations (including exhaustion), and 16,310
releases. It compares all 100 payload records, complete stored free and active
arrays including stale entries, positions, counts, and original cursor values
after every call. Initial payloads contain random bytes. Every initial capacity
from 0 to 100 and every release position at each capacity is exercised, followed
by immediate reuse and draining. Eight additional runs of 2,048 operations
exercise arbitrary allocation/release order and repeated full/empty boundaries.
Twenty-five native guards reject invalid arguments and corrupted metadata.

The original initialization, allocation, release, and `052B0` routines execute
in the oracle. Heap allocation returns a controlled stable buffer. The original
multiplayer clearing branch calls BIOS CpuSet; its zero-fill effect is supplied
at the BIOS boundary. The single-player DMA hardware branch is not exercised.
Each allocation must issue no clear call; initialization and release must issue
their exact ordered clear calls. Payload clearing in the native side is an
explicit test adapter implementing the documented caller contract.

The C library builds with strict warnings and undefined-behavior instrumentation.
`analysis/native-entity-pool-validation.json` records results, ROM identity, and
source hash. This is behavioral recovery, not a new byte-exact GBA match.
