# Preserved recovery sources

This folder keeps the code and research recovered from the original game. It helps contributors understand how the game works; you do not need it to play.

- `exact/src/` contains C code verified to compile to the same bytes as the original routines.
- `exact/include/` and the build files support that recovered code.
- `research.json` keeps the wider research, including attempts that are not exact matches.
- `manifest.json` lists the saved files, counts and checksums for this snapshot.

This is a research snapshot, not a complete game build. The function catalogue is still incomplete, so its counts do not measure how much of the whole game has been recovered. The playable PC port has its own code in `src/`, `include/` and `platform/`.

To refresh this folder from the original research workspace, run:

```sh
python scripts/export_recovery_sources.py
```

Rebuilding the original ROM also needs the local decomp.wtf setup and your own European ROM. This folder does not include the ROM, its remaining assembly, or private database settings. The saved research is not automatically imported into a new database.

See [the development guide](../../DEVELOPMENT.md) for setup and research details.
