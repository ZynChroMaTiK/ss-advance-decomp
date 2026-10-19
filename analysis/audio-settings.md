# PC Settings and alternate music

Options contains Settings, Extras, Language, Clear Data and Credits, in that order. The redundant
Music On/Off row is removed; use Music volume0% to silence music. Existing
option IDs1..4 are retained internally; Extras is ID5, with keyboard navigation stopping at
the first/last visible rows and mouse rows packed together. Settings opens a separate extensible page
with Display mode (Fullscreen / Windowed), Renderer (Software / GBA), Music style (Original / SoundFont
/ MIDI), Music volume and Sounds volume, followed by Back.
Both volumes show a bar and percentage, bounded0..100. Up/Down selects a row;
Left/Right changes volume by5 points or toggles a choice. Enter toggles a choice or
raises a volume. Click a bar to set its value. Back/Esc returns to Options.
Extras contains Jump (On / Off), Third-person (On / Off), In-game NETRICSA
(On / Off), Alternate weapons (On / Off), Automap (On / Off), HUD (Original / Classic),
Weapon bobbing (On / Off), Surface decals (On / Off), and Back. Bobbing defaults Off, moves the first-person
weapon and muzzle flashes together, and leaves the crosshair steady. Settings
version10 stores it in byte8 bit6; bit7 retains aspect, and the lower six bits
hold the version. Version11 adds default-Off Surface decals in bit5 and uses
the lower five bits for the version, retaining16 bytes and the existing CRC.
Versions1..10 load with Surface decals Off. Versions1..9
load with bobbing Off while retaining other preferences. Alternate weapons defaults
to Off; existing Settings versions1..5 migrate with that preference disabled.
Settings version6 introduced byte9 bit7 while retaining the16-byte CRC-protected file.
Version7 adds Automap in byte10 bit7; music volume0..100 occupies its lower seven
bits. Version8 adds HUD style in byte11 bit7; sound volume occupies its lower seven
bits. Version1..7 files default to Original HUD, preserving their other choices.
Version1..6 files decode with Automap Off; explicit version7/8 choices are
preserved. Options -> Extras exposes the
saved toggle; M opens/closes the overhead map during gameplay instead of muting
audio. Other option bit positions, style values and music levels are unchanged.
See [alternate-weapons.md](alternate-weapons.md) for the Colt variant behavior.
Its toggles accept Left/Right, Enter or a click; Back/Esc returns to Extras'
row in Options. Settings returns to its own row.
English, German and French labels use the existing ROM fonts. Left/Right cycles
only available styles: Original is always present, SoundFont requires a loaded
bank whose file still exists, and MIDI requires an open Microsoft GS Wavetable
Synth device. No missing-SoundFont choice or installation error is shown during
ordinary cycling. The default file is `soundfonts/SC-55.sf2`; `--soundfont` can
override it. Banks are loaded at launch; restart after adding a missing file.
Saved unavailable styles fall back to Original without changing volume values.

Defaults are borderless Fullscreen, Software, Jump Off, Third-person On, In-game NETRICSA On,
Original,100%,100%. Renderer selects the actual
full-resolution PC renderer or native240x160 GBA reference renderer. Settings
apply immediately; --original-renderer overrides a saved choice at launch.
Jump Off suppresses new Space jumps at the frontend input boundary, without
changing ongoing vertical physics. Menus/map confirmation still accepts Space.
In-game NETRICSA Off ignores Tab/N during gameplay without setting a frontend
error or interrupting simulation. The pre-level briefing and its Tab-close
behavior are independent of that toggle.
Third-person Off consumes pending H toggles and returns to first-person on the
next game tick. On permits a fresh H press without automatically changing views.

Original retains its preceding instrument
balance, including the half-amplitude music adjustment before the output bus. SoundFont mode
does not inherit that attenuation or the original PCM boost. Its combined
music bus now has a separate +3 dB lift requested for the whole mix, including
both retained ROM samples and synth instruments, preserving their balance.
The SoundFont layer alone has a 15 dB balance trim before that shared gain,
requested after listening to the replacement Musical Artifacts 1228 SC-55 bank.
Separate stems over the first 8 seconds of all 26 sequences measured the raw
SoundFont layer 11.83 dB above retained PCM in aggregate over 25 mixed sequences.
The initial 12 dB trim was lowered another 3 dB at the user's listening request.
The stable trim balances the layers without pumping during
sample rests or flattening musical dynamics. It is calibrated for this bank;
other banks and individual instruments can still differ in perceived loudness.
Original now receives an additional 3x output gain (+9.54 dB), and SoundFont
receives 2x (+6.02 dB), at the user's request to bring both modes up to peak
level. This raises each entire mix together;
the SoundFont synth trim and its previous shared +3 dB lift remain in place.
Music volume and original sequence volume/fades still apply in both modes.
A linked-stereo peak guard reduces either music mix only when needed to avoid
output overflow at 32760/32767, recovering toward unity in100ms. Quiet passages
retain their dynamics; this does not continuously normalize every note to peak.
Sound effects retain their existing balance in both music styles. Sound volume
also controls menu clicks. M is a mute shortcut on non-game screens; during
gameplay it toggles the Extras-enabled automap.
Settings apply immediately. Changing style preserves the sequence position,
tempo, music command and screen transition state; currently held notes restart
their instrument envelope when entering SoundFont mode if they are PSG.
ROM sampled instruments continue at their existing sample position/envelope.

