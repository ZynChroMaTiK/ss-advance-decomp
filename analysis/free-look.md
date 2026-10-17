# PC vertical aiming

The playable Windows frontend consumes both axes of captured relative mouse
input. Moving the mouse up looks up. Sensitivity is the existing 64 angle units
per count (65536 per turn). Pitch accumulates without wrapping and is limited
to +/-14564 units, approximately 80 degrees. Focus loss, paused/disabled input,
and repeated sampling consume pending deltas without applying stale motion.
Movement stays on the horizontal plane. H and jump retain their existing rules.

The first vertical input enables direct weapon aiming, replacing the original
vertical target search and adjacent-yaw retries. Hitscan uses full yaw precision,
the live eye height and tan(pitch), checking floors and ceilings before later
wall/entity hits. Projectile muzzle placement and slope follow the same aim.
The crosshair uses the original type18 HUD object in both views, including its
original placement, target animation and weapon/death/automap visibility rules.
The temporary custom projected crosshair has been removed. Level restart
resets pitch and restores the initial aiming policy. The explicit original
renderer option retains the horizontal GBA reference and ignores mouse Y.

Third-person PC view now orbits its rear offset with aim pitch while retaining
the world-up height bias and swept clearance. A dedicated presentation query
finds the actual weapon-ray contact, without impact-particle backoff. The PC
camera converges onto that point so the original centered crosshair marks the
shot, including nearby floors, walls and targets. No-hit rays converge at the
weapon range. Camera angles remain separate from weapon yaw/pitch and original
reference-renderer state. See [chase aiming](chase-aim.md).

`pc_renderer_pitch.inc` adds world-space camera rotation to the dynamic PC
renderer. Walls and upright directional actor billboards use near-clipped
triangles with reciprocal-depth interpolation of texture coordinates and depth.
Subsector planes are reconstructed once by clipping map bounds against BSP
partitions and directed subsector wall halfplanes, closing implicit subsector
edges. A topology fingerprint invalidates the cache on actual geometry changes;
sector heights and colors are read live. Flat sky lookup storage is cached by
resolution; column indices follow yaw and row indices follow pitch every frame.
Triangle rasterization walks covered scanline spans and increments perspective
attributes across each span.
Live sector heights preserve moving doors. The sky remains perpendicular to the
view, with horizontal panorama scrolling and vertical translation following the
projected horizon. At level aim its artwork covers the upper three quarters;
each column extends its edge pixels above and below the translated image. Original
art, HUD and weapon sprites remain in use. Pitch zero
retains the prior column renderer; nonzero pitch uses geometry rasterization.
Both paths retain the current vertical field of view and automatic resolution.

`test-free-look` compiles the full native composition under ASan/UBSan with leak
detection. Its independent reference traces individual 3D rays through sector
portals and planes, comparing 30,100 depth samples across 43 views, including
+/-80 degrees, angles on either side of level and live sector-height edits. Other
checks cover input overflow, focus/disable suppression, pitch clamps, H/jump,
read-only rendering, floor impacts, elevated target hits/misses, and an actual
rocket fired through normal weapon selection/input after a test-only inventory
grant. Its native muzzle position and velocity are checked against the aim ray.
Original HUD pixels also match the reference sprite composition at integer and
fractional scaling: 18,280 compared pixels. The current suite passes3,899,155
assertions plus the depth comparisons. Flat-sky fixtures identify visible sky
through actual geometry/HUD using two solid backgrounds, then check horizontal
and vertical test stripes across nine pitch/yaw combinations from-80 to+80
degrees at640x480 and127x73, including pitch reversals and returning to level.
Test-only sky floors expose the lower fill; all map colors are restored after.
They verify vertical placement, both edge fills, straight rows/columns, stable
cache allocations and that no row samples neighboring image
data. Snapshots
are `build/native-tests/look-up.bmp` and `look-down-chase.bmp`.

Run `.\decomp.ps1 test-free-look`. Results and source hashes are in
`analysis/free-look-validation.json`. These are PC extension checks, separate
from the original-routine ROM comparisons. Hidden Windows tests additionally
use `--smoke-look` to drive both axes through the actual frontend composition.
Run `powershell -ExecutionPolicy Bypass -File scripts/test-free-look.ps1` for
first-person up, chase down, 4K up and original-renderer checks. Results are in
`analysis/windows-free-look-validation.json`. The normal-input completion check
passes in4,653 frames after the unlimited-range extension. Current native free-look
checks and Windows up/down chase,4K and original-renderer snapshots now include
the sky pitch-tracking change. The HUD fixture uses the gameplay compositor's
restored original scanline bar colors.
Physical-device interaction remains unverified.
