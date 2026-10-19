# Retained older HUD code and artwork

The European ROM contains functional older HUD components, not only graphics.
It does not currently provide a complete older-layout constructor or selector
that this audit has found. A subsequent native reconstruction now makes the
retained artwork usable through Extras -> HUD -> Classic. See [classic-hud.md](classic-hud.md)
for its live bindings, footage reference, implementation and validation. The audit
below describes the original ROM, independently of that added presentation mode.

## Confirmed code

| Original routine | Retained behavior | Current binding |
| --- | --- | --- |
| `0800E700`, sprite type5 | Three health digits, subtypes0/1/2 selecting hundreds/tens/ones. Reads display-local entity health at+32 and chooses animations24..2D (hex), the large green digit images110..119. Automap hides it. | Still called by sprite dispatcher at0800F916 for type5. Current101F4 constructor never creates type5. The equivalent handler already exists in native hud_sprites.c. |
| `0800EFB4`, sprite typeB | Fixed eight-digit numeric display, subtype0 for ones through7 for ten-millions. Reads GAME+34C and chooses animations2E..37, small green digit images120..129. Automap hides it. | Still called by dispatcher at0800F93A for typeB. Current101F4 constructor never creates typeB. The original meaning of GAME+34C remains unresolved; this is not the current score field GAME+88. |
| `0800EE60`, sprite typeA | Formats pending bonus GAME+8C and places it at GAME+75. | This belongs to the level-completion tally, so its absence from ordinary HUD construction does not make it an obsolete HUD component. |

The old digit handlers retain real formatting/animation selection; they are not
empty stubs. Their existing dispatcher calls also mean these functions should
not be described as having no references. Their missing ordinary object creation
is the relevant distinction.

The later Classic corrections also checked the current original F1C0/F27C
weapon/ammo handlers directly: player+78 indexes animations42..4B andDF..E8.
Forty whole-ROM calls cover ten weapons, both handlers and automap visibility;
the first animation images and original ammo capacities are recorded in the
audit JSON. They establish retail identities and data, not a recovered older
green HUD constructor. See the mapping table in [classic-hud.md](classic-hud.md).

## Artwork and animation bindings

The exported sprite graphics were visually inspected against the ROM tables:

- Images63/64: old green outlined panels, animations17/18 (hex).
- Images65/66: green/white small status panel; animations19 and1A.
- Images80..88: green head/heart, frame, ammo-like and other interface icons;
  animations1B..23. Some icons' intended roles remain inference from artwork.
- Images67..75: the nine older green weapon icons. None occurs as a frame in
  any of the233 sprite animation sequences. This does not prove there is no
  possible direct/computed image access elsewhere in the game.
- Images110..119 and120..129: the two green numeral sets selected by the
  surviving handlers above. The ROM also retains another white digit set.

No animation17..23 is selected by the current original HUD constructor. The
modern bar/icon constructor selects animations54..5B and other later resources
instead. A static immediate-argument scan of08000000..0801D000 also found no
direct constant call selecting17..23 and no candidate immediate type5 store to
an object's+4 field. That scan is supporting evidence only: dynamic arguments,
register aliases and general control-flow reachability are not proven by it.

Consequently, restoring an older HUD would require creating/binding its objects
and reconstructing their positions, scaling and missing data flow. The retained
assets alone do not establish an original full layout, and no hidden old-HUD
menu option was found.

## Reproducible checks

Run `scripts/audit_legacy_hud.py` with the local Linux Python environment. The
report is `legacy-hud-audit.json`, with the unchanged ROM identity and audit hash.

- 66 complete original E700 calls: eleven health values, all three subtypes,
  normal/automap visibility. Original FC04 and arithmetic execute unchanged.
- 96 complete original EFB4 calls: six nonnegative values within eight digits,
  all eight subtypes and normal/automap visibility. This establishes that input
  domain only; it is not proof of the unidentified source field's gameplay role.
- 160 complete original101F4 calls: all16maps,1..4players and every motion-player
  index. Only the F9B4 allocator is controlled, returning stable fresh48-byte
  objects; construction and original animation setter execute from the ROM.
  Counts are74single-player objects or59+2*players, matching prior recovery.
  None is type5/B; none selects animation17..23.
- Static Thumb BL scan through080C6000 confirms the dispatcher call sites above.
  It does not establish the absence of all indirect/computed references.

The audit uses Unicorn only as a test tool. No ROM edits, new executable build,
campaign replay, new matching claim or runtime emulation was introduced.
