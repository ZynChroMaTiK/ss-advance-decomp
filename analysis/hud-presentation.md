# Native HUD presentation integration

The native diagnostic viewer now combines the actual level renderer and actor
renderer with the original starting weapon and HUD assets. This is a bounded
presentation path, not a completed game loop. `hud_scene.c` owns its 84 sprite
objects, pool, animation state, tile cache and OAM output. It borrows the player's
canonical inventory and body, ROM-derived sprite assets and native math tables.
Its address must remain stable because embedded objects refer to their owner.

Initialization runs the native `1240C` loadout before the native `101F4` HUD
constructor, matching original `F84C`. A single player uses 75 slots: one weapon
and 74 HUD objects. No replacement art or always-resident tile mapping is used.
The normal frame applies the bounded idle weapon controller, scalar HUD
controllers, `E460` animation, `DBDC` visibility/cache preparation, `FCE4` OAM
construction and `E358` tile transfers. `display.c` then composes the original
240-by-160 display from the 120-by-120 world, original palettes and physical OBJ
tile bytes.

The requested weapon at player offset `+78` drives the HUD weapon/ammunition
selection. The committed weapon at `+28` drives the crosshair gate. These are
distinct during lowering. The wrapper projects both after the weapon step, so
the current frame sees a completed switch. In particular, the initial knife
does not display a crosshair. The whole-original integration test found and
corrected both projection mistakes; directed native tests retain the distinction
before and after commit.

## Whole original frame comparison

Run `tests/test_hud_scene_rom.py` with the project's Linux Python environment.
Its C bridge is `tests/test_hud_scene_rom.c`. The report is
`native-hud-scene-rom-validation.json`, including source and fixture hashes.

The test runs original `FA78` pool reset and whole `F84C` initialization, then
whole `F85C`, `E460`, `DBDC`, `FCE4` and `E358` for 128 consecutive frames at
each of the 16 level indices. The original weapon controller, all ordinary HUD
controllers, numeric formatting, animation, allocation, sorting and transfers
execute without substituted routine callbacks. Only BIOS/DMA memory copies are
modeled. Initial cache memory is seeded with the exact recovered `D9F8` defaults;
this test does not execute its heap allocator.

Input state 3 selects the genuine original `F150` branch that skips aim, firing
and weapon movement. Health, armor, score, time, automap visibility and pause
change over successive frames. Active notice text is excluded. The native
wrapper does not produce the original `101D0` submission queue, so the original
queue executes but is not compared. The separate sprite-dispatch proof covers
that queue.

Results:

- 16 complete original `F84C` initializations.
- 2,048 complete original presentation frame chains.
- 172,032 sprite records, including normalized linked-list references.
- Active/free indices, cache entries, stale allocator records, request-ring
  contents, numeric scratch, weapon/inventory fields and cache counters match.
- 2,097,152 OAM bytes and 67,108,864 physical OBJ tile bytes match.
- 64 RGB555 frames match an independent hardware sprite painter using the
  original OAM and VRAM results, both original palette tables and a controlled
  indexed background. That background tests transparency and priority; real-map
  backgrounds are covered separately below.

Fresh sprites have previous-frame `FFFF`. The original cache code can probe
beyond its allocation for masked image `3FFF`. The oracle explicitly zeroes that
unowned memory, while native cache code safely ignores the sentinel. This is a
defined test domain, not proof of the original undefined memory contents.

## Real-map native sanitizer integration

Run `tests/test_hud_scene.py`. It compiles `tests/test_hud_scene.c` with all native
sources under strict C11, ASan and UBSan, `-no-pie`, and leak scanning disabled.
The fixture reuses the established actor-scene fixture to construct real maps,
players and map actors. It does not run ROM instructions. The report is
`native-hud-scene-integration-validation.json`.

All 16 maps run 240 frames combining native movement, jump, H-view, map and actor
pixels, the actual starting loadout, HUD, animation, cache, OAM and display.
Canonical inventory health/armor changes are explicit test stimuli. The fixture
checks that HUD work leaves the registered body unchanged, mirrors inventory
correctly, hides the first-person weapon in third person, respects pause/input
state, and raises the idle weapon through native `12160`.

The final run passes 187,759 assertions across 3,840 ordinary presentation
frames, plus 32 directed pending-selection/commit frames. It includes 48 health
changes, 1,600 frames with the weapon hidden and Sam visible, 1,278 airborne
frames, 3,696 raised-weapon frames and 464 guards. Guards include nested updates,
invalid active indices/counts, missing object bindings, null or foreign paired
weapon pointers, invalid tick/palette inputs and calls after free. Free preserves
the borrowed player, world and asset owners.

Two complete captured frames per map pass the independent RGB reference,
32 frames total. Ignored local artifacts include
`build/native-tests/hud-scene-00-000.png` and `hud-scene-00-140.png`, showing the
first-person starting knife and third-person Sam with the HUD respectively.
The first frame can still lack cached HUD tiles; the real transfer schedule is
preserved rather than preloading artificial residency.

These tests cover presentation, idle weapon raise/lower and explicit state
projection. They do not establish firing, combat, complete enemy behavior,
active notices, audio, menus, multiplayer or a finished native game. The H hide
behavior and jumping remain requested native extensions, not GBA matches.
