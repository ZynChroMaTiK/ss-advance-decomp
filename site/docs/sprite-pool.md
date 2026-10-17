# Native sprite pool and HUD controllers

`sprite_pool.c` recovers the original84-object allocation bookkeeping and
`hud_sprites.c` constructs the original HUD and updates its ordinary gameplay
sprites. All objects use the existing exact48-byte `SsaWeaponObject` projection.
The pool borrows the dispatcher's84 distinct object pointers; the caller owns
their stable storage. This permits actual weapon runtime objects to be bound
before initialization, rather than copying a separate invented weapon preview.

## Initialization and ownership

The original `0800F84C` calls **`1240C` weapon initialization before `101F4` HUD
construction**. `101F4` creates74 objects for one player, or59+2*players for
multiplayer. Its source is fixed positioning/type/animation setup, not a table
of invented HUD values. All constructor fields and allocation order are native.

`ssa_sprite_pool_init` binds an existing `SsaSpriteDispatch` whose84 object
pointers are already populated, then performs `FA78` reset. The recovered
operations are:

| Original | Native operation | Important retained behavior |
|---|---|---|
| `FA78`/`FAE8`/`101C4` | init/reset | Clears every payload, sets both scales4096, layer1, animation/previous-frameFFFF and visible flag1; resets counter/submission count. |
| `F9B4` | allocate | Pops the next free slot; writes only slot and active-index; appends the physical slot to active order. |
| `FA08` | release | Pushes the released slot, swap-removes its active entry, updates the moved object's active-index, then initializes the released payload. |
| `FB5C` | mark | ORs flag04000000 into every active object. |
| `FBA0` | remove types | Inclusive type range; captures initial count and retains the cursor after a removal so the swapped object is processed. |

Unused active/free/submitted array entries remain stale exactly as in the ROM.
Reset preserves dispatch current/state/index. Release does not adjust those
fields either: the already recovered `F85C` dispatcher rereads the current
object's now-zero type and follows its original cursor behavior.

Allocation/release can run inside a sprite dispatcher handler. Bindings must not
change, objects cannot move or be freed, and the pool cannot be reentered. Reset
is prohibited during dispatch. Native checks reject duplicate bindings, broken
active/free membership, corrupt object indices, foreign/double releases and
out-of-capacity state. `ssa_hud_sprites_create` preflights its required free
capacity; the original instead dereferences a null allocation when exhausted.

`E5F0`'s generic heap allocation and OAM buffer setup are outside the pool API.
`E914` is **notice text formatting/state setup**, not sprite creation. Earlier
callback naming that calls it sprite creation should not be treated as evidence
that it allocates `SsaWeaponObject` records.

## HUD state and handlers

`SsaHudSpriteState` projects real display-local state: movement flags, current
weapon, weapon flags/action/aim marker, body health, armorC4, optional boss health,
ammo, score/time, game bytes/flags and the pause rectangle. It also retains the
original numeric and rectangle scratch. The caller supplies authoritative
gameplay values. Native code does not fabricate changing health, score, ammo,
firing, impacts or enemy behavior.

`ssa_hud_glyphs_load` copies the real128 glyph-width bytes at `0844BC0C` without
changing state or scratch. Ordinary initialized HUD dispatch is covered:

| Type hex | Routine | Recovered behavior |
|---|---|---|
| 3 | F100 | Display-local automap visibility and completed-animation release through the native pool. |
| 4 | F0B8 | Automap visibility; F0B4 is an empty original helper. |
| 5 | E700 | Original three health-digit selector, including unsigned divide/modulo on sign-extended health. |
| 6 | E7A4 | Ammo digits, weapon1/2 ammo999 writeback, shared3-entry layout and original Y positioning. |
| 7 | F1C0 | Weapon icon animation; IDs1..10 map42..4B, out-of-range selects4B. |
| 8 | F27C | Ammo icon animationDF..E8, out-of-range selectsE8; weapon1 moves it toY=C800. |
| 9 | ECF8 | Score digits, shared8-entry layout, grouping gaps and flag04000000 clear. |
| C | F344 | Shared5-entry time layout, actual `%d:%02d` formatting after ticks/60. |
| D | E9D4 | Complete inactive-notice or automap branch only: clears game flag200 and sprite visible bit. |
| E | F4A4 | Pause rectangle and overflow continuation sprite, including shared four-word scratch. |
| F | F5A4 | Body-health, armor and boss bars/icons, including boss health/30, wrapped width multiplication and positioning. |
| 10 | F6C8 | Game-byte-selected animation6B+value. |
| 11 | F724 | Per-player byte animation, shared one-shot skip byte, automap gate. |
| 12 | F794 | Aim marker positioning and D9/DA animation selection from weapon flags, preserving original visibility gates. |

Type2 `F150` is already part of `weapon_runtime.c`; it is not replaced by an
idle HUD handler. Other sprite controller types remain explicitly unsupported.

The numeric layout is not recalculated independently for each digit. The last
subtype (ammo2, score7, time4) rebuilds the shared scratch, while earlier subtypes
consume its previous contents. This includes original first-frame/stale-buffer
behavior. Ammo and score use `095FC`'s unsigned decimal conversion. Negative
signed ammo therefore becomes a ten-digit unsigned value in the original,
overflowing its3-entry layout; native code rejects that unsafe case. Native
limits preserve the original3/8/5-character capacities rather than overwriting
adjacent scratch. Time's previous-glyph spacing uses a quarter width, unlike
ammo/score's half width.

Caller adapters must copy back genuine mutations: ammo1/2 can become999,
inactive notices clear game flag200, and the per-player skip byte clears after
its one-shot use. Scratch persists between frames. Original GAME+68 motion
index controls multiplayer constructor marking; display-local0300464E selects
the live HUD values and must remain distinct.

The active-notice path of E9D4 is **not implemented**. It returns0 with object
and state unchanged, as do unsupported controller types; adapters must treat
that as an explicit unsupported path. A handled controller returns1 and invalid
bounded input returns-1. A failure can preserve earlier scalar changes, matching
the surrounding modules' prefix-effect contract. This is not a silent successful
message renderer.

## Validation and exact matching

`tests/test_sprite_pool_rom.py` executes whole original routines, modeling only
the BIOS CpuSet and DMA memory-copy service used to clear objects. The arithmetic,
allocation, native FC04 animation setup, unsigned division, formatting library,
HUD branches and shared scratch transformations execute in the original ROM.

The final proof passes18,576 pool calls,480 complete101F4 construction cases
across all16 maps/player counts/motion-player indices and existing allocation
prefixes,40,960 scalar/bar/rectangle/inactive-notice controller comparisons,
16,384 numeric controller comparisons and1,024 completeF100 transient-release
comparisons. Full payload/active/free/submitted backing is compared, including
1,600,704 object records. There are679 native guard assertions. A separate
ASan+UBSan executable runs256 native pool/HUD cycles using actual ROM glyph
widths and valid health/ammo state (`-no-pie`, leak scanning disabled).

Hashes and exact scope are in `native-sprite-pool-validation.json`.

The actual decomp.wtf byte matcher verified **FB5C exactly:68 bytes /34
instructions**, on the second manual compile. The matching source is
`recovered/sprite_mark.c`; using an explicit byte-offset temporary preserves the
original load scheduling where typed array indexing did not. The engine released
the successful claim and stores a recovery note. Export/full-ROM rebuild remains
the parent integration step; no near-match is counted as exact.

HUD construction/controllers do not alone prove final OAM composition, physical
input, firing or the complete native game. Those require the separate renderer
and gameplay adapters, with their own integration evidence.
