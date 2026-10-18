# Third-person aim alignment

The PC chase camera now moves with aim pitch and points toward the common
sight ray's contact point. The original centered crosshair artwork, target animation
and visibility rules remain unchanged.

`ssa_view_pose_pitch` rotates the rear boom while keeping the configured lift
in world-up: the horizontal rear distance is `distance*cos(pitch)` and vertical
offset is `lift-distance*sin(pitch)`. At level aim this retains the256-unit rear
distance and64-unit height bias. It never swings in front of the player at steep
downward angles. The existing four-unit clearance sweep, one-unit substeps,
retraction and close-wall first-person fallback remain active. Jumping moves the
pivot with the live eye. The original `ssa_view_pose` API retains zero-pitch
behavior for existing callers.

The close-wall fallback now requires at least64 units of retracted boom distance,
increased from32. Below64 units the camera returns to the player's eye; H's
third-person preference remains set, so clearance automatically restores chase
view. This applies to both software and GBA rendering. Default framing remains
256 units behind and64 units above the eye. The camera fixture checks63/64/65
units of clearance, including the exact switch boundary.

`SsaNativeGame.camera_trace` owns separate query storage. During view preparation
it traces the player's real weapon heading, shoot height and slope along a common
unlimited sight ray for every weapon. Free
aim uses full yaw precision and live eye height; the original aiming policy uses
its body-facing heading,96-unit shoot height and current auto-aim slope. The new
`ssa_weapon_hit_query_unlimited` with `exact=1` returns contact coordinates without the
original wall/entity offset used for impact particles. It applies no damage or
script effects and cannot overwrite the firing trace. No-hit rays use their
map-boundary endpoint. Camera convergence is independent of attack reach: switching to
the chainsaw no longer pulls the target closer and tilts the camera downward.
Weapon 1 retains96-unit melee reach; ranged attacks now have no distance cap.
The ROM range table is preserved for the original routines and nearby muzzle
placement. See [unlimited range](unlimited-range.md).

`ssa_view_angles` turns the displaced camera toward that point using ROM angle
tables without discarding coordinate fractions. `pc_camera` and `render_pitch`
hold this PC presentation state; `render_view.camera` retains the original
reference camera, and player aim remains unchanged. The PC renderer and sky
cache consume the presentation pitch, including convergence at level aim.

Third-person render snapshots hide type96 particles carrying flag0x2000, the
five original muzzle-flash families (particle IDs8–12). The snapshot clears
fixed-screen mode and sets invisibility because the reference fixed-screen
projector bypasses the normal invisibility check. Actual entity flags, particles,
animation lifetime and weapon state remain unchanged. Returning to first person
restores normal snapshots immediately, including flashes already alive during
the H toggle. World impacts and projectiles retain their normal presentation.

The local player's body facing is captured when death animation0x68 begins.
Dead render sources use this captured direction while mouse yaw continues to
control the camera. Looking around the corpse therefore selects the original
directional death images instead of rotating its body with the view. Both the
base body and white-shirt overlay keep all five directions and six animation
frames, including the final held corpse frame. Gameplay facing and original
death timing are unchanged; living sources continue using live movement facing.

Run `.\decomp.ps1 test-chase-aim`. ASan/UBSan with leak detection passes9,944
assertions over232 aligned views, including88 third-person views across four
headings and eleven pitches through +/-80 degrees, with jumping, and144 frames
of normal weapon lowering/raising through pistol/chainsaw/long-range selection.
Every sampled view is identical for all ten weapon selections at the same
player/world pose. Independently projected sight contacts
land within0.213 pixels of screen center at4K. It also checks that preparation
does not modify the player or firing-query state, rendering is read-only,
clearance holds and H restores first person. Test-only weapon inventory is
granted before normal selection/raising; aiming uses actual PC input events.
The weapon-independent view regression fails against the previous camera code;
the original ROM range records are explicitly checked independently of PC policy.
All five muzzle-flash families are checked across third/first/third H toggles:
the reference projector emits none in third person, PC pixels match a flash-free
frame, first person draws them again, entity flags stay intact, and a world
impact control retains its normal flags.
The real combat/death/mouse-input path adds64dead-player frames spanning both
renderer policies. It verifies fixed body facing, continuing camera rotation,
all six animation frames and all five projected image directions for both body
and shirt, with native reference and software draws. MSVC gives the same result.

`test-view` separately passes10,837 assertions,2,048 real-map camera frames and
128 pitched poses across all16 maps, including obstruction retraction/fallback.
Original hitscan still passes8,192 ROM comparisons. The PC perspective/HUD suite
and full normal-input level completion pass; Windows normal startup/restart,
600 gameplay frames and4,653-frame completion pass as well. Earlier Windows
4K/free-look snapshots predate the range extension.
The corrected chase snapshot is `build/native-game-smoke/look-down-chase.bmp`.
Physical-device interaction remains unverified. Evidence is in
`chase-aim-validation.json`, `native-view-validation.json`,
`free-look-validation.json` and current Windows validation reports.
