# Project website

This folder is the [project website](https://zynchromatik.github.io/ss-advance-decomp/): downloads, setup help, maps and the graphics gallery.

You can browse the site without a ROM. **To play, you’ll need your own European version of the Serious Sam Advance ROM.** The ROM is not included.

## Try it on your computer

From the repository root, run:

```sh
python -m http.server 8080
```

Open <http://localhost:8080/site/>. Use this local server rather than opening `index.html` directly. The 3D viewer needs WebGL2.

## Where to edit

- `index.html`: homepage, features, controls and download help.
- `styles.css`: page layout and appearance.
- `app.js`, `viewer3d.js` and `wiki.js`: navigation, 3D maps and game guide.
- `graphics.js` and `graphics.css`: the graphics gallery and its controls.
- `../scripts/build_site_graphics.py`: builds the gallery from the original graphics export.
- `../scripts/build_site_sources.py`: turns the secret-source records into a readable page.
- `../scripts/wiki_entities.json`: weapon and enemy descriptions.
- `../scripts/build_wiki.py`: level, item and gameplay descriptions.
- `../README.md` and `../analysis/`: the source for the published help and developer notes.

Change these source files, then run the refresh commands below. Editing a generated page directly means your changes will be lost the next time it’s rebuilt.

## Refresh and check

From the repository root:

```sh
pip install -r scripts/site-document-requirements.txt
python scripts/refresh-site-docs.py
node scripts/test_site.mjs
node scripts/test_site_routes.mjs
node scripts/test_site_wiki.mjs
node scripts/test_site_graphics.mjs
python scripts/test_site_graphics.py
python scripts/test_site_documents.py
python scripts/test_site_audit.py
python scripts/test_site_http.py
```

These commands reuse the maps and artwork already in the repo, so you won’t need a ROM. GitHub Actions runs the website checks and publishes `site/` when changes reach `main`.

To refresh the gallery, run `python scripts/build_site_graphics.py` after creating `build/exports/ssa-graphics.zip` with the graphics exporter. It keeps the original PNGs unchanged and compares their visible pixels to remove duplicates. Generated contact sheets and tile overviews are left out; different sizes and colours stay available. The source archive is unchanged and stays out of the website.

Use the Category filter to browse enemies, weapons, menus and other parts of the game, or choose All to see everything. Results form one continuous list, with images loaded as you scroll. Selecting a picture updates the preview without moving the page. Search also finds specific actions such as walking or firing, and each preview explains how the picture is used.

Image names distinguish animation frames and viewing angles. Resolution tags such as Full resolution, Half resolution and Quarter resolution identify the different sizes (mips) on cards and previews. You can search for these tags too. Actor sizes and views come from the original layout flags; some enemies start at 75% resolution. Extra stored images outside a declared layout are called stored variants instead of guessing their resolution role. Original picture IDs and links stay the same.

Single resolution means the sprite has just one stored size. Rockets, cannonballs and energy shots use this layout; their animation frames and viewing angles can have different dimensions without being mips. The falling piano, anvil and safe have full and half-resolution versions. Previews show the original mip level for sprites and textures that have several stored sizes. HUD pieces, weapon-in-hand frames, menus, fonts and other screen artwork are not given distance-mip labels. The shorter sky previews are crops of the top 60 rows, not mips. The gallery checks every actor resource and texture bank against the original export, including shared pictures and old links.

The Palette variants chip marks pictures with different Egypt and Rome colours. Use Sprite colours to switch between them, or search “palette variants” to find them. Pictures that look identical in both palettes don’t get the chip.

Textures with the same design at different brightness levels have Light variant, Medium variant or Dark variant tags. These describe the original pictures, not a brightness adjustment made by the gallery. Search “brightness variants” to find them, or follow the preview links to compare a set at the same resolution. `scripts/graphics_brightness.py` lists the sets checked against the original pixels. Different drawings, switch states and mirrored pictures remain separate designs.

Textures are split into Egypt and Rome, with walls and doors together in each set. Their setting comes from the original texture bank. Unused pictures stay with their subject, and the old interface pieces have their own Old HUD category. The Unused chip also appears in the preview with a short explanation. Searching “unused” finds these pictures within the selected category.

`scripts/graphics_usage.py` checks texture references in all 16 original maps, including multiplayer maps, every wall layer, both switch states and all three image sizes. It matches texture names the same way as the game. A shared picture gets the chip only if none of its copies is used. Other unused labels cover frames outside the mapped animations and screens; this is a reference check, not proof from playing every possible game state. The old numeric HUD's controller remains in the code, but the HUD constructor does not create it. `scripts/graphics_content.py` supplies picture descriptions. Original pixels and old picture links are preserved; links ending in `/rome` keep the Rome colours.

Run `python scripts/build_site_sources.py` to refresh the readable secret-source page from the existing map records. Website instructions should describe the download; label newer work as development progress until a release includes it.

For more on exporting artwork, changing the viewers and checking your work, see [the development notes](DEVELOPMENT.md).

Serious Sam and its artwork belong to their original owners. This is an unofficial fan project. Three.js is included under its [MIT license](vendor/LICENSE).
