# Native camera and display-local view selection

`render_view.c` recovers the whole original02B88 routine. It clears through07C94,
scans all four players for active4/automap8 flags, updates their unsignedA8 zoom
from held1/2 and GAMEticks, then selects the display-local player's automap or
world route. It applies the original upper65536/lower4096 clamps after32bit
wrapped arithmetic. Player count and movement-control gates are not consulted.

World routing runs native94E8 sine/cosine with negated camera yaw, then calls
the remaining B6B0 world event and optional02CA0 overlay. Original camera fields
at03004020 are x/y/z+0/4/8, heading+C, sine+10, cosine+14 and sector+18. The host
camera is deliberately not a GBA binary overlay. Clear/automap/world/overlay
callbacks carry their original scalar arguments; callbacks may change scalar
state that later reads observe. Failures retain completed prefix effects.

`ssa_render_view_pose` copies the native eye/chase pose and gameplay yaw, finding
the camera's containing sector in the actual decoded map. Displaced third-person
poses therefore do not reuse the player's sector. It leaves gameplay state
unchanged and rejects an invalid map or busy renderer before changing the camera.
The world step refreshes cached sine/cosine. There is no pitch/tilt or player-body
drawing in this adapter.

`test-render-view` passes84,032 whole02B88 comparisons, including all65,536
negative-yaw cases with real94E8 instructions, all256 tick values, unsigned zoom
wrap/clamp edges, live callback mutations and5 callback failure prefixes.
It checks206,005 callback reentry attempts and6 dependency/use-site guards.
Only the four remaining display-stage calls are controlled in the oracle.

The sanitizer fixture connects2,048 native PC/game frames across all16 maps to
the camera adapter, including1,751 airborne frames and1,464 displaced camera
frames. All274,785 assertions pass. Its WORLD callback observes camera inputs;
it does not claim scene pixels. Complete native world pixel integration is
separately verified in `world-scene.md`.

The requested decomp.wtf matcher was used for02B88. Two manual attempts reached
72.1%, not exact; its claim was released with notes and the best candidate is
`recovered/render_view.c`. No exact-match count is added for this function.
