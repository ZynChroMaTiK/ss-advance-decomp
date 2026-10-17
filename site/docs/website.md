# Project website and level explorer

Published 2026-10-17: https://zynchromatik.github.io/ss-advance-decomp/

The static frontend is in `site/`, with Project, Atlas, 3D Explorer and Development
routes. Hash navigation and relative asset URLs support the repository Pages prefix.
The native project's source and ROM were not published in this website commit.
The original ROM remains unchanged (SHA1 `4ff48d8c18b70a5184758f1917343ae85a70858e`).

The workspace itself is not a Git checkout. An isolated publication checkout lives
at `build/site-publish`, tracking `ZynChroMaTiK/ss-advance-decomp` main. Initial
website commit: `176705678e3d027afbc31ba258309a0f3a343a1c`.
GitHub Pages uses Actions; `.github/workflows/pages.yml` publishes only `site/`.
Deployment run `39028072862` succeeded after Pages was enabled. Live HTML matched
the local SHA256, catalog contained all 16 entries, and the viewer module and
level 16 JSON returned HTTP200 with correct MIME types.

`scripts/export_site_levels.py` exports original wall faces, UVs and textures,
clean convex BSP polygons, initialized room heights, markers and secret evidence.
Use `mesh_loops` and `initial_floor`, not raw floor values, for rendering.
Ceiling doors preview at wrapped signed16 floor+224; lifts retain initialized
heights. Sky ceilings (selector zero) are omitted. The viewer uses vendored
Three.js 0.180.0, nearest texture filtering and original palette colors.
All actor/item markers use original sprites; the 71 multiplayer pickup respawn
sites use an R symbol because no standing actor sprite exists at those locations.
Scripted placements can be enabled separately. Repeated identical placements
share a marker; all original records remain in the data and 2D atlas.

The explorer is static, with orbit/top/player-start/free-flight controls, wall
and ceiling cutaways, wireframe, door preview, selectable geometry and sprites,
and secret focus/access buttons. It does not simulate collisions or scripts.
Only five documented secrets are identified; all decoded rooms are visible.
This does not establish exhaustive secret or easter-egg discovery.

`scripts/build_project_site.py` assembles the embedded atlas and reference pages.
It adapts the existing atlas shell and synchronizes level selection with the host.
The standalone `build/exports/ssa-topmaps.zip` is retained unchanged and downloadable.
The development pages describe the current ten-level native prototype accurately;
the site itself is not a playable port.

Validation completed:

- `scripts/test_site.mjs`: actual Three.js imports and 974 generated meshes,
  35,629 triangles across floors and both wall states, 48 camera aspect checks.
- `scripts/test_site_routes.mjs`: real routing module, all 16 options, shared
  selection, iframe synchronization, cross-tab links and message guards.
- `scripts/test_site_http.py`: 315 resources served beneath `/ss-advance-decomp/`,
  including every level, texture and sprite icon; correct JS MIME types.
- `scripts/validate_project_site.py`: 396 HTML/link/script checks, JS syntax and
  DOM bindings, CSS parsing, finite/nondegenerate geometry, sprite/texture paths,
  dependency and embedded-atlas hashes, unchanged ROM and ZIP CRC.

The Node checks used the available Node22 runtime. The Python validator used
system Python3.10 and the matching quickjs/tinycss2 packages under
`build/topmap-validation-tools`; the native venv has a different Python ABI.
No browser was connected, so interactive browser/WebGL visual QA remains unverified.

Deployable archive: `build/exports/ssa-project-site.zip` (11,487,588 bytes).
SHA256: `d5ff6ddb1ea3aa3ebf89fdb79fb4e5a6cacf0697b146894613cb68c0838b40c6`.
Validation evidence: `build/exports/ssa-project-site-validation.json` and
`build/exports/ssa-site-http-validation.json`.
