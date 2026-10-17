# Original input distribution

`src/input_dispatch.c` recovers the complete local/bypass return path of
`08005898`, at `08005C26..08005C5A`. The genuine multiplayer path is explicitly
unsupported: `ssa_input_dispatch_local` returns
`SSA_INPUT_DISPATCH_NETWORK_REQUIRED` (-2) before changing any state. This is
not a full native implementation of `05898`, nor an exact compiler match.

## Local routing and key edges

The original enters this branch when any of these is true:

- GAME player count at +10 is at most one.
- GAME flags at +04 contain bit 100.
- GAME exit flag at +7D is nonzero.

It scans **four** player records regardless of player count. For each slot it
reads movement flags at player +BC. Only a slot with flag 4 and index equal to
GAME +68 receives the packed input word at player +A4. Nonselected or disabled
slots retain their old input. Motion indices 4..255 select no slot. GAME +68
is not the display-local index at `0300464E`; that index is never read here.

The input word comes from `03003F84`. Despite its informal raw-input name, the
frame routine `0244C` copies **processed** input +04 there after calling `03028`.
The physical `raw_keys` word at input +00 can differ during playback. The low
halfword is held buttons, and the high halfword is newly pressed buttons.
`05898` copies the complete word unchanged and synthesizes no edges. Existing
`input.c` handles those edges when polling: processed pressed bits compare the
current selected held value with the previous processed held value.

The distributor never reads input mode +14 and never suppresses keys because
that mode is 3 or 4. Relevant later consumers have their own mode checks.
Pause bit 1 is not a distributor gate: only GAME bit 100 selects bypass. A
paused frame can therefore still distribute processed input. The local branch
does not change counters, timestep, input words, player flags or positions.
The frame's GAME +00 increment happens before calling `05898`, in `0244C`.

## Native API

`SsaInputDispatch` contains four pointer projections of canonical movement
flags and packed movement keys, plus the five bytes/halfwords needed to select
the branch. Bind all four slots, including inactive records. Borrowed storage
must remain stable and distinct. The local path validates every projection
before writing, so a null field returns -1 without partial writes. A valid
local call returns 0. Genuine network mode returns -2 before dereferencing the
local projections or performing any transport operation.

Pass `frame.input`, or equivalently the processed poll word copied by the frame,
as the `packed_keys` argument. Do not substitute display-local selection, clear
other players' keys, or strip high-halfword pressed bits. Native PC input can
produce the same logical packed word; relative mouse yaw, pitch, jump and H-view
are separate requested host extensions.

## Unsupported multiplayer path: recovered control flow

The disassembly establishes the following boundaries; these are documentation,
not implemented transport functionality:

1. Call `14D44(0)` and save GAME +08 as the timeout counter. If the packet
   sequence word at GAME +64 is zero, clear player +A4 for the first
   player-count records, increment the sequence to one, and prepare an initial
   packet with zero keys. Pack the motion-index player's position/heading and
   pop one queued event before sending via `150C8(packet, GAME+12)`.
2. Otherwise repeatedly compare `15198()` with player count and compare the
   unsigned subtraction of current GAME +08 and the saved halfword with 30.
   A mismatch or a larger elapsed result sets GAME flag 100 and returns.
   This subtraction is not wrapped back to 16 bits before comparison.
3. Collect each player's current-sequence keys through `15078(sequence,index)`.
   Return value FFFFFFFF means not yet received. Once received, retrieve packed
   position with `15028` and event with `14FD8`. Already received slots are
   skipped while waiting for the others. Timer advancement and the receive
   machinery are external to this routine; inventing successful empty receives
   would change behavior.
4. Clear the current damage-source global `0300443C`. For each player, copy the
   received packed keys unless that player's BC bit 10 is set, in which case
   store zero. This network application uses the death/control flag 10, not
   the local branch's enabled flag 4.
5. For every player except GAME +68, unlink its body through `A65C`, decode
   received X/Y and heading into the player record, synchronize body fields
   through `05E04`, then insert through `A5D0`. The local/motion player's pose
   is not overwritten.
6. Set credit global `03003F40` to index+1. A nonzero event with upper halfword
   above FF calls pickup dispatcher `1B99C(target_body, NULL, upper>>8)`;
   otherwise call damage `037F4(target_body, upper)`. The lower halfword selects
   the target player record. These are real pickup/combat effects, not mere
   input samples. The routine leaves the final credit value in that global.
7. Increment GAME +64 with 32-bit wrap and build the next packet. Keys are the
   current `03003F84` word. Sequence low 16 bits go to GAME +56; GAME +58 holds
   sequence bits16..23 in its low byte and the tick byte (+07) in its high byte.
   The initial-sequence path instead writes the full sequence upper halfword
   and does not combine in ticks.
8. Position packing is `(uint32_t)x>>18`, OR `((uint32_t)y>>4)&0x0FFFC000`,
   OR `(heading<<16)&0xF0000000`. Remote decode stores X as `packed<<18`, Y as
   `(packed<<4)&0xFFFC0000`, and heading as the signed top nibble shifted by12
   and truncated to16 bits. If player +F8 is nonzero, decrement it and pop a
   four-byte event from +E8; otherwise write zero event. Send through `150C8`.

No network path is silently replaced by success, and no multiplayer packet
effects are claimed by this module. The function itself directly reads the
GAME tick byte and timeout counter; it does not directly update either.

## Differential evidence

`tests/test_input_dispatch_rom.py` compiles the native source with strict GCC
warnings and UBSan. It executes the user's original ROM routines only in the
test oracle. Passing evidence:

- **180,916** complete `05898` local/bypass calls, comparing full 256-byte game,
  four 256-byte player records and the input structure.
- **3,072** actual `03028` poll → processed-word copy → `05898` chains, including
  physical buttons, playback that differs from raw input, and pause.
- **19,788** separate network-entry checks, stopping at the first original
  transport call. These are not counted as completed-routine comparisons.
- All 65,536 GAME flag values and player flag values; all player-count/exit
  byte combinations; all 256 motion indices with all sixteen enabled-slot masks;
  both packed key halfwords; 717,961 preserved nonselected-slot observations.
- Ten native guards, including atomic failure on missing projections and
  unsupported network routing without local projections.

The completed-routine/chain total is **183,988 ROM comparisons**. Hashes and
separate counts are recorded in `input-dispatch-validation.json`. This does
not provide an OS input backend, rendering or a playable native executable.
