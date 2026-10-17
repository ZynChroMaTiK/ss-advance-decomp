# Native particles, impacts and extended projectiles

`particle_create.c` recovers original `08014B60` and `08014C58`. All 16 original
particle IDs allocate type `96` from the shared 100-slot pool. The three ROM
tables at `0844C07C`, `0844C0BC` and `0844C0FC` supply type, animation and period;
the loader copies them and retains no ROM pointer. Creation sets XYZ/type/state,
sets animation, inserts the body, emits the original sound where applicable,
then adds flags. IDs 0–7 emit sounds 22,23,24,26,27,64,25,26 (hex); IDs 8–12 add
flag 2000; every ID adds flag 20. Exhausted allocation succeeds without an effect.
On a later callback failure, allocated storage and earlier effects remain.

The controller requests destruction on any nonzero animation completion byte.
The shared native `00908` actor-animation stage advances these animations. It
does not require test-driven completion or a presentation callback pretending
to create particles. Sound remains an explicit service; the tests observe it.

`ssa_weapon_impact` recovers complete `08009D4C`: only a source with body flag10
and player index equal to GAME+68 creates an impact. Wall hits select particle0,
entity hits particle2. A knife entity hit advances the shared original LCG and
selects particle6 with whole-unit Z jitter from -16 through +15. The random
state must be synchronized with the caller's shared game random state; it is
not a separate per-weapon generator. The source weapon is the current weapon
before selection changes. Impact creation still precedes combat damage.

`projectile_target.c` recovers `0801097C`, using native `10830`, the already
verified direction/distance tables, and original wrapped fixed division.
Its explicit target is source+20, not an inferred player fallback. No target
means zero vertical velocity. The original writes through NULL when allocation
fails; the native wrapper safely returns exhausted instead. The request's
fourth geometric parameter is a Z offset, not a projectile speed override.

`projectile_extended.c` recovers complete controllers for 64,68,6A,E3. Type68
intentionally ignores motion's impact return and adds its timer a second time
in states1/2. After the first signed timer threshold it creates 15 children,
initializes their velocity and animation, then changes the parent. After the
second threshold it waits for animation completion. Type6A emits two angled
children on the original 64-tick crossing condition and updates each child
immediately, including collision, explosion, particles and deferred release.
The falling E3 effect subtracts gravity once per call, multiplies only movement
by ticks, lands first, and explodes on the next update. Its particle precedes
the explosion. Callback failures preserve the completed prefix.

Type64's mandatory `053D0` relocation through `CE50`/`D000` is now native,
with original wall clipping, sector-only links and Z clamping. Shared-pool
construction and entity dispatch are connected. See [sector clipping](sector-clip.md)
for12,048 original-routine comparisons and272 integration frames. The earlier
controlled relocation test below remains a separate outer-controller proof.

`projectile_runtime.c` now binds particle rules and sound callbacks, creates
native effects from projectile impacts, and updates 64..6B,96,E2,E3. Native
enemy controllers create ordinary or target-aimed projectiles inside their
existing combat scope. Public outside-scope creation establishes combat locks;
the separately named held-phase entry point admits only combat+spawn or live
level+script+spawn phases. Every creation rejects query reentry and nested
runtime/creator calls. Rules, sound context and all pool storage stay stable
until teardown. The old particle presentation boundary remains available only
when no native particle backend is bound.

Split children and enemy shots preserve their source's stable pool address
after the source is released. Original clearing/reuse changes the contents at
that address; it is not a generation-tracked ownership reference. Accepting a
known pool address avoids rejecting original surviving shots and does not
permit arbitrary pointers. E2 follows a live conversion target and retains its
stricter validation. All 100 owned slots, including reused initial map slots,
use the same allocation/projection rules.

Validation:

- `test_particle_create_rom.py`: 4,096 complete 14B60 calls, 256 complete14C58
  calls, 8,192 complete9D4C calls, 15,295 callback reentry checks. Allocation,
  insertion and audio are controlled boundaries; 05394 executes from the ROM.
- `test_projectile_target_rom.py`: 8,192 complete1097C calls with original10830,
  trig, distance and fixed multiply/divide. Only allocation/insertion and BIOS
  signed division are supplied. Pool exhaustion is separately guarded.
- `test_projectile_extended_rom.py`: 16,384 whole controller comparisons,
  including 5,445 child creation requests and 36,520 reentry checks. It compares
  all116 actor bytes and ordered callbacks. Motion, creation, portal relocation,
  explosion, particles and release are controlled boundaries; trig/fixed math
  execute from the original ROM.
- `test_particle_runtime.py`: 48,321 ASan+UBSan assertions on all16maps,
  256 complete particle animation/release cycles over9,888 frames, 16 spreads,
  16 splits and16 falling explosions. All240 spread children continue after
  parent release. There are656 guards and16 sound-failure prefix checks.
  Audio is observed, not played. The controlled VM-phase admission case checks
  the explicit held-lock API; it is not a substitute for whole-game VM tests.

Reports and source hashes are in `native-particle-create-validation.json`,
`native-projectile-target-validation.json`,
`native-projectile-extended-validation.json` and
`native-particle-runtime-validation.json`. Native runtime code never executes
ROM instructions; Unicorn is used only in the differential tests.

The actual decomp.wtf matcher was used for14C58. One manual compile matches the
22 executed bytes but scores100 (90.9%): the imported24-byte extent includes
two trailing zero alignment bytes. This is **not** counted as an exact match.
No artificial padding or assembly was added. The claim was released with notes.
