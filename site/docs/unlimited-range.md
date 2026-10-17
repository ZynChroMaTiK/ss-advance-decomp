# Unlimited ranged attacks

The native playable build removes weapon-distance limits from ranged hitscan
attacks, including shotgun side pellets. Player projectiles continue until an
impact instead of expiring after480 ticks. The chainsaw now uses the requested
192-unit PC reach (the original ROM table is96), with a finite trace;
damage, ammo consumption, cadence, spread, blast radii and enemy-projectile
expiry are unchanged. Existing unfinished special-weapon behavior remains outside
this range change.

`SsaWeaponRuntime.unlimited_ranged` and
`SsaProjectileRuntime.unlimited_player_range` enable this PC policy in
`ssa_native_game_init`. Both default to zero for original-behavior integrations.
The ROM rules and original controller, hit-query and projectile-motion APIs retain
their recovered behavior. No changes are made to the ROM or matching export.

`ssa_weapon_hit_query_unlimited` advances512-unit segments until the first
entity, blocking wall, floor, ceiling or world boundary. This segment size is a
numerical step, not a maximum distance: the original traversal visits at most64
cells per query, and long rays must not silently stop collecting geometry.
Every segment preserves the actual source identity and resets its own intercept
storage. Coordinate-boundary checks prevent fixed-point wrap. Queries do not
apply damage or invoke gameplay callbacks.

Manual aim queries use the extended ray. Original autoaim retains its nearby
vertical selection and can acquire distant targets on the level ray. Hitscan
events preserve original nearby impacts and extend clear rays beyond the former
cutoff; actual combat still performs impact callbacks, damage and hit credit.
Near-muzzle projectile fractions remain relative to the controller's original
range so distant contacts do not pull a projectile backward on creation.

`ssa_projectile_motion_unlimited` removes age expiry only for player-owned shots
and saturates their age at480, preventing later16-bit timer wrap from resetting
owner-clearance behavior. Collision, bounce, penetration and impact processing
still run. Original enemy motion and effect timers are retained.

The third-person camera uses the same extended sight ray for all ten selections,
including the chainsaw. Weapon switching therefore preserves the previous camera
fix and converges on distant ranged contacts.

Run `.\decomp.ps1 test-unlimited-range`. ASan/UBSan with leak detection passes
1,124 checks: a29,000-unit corridor exceeding64 block cells, distant entities,
wall occlusion, floors/ceilings, open world boundaries, coordinate limits,
1,000 projectile steps after former expiry, original/enemy expiry, and eight
actual combat attacks at2,304 units with weapons2–5 in both autoaim and free aim.
The same distant target remains unharmed by the chainsaw. Test fixtures control
positions, health and inventory; this is separate from normal-input completion.
Evidence is `unlimited-range-validation.json`.

The normal-input level run completes on GCC sanitizers and MSVC in4,653 frames,
28,000 score,7 pickups,19 spawned enemies,23 health,13 pistol and22 shotgun
shots. The bot's threat/supply priorities were adjusted for changed wave timing;
no gameplay state is granted or forced in that run. The Windows launcher passes
startup/restart in both renderer modes and600 gameplay frames. Third-person
alignment passes9,223 checks and232 views with maximum4K error0.212615 pixels.
Original hitscan and projectile motion still match8,192 and16,384 ROM cases.
The split simulation/PC rendering route also passes with77 normal and77 pitched
1280x720 frames across the same4,653-tick completion.
