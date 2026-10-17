# Serious Sam Advance project website

A buildless, static frontend with PC Port, Features & Roadmap, Levels, NETRICSA,
and Build & Source sections. Levels combines the 2D atlas and 3D world viewer with
one selection and shareable `#levels/1/2d` or `#levels/1/3d` URLs. Legacy
`#atlas/1` and `#explorer/1` URLs redirect to the corresponding view.

NETRICSA is a searchable field guide with 54 entries: 16 levels, 10 weapons,
12 enemies, 5 item families, 6 game-system guides and 5 documented secrets.
Entries have shareable URLs such as `#wiki/weapon-pistol`. Original NETRICSA
illustrations accompany project-authored summaries; source and coverage links
distinguish original intel from recovered behavior and PC additions.
`python scripts/build_wiki.py` regenerates the data from the committed map
exports, `scripts/wiki_entities.json` and original artwork. No ROM is needed.
`node scripts/test_site_wiki.mjs` checks entries, filtering, artwork and links.
Target: https://zynchromatik.github.io/ss-advance-decomp/

Prominent page titles, section headings and NETRICSA entry titles use the original
16px GBA font. The white/green color web fonts preserve the exported glyph pixels,
palettes and spacing, with normal text selection, wrapping and accessible heading
semantics. Body text, controls and small labels keep the readable UI font.
Rebuild with `python scripts/build_site_fonts.py` after installing
`scripts/site-font-requirements.txt`. Inputs and pixel-equivalence hashes are in
`assets/fonts/`; the converter checks all 126 glyphs in both saved WOFF files.

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
and validation inputs. The repository now includes the native source, tests,
recovery sources and research alongside this frontend. Toolchains and the ROM
remain local. The existing exported website can be served without regeneration.

From a fresh checkout, Node.js 22 can run `node scripts/test_site.mjs` and
`node scripts/test_site_routes.mjs`. Python 3 can run
`python scripts/test_site_http.py`. Full export validation additionally needs
`pip install -r scripts/site-validation-requirements.txt`, the original ROM and
the HTTP validation result. It packages `build/exports/ssa-project-site.zip`.

## Viewer controls and scope

Orbit: drag, scroll to zoom, right-drag to pan. Touch: one finger rotates, two
fingers zoom/pan. Free flight: drag to look; WASD moves, Q/E changes height,
Shift speeds up, Escape returns to orbit. The canvas must have keyboard focus.
Player start moves to the original start location. Select a wall, floor or sprite
for its data. Secret buttons focus their original locations and link evidence.

Floor polygons come from cleaned convex BSP leaves. Walls use original mip0
textures, offsets, peg flags, palette and decoded heights. Every sector includes
its floor; indoor ceilings are included and sky boundary planes are omitted. All surfaces
render one-sided, with backface culling. Floors face upward and ceilings face into rooms. Walls, floors, ceilings, items,
enemies, scripted placements and secret pins are visible by default; individual
layers can be hidden. Hide ceilings for an unobstructed overhead view.
Open-door preview uses recovered ceiling-door height floor+224; lifts retain their
initialized height. This is a static inspection tool with no collision or gameplay
simulation. Sprites mark locations; co-located records are preserved and scripted
placements can be toggled separately.
Five documented secret pins are preserved; this is not a claim that all secrets
have been identified. All 16 maps and every decoded room are included.

The native project currently supports ten campaign levels. Complete original
feature and behavior parity remains unfinished. Reference pages are a dated
snapshot, not a live build status feed.

Three.js is MIT licensed (vendor/LICENSE). Original game art belongs to its
respective owners. This is an unofficial fan project.
