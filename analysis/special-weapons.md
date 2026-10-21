# Native special weapons and palette effects

## Frozen animation render repair

A normal-input level11(index10) development probe exposed an Arachnoid changing from
walking to attack animation72 while Time Strike flag800 held00908 playback.
The canonical displayed frame remained4, but that resource has only two frames
per mip. Reference projection then selected an image outside its six-image table;
the PC renderer could instead select a frame from another mip.

`ssa_native_game_prepare_view` now repairs only the render snapshot when flag800
is set, the animation index is freshly reset to zero, the displayed frame is
outside that resource's frame stride, and the new sequence's first frame is valid.
The canonical actor, animation index, elapsed time, completion marker, health,
Time Strike timing and damage remain unchanged. Valid frozen frames and FFFE/FFFF
display markers retain their original behavior. Default recovery animation and
projection functions are unchanged; this is a native presentation repair.

The special-weapon suite now passes32318ASan/UBSan/leak assertions and matches
MSVC. Twelve directed cases cover both views: stale and valid frozen frames,
unfrozen metadata, nonzero animation indices and both display markers. Actual
reference projections confirm the repaired sprite is submitted with a valid image;
PC flat/pitched rendering also succeeds, and full actor snapshots remain identical.
The natural failing probe at frame2880 subsequently renders beyond that frame.
This adds no campaign admission or byte-matching claim.

## Weapon integration

Weapon 8 and weapon 9 now connect the recovered weapon controller to native
combat, entity allocation, actor animation, world insertion and presentation.
They previously reached unsupported callbacks in the playable frontend.
The original controller and its default ROM-comparison entry points are unchanged.

The PC frontend now admits E3 physics updates at the measured original cadence,
instead of calling its gravity at60Hz. `test-original-time-strike.ps1` runs the
unchanged ROM in the pinned test-only mGBA core after180 warmup refreshes. A RAM
fixture replaces one already linked pickup at its existing XY/floor with a
weapon9 E3,520 units above the floor; original controller, rendering and frame
timing still execute. Nine steady intervals take60 hardware refreshes (three6,
six7). The native adapter uses that average, three updates per20 GBA refreshes,
with the rational16777216Hz/280896-cycle clock mapped to60Hz simulation. Each
admitted call passes6/7/7 elapsed original ticks to unchanged11424. It still
subtracts two units of velocity once per call, lands first, and explodes on the
following admitted update. A520-unit drop now expires at67 PC ticks, about1.12s;
no gravity or damage advances from rendering. Clock state clears on allocation
and release, including slot reuse. Pause skips the entity stage and freezes it.
This is a measured-scene average, not emulation of scene-dependent GBA CPU cost.

The user chose to retain original100-damage blast, difficulty scaling, falloff,
boss resistance, credit and group/death behavior. Time Strike therefore does
not guarantee a kill on tougher enemies. Its PC targeting policy accepts only
live CD..D8 enemies with canonical positive health and without temporary damage
protection or flag20. Missing, corpse, item and protected targets now play the
normal weapon firing animation without ammo/stats/flash/sound/effect creation;
aim is checked again at attack commit. An invalid attempt is consumed at start,
so acquiring a target during its gesture cannot fire a deferred shot. Losing
a valid target at commit also finishes the silent gesture. Weapon handoff still
waits for this animation to finish.
This is an explicit requested PC improvement; the original controller entries
still reproduce the ROM's ammo/sound consumption on a miss.

Weapon 8 requests the original `0x80000009` palette flash, applies the recovered
zero-radius, 1,000-damage explosion and sets its 120-tick cooldown. The original
sector reject rules choose affected CD..D6 enemies. Players and D7/D8 bosses are
excluded by that recovered blast path; it is not an invented map-wide boss kill.
The original ammo, animation and held/released input gates still apply.

Weapon 9 retains its original aim-selected target, sets eligible target flag
`0x800`, requests palette flash `0x40000009`, and allocates type E3 from the same
100 slots used by enemies, projectiles and particles. Its recovered fields set
the effect above the target by its height plus 400 units. Target X selects the
original animation 0x60/61/62 and visual radius. The effect stays at its captured
XY, falls under the native 11424 controller, lands, then emits particle 7 before
exploding on the following update. Shared actor animation supplies real frames.

E3's original owner remains zero and weapon field is 9. Its target pointer is
the original source/attachment field, not a new player-owner assignment. This
preserves the original absence of player kill credit for this effect. Destruction
unlinks/releases its stable slot without issuing a script-group defeat for the
effect itself. Enemy controllers retain their normal death/group processing.

Effect allocation/animation/insertion now runs inside the weapon's existing
combat scope and ownership locks. No presentation observer allocates or mutates
entities. Full allocation preserves the original weapon-9 behavior: flash,
target flag and ammo/shot accounting remain, but no falling effect is created.
Earlier effects survive a later failure, and runtime teardown releases allocated
slots. This does not change the stricter E2 conversion-target lifetime contract.

