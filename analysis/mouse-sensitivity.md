# Mouse sensitivity

Settings puts one Mouse sensitivity slider above Field of view
and the audio controls. It spans10..300% in10% steps;100% retains the prior
64 angle units per raw mouse count. Sensitivity defaults to100% for fresh settings,
Clear Data and migrated saves. The same value controls both axes; changing it does not enable vertical aim
when GBA or autoaim disables manual pitch.

The grid uses language row16; mouse label/bar32/40; FOV label/bar56/64;
music style88; music volume96/104; sounds volume112/120; Back144. Bars share
X32..176 and percentages start at184. English/German/French use the same layout.
Pointer adjustment and keyboard arrows use the same range and value mapping.

`ssa_pc_input_sensitivity` sets one positive coefficient for yaw and pitch.
Runtime settings apply rounded64*(percent/100) units/count. Accumulated input is neither replayed nor
rescaled when settings change. Yaw wrapping, pitch saturation, focus handling,
menu suppression and the original autoaim policy remain intact.

## Preferences v18

The record remains16 bytes with CRC32 over bytes0..11, stored at12..15. The
compact header is ASCII `SSA` followed by version18. New payload:

- Byte4: music style bits0..1, renderer2..3, display4, HUD5, aspect6.
- Byte5: music volume0..6, automap7.
- Byte6: sounds volume0..6, jump7.
- Byte7: FOV offset0..5, vertical aim6, third-person7.
- Bytes8/9: equal mouse slider values0..29 (percent=(value+1)*10). Existing
  two-axis v18 files migrate to the horizontal value in byte8; byte9 is still
  range-validated and both bytes are made equal on the next save.
- Byte10: in-game NETRICSA0, enhanced weapons1, bobbing2, surface decals3,
  water reflections4, mipmapping5, godrays6.
- Byte11 and unused high bits are reserved and must be zero.

Versions1..17 retain the prior decoder, with mouse sensitivity default100%. All earlier
renderer, FOV, audio and extras migrations remain unchanged. Older executables
reject v18 rather than silently reading the new payload as legacy preferences.

Validation covers equal axis deltas/defaults/limits, all30 slider values, unequal v18 migration,
legacy migrations, invalid records, keyboard/pointer edits, native application,
Windows preference persistence and localized menu screenshots. No campaign
playthrough is required for this settings/input change.
