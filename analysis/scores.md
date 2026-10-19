# Original Scores viewer

## Name entry and saved records

The native port now also recovers original `08016F70` ranking and `08016F98`
name entry. The editor keeps the original12slots, character cycling, repeat
delay/wrap, selected-glyph colors and underscore-to-space conversion. B/Esc
does not discard a qualifying record, as in the original. A tie inserts before
the existing equal score. The first editor tick shifts entries to reserve the
slot; confirmation commits the score/name, highlights it and scrolls to its
six-entry group. Only the submitted event is persisted; the intermediate editor
table is never flushed. Nonqualifying scores return to Main.
The PC extension accepts typed letters/digits/spaces, Backspace, Delete,
Home/End and cursor movement; letters including WASD/M do not navigate or mute.

The frontend offers this flow after confirmed Pause Quit and
after the campaign ending. Quit uses the current run score; the ending uses the final bonus-adjusted
score. The following New Game/password/selected start clears completion latches
and session totals, preserving stored records. The latest user correction returns
directly to Main after death without restart input or name entry; see [run-exit.md](run-exit.md).

`scores.dat` is a separate320-byte, version1, little-endian CRC32 file. It stores
18score/name records plus the last entered name. Existing16-byte `save.dat` and
`settings.dat` remain compatible. Custom `--save-file PATH` uses `PATH.scores`.
Missing/corrupt files use ROM defaults without writing during load. A submitted
record merges against the latest file under an exclusive lock, then flushes and
atomically replaces it. Failed writes retain the record for the current session
and show a notice. An explicit submission can replace a corrupt file. Confirmed
Clear Data resets scores, progress, language and settings; cancelling retains them.

Validation:8192 original ranking comparisons;2886 original name-editor frames;
6111 original viewer frames and10252800 composed pixels across3languages.
The native codec/typing suite passes10339 ASan/UBSan/leak checks with54typed
submissions covering all18positions in every language. GCC and MSVC agree.
Windows passes754checks: six keyboard/mouse name paths, six Quit-run handoffs,
six ending handoffs and six fresh-run resets, plus disk reopening, stale-writer
merge, atomic replacement failure, corrupt-file recovery and Clear Data.
See `decomp.ps1 test-score-entry`, `decomp.ps1 test-scores` and
`scripts/test-scores.ps1 -SkipBuild` after building Windows.

Final regression checks pass:76171 native menu checks;590241 profile/unlock/
Clear Data checks;38874 settings/audio checks;143825 native pause/completion
checks and6205 Windows pause/completion checks. Windows settings/input/audio,
both-renderer launcher startup/restart and concurrent launch checks also pass.
All122 Windows libraries and the frontend build. Current executable SHA256:
`a538c5241bc47343a1c4b8e4f9a334a8c837784d1d250067d480a374ce6be7bb`.
Source/report hashes were checked. No campaign replay, ROM modification or new
byte match was performed. Live matching status remains78/526functions,
2816/87648catalogued bytes (3.21%); the catalogue is incomplete.

## Viewer recovery history

Main → Scores now runs a native reconstruction of original `0801737C` rather
than reporting an unsupported destination. It uses the original 18 seeded
records initialized by original `08015530`: scores from `0x44C144` and names
through the pointer table at `0x44C18C`, each a four-byte score and twelve-byte name.
The ROM's name templates have placeholder scores; they are not a populated list.
The three localized headings are original text entries247..249. All artwork,
font metrics, palette choices and background layers are shared with the menu.

The initial scroll begins at160 pixels. The three group stops are16,−160 and
−336 pixels; entrance and page movement advance two pixels per60Hz tick.
Page changes are admitted only at a stop. Up/Down changes group, with no wrap.
Original arrow indicators blink on counter bit32. Enter or Esc plays command16
and leaves on the following tick. Mouse clicks on the upper/lower arrow areas
change group; a click in the middle leaves. Menus retain the existing music
sequence and freeze gameplay.

`tests/test_scores_rom.py` reuses the bounded menu/font ROM oracle without
running unrelated historical menu-layout comparisons. It compares viewer state,
ordered sound commands, glyph submissions and composed pixels in all3languages,
after executing the actual original default-score initialization loop,
including the entrance, endpoint inputs, moving inputs, forward/backward pages
and exit. Results:1545 original frames and2534400 composed pixels. The native
menu sanitizer suite also exercises routing through Main and mouse hit testing.
Windows paths and previews are included in `scripts/test-audio-settings.ps1`.

The existing full `test-menu-rom` command was also attempted. Its Main-menu
comparison stops at the preexisting PC layout difference: five rows starting
atY74 (including Quit), versus the original four rows atY82. That historical
all-menu fixture has not been updated for every PC menu addition. The focused
Scores oracle and native menu suite pass independently; no passing full-menu
ROM comparison is claimed.

Initial viewer checks:76171 ASan/UBSan menu checks over10699frames;38874 sanitized
settings/audio checks;23349 Windows settings/input/audio checks including six
Scores routes and18 preview images. All121 Windows libraries and the frontend
build with the existing static runtime configuration. Launcher startup, restart,
both renderers, concurrent launches and per-session error logs pass. Final
executable SHA256: `397dfee788a3f378a870a45dffb6f38e90fc5bd2d4d031423fc736d889b4c575`.
No campaign replay was run; the original ROM remains unchanged.

The initial pass restored the viewer with the original seeded records. The
name-entry/storage/ending work above supersedes that boundary. No campaign
playthrough is claimed. The later death/retry integration is documented above.

Ranking routine `08016F70` is unsigned: a tie takes the earlier slot, and18
means the score did not qualify. `recovered/high_score_rank.c` records the
reconstruction. Its second manual compile produced68.2% similarity because of
the compiler's return sequence; it is **not** an exact match and the claim was
released with notes. Existing matching counts were not increased.

Reports: [ROM viewer](scores-rom-validation.json),
[native codec/typing](scores-validation.json), [Windows Scores](windows-scores-validation.json),
[native menu](menu-validation.json), [Windows input/settings](windows-audio-settings-validation.json).
