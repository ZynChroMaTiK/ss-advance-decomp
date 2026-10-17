# Serious Sam Advance project website

A buildless, static frontend with Project, Atlas, 3D Explorer and Development sections.
Target: https://zynchromatik.github.io/ss-advance-decomp/

Serve this directory over HTTP. ES modules and JSON fetches do not work by opening
index.html as a file. Example from the repository root:

    python -m http.server 8080

Open http://localhost:8080/site/. All URLs are relative, including the locally
vendored Three.js 0.180.0 runtime. No CDN, backend, account or original ROM is
required to view the exported website. WebGL2 is needed for the 3D Explorer;
the atlas remains available without it.

## GitHub Pages

Commit site/ and .github/workflows/pages.yml to main. In repository Settings →
Pages choose GitHub Actions as the source. The workflow publishes only site/.
No ROM, native binary, save data or local toolchain is uploaded.

## Regeneration

Using the project's Python environment, from the workspace root:

    python scripts/export_site_levels.py
    python scripts/build_project_site.py
    python scripts/validate_project_site.py

The exporter reads the user's original ROM, validated topmap export, and graphics
archive. The build script embeds the atlas, copies the archive and generates
readable reference pages. scripts/fetch_site_vendor.py refreshes the pinned
official Three.js modules; no dependency download is required for ordinary builds.
Regeneration runs from the full native project workspace with its recovered data
and validation inputs. This website repository contains a frontend snapshot;
it does not include the native game's source tree or toolchains.

## Viewer controls and scope

Orbit: drag, scroll to zoom, right-drag to pan. Touch: one finger rotates, two
fingers zoom/pan. Free flight: drag to look; WASD moves, Q/E changes height,
Shift speeds up, Escape returns to orbit. The canvas must have keyboard focus.
Player start moves to the original start location. Select a wall, floor or sprite
for its data. Secret buttons focus their original locations and link evidence.

Floor polygons come from cleaned convex BSP leaves. Walls use original mip0
textures, offsets, peg flags, palette and decoded heights. Sky ceilings are open.
Open-door preview uses recovered ceiling-door height floor+224; lifts retain their
initialized height. This is a static inspection tool with no collision or gameplay
simulation. Sprites mark locations; scripted placements are separately selectable.
Five documented secret pins are preserved; this is not a claim that all secrets
have been identified. All 16 maps and every decoded room are included.

The native project currently supports ten campaign levels. Complete original
feature and behavior parity remains unfinished. Reference pages are a dated
snapshot, not a live build status feed.

Three.js is MIT licensed (vendor/LICENSE). Original game art belongs to its
respective owners. This is an unofficial fan project.