Preferences use a separate16-byte, versioned CRC32 `settings.dat`. Version5
adds display mode at byte9 bit6 (0=Fullscreen,1=Windowed). Versions1..4 migrate
to Fullscreen while preserving their existing fields. Version4 added
in-game NETRICSA at byte9 bit4 and third-person permission at bit5,
retaining renderer/jump/audio values. Versions1/2 migrate with in-game NETRICSA
On; versions1..3 migrate with third-person permission On. Version1 also defaults
to Software and Jump Off. Explicit jump choices in versions2..6 are preserved.
Clear Data restores Jump Off along with the other defaults. Invalid data leaves caller defaults intact.
Style values
0/1 remain Original/SoundFont; 2 adds MIDI without changing the file layout or
invalidating existing preferences. Progress in
`save.dat` and its format are unchanged. With `--save-file PATH`, preferences
use `PATH.settings`, isolating tests and alternate profiles. Writes flush a
temporary file then atomically replace the previous settings. Invalid files
leave defaults intact and show a notice. Failed writes retain session values.
Clear Data retains these preferences; its existing warning covers
progress and language only.

Settings and Extras now align to the original8-pixel background grid. Both
pages use label X32 and value X144. Settings rows are Display mode Y24,
Renderer40, Aspect ratio56, Music style72, Music volume88, Sounds volume112
and Back144. The Sounds label and slider moved up one8-pixel tile on2026-10-19.
Slider borders run from X32 through176 at Y96..103 and120..127; percentages
start at X184 on each slider's top row. Click normalization uses those same
endpoints, so the border extremes reach0/100%. Extras' eight rows sit at
Y16/32/48/64/80/96/112/128, title Y0, with Back144. Titles and Back text also snap their centered X anchors to the
grid. Keyboard and mouse share these positions. Labels are composed per row to preserve
the original128-glyph batch limit in all three languages.

Fullscreen uses a borderless WS_POPUP window sized to the current monitor's
full bounds, without exclusive display-mode changes or a topmost flag.
Windowed restores the prior window style, placement and size within the
session. Saved mode applies before the initial window is shown. DPI/display
changes refit fullscreen bounds; existing input confinement follows resizing.
Hidden validation windows retain their visibility when toggling modes.

F11 toggles borderless Fullscreen / Windowed globally, before individual screen
key routing. The hotkey updates the existing display preference, mirrors it into
the menu and persists it with the same settings file. Held-key repeats and key-up
do not toggle again. Window geometry restoration, focus/confinement and failure
rollback use the same display_mode_apply path as the Settings row. Windows
message tests cover both renderer selections and menu/gameplay/NETRICSA/pause/
story routing flags, repeated key-down suppression, unchanged simulation/input,
geometry, visibility and disk persistence. Physical F11 delivery is not automated.

Display-mode integration passes38852 sanitized portable Settings checks and
21179 Windows Settings checks, including defaults, v4 migration, persistence,
keyboard and real mouse routes, monitor/client bounds, restored resizable
window geometry and unchanged hidden-window visibility. General menu checks
pass72946 assertions. All121Windows libraries/frontend build; normal launcher,
startup/restart and concurrent-launch checks pass both renderers. English,
German and French Settings previews were visually checked under
`build/audio-settings/settings-*.png`. No campaign replay or physical input
validation is claimed. Current frontend SHA256:
`50793fa0efd58b743330d60d73b7aec8aba98f4c2256fc0950b3531f0b58a616`.

## Renderer

