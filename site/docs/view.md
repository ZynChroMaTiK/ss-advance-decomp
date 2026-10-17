# H-toggle third-person camera

`src/view.c` adds the requested PC camera feature. It is new native code, not a
recovered GBA routine or an exact reproduction of a particular Serious Sam PC
release's camera tuning. Existing matching C, movement, jump and original
camera-height routines are unchanged.

Initialize `SsaView` once per local player with `ssa_view_init`. First person
is the default. Feed normalized H key events to `ssa_view_key`; H and h are
accepted. Each fresh enabled press switches first/third person. Holding the
key or OS autorepeat does not toggle again. Key release rearms the action.
Disabled input, including menus/text entry, consumes a press without changing
mode; enabling controls while it remains held does not trigger it. Call
`ssa_view_focus_lost` on focus loss to clear the held latch, preserving mode.

The playable Windows frontend supplies keydown/up and repeat status through its
PC input adapter. H toggles the actual camera and rendered Sam body.

## Camera behavior

After horizontal movement, vertical/jump simulation and `ssa_camera_step`,
call `ssa_view_pose_pitch` with the resulting player body, original eye state,
heading and aim pitch. The zero-pitch `ssa_view_pose` wrapper remains available.
The heading convention matches the recovered game: zero faces +Y,
`0x4000` faces +X. First person returns the exact player X/Y and recovered eye Z.

Third person requests a position 256 world units behind the player and 64
above the eye. Its look target is 256 units ahead of the player's eye. `distance`
and `lift` are tunable signed 16.16 values, bounded to 0..256 and 0..64 units.
The rear boom now orbits with mouse pitch; the lift stays in world-up. This
preserves the default level framing while keeping the camera behind the player
at steep downward angles. Native PC presentation traces the real weapon contact
and turns the displaced camera toward it, aligning the original centered
crosshair with the shot. Player weapon aim is unchanged by this convergence.
The original reference renderer remains horizontal. See [chase aiming](chase-aim.md).

A four-unit camera clearance volume moves from the eye toward the desired
position in swept steps of at most one unit per axis. It checks static walls,
portal openings, floors and ceilings using the recovered map query. The boom
stops before the first obstruction. If less than 32 units fit, the actual pose
falls back to first person while preserving the user's third-person preference.
It expands again when space clears. Missing/invalid collision data also falls
back to the eye view. The sweep is deliberately conservative and does not
resolve dynamic enemies, moving entities or near-plane/frustum clipping.

Give the camera a **dedicated** `SsaMapCollision` workspace. Its queries overwrite
that workspace's temporary contacts and heights. Do not dispatch those contacts
as gameplay events, and do not overwrite a pending movement query with camera
queries. The camera never moves the player, changes heading or modifies the
original eye state. Jumping changes eye Z before pose construction, so both
view modes follow takeoff and landing.

## Rendering integration

`SsaViewPose` contains position, look target, actual third-person flag, effective
boom distance and obstruction flag. The renderer should show the player body
only for actual third person, hide the first-person weapon in that mode, and
restore the first-person weapon on eye-view fallback. Gameplay aim remains
anchored to the player. The base pose supplies a provisional point along aim;
`SsaNativeGame.pc_pose` replaces it with the dedicated exact-contact query for
PC camera convergence. Its `pc_camera`/`render_pitch` are independent from the
original reference camera and the player's weapon orientation.

The playable frontend renders Sam's original body/animation and hides the
first-person weapon during actual chase view, restoring it on eye-view fallback.
Both the original reference renderer and dynamic PC renderer consume this pose.

## Validation

Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\decomp.ps1 test-view`.
Native tests compile with warnings-as-errors, AddressSanitizer and undefined
behavior checks, using a non-PIE executable to avoid local WSL sanitizer startup
stalls. They cover H press/release/repeat, disabled controls, focus
loss, default mode, four cardinal headings, body/eye immutability, wall
retraction, close-wall fallback, portal floors/ceilings, automatic expansion,
missing map handling, configuration bounds and coordinate saturation.

The local WSL test repeatedly stalled with default leak scanning, while the
same executable and driver completed with `ASAN_OPTIONS=detect_leaks=0`.
The test runner therefore defaults to disabling leak scanning, retaining
AddressSanitizer and undefined-behavior checks. Explicit caller ASAN options
can override this. The child test has a 60-second timeout. Leak freedom is not
claimed by this validation; the selected sanitizer options are in the report.

They also simulate complete jumps and H toggles at all sixteen real player
starts: 2,048 camera frames, including destination clearance checks. Total:
10,962 assertions, including128 additional pitched clearance poses across the
same16 maps. Results and source hashes are saved in
`analysis/native-view-validation.json`. These are new-feature behavior tests,
not byte matches or differential comparisons with original third-person code.

The updated 256-unit rear offset,64-unit lift and pitch orbit pass these checks.
The rebuilt Windows frontend also passed its 600-frame gameplay smoke test and
the unchanged 5,258-frame level-completion check. The current chase snapshot is
`build/native-game-smoke/gameplay.bmp`; results are in
`analysis/windows-native-game-validation.json`.
