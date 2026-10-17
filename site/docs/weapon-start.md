# Native initial weapon startup

`ssa_weapon_runtimes_start` composes original `1240C` loadout behavior with
existing native player gameplay, combat and weapon runtimes. It accepts the
combat runtime's indexed gameplay/runtime arrays and the original loadout and
god-mode settings. Previously, `ssa_weapon_runtime_init` required callers to
provide a current/requested weapon and populated inventory themselves.

The new entry point starts from unarmed gameplay with current/requested zero.
It allocates each active player's sprite in that weapon runtime's embedded
object storage, using native object defaults. It then runs the recovered
loadout initializer, imports its weapon view, binds aim/interaction workspaces,
and preserves the exact initialized object. Embedded allocation replaces the
original sprite pool boundary; this is not a claim that the global sprite pool
or its outer dispatcher is recovered. Inactive players remain untouched.

All arrays/storage remain caller-owned. The adapter rejects busy, duplicate,
incorrectly indexed and already bound inputs. Original loadout changes and
earlier runtime bindings persist if a later workspace allocation fails. In
that case, the caller must free successfully bound earlier runtimes before
retrying the whole group. No sprite presentation callback runs during startup.

The integration test begins with empty weapon ownership and fresh gameplay.
It starts all original settings 0–36 plus the god-mode override on all 16
maps: **608 starts**. It checks original ownership, selected animation, view,
pending requests, ammo preservation for setting zero, and the unusual single
cooldown clear at each player's index. It also proves that jump state and
H-view mode survive startup.

Without changing the initial selected weapon to a test loadout, the suite
fires **192 shots** from weapons 1, 2, 3, 6, 7 and 10. Hitscan attacks use actual
map aim/hit queries and damage native scripted enemies. Projectile attacks
allocate from the shared native pool and run **2,272** real collision/flight
frames. Weapon 9's initialization is checked, but its unbound presentation
effect path is not treated as working firing. The test observes presentation
requests and uses controlled scripted enemies; no rendering/audio is supplied.

`tests/test_weapon_start.py` passes **84,077 assertions**, **5,057 weapon
frames**, **2,048 movement/jump/H frames** (880 airborne, 512 third-person),
**622 guard checks** and **6,120 cooldown field checks**. Three genuine
two-player configurations cover both active players, an untouched inactive
second player, and an injected second-player `calloc` failure after the first
runtime binds. That failure leaves the first binding usable and cleans the
second runtime's partial scratch. Test-only linker wrapping injects the
allocation failure; production code has no fault-injection hooks.

The build uses ASan and UBSan, `-no-pie`, and `detect_leaks=0`. The report and
source hashes are in `native-weapon-start-validation.json`. This integration
is entirely native; original `1240C` differential proof is separate in
`weapon-loadout.md` and `weapon-loadout-validation.json`.
