# Preserved recovery sources

This snapshot preserves the active decomp.wtf project's authored source and
research. Regenerate with `python scripts/export_recovery_sources.py` in the
original research workspace. `manifest.json` records counts and SHA-256 hashes.

- `exact/src/`: 74 C sources from the verified exact-match export.
- `exact/include/`: shared header for that export.
- `exact/build.py`, `build.json`, `functions.cfg`: exporter build metadata.
- `research.json`: 526 catalogue records, 348 best sources, 527 compiler
  attempts, 102 notes and the shared context header. Nonmatching attempts are
  retained as research, not represented as exact recovery.

The exact-source directory is not a standalone ROM build: ROM-derived assembly
and the base ROM are deliberately absent. Use the pinned decomp.wtf bootstrap
and your own ROM to regenerate the local project/export; this archival JSON is
not automatically imported into a new database. The portable native game builds
from the repository's `src/`, `include/` and `platform/`, independently of this
snapshot. The catalogue remains incomplete and is not whole-game coverage.

The exporter reads the database in read-only mode and uses explicit field lists.
Authentication, agent identity, local server settings, ROM bytes, compiled bytes
and assembly dumps are excluded. The raw database is never published.
