# Native pickup dispatch

`ssa_pickup_touch` in `src/pickup.c` recovers the complete control flow of
`0801B99C`. It calls the native inventory helpers described in `inventory.md`.
The handler accepts player entity types C9..CC, applies local/multiplayer gates,
dispatches item IDs, and performs effects in original order. It deliberately
adds no distance or height test: the original special contact is reached before
the ordinary entity overlap test.

Weapons queue acquisition rather than immediately granting ownership. Native
`ssa_weapon_update` now consumes that queue; see `weapon-selection.md` for
ownership, ammo grants and requested-weapon behavior. Ammo
acceptance depends on ownership in single player and on capacity. Stat C4/C6
pickups gate at 100; the health items gate at 100 or 200. The 100-point helper
has different cap behavior from smaller additions. Unknown items follow the
original generic sound/message-and-remove path; item 63 emits sound only.
The exact meaning of stat C4 versus C6 is not yet named without further evidence.

PC pickup policy (2026-10-19): the playable native game enables
`SsaPickupRuntime.pc_pickups` and dispatches through `ssa_pickup_touch_pc`.
Loose ammo and ammo-filling packs can store reserves before the matching weapon
is owned; this does not grant the weapon. Duplicate owned weapon pickups add
the same half-capacity ammunition as acquiring that weapon normally, without
queueing acquisition or forcing a weapon switch. Ammo caps still apply; loose
ammo and duplicate guns remain on the ground if their pool is full. The second
Colt retains its alternate dual-acquisition path, including when Colt ammo is
already full. These rules apply with Alternate weapons both Off and On.
Original entry points and inventory helpers retain their ROM behavior; only
the PC pickup call uses the ownership-independent ammo eligibility. Actual
multiplayer request/replay gates and pickup trigger/effect/removal order stay
unchanged. Existing carry snapshots already preserve reserves for unowned guns.
Focused verification: GCC ASan/UBSan/leaks and MSVC agree on2320339checks,
6507frames and123pickup contacts in the extended alternate-weapons fixture.
It covers all eight finite ammo/gun pairs, stored ammo surviving carry and
first weapon acquisition, duplicate grants without switching, caps/refusals
and ammo packs in both alternate modes. The original dispatcher still passes
5420 ROM comparisons; Windows input and original switch regressions pass.
125libraries/frontend rebuilt. See pc-pickup-policy-validation.json and the
current native-pickup/alternate-weapons reports. No campaign playthrough.

An entity trigger bit 0x100 causes the low-byte script ID to be dispatched and
then clears that bit, even if inventory subsequently refuses the pickup. The
callback may change the trigger; the mask applies to its updated value.
Normal multiplayer contacts send a request without granting the item or removing
it. A nonzero replay override looks up the nearest pickup, checks its item ID,
and removes it **before** inventory acceptance. Such replay does not increment
the normal collected counter. Normal successful pickups increment the wrapping
32-bit counter before removal. Sound and message requests are local-only.

## Runtime boundaries

`SsaPickupPlayer`, `SsaPickup` and `SsaPickupGame` are native views. They contain
only recovered fields. The caller supplies a per-player inventory and retains
stable storage throughout dispatch, including after object removal. The
`handle` members allow association with world entities or spawn objects.
Effects callbacks deliver script, sound, HUD and network requests; those systems
are not implemented by this module. Nearest-selection and entity destruction
are also explicit callbacks. A replay caller must supply the original active
entity ordering when implementing nearest selection; geometric nearest behavior
is not provided by this dispatcher.

Return values are a native integration aid: -1 invalid integration, 0 ignored
or refused, 1 applied, 2 multiplayer request. Original 0801B99C returns void.
Errors can follow earlier trigger or replay-removal effects; there is no rollback.
Callbacks must not reenter dispatch. The world executor can call this handler
synchronously; see `world.md` for its narrower mutation contract and the native
integration example in `tests/test_pickup_world.c`.

The original grid walker loads the current entity's next pointer after calling
the handler. Destruction clears the entity, so collecting it ends that cell's
walk. Native world execution preserves this behavior instead of saving next
and collecting the rest of that cell in the same query.

## Evidence

`test-pickup` passes 5,420 complete original-dispatcher comparisons: 2,640
normal-contact cases, 780 replay cases and 2,000 randomized gate/default cases.
All inventory helpers execute as original instructions without hooks. The only
controlled boundaries are script, sound, HUD, network, nearest lookup and
destruction. Tests compare full player/entity/game bytes and 6,482 ordered
effect calls, with inventory/counter snapshots at every call, plus six native
guards. GCC uses strict warnings and UndefinedBehaviorSanitizer.

This proves recovered dispatch/inventory behavior under those boundary contracts,
not script execution, real networking, audio playback, or a complete gameplay
loop. See `native-pickup-validation.json` for source hashes and counts.
