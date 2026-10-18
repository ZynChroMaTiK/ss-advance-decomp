# Project website

This folder is the [project website](https://zynchromatik.github.io/ss-advance-decomp/): downloads, setup help, maps and the NETRICSA guide.

You can browse the site without a ROM. **To play, you’ll need your own European version of the Serious Sam Advance ROM.** The ROM is not included.

## Try it on your computer

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

Change these source files, then run the refresh commands below. Editing a generated page directly means your changes will be lost the next time it’s rebuilt.

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

These commands reuse the maps and artwork already in the repo, so you won’t need a ROM. GitHub Actions runs the website checks and publishes `site/` when changes reach `main`.

For more on exporting artwork, changing the viewers and checking your work, see [the development notes](DEVELOPMENT.md).

Serious Sam and its artwork belong to their original owners. This is an unofficial fan project. Three.js is included under its [MIT license](vendor/LICENSE).