`audio.c` remains the ROM-data sequencer. A borrowed `SsaAudioSynth` callback
interface replaces only PSG music. Every resolved type0 sampled instrument
continues through the ROM renderer, including PCM entries inside rhythm/key
splits. The synth receives these notes only to retire a previous PSG note on
the same track; it adds no duplicate layer. Note gates and tied-note identity are tracked
independently of original PCM/PSG envelopes, so an original sample's ending
cannot strand a SoundFont note. Sound effects continue through the original
mixer and source priorities. User bus levels precede effects headroom mixing.
Reset, stop, music disable, transitions and close silence the alternate synth.
Mute advances both renderers silently. Original synthesis keeps advancing in
SoundFont mode, allowing switching back without resetting the song.

`src/audio/soundfont.c` wraps pinned MIT TinySoundFont. The GM arrangement is
explicit, not a direct reinterpretation of custom ROM instrument numbers.
Program127 is the ROM rhythm split: its PSG24..39 entries map to a GM drum kit
independently of the instrument-remapped pitch, while sampled entries stay
native. Other PSG voices use bass/lead/brass. ROM sampled melodic instruments
and sampled percussion are preserved rather than assigned GM replacements.
Wave transposition retains the original lower octave. These preset choices are
an authored arrangement and still need subjective listening/tuning. They are
not a recovered original SC-55 arrangement. No claim of hardware SC-55 fidelity.

The synth preallocates128 voices/16 channels, synthesizes32-sample blocks
(at most31 samples of event latency,0.65ms at48kHz), and uses the existing
WinMM audio worker and mutex. No synth allocation occurs per note. Loading is
outside the worker, with only attachment serialized. The frontend accepts an
external SF2 up to64MiB, requires a GM drum kit, and leaves Original available
when loading fails. The supplied SC-55 file remains local; install/reinstall
with `scripts/install-soundfont.ps1`. Use `--soundfont PATH` for another bank.

## Microsoft GS MIDI

`platform/win32/midi_win32.c` enumerates WinMM outputs and opens the specifically
named **Microsoft GS Wavetable Synth**, not the MIDI mapper or external hardware.
It sends note on/off, GM program changes, pan, volume, pitch bend and expression
to the Windows synthesizer. `include/ssa/music_instrument.h` shares the authored
PSG arrangement with SoundFont mode. All resolved ROM PCM music and sound effects
remain native; no SoundFont file is needed for MIDI playback.

The existing sequencer schedules events through the audio worker. Switching
modes preserves its position, tempo and current tracks. MIDI channel 10 carries
percussion; melodic tracks use the other 15 channels. Reset, stop, mode switches
and shutdown silence MIDI notes. Music volume, M mute and sequence
fades control MIDI expression, independently of sound effects. Changed controller
values are cached; messages are not resent on every audio sample.
The subsequent listening adjustment raises MIDI instrument channel-volume
values by 80%, capped at 127, following the user's request for more level
relative to the original samples (previously 40%). Tracks at volume 70 now
send 126 instead of 98; already saturated tracks remain at 127.
The latest listening correction additionally halves retained music PCM in MIDI
mode (-6.02 dB), bringing the MIDI instruments forward relative to the sample
layer. Increasing CC7 alone cannot help tracks already at 127. The trim runs in
the MIDI-only music hook before its delay ring, including when delay is zero,
and before sound effects are mixed. Velocities, user volume/fades, timing,
sound effects and Original/SoundFont mode levels remain unchanged. This is a
relative balance correction; perceived loudness still varies by instrument.
The Windows Settings checks cover all26 sequences, including the half-level
stereo impulse at185ms and zero delay, unchanged SFX, mute/fades and reset.
The full Settings fixture now passes its rendering/input/persistence checks
as well; see `windows-audio-settings-validation.json`.

The map-to-level MIDI silence was reproduced with process-only loopback in
both Egypt and Rome. After a full map fade, the old reset callback ran while
`master_gain` was still zero, before the mixer cleared its fade state. The
sequencer continued producing notes and the cached expression later reported
127, but measured MIDI-only level output was exactly zero in both cases.
Manually resending expression restored output. `ssa_audio_reset` now restores
its mixer state first and calls the synth reset afterward, so the backend reads
the new scene's gain when resetting MIDI controllers. No instrument mappings,
volume balance, fade speed or sequence timing changed.

Run `scripts/test-midi-transition.ps1 -SkipBuild` after a Windows build. The
regression plays the actual map tune, fades fully to silence, then starts the
level tune through the real WinMM worker. A test-only hook removes retained ROM
PCM music from the captured mix, isolating Windows MIDI. The test captures only
its own process output, without a microphone, and requires audible MIDI before
and after the fade on maps0 and6. This checks rendered output rather than only
cached controller values or note counts. See
`windows-midi-transition-validation.json`.
The corrected build passes both rendered-output transitions, the full21179
Windows Settings checks and660209 GCC ASan/UBSan/leak audio checks across131
populated songs, with zero decoder errors. All121Windows libraries/frontend
build. Current executable SHA256:
`2f141c9afaaf4381f5ef296eab47f7d0fee14eba969231a26f2bf2ddbc1ff9a1`.

