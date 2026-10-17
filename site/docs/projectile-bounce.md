# Projectile wall reflection

`projectile_bounce.c` recovers full `08004718` and its `0800468C` wall predicate
using the native wall-ray traversal. Each update traces three leading/trailing
corner rays, chosen from the signs of X/Y velocity. All three traces run even
after a blocking line is found. The nearest strictly smaller fraction wins;
previous fraction/line and ties retain original behavior. No position is moved.

One-sided lines pass when the body lies on their reverse side. Two-sided lines
pass only when opening top is at least wrapped Z+height AND opening bottom is
strictly less than Z. Equality at the floor blocks. Opening state survives
between rays; one-sided predicates do not rewrite it. Fraction starts at
`0x10001`, and values at most `0x10000` produce reflection.

Reflection quantizes displacement to signed whole units before computing its
angle/length. The outgoing angle is twice the line tangent minus the movement
angle. Native sine/cosine and fixed division reproduce original velocity,
including zero ticks and wrapped height tests. This is original behavior,
not a floating-point physics replacement.

Initialize one dedicated `SsaProjectileBounce` against a loaded map. Keep the
map stable, free the scratch before unloading, and do not share enemy sight,
movement or camera scratch. Nested bounce, ray, wall-collector or traversal
operations are rejected. Init/free return 1/-1; update returns 1 for reflection,
0 for no blocking wall, -1 for invalid integration.

`tests/test_projectile_bounce_rom.py` passes **16,384** complete original routine
comparisons across **all 16 maps**: **5,141** reflections and **11,243** clear
updates. It executes all **49,152** original wall rays and **19,377** wall
predicates, comparing complete entity bytes, current/previous blocking lines
and fractions, opening globals and traversal blocking fraction. Only the BIOS
integer division service is substituted. Maximum observed intercept count is
8, within the original 32-record scratch. **144** native guard checks cover
nested scratch and repeated initialization/free. Strict compiler warnings and
undefined-behavior sanitizer pass. Evidence and source hashes are recorded in
`projectile-bounce-validation.json`.
