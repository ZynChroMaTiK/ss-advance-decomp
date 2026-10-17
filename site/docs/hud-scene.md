# Native diagnostic weapon and HUD presentation

`hud_scene.c` owns the native84-slot sprite pool, original HUD objects, sprite
animation state, cache, OAM and normal display registers. It borrows stable
sprite assets, math, inventory and player body. Keep the owner at a stable
address until free: its embedded pool and controller point into its storage.
No heap-owned game assets or world bodies are freed by this adapter.

Initialization runs real1240C weapon loadout allocation before101F4 HUD creation,
matching F84C. It supports the original loadout setting0..36 and initializes
the74 single-player HUD objects. The viewer selects setting0, producing the
original starting knife. This is not a new campaign/loadout menu.

Original HUD selection reads requested weapon at PLAYER+78, while the crosshair
reads committed weapon at PLAYER+28. Both are projected after the weapon slot's
update, so a same-frame commit reaches the HUD immediately. Knife1 and weapon8
hide the crosshair through the original controller. Public state field comments
identify those original offsets despite their earlier generic field names.

The API is deliberately `step_idle`: it runs12160 weapon raise/lower and the
recovered scalar HUD handlers, then E460 animation, DBDC cache preparation,
FCE4 OAM and E358 uploads. It does not replace the full F150/122AC firing path
with a successful dummy callback. Input-state3 skips weapon raising, matching
the original wrapper branch. H hides the first-person weapon's presentation
flag while the separate actor pass exposes Sam's body snapshot.

HUD source fields are explicit. The viewer uses its spawned entity's actual
health and inventory armor. Score/time remain initialized until game orchestration
is integrated. Tests can drive real canonical health/armor changes separately.
Shared numeric scratch and glyph widths persist across sprite order and frames.
The original unlimited-ammo HUD write for weapons1/2 is copied back to the
borrowed inventory. Active text notices fail explicitly; their inactive branch
is recovered. The adapter does not synthesize score, kills or gameplay events.

Drawing composes actual world/actor indices, original background/OBJ palettes,
raw OAM and physical cache bytes into240×160 RGB555 output. Hardware sampling is
`source(x/2,floor(3*y/4))`. The platform expands RGB555 to its32-bit DIB and
letterboxes the3:2 display in the current client area. See `display.md` for
verified normal-mode scope and explicit unsupported effects.

Native integration uses all16 actual maps, movement/jump/H, the real actor pass,
loadout, HUD controllers and presentation pipeline. Independent pixel references
compare captured hardware-presentation inputs with resulting RGB pixels.
Evidence is in `native-hud-scene-integration-validation.json`. The Windows
driver adds3840 hidden-window frames,3856 HUD draws,2112 raised first-person
weapon draws and21807376 changed presentation pixels. Captured private BMPs
show the original knife/HUD and H body. Physical input delivery remains untested.

The separate `test-hud-scene-rom` executes16 complete original F84C initializations
and2048 consecutive original F85C/E460/DBDC/FCE4/E358 frames. Input-state3 selects
the genuine F150 branch that skips aim/fire, with no gameplay handler stubs.
It compares172032 sprite records, numeric/cache/pool state,2097152 OAM bytes
and67108864 physical OBJ bytes, plus64 independent RGB references from original
OAM/VRAM. Native real-map integration passes187759 sanitizer assertions and464
guards, including32 directed pending/committed weapon frames. The sentinel retention probe's unrelated memory
is explicitly zero-seeded. Native submission-queue stores are outside this
adapter's scope and are not compared. See `native-hud-scene-rom-validation.json`.

AI, firing/combat, pickup execution, scripts, audio and complete game startup are
still outside the diagnostic viewer. Native HUD rendering does not turn that
viewer into the completed game port.
