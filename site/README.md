# Project website

The [website](https://zynchromatik.github.io/ss-advance-decomp/) introduces the PC port, explains how to play, and lets visitors explore the original maps and game guide.

You can browse the site without a ROM. **Playing the PC game requires your own European version of the Serious Sam Advance ROM.** The ROM is not included.

## Preview it locally

From the repository root, run:

```sh
python -m http.server 8080
```

Open <http://localhost:8080/site/>. Use this local server rather than opening `index.html` directly. The 3D viewer needs WebGL2; the 2D maps also work without it.

## Where to edit

- `index.html`: homepage, features, controls and download help.
- `styles.css`: page layout and appearance.
- `app.js`, `viewer3d.js` and `wiki.js`: navigation, 3D maps and game guide.
- `../scripts/wiki_entities.json`: weapon and enemy descriptions.
- `../scripts/build_wiki.py`: level, item and gameplay descriptions.
- `../scripts/topmap_viewer.html` and `topmap_viewer.js`: 2D map text and controls.
- `../README.md` and `../analysis/`: the source for the published help and developer notes.

Edit the source files before refreshing generated pages so your wording survives future updates.

## Refresh and check

From the repository root:

```sh
pip install -r scripts/site-document-requirements.txt
python scripts/refresh-site-docs.py
python scripts/refresh_atlas_ui.py
node scripts/test_site.mjs
node scripts/test_site_routes.mjs
node scripts/test_site_wiki.mjs
python scripts/test_site_documents.py
python scripts/test_site_audit.py
python scripts/test_site_http.py
```

These refresh commands use the existing map exports and artwork; they do not need a ROM. GitHub Actions runs the website checks and publishes `site/` when changes reach `main`.

For asset exports, viewer details and the full check list, see [the development notes](DEVELOPMENT.md).

Original game artwork belongs to its respective owners. This is an unofficial fan project. Three.js is included under its [MIT license](vendor/LICENSE).
