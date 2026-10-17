# Sprite controller dispatch and submission queue

`src/sprite_dispatch.c` recovers the entire `0800F85C` scheduler, its `101C4`
reset, and its `101D0` queue append. The object projection reuses
`SsaWeaponObject`, the original48byte sprite layout already used by weapon
controllers and animation. Objects remain borrowed; this module is not an
allocator, renderer, or implementation of every sprite controller.

## Exact ordering

F85C captures the sprite active-list pointer `030055C0` before calling101C4.
101C4 only clears byte `03005674`, the submission count. Existing queue bytes
remain stale beyond the new prefix. F85C then increments counter `030055B0`
with unsigned32wrap, writes state `030055A4=1`, and starts logical index0.

Each iteration reads the slot from the physical active-list cursor, selects
`base0300559C + slot*48`, writes that pointer to current `030055A0`, and writes
the logical index to `030055A8`. Type at object+04 selects the following table.

| Type | Handler |
| --- | --- |
| 2 | F150, weapon visibility/controller wrapper |
| 3 | F100 |
| 4 | F0B8 |
| 5 | E700 |
| 6 | E7A4 |
| 7 | F1C0 |
| 8 | F27C |
| 9 | ECF8 |
| A | EE60 |
| B | EFB4 |
| C | F344 |
| D | E9D4 |
| E | F4A4 |
| F | F5A4 |
| 10 | F6C8 |
| 11 | F724 |
| 12 | F794 |

Type1 and all types outside2..12hex have no handler. This does not exempt a
nonzero type from submission. The dispatcher itself has no pause test;
individual handlers, including F150, implement their own gating.

After the handler, F85C writes state1 and rereads the **current global pointer**.
If that object's type is nonzero, it writes state2 and calls101D0 with the
object. 101D0 reads the old submission count byte, increments the count byte,
then stores the **low byte of object.active_index (+0A)** at
`03005620[old_count]`. The queued value is neither the object slot nor the
current logical loop index. F85C advances its physical list cursor only after
this submission. It increments the logical index unconditionally.

The live active count `030055C4` is reread at every loop condition. A deletion
can leave the physical cursor on a swapped-in entry while the logical index
advances. Consequently fewer entries may be processed after a removal. An
append can run in the current frame. A type0 entry that remains in the list
is reread at the same physical cursor for subsequent logical iterations.
Clearing an object during the optional post-submit observer cannot undo the
already selected cursor advance. Successful return writes state3 and restores
current to base slot0; current_index remains its last value. With an empty
list, current_index is unchanged.

## Native ownership and errors

`SsaSpriteDispatch` embeds84physical active indices and84borrowed object
pointers. Every pointer, including empty slots, must be nonnull and remain
stable during the step. The embedded index array deliberately cannot be
rebound during callbacks, preserving the original captured cursor semantics
without exposing arbitrary host pointer traversal. The caller owns allocation,
active order and each object's active_index metadata.

The mandatory handler callback receives the verified original entry address
and initial slot. It may implement a recovered controller or return an error
for an unavailable one. The optional submit callback observes the real native
queue append **after** its stores. It does not replace101D0 and need not exist
for headless queue construction. E460 animation runs afterward in the outer
frame; submission itself does not advance animation or draw sprites.

Callbacks may modify payloads, active entries/count and projected globals,
including redirecting current to another bound object. They cannot rebind the
objects array, modify ops/updating, free borrowed storage, or reenter. The
logical index is a local loop variable: callback changes to current_index do
not change iteration control, though a last callback's value can persist.
Likewise callbacks see state2 from a preceding submission until the next
handler returns and the postlude writes state1.

Return0 means complete; -1 means invalid integration or callback failure.
Entry guards leave state unchanged. After entry succeeds, failures retain
earlier effects and restore state3/currentobjects[0]/updating0. The native
queue has84bytes. A submission when count>=84 returns an error instead of
overwriting adjacent globals as the original would; valid84iteration frames
cannot exceed capacity unless a callback changes the count. The original
byte wrap at255 is therefore outside the safe native queue contract.

## Validation

`tests/test_sprite_dispatch_rom.py` executes the whole original F85C and both
original101C4/101D0 routines. Only controller calls are controlled. Optional
observation occurs at101E6 after the actual append stores and before return.
The comparison includes every object byte, all84active entries, every queue
byte, counter, state, logical index, current pointer and count at each callback
and return. It covers all65536type values, every capacity and removal position,
append/release/redirection, zero-type cursor repetition, post-submit clearing,
random mutations, failure prefixes and callback reentry. Random object metadata
also verifies low-byte truncation of active_index. Guard and optional-observer
checks are native safeguards, not additional ROM behavior.

See `native-sprite-dispatch-validation.json` for current counts and source/ROM
hashes. Compilation uses strict C11 warnings and UBSan. This is native behavior
recovery; no exact GBA matching claim is made for F85C or its helpers.
