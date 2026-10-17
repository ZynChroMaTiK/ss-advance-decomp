# Pickups bound to native gameplay ownership

`pickup_runtime.c` binds the verified `1B99C` dispatcher to the native world,
shared entity pool, adopted map actors and canonical player inventory. Initialize
it after `ssa_script_spawn_adopt_plan` and combat binding, with stable inventory
rules and a presentation observer. It installs `world.execute_special` only when
that slot is unused, and removes its own binding on free. Free before combat,
spawn, player, level or world teardown.

The item is the actor's **animation ID at original +58**, not the state byte at
`+3E`. The trigger is `script.trigger`, original `+68`. The adapter resolves the
querying player by stable body identity and the encountered E1 by owned pool
slot. It accepts adopted objects inside the initial reserved prefix as well as
later allocated slots. Nonplayer entities retain the original ignored-contact
behavior. Unowned or inactive E1 storage is an explicit integration error.

The native pickup dispatcher mutates the authoritative inventory. New weapons
enter the pending queue; original weapon selection subsequently grants ownership
and ammunition. Health mirrors synchronize before presentation callbacks and
again on return. A trigger queues the original script before inventory acceptance
and clears its trigger bit even if a full-health pickup is refused. Sound and
message events remain explicit observer callbacks. Ordinary multiplayer requests
can be forwarded, but packet replay and nearest-pickup removal are outside this
adapter.

Successful removal unlinks only the world's current `executing_special`, then
uses the native pool release and clears its static actor storage. No storage is
freed or reused during the query. This ends traversal of that cell, preserving
the original read-next-after-clear behavior; other cells can still be visited.
The release is checked against a pool copy before any inventory effects, so
corrupt pool bookkeeping fails without partially unlinking the item.

The adapter holds its own, combat, spawn and player update locks across effects.
It preserves an enclosing player/level update, because normal pickup contacts
occur inside movement. The script queue itself must be idle. Observers cannot
mutate world ownership or reenter runtime operations. A script queue failure
latches `failed` and returns an error. Since the recovered dispatcher uses void
callbacks, subsequent inventory effects can remain after that failure; later
presentation and removal are suppressed. There is no rollback or silent success.
Free/reinitialize the adapter as part of handling a failed game state.

`tests/test_pickup_runtime.py` builds all native sources under strict C11 and
ASan/UBSan (`-no-pie`, leak scanning disabled). Its fixture initializes actual
maps, original spawn plans, native players, map-actor adoption and combat owners.
Player repositioning for test coverage uses normal world remove/insert. It
collects every original map pickup through genuine spatial queries, including
repeated queries where cell traversal stops after removing another pickup first.

The final run passes 10,015 assertions on all 16 maps: 355 world queries,
287 original map pickups, 60 synthetic pickups, 40 refusals, 685 presentation
events, 3,425 callback reentry checks and 32 failure cases. Directed cases cover
pending weapon acquisition through native `ssa_weapon_update`, shared-cell
ordering, adopted-slot reuse, health mirroring, trigger-before-refusal ordering,
inherited movement locks, corrupt pool metadata and invalid trigger data.
`native-pickup-runtime-validation.json` records source and fixture hashes.

This integration test does not execute the queued script instructions or render
pickup messages; those remain separate already recovered runtime/presentation
components. The native adapter executes no ROM instructions.
