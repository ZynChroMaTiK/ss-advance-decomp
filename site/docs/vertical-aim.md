# Optional vertical aiming, 2026-10-20

Options > Extras includes Vertical aim, On by default. English,
German and French labels use the existing one-tile spacing. Keyboard arrows,
confirm and mouse selection cycle its value; settings persistence and Clear
Data use the common field catalogue.

When Off, native software rendering and GBA rendering use the same original
auto-aim queries, slope, target and marker. Mouse yaw remains active; vertical
mouse input is discarded before polling and cannot latch free aim. Applying
the setting also clears accumulated pitch immediately. The camera stays level
in first and third person, with the cursor projected onto the auto-aimed shot
using a non-bobbing eye. Native cursor projection uses the software renderer's
focal length; GBA retains its original reciprocal-table projection. Rendering
resolution, aspect ratio, jump and HUD selection remain independent.

When On, software rendering retains its existing vertical mouse aiming and
third-person camera convergence. GBA always uses original aiming, even with
the preference On. No original weapon controller or auto-aim routine changed.

Settings v13 keeps the16-byte disk size and all previous payload bit positions.
Byte7, formerly the signature terminator, stores bit0 for vertical aim. Its
other bits are reserved and rejected. The seven-byte signature remains
`SSA-OPT`, with CRC coverage unchanged. v1..12 files require byte7 zero and
load with the new preference On while retaining their existing choices.
Existing v13 files retain their explicit On/Off choice. Fresh settings and
Clear Data use On, following the user's updated default preference.

Validation:

- Full static Windows Release build and all eight focused CTest checks pass.
- Native settings:1,236 checks, including120 identical input frames per renderer,
  first/third-person auto-aim equivalence, vertical input suppression, enabled
  software pitch, GBA override and clearing pending input on disable.
- Settings fields:140 checks, including v12 migration, v13 round trips and
  rejected reserved/header flag bits.
- Actual elevated targets and cursor OAM placement:1,289 checks per mode,
  nine projected views and192 non-bobbing cursor cases each. Maximum cursor
  error:1.310 GBA pixels for the original quantized path,0.003 for native.
- Enabled vertical third-person aiming:9,944 checks,232 aligned views.
- Settings/menu/audio suite:39,065 portable checks and23,670 Windows checks.
  All three localized menu labels, mouse/keyboard toggling and disk persistence
  pass; Windows audio output is muted.

No campaign playthrough was run. Existing pitch-specific regression fixtures
explicitly enable the extra.
