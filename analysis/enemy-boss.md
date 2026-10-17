# Native D7 and D8 bosses

`enemy_boss.c` implements both original boss dispatchers, D7 at `08014AB8`
and D8 at `08014648`, including their eight states. `enemy_control_runtime.c`
connects them to native actor animation, the shared projectile pool, combat,
script queues, group counters and presentation. `entity_runtime.c` now routes
both original entity types to that adapter. The native HUD reads the canonical
boss pointer and activation health for its original health gauge.

The playable frontend now admits eight maps. The sixth map has a complete
normal-input progression proof with earned inventory, native boss combat,
music and scripted completion. The later D8 boss map remains outside the admitted
campaign; controller tests alone do not establish its natural progression or
complete original feature parity.

The dispatcher decreases its signed halfword timer before every state handler.
Activation and corpse handlers decrease it again. Preserve that double aging:
the boss corpse uses the shared720 timer but expires in roughly360 updates.
An invalid native phase fails explicitly; normal phase values are0 and1.

The bosses first turn toward the player body. D7 shoots after actor animation
index8, D8 after index6. Each creates type6A with a96-unit forward offset and
64-unit height offset, then plays its launch sound before assigning vertical
velocity. The original distance/division/multiplication pipeline aims at the
player body's vertical center. D8 additionally sets the projectile state to1;
D7 leaves it at0. Five launches finish a volley cycle. Flag1000 prevents a
second launch from the same animation. Animation completion and damage reaction
still occur in their original order, including a shot on a completion frame.

After shooting, the boss turns toward its saved facing, performs its summon
animation and queues the original ROM script for the current phase. The phase
tables are `44C078/44C07A` for D7 and `44C074/44C076` for D8. While waiting, the
boss checks the exact indexed VM alive byte. Zero advances the phase modulo2,
restores five launches and returns to the attack cycle. Group indexing here
already comes from the ROM table; do not subtract one again.

Hurt/death facing uses the separate player record XY, while attack turning and
aim use the player body. Only waiting state5 enters the hurt animation; other
reaction states clear flag1 directly. Hurt sound uses the shared60 timer. Death
sets state7, selects the original death animation, releases its script group,
sets720, plays its death sound and requests music20. The next corpse update
clears the boss pointer before running the shared sinking/fade/release helper.
No boss-specific reward or fabricated exit is added.

The original creation routine dereferences its returned projectile without
checking allocation failure. The native boss reports an error on pool exhaustion
instead of dereferencing NULL. Earlier allocations and effects persist on all
callback failures. The private projectile create/commit services keep stable
storage under the existing shared combat/spawn locks and synchronize the D8
projectile state back to its owned actor. Presentation observers cannot mutate
or reenter the runtime. Script queue requests are consumed natively and are
not passed to presentation as pretend-success callbacks.

## Validation

`tests/test_enemy_boss_rom.py` executes the two complete original dispatchers
with Unicorn as a test-only oracle. Actual ROM math, fixed division, projectile
construction, animation setters and lifecycle helpers run. Presentation, group
release and script queue requests are ordered boundaries. It compares complete
projected entity/projectile state, globals, counters and callback snapshots.
There are4,948 original comparisons,5,708 reentry guards,22 callback failures,
276 projectile creations and477 script requests. This includes all state and
animation-marker branches, wrapped timers and both reinforcement phases.

`tests/test_enemy_boss_runtime.c` adds97,763 ASan/UBSan/leak assertions on all16
map layouts. A native script fixture places each boss at the original player
start and borrows its real boss script bank. Native clocks complete32 five-shot
cycles,160 projectile creations,32 script queues,32 hurt callback failures and
32 death/release cycles. It observes832 presentation/reentry events. Created
projectiles are inspected and released; this fixture does not claim flight,
reinforcement-wave completion or actual boss-map exits. Windows MSVC x64
produces identical counters. Another2,400 full native game frames use unmodified
ROM data on actual maps5 and11, explicitly queueing original encounter scripts10
and6 as a test fixture. The complete entity dispatcher, VM, animation and game
frame run;2,304 boss-health HUD frames and20 reference draws pass. Idle gameplay
does not prove a normal route through those levels. Existing projectile flight
proofs remain separate.

Run `decomp.ps1 test-enemy-boss`, `decomp.ps1 test-enemy-boss-runtime` and
`scripts/test-native-boss.ps1`. Reports are `enemy-boss-rom-validation.json`,
`enemy-boss-runtime-validation.json` and `windows-native-boss-validation.json`.

The actual decomp.wtf matcher was used for wait helper `14804`. Two attempts
reached88.0% and89.5%; neither is exact. The best candidate is
`recovered/enemy_boss_wait.c`; the claim was released with notes on remaining
literal-load ordering and register differences. Native behavioral recovery does
not add a byte-matched function.

## Natural sixth-map completion

The campaign bot now recognizes D7/D8 as real combat targets and handles maps
without an initial exit pickup. It only inspects world/script data and delivers
normal PC input; it does not queue encounters, grant equipment or release groups.
The sixth map starts with the fifth map's earned inventory, naturally triggers
the D7 encounter and completes the original VM progression in13,291 frames:
884,000 cumulative score,12 pickups,140 scripted enemy spawns and94 health.
There are1,037 boss lifetime frames,763 matching boss HUD frames,767 active
boss-projectile frames and752 wounded-boss frames, with observed group release
and defeat. This route defeats D7 before its reinforcement-wait state; its
summoning phases remain covered separately by the controller/runtime fixtures.

The real mixer runs all13,291 frames (10,632,800 stereo frames at48kHz), observes
one battle-music19 request and one post-boss20 request, and reports zero audio
errors. Local weapon, door and enemy loops now recognize19/20 as ongoing gameplay;
only accepting normal level-music2 previously silenced those loops in boss phases.
Four additional real-mixer minigun cases cover both phases and legacy/source-aware
sinks, retaining proper release and wind-down. The focused Windows/GCC suite now
passes126,794 assertions. Reports: campaign-validation.json,
windows-native-campaign-validation.json and weapon-fixes-validation.json.

The launcher now admits through map index7 and carries/restarts earned entry values using
the existing policy. This does not add interchapter cutscenes, later campaign
routes or complete original behavior parity. The latest route uses the restored
original minigun cadence, firing-animation weapon switching and immediate E use.
The bot adjusts distance, aiming and supply routes with ordinary PC input.
No original ROM instructions
execute in the native game.
