# Serious Sam Advance for PC

This is a fan-made Windows port of the GBA game. It keeps the original levels,
weapons, enemies and music, with keyboard and mouse controls and a few optional
extras. You can play all twelve campaign levels.

**[Download v0.8.0 Alpha 2](https://github.com/ZynChroMaTiK/ss-advance-decomp/releases/tag/v0.8.0-alpha.2)**
· [Website](https://zynchromatik.github.io/ss-advance-decomp/)
· [Explore the maps](https://zynchromatik.github.io/ss-advance-decomp/#levels/1/3d)

It’s still an alpha, so you’ll find bugs and unfinished bits. The game runs
directly on Windows—there’s no emulator to set up.

## Download and play

**You’ll need your own ROM of the European version of Serious Sam Advance.**
A ROM is a file containing the original GBA game. Other regional versions
won’t work. The download doesn’t include a ROM.

1. Download the **Windows x64 ZIP** from the release page.
2. Extract it to a folder where you can save files, such as your Games folder.
3. Name your European ROM **`SSA.gba`** and put it in the **`orig`** folder.
4. Double-click **`play-native.cmd`**.

The download is ready to run once you add your ROM. Extra SoundFont music is
optional; you can stick with the original music. If you’re updating, keep
`save.dat`, `settings.dat` and `scores.dat` so you don’t lose your unlocks, settings or records.

If the game cannot load your ROM, check its region and filename. The supported
European ROM has SHA-1 `4ff48d8c18b70a5184758f1917343ae85a70858e`.
This fingerprint is only needed if you want to check the file you have.

## What you can play

- All twelve campaign levels, from Egypt to Caesar's Palace.
- Original enemies, weapons, pickups, music and sound effects.
- Mouse look, optional jumping and a third-person camera.
- Graphics that fit your window, or the original GBA-resolution view.
- NETRICSA briefings and an in-game guide, chapter screens and the ending.
- In-level automap with live walls, player/entity markers and zoom.
- Pause menus, end-of-level bonuses, saved level unlocks and original passwords.
- Finite lives, automatic level retries, and Game Over after the last life.
- The original Scores screen, name entry and saved high scores.
- English, German and French menus.
- Optional dual Colts and alternate double-shotgun behavior.

Multiplayer isn’t ready yet. There are also
bugs and differences from the GBA game to work through. All levels are open for
playtesting, so please report anything that goes wrong.

## Controls

| Control | Action |
| --- | --- |
| WASD | Move |
| Mouse / left-click | Look / fire |
| Space | Jump (enable in Extras) |
| H | Switch between first- and third-person view |
| E | Use doors and switches |
| 1–0 / mouse wheel | Choose a weapon |
| Q | Return to the previous weapon |
| R | Reload Colts (Alternate weapons) |
| Tab / N | Open or close NETRICSA |
| M | Open or close the automap (enable in Extras) |
| + / − / mouse wheel in automap | Zoom the map |
| Esc / P | Pause or resume |
| F11 | Toggle borderless fullscreen / windowed mode |

## Settings and extras

Open **Options → Settings** to choose fullscreen or windowed play, change the
graphics mode, and adjust music and sound volume. Set a volume to 0% to mute it.
Below Renderer, **Aspect ratio** offers **Native** (fill the window, default)
or **3:2** (preserve the original GBA view with black bars). This choice applies
to the software renderer; the GBA renderer always uses 3:2 and disables the row.

Music choices are **Original**, **SoundFont** and **MIDI**. SoundFont uses an
optional instrument file; MIDI uses the Windows GS synthesizer. You’ll only see choices
that work on your computer. See the
[SoundFont setup guide](soundfonts/README.md) if you want to try it.

Under **Options → Extras**, you can turn jumping, third-person view, in-game
NETRICSA and the automap on or off. **Weapon bobbing** moves first-person weapons
and their muzzle flashes together. **Jump**, **Alternate weapons**, **Automap**
and **Weapon bobbing** are off by default.
**Surface decals** adds subtle connected stain tiles on floors and ceilings,
and sparse animated glints and ripples on water. **Water reflections** adds
reflected scenery with multiply blending. Both are off by default and work in
both renderers. All nine Extras choices fit on one page.
The **HUD** row switches between **Original** (default) and **Classic**, a usable
green HUD built from the retained older artwork. Both graphics modes support it,
and the game saves your choice.
Classic also shows a centered weapon selector when switching weapons. The selected
weapon and its ammo highlight white immediately; the selector hides after three seconds or when firing.
Classic shows the high score at the top center, with the timer underneath.
Its message counter shows unread NETRICSA weapon/enemy dossiers. Enemy
dossiers unlock after a kill. Viewing an entry clears its unread status.
Opening NETRICSA during play selects the newest unread dossier automatically;
reopening it after reading advances to the next unread entry.
Every level's starting equipment is already familiar and does not add unread
messages. Weapons discovered during play still create new entries.
In Classic HUD, new entries play a notification cue twice with half a cue's duration of silence between them,
and blink the analyzing text in HUD green without a background, then move the
blinking message panel inward and back like TFE. Classic HUD follows Serious
Engine-style positioning in widescreen and4:3 windows.
Original HUD does not play the new-entry notification cue.
Its timer has a compact backing, usable buttons show the retained green Use
sprite, and Tommygun ammo uses a custom vertical-bullet badge distinct from Minigun rounds.
Low health/ammo counters and ammo bars turn red; low armor turns gray, using
TFE's alert thresholds. The white damage/ammo flashes still take priority.
Original HUD preserves the GBA layout and pixel padding: lives upper left,
weapon icon upper right, score/time top center, status bars lower left and ammo
lower right. GBA rendering keeps the original coordinates intact; PC rendering
anchors those same groups to the window edges without changing their padding.
Existing saved choices are retained. Turn Alternate weapons on to use
single/dual Colts with reloads and a different double-shotgun spread. Once you
have both Colts, press **2** again to switch between one and two guns.

## Saves and problems

Beating a level unlocks the next one in **Select Level**. New Game keeps your
unlocks. Passwords start you with the original weapons, ammo and difficulty for
that level. The game saves your unlocks and settings, but not your place halfway through a level.
Finishing the campaign or quitting a run through Pause
offers name entry if your score qualifies. Type up to twelve characters and
press Enter to save it. Dying consumes a life and automatically retries the level
with its starting equipment and score. After the last life, Game Over plays
before returning to Main. No restart confirmation is required.

If the game closes with an error, the launcher shows it and saves a log in
`build/logs/`. If you report a problem, please include the level, what happened, your
graphics/music settings and the log if you have it.

**[Report a problem](https://github.com/ZynChroMaTiK/ss-advance-decomp/issues)**
· [See what is planned](https://zynchromatik.github.io/ss-advance-decomp/#features)

## Building it yourself

Want to work on the code? Here’s how to build it.

Install Git and Visual Studio 2022 Build Tools with the **C++ desktop workload**
and **CMake tools**. Clone this repository, open PowerShell in its folder and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
```

You can build without a ROM. To play afterwards, put your own **European ROM** at
`orig/SSA.gba` and run `play-native.cmd`.

Run focused regression checks with `scripts/test-focused.ps1` (add `-SkipBuild`
to reuse the build). These cover settings, saves, audio status, rendering and
frontend screens without running campaign playthroughs. See
[the technical refactor notes](analysis/technical-refactors.md) for details.

## Looking through the code

The game source is in `src/`, `include/ssa/` and `platform/`. The website is in
`site/`. Tests and research notes explain how the port compares with the original.

- [Build setup and earlier development notes](DEVELOPMENT.md)
- [Current porting notes](analysis/porting.md)
- [Code recovered from the GBA game](recovered/snapshot/README.md)
- [Website editing guide](site/README.md)

Serious Sam and its artwork belong to their original owners. ROMs, personal
saves and SoundFont files are not included in this repository.
