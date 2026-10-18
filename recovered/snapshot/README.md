# Code recovered from the GBA game

Curious about how the GBA game works? This folder keeps the recovered code and
research notes. You can skip it if you just want to play.

**To play, you’ll need your own European version of the Serious Sam Advance ROM** at `orig/SSA.gba`. Other regional versions won’t work. No ROM is included.

- `exact/src/` contains C code verified to compile to the same bytes as the original routines.
- `exact/include/` and the build files support that recovered code.
- `research.json` keeps the wider research, including attempts that are not exact matches.
- `manifest.json` lists the saved files, counts and checksums for this snapshot.

The playable PC port lives in `src/`, `include/` and `platform/`. This folder
is a saved copy of the research. The list of original functions is still
incomplete, so its counts aren’t a percentage of the whole game.

To refresh this folder from the original research workspace, run:

```sh
python scripts/export_recovery_sources.py
```

Rebuilding the original ROM also needs the local decomp.wtf setup and your own European ROM. This folder does not include the ROM, its remaining assembly, or private database settings. Setting up a new database won’t import these notes automatically.

See [the development guide](../../DEVELOPMENT.md) for setup and research details.
