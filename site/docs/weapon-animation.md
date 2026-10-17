# Native weapon animation timing

`src/weapon_animation.c` recovers the sprite animation setter `0800FC04`,
weapon idle reset `080116BC`, and each object's complete animation advancement
from `0800E460`. These are native data/state operations. They execute no ROM
instructions and require neither a renderer nor a GBA emulator at runtime.

`SsaWeaponObject` preserves the original 48-byte fixed-width object layout.
Its link field is an opaque integer, not a dereferenceable host pointer.
Allocation/list maintenance and presentation are outside this module. The
native initializer provides the cleared defaults from `FAE8`, replacing GBA
DMA/BIOS clearing with ordinary host memory initialization; that initializer
is checked natively and is not counted as a ROM differential comparison.

Load `SsaWeaponAnimations` once from the user's ROM. The loader copies the
528 animation words at `0844AF4C` and validates all 233 pointers in the table
at `0844B36C`. Actual sequences contain at most 18 words including termination.
It retains no ROM pointer. Failed loads leave the destination unchanged.

`FC04` accepts IDs 0 through E8 and FFFF; every other ID leaves the object
unchanged. It writes the ID and the low byte of the supplied period, then
clears frame index, elapsed time and completion byte. `116BC` first clears
weapon flags 1400 and requests current weapon minus one, period 3. Current
weapon subtraction wraps/truncates before the animation validity check.

Advancement preserves these original details:

- GAME flag 1 skips only sprite type 2. Disabled animation FFFF sets the low
  14 bits of both previous/current frame fields without changing timing.
- A valid current frame is copied even when period is zero. Frame high bits
  C000 are preserved. Sprite flag 10 increments elapsed time by one; other
  objects use the GAME tick byte. Addition wraps to a byte before comparison.
- Each elapsed period consumes another frame. Sequence FFFF loops to frame 0
  and sets done to 1. FFFE sets done to 1 and period to FF, retaining the last
  frame and index; it does not change the animation ID to the disabled sentinel.
- FFFA through FFFD set the completion byte to FA through FD while advancing
  normally. Completion remains latched until a setter or consumer clears it.

Call `ssa_weapon_animation_step` after gameplay consumes the preceding frame's
completion state. Original main `0244C` performs entity updates, movers and VM,
then sprite controllers `F85C`, then `E460`, and finally rendering. This module
does not implement the outer object dispatcher or draw the weapon.

`tests/test_weapon_animation_rom.py` executes the complete original FC04,
116BC and E460 routines without instruction hooks. It compares entire object
bytes, control bytes and preserved game state. The native shared library uses
UBSan. The passing results are:

- 131,072 setters: every 16-bit ID and every 16-bit period.
- 8,192 idle resets, including wrapped current weapon values.
- 59,648 original-data tick cases: all 233 sequences and every byte timestep.
- 74,560 consecutive real-data playback frames with periods 0, 1, 3, 5 and 255.
- 768 disabled-animation cases and 1,024 explicit marker-data mutation cases.
- 256 complete original active-list frames with 84 objects in shuffled order.
- 23 native guards and one initializer check.

There are **275,520 ROM comparisons** total. The 84-object batch counts as one
whole routine comparison per frame, not 84 comparisons. Actual table extraction,
completion timing, paused type handling and byte overflow are covered. Full
counts and the native source hash are recorded in
`native-weapon-animation-validation.json`. Native animation timing alone does
not establish projectile behavior, sprite rendering or a playable native port.

The straightforward struct-based candidate in
`recovered/weapon_animation_set.c` matches `0800FC04` exactly: 56 bytes and
28 instructions, first manual attempt through decomp.wtf `compile_and_diff`.
The attempt record is `weapon-animation-match-attempts.json`; the claim was
released automatically and behavior notes were saved through `add_note`.
Export/rebuild is coordinated by the parent integration task.
