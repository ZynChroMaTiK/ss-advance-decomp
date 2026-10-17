# Attached conversion effect

`projectile_effect.c` recovers the complete type-E2 controller at `080113E4`.
It follows the target stored in entity+20: unlink, copy target X/Y, set Z to
target Z + height + 48 units, then insert again. A target with flag20 requests
destruction instead. The native caller defers release until the helper returns.
All position arithmetic wraps to 32 bits. This effect does not apply the
target's periodic health drain or retargeting. Those behaviors belong to the
enemy controller and shared navigation; see `projectile-conversion.md`.

`test-projectile-effect` compares 8,192 complete original executions with native
C under UBSan. The test compares the full entity bytes and callback order/XYZ;
only unlink, insert and release are controlled. Six invalid-input guards,
8,349 callback reentry checks and both callback failure paths also pass.
Earlier effects remain on callback failure. See
`projectile-effect-validation.json` for the source hash and ROM identity.

The caller must retain both entity and target storage, prevent target slot reuse
while attached, and consume the destruction request before releasing either.
No rendering or animation playback is implemented here.

The separate matching candidate `recovered/projectile_effect_follow.c` is
97.9% similar, not byte-identical. Three manual compiler attempts retained a
different ordering of the flag load and mask constant; the claim was released
with notes. This near-match is not included in exported C or exact-match counts.
