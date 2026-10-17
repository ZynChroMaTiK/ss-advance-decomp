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

## Legend removal update (2026-10-17)

Removed the map key, doors/lifts list and secret-coverage sidebar from all 16
generated sheets. HTML panels retain the information. Sheet dimensions are now
1760x1900; map geometry, sprites, labels, titles and scale retain their positions.
Updated the full-sheet viewBox, overview aspect ratio, hero crop, generators,
SVG/PNG exports, embedded atlas, integrity manifests and both local archives.
All 16 sheets pass package checks and the overview was visually inspected.
The static-site validator and 315-resource HTTP check pass.

Current local atlas archive SHA256:
`7ac13e21e6ff4d14d48d069c2aba911d9b47285b3092f3896c3f180de37700bd` (3,709,761 bytes).
Current local project-site archive SHA256:
`44cf0cbc34ddbba5cfd9e6c8baafb9306770a43c6bcc43fb6f0b5e8ee1dcb867` (10,416,252 bytes).
These supersede the initial archive figures above.

Publication initially failed because of upload errors, not missing authorization.
Local publication commit is `29976201d1e1f9ce092f26b2b433233331f4c30e` in
`build/site-publish`. Two Git pushes failed with HTTP408 (default transport and
buffered HTTP1.1). An API fallback uploaded some content-addressed blobs, but
the large HTML request returned HTTP401, the connector returned HTTP500, and
the sequential uploader stopped on level11 SVG with HTTP400. No tree, commit
or branch update was made through that fallback. The live main branch remained
`176705678e3d027afbc31ba258309a0f3a343a1c`; the live site therefore remains the
previous version during those failed attempts.
Temporary API request files are under `build/site-api-upload` and contain only
website content, not credentials.

After the user restored their connection, a normal push succeeded. Commit
`29976201d1e1f9ce092f26b2b433233331f4c30e` is now published on main.
Pages run `39030103428` completed successfully. The live level01 SVG matches
the local SHA256, contains no legend headings, and the live validation report
confirms `sheet_legends_removed: 16`. Publication is complete.

## Full-source publication and PC-focused website (2026-10-17)

The repository now includes native source, Windows frontend, tests, scripts,
patches, research and recovery sources alongside the existing Pages website.
ROMs, generated assembly, saves, caches, toolchains, upstream checkouts and local
server databases remain excluded. `recovered/snapshot` preserves 74 exact C
sources, 348 best candidates, 527 attempts and 102 notes using explicit allowed
fields; it does not publish the live database or authentication records.

The website now leads with the PC port, implemented features, remaining parity
work and fresh-checkout build instructions. A single Levels tab shares selection
between 2D Atlas and 3D World. Old atlas/explorer links redirect to the corresponding
view; the iframe bridge preserves origin/source/range validation and async viewer
initialization cannot reactivate a hidden page.

Every sector now includes floor and ceiling meshes, including sky boundary
planes. All mesh materials are double-sided. All placement records are retained,
including co-located and scripted records. Floors, ceilings, walls, items,
enemies, scripted placements and secrets default visible; layer controls remain.
The sky boundary is a ceiling plane in the inspection tool, not the game's sky
renderer. Door preview remains static and gameplay is not simulated.

Node tests pass on all 16 maps: 1,916 geometry buffers, 42,168 triangles, 48 camera
fits, actual material ray intersections from both sides, default layer visibility,
unified routing, legacy links, selection synchronization and async navigation.
Pages continues to upload only `site/`. Separate workflows compile the Windows
source without a ROM and validate site geometry, routing and HTTP resources.
Browser/WebGL visual QA remains unavailable because no browser is connected.
