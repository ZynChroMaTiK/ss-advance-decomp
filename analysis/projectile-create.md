# Projectile creation

`projectile_create.c` recovers complete `08010830` and the slope wrapper
`080109F0`. It uses native fixed arithmetic and the ROM's trigonometric data;
it executes no ROM instructions. Allocation `05170` and insertion `A5D0` are
explicit callbacks, so the runtime can bind the existing pool/world ownership.
There is no animation call in either routine.

The request carries the original full-width type, source, forward/side offsets,
Z offset and heading. Heading is truncated to 16 bits for sine/cosine; facing
stores bits 8–15. Position is source plus the rotated forward/side vector;
each multiplication floors separately before wrapped addition. Source velocity
is inherited only when source body flag `0x10` is absent. Type exactly `0x6A`
gets speed 12 and radius/height 32; all other full-width type values get speed
16 and radius/height 16. Stored entity type truncates to a halfword afterward.

Creation sets state/timer zero and flags `0x104`, then inserts. Only after
insertion does a player source set owner to source player index plus one and
OR flag `0x4000`. The source pointer is also assigned after insertion. The aimed
wrapper then overwrites Z velocity with fixed(speed, slope), including for
nonplayer sources; it does not add inherited vertical velocity.

`SsaProjectileCreator` rejects nested creation. Allocated projection and body
must remain stable; no replacement, freeing or source mutation is permitted
inside callbacks. `allocate` returning success with NULL is normal exhaustion;
the API returns 0. Successful creation returns 1; invalid integration or callback
failure returns -1. Output retains any allocated object after later failure,
and earlier insertion/effects are not rolled back. The caller owns cleanup.

Validation: `tests/test_projectile_create_rom.py` passes **69,632** whole-ROM
comparisons, including every 16-bit type and every heading, full-width argument
edges, player/nonplayer velocity inheritance and **256** allocation exhaustion
cases. The original sine/cosine and fixed multiply instructions execute.
Allocation/insertion are controlled and compared through ordered full 116-byte
snapshots, including insertion-time floor/ceiling changes. There are **12**
native guard checks, **278,028** callback reentry checks and **2** callback failure
paths. Native code builds with strict warnings and undefined-behavior sanitizer.
Hashes and counts are in `projectile-create-validation.json`.

`recovered/projectile_create_aimed.c` matches `080109F0` exactly: **48 bytes,
24 instructions**, verified through the actual decomp.wtf `compile_and_diff`.
The first attempt was 99.5% because `_0800036C` was not a recognized symbol;
using the canonical `sub_0800036C` thunk name resolves it without changing the
generated instruction sequence. The second attempt is byte-identical. No
catalogue split was required. This matching candidate is separate from the
guarded native implementation.
