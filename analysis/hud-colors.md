# Original gameplay bar colors

The playable PC and reference renderers use `ssa_display_compose_gameplay`.
The static compositor remains available for raw OAM/asset diagnostics.

Original0750C initializes OBJ palette entries139..143 from0844A804 (blue boss
ramp) and selects the health ramp at0844A818, or0844A822 when signed health>100.
It schedules a VCOUNT interrupt at130. Original0787C switches entries139..143
at that row to0844A80E (green armor ramp) when signed armor is nonzero, retaining
the boss ramp otherwise. The next interrupt at142 installs the health ramp.
The bars share these palette indices; a single static sprite palette loses
their original colors. No sprite artwork, size, health values or thresholds
were changed. Both renderers read canonical player health and stat_c4.

`decomp.ps1 test-hud-colors` runs the actual0750C palette setup tail and complete
0787C handlers in test-only Unicorn, following their DISPSTAT schedule. It
compares native composed pixels against those hardware palette snapshots:
32 signed health/armor combinations,64 original interrupts,1,228,800 pixels,
including both health ramps, absent armor and neighboring untouched entries.
The caller's palette remains unchanged. See hud-colors-validation.json.

Existing display validation passes635 independent RGB frames and418,281
sanitizer assertions. PC renderer validation passes11,169,482 checks at seven
resolutions and24 jump/H frames with no game-state mutation. MSVC rebuilds
all106 libraries and the game. Hidden180-frame Windows smoke launches pass in
both renderers; their snapshots are under build/native-tests/hud-colors-*.bmp.
The PC snapshot was inspected and shows the original red health bar. These
targeted checks do not rerun or extend the eight-map campaign completion proof.