`palette_effect.c` recovers the background-palette phase of original 0750C.
It reads the ROM's two map-family palette tables, preserves request high bits,
and reproduces count, byte-counter wrap, protection-flag-8 and backdrop-entry-0
ordering. Hurt, weapon-8, weapon-9 and bit-0x20000000 table selection are covered.
The request is the same field as combat's `hurt_flash`, so returning it to zero
also permits subsequent hits to emit the original hurt sound again.

Both renderers use the resulting background palette for world and billboard
actors. Weapon/HUD object palettes stay separate, as in 0750C. Palette updates
occur once per native simulation tick, including paused ticks; repeated rendering
does not advance them. The recovered state progression is verified per display
call. This does not reproduce scene-dependent original GBA rendering time, and
does not implement all remaining display-register, HUD or menu palette behavior.

`test-palette-effects` executes the whole original 0750C in the isolated Unicorn
oracle, controlling only blend-register setup and hardware DMA copying. It compares
7,936 calls on all 16 maps, 2,031,616 palette entries, requests and byte counters.
Six malformed-request/data guards verify unchanged output on failure. The game
runtime never executes ROM instructions. This is behavioral recovery, not a new
byte-matched catalogue function.

`test-special-weapons` uses real native map/world/weapon/combat composition:
16 blast cases, 16 falling-effect cases and a full-pool case. Four additional
Windows-style quick-click fixtures exercise both weapons and both view modes,
real entity scheduling, palette restoration, renderer independence and repeated
hurt sounds. These controlled fixtures grant test weapons and reposition a target;
they do not establish campaign acquisition or full original feature parity.
The separate six-map normal-input campaign still uses only earned equipment.

Reports: `palette-effect-rom-validation.json`, `special-weapons-validation.json`
and `windows-special-weapons-validation.json`. Commands:
`decomp.ps1 test-palette-effects`, `decomp.ps1 test-special-weapons`, and
`powershell -ExecutionPolicy Bypass -File scripts/test-special-weapons.ps1 -SkipBuild`.

Time Strike correction validation:22,349 ASan/UBSan/leak assertions, with the
existing16-map blast/falling cases and full pool;12 new timing/damage scenarios
span all three difficulties, ordinary enemies and a boss. They check unchanged
health until impact, exact early drop positions,67-tick lifetime, no invented
kill credit, fractional-clock reset on reused slots and batched catch-up.
Ten rejection scenarios cover empty rays, corpses, items, protected enemies
and targets lost between start/commit in both aim modes, then a fresh valid shot.
Four normal native input/view cases now also assert the target actually dies.
MSVC and GCC agree on all counters. The existing Windows automatic-weapon/input
regression still passes53,141 checks. Original controller recovery remains
verified by11,652 weapon and16,384 extended-projectile ROM comparisons,
including4,096 E3 cases. All107 Windows libraries and the frontend rebuild.
The nine-map campaign was not replayed for this focused weapon correction.
Original timing trace and source hashes: `original-time-strike-validation.json`.
Reproduce it with `powershell -ExecutionPolicy Bypass -File scripts/test-original-time-strike.ps1`.

## Smooth fall and empty-fire feedback

`ssa_projectile_runtime_render_z` predicts E3's next admitted6/7-tick position
and blends toward it using the existing rational clock phase. It clamps to the
original integer-Z landing height. Registered bodies, velocity, target locking,
damage, allocation and the67-tick reference lifetime are unchanged. Rendering
does not advance the clock. The native snapshot stores per-source Z offsets;
both PC perspective paths and the reference actor collector project temporary
body copies. Stable world identity/list ordering remains the original body's.
The original projection/scene functions are wrappers with no offsets, preserving
their ABI and ROM output.

The focused suite now passes32,226 ASan/UBSan/leak checks on GCC and MSVC,
including732 changing visual positions over the12 existing timing scenarios,
648 empty-gesture frames across16 invalid/lost/reacquired-target cases, repeated
read-only rendering in both views and both renderers, and unchanged100-damage
impact behavior. Prion speed adds10 real-hit native cases/60 observed game ticks
covering all eligible enemy types,1x/2x walking/retarget/drain/animation clocks,
unconverted actors, pause and original corpse timing. These are controlled
fixtures, not campaign acquisition or completion proof.

Quick clicks during raising retain one visual-only attempt until the weapon
is ready. The empty-attempt latch is set after the final adjacent-aim retry,
so original auto-aim assistance still works. A target entering the ray while
an empty gesture waits cannot turn that attempt into a charged shot. A second
click during the gesture's recovery is now discarded, matching the no-reload-queue
input policy. Focus loss, disabled
controls, pause, weapon changes and respawn clear pending state as appropriate. Valid discrete Time Strike
attempts consume their buffered click at animation start; target loss at commit
cannot create a second automatic gesture. The weapon-handoff regression passes
3,295 checks/20weapon-view cases and506 waiting frames on both compilers,
including switching away only after an empty Time Strike gesture completes.

The seventh-map exploratory campaign run reached late waves without a native
runtime error, but the input bot died at 11,764 ticks before the exit. Logs remain
in `build/native-campaign/seventh-probe.*`. Six maps remain admitted; later-map
completion, original continue flow and other documented presentation gaps remain.