Windows renders MIDI separately from the game's PCM buffers. A process-only
loopback test of the actual GS output and the same three 800-frame wave buffers
measured MIDI 184.875..185.000 ms behind simultaneous PCM markers. MIDI mode now
buffers only the retained music samples for 185 ms (8,880 frames at 48 kHz),
after their gains/fades but before music peak protection and effect mixing.
The optional synth `mix_music` callback implements this delay. Sound effects,
menu clicks, Original and SoundFont modes do not pass through it. Resets, stops
and mode changes clear the ring so old music cannot leak into another song.

The corrected four-marker check measured residual offsets of -0.021..0.000 ms
on this device. This is a device calibration, not a guarantee of sample accuracy
for every instrument or audio driver; MIDI events still have the audio worker's
block scheduling granularity. `--midi-delay MS` overrides compensation from
0..500 ms for other setups. No calibration recording runs during game startup.
The synth's output level is independent, and the game's PCM peak limiter cannot
limit the combined Windows MIDI/PCM output. GM percussion also shares channel-wide pan/pitch/volume.
The existing 15 dB SC-55 trim remains specific to SoundFont mode.

API references: [Microsoft's built-in GS synth](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/midi#using-the-built-in-windows-general-midi-synth),
[WinMM short messages](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-midioutshortmsg)
and [MIDI reset](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-midioutreset).

## Validation

`tests/test_audio_settings.py` builds the focused portable fixture with
ASan/UBSan and leak detection. It checks all26 music sequences for8seconds each,
matching original sequence states while producing SoundFont PCM with no decoder
errors or hard clipping. It checks mute/reset, same-track note progression,
volume isolation, unchanged sound effects, live style switching, arbitrary
buffer splits, settings CRC/ranges, mouse controls, both slider endpoints and
all three languages. Rendered menu images are in `build/audio-settings/`.
Counts and source hashes are in `audio-settings-validation.json`.
The current peak check requires both music modes to reach 32759..32760 at
100% volume over these excerpts, with no clipped samples or decoder errors.
Focused checks additionally compare retained PCM waveforms with the respective
mode output gains and shared +3 dB hybrid balance (allowing integer rounding),
verify that no program with resolved PCM type generates a SoundFont layer,
compare PSG synth waveforms and energy to the raw bank with the 15 dB trim,
and check user volume and peak protection independently of Original's fixed
mix adjustments.

`ssa_native_settings_win32_check` uses the real frontend message handler,
settings change hook, atomic file writer and muted WinMM worker. It checks
Extra toggles and persistence, actual Space suppression/jumping, H suppression/
restoration and first-person return, disabled Tab/N,
enabled Tab opening, and the pre-level briefing with in-game NETRICSA disabled.
The portable fixture covers all sixteen renderer/jump/NETRICSA/third-person
combinations and all three older settings formats, plus keyboard/mouse navigation
in three languages. Counts and hashes are in the current validation reports.
It also checks
live switching without song restart, saved/reloaded levels, worker progress,
map/game restarts retaining settings, corrupt files and failed writes. MIDI
checks use the actual Microsoft GS Wavetable Synth with expression muted, cover
all 26 music sequences, persisted MIDI selection, switching without song restart,
volume/fade/mute messages, GM percussion, pitch controls, PCM exclusion and note
cleanup. A test-created SF2 copy is deleted to verify that a cached bank is no
longer selectable when its file disappears; the user's file stays untouched.
The delay fixture checks the exact stereo impulse offset, zero-delay bypass,
range validation and reset flushing, and compares sound effects sample-for-sample
against Original to ensure they remain immediate.

`scripts/test-midi-sync.ps1` is a separate audible diagnostic: four markers
without compensation and four with it, capturing only its own process output
through Windows process loopback. It never uses the microphone or saves captured
audio. It requires Windows process-loopback support (build 20348 or newer) and
the GS device. Timing results and hashes are in
`windows-midi-sync-validation.json`; the compensated tolerance is 15 ms.
The original menu, profile and audio checks also pass. No automated campaign
playthrough or physical listening test was performed for this feature.

Extras compact rows (2026-10-20): each option now occupies one8-pixel grid
row, at Y16..80. All nine choices are visible together; the old scrolling
state and arrows are removed. Pointer hit regions follow the same8-pixel rows.
The title remains at Y0 and Back at Y144. Existing keyboard order, values,
localized labels and settings serialization are unchanged.
