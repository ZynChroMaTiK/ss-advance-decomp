# Original-asset mipmapping (2026-10-21)

Graphics now contains Mipmapping below Aspect ratio. Software and Vulkan allow
On/Off; GBA displays a disabled On value and skips the row with keyboard and
mouse input. The stored native preference survives switching through GBA.
Fresh settings and Clear Data enable mipmapping; v1..15 settings migrate with
native mipmapping Off to retain their previous appearance.

This reuses original ROM artwork, not generated downsampling. The GBA wall and
actor renderers already select the ROM's texture archives and sprite mip banks;
their recovered selection/raster routines are unchanged and remain always On.
Native rendering now consumes the same decoded assets. The original ROM is
unchanged. Palette indices, including transparent sprite index zero, remain
unaltered. Missing wall mip images retain the original base-image fallback.

Software wall columns estimate the screen texel footprint including oblique
walls. Pitched software polygons use perspective-correct UV derivatives;
Vulkan uses fragment derivatives. The original three wall levels are selected
at two/four base texels per pixel, with UVs scaled by the actual selected image
dimensions. Polygon depth, wall alignment and texture pegging remain tied to
the original geometry/base image. GPU material records carry the two additional
ROM image descriptors; repeated materials reuse uploaded data within the draw.
No hardware-generated mip chain or palette interpolation is used.

Native world sprites select among the existing resource mip flags according to
their projected size, keeping animation frame, facing and mirror selection.
Each authored mip supplies its own dimensions/origin and original world scale.
The common sprite choice feeds flat Software, pitched Software and Vulkan.
Fixed-screen weapons/HUD retain their existing artwork and layout. Original
flat-colored floors/ceilings and the fixed sky backdrop have no added mip art.
Mirrored scenes use the same policy; GBA reflections also force it On.

Settings remain16 bytes, including the same CRC over bytes0..11. Version16 uses
the formerly unused zero value in byte8's low version nibble; byte6 bit7 stores
the mip preference. Byte6's low seven bits remain ASCII T. v1..15 require the
original complete SSA-OPT signature, while v16 accepts the new flag. All other
packed fields retain their v15 positions. This does not change progress saves,
score files or legacy renderer migration. Older executables reject v16 rather
than silently misreading it.

Focused checks: `tests/test_mipmaps.py` (ASan/UBSan and leaks), the `mipmaps`
CTest target, settings round trips/migration, and Graphics keyboard/mouse tests
in `scripts/test-audio-settings.ps1`. Mip tests inspect submitted wall pointers
against the ROM mip archives, verify actual authored distant sprite selection,
exercise GBA preference override, and compare software On/Off/pitched output
with exact restoration when switched Off. Vulkan tests include a colored-level
fixture proving actual GPU level selection, real scene comparisons with mips
On, mirrored rendering and exact serial/parallel composition. No campaign
playthrough is required or performed for this presentation change.

Validation: 4,275 mip checks pass with ASan/UBSan and leak detection; Software
On/Off comparisons change881 pixels and restore exactly. All ten focused CTests
pass. Vulkan passes6,076,743 checks across28 paired scene draws with validation
enabled. Windows settings integration verifies persistence, forced GBA On and
restoration of saved Off. Surface checks agree between MSVC/GCC across55,383,897
assertions. The Graphics menu was also visually inspected.
