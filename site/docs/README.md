# Serious Sam Advance for PC

This is a fan-made Windows port of the GBA game. It keeps the original levels,
weapons, enemies and music, with keyboard and mouse controls and a few optional
extras. You can play all twelve campaign levels.

**[Download v0.8.0 Alpha 1](https://github.com/ZynChroMaTiK/ss-advance-decomp/releases/tag/v0.8.0-alpha.1)**
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
`save.dat` and `settings.dat` so you don’t lose your unlocks or settings.

If the game cannot load your ROM, check its region and filename. The supported
European ROM has SHA-1 `4ff48d8c18b70a5184758f1917343ae85a70858e`.
This fingerprint is only needed if you want to check the file you have.

## What you can play

- All twelve campaign levels, from Egypt to Caesar's Palace.
- Original enemies, weapons, pickups, music and sound effects.
- Mouse look, optional jumping and a third-person camera.
- Graphics that fit your window, or the original GBA-resolution view.
- NETRICSA briefings and an in-game guide, chapter screens and the ending.
- Pause menus, end-of-level bonuses, saved level unlocks and original passwords.
- English, German and French menus.
- Optional dual Colts and alternate double-shotgun behavior.

High-score lists, name entry and multiplayer aren’t ready yet. There are also
bugs and differences from the GBA game to work through. All levels are open for
playtesting, so please report anything that goes wrong.

## Controls

| Control | Action |
| --- | --- |
| WASD | Move |
| Mouse / left-click | Look / fire |
| Space | Jump |
| H | Switch between first- and third-person view |
| E | Use doors and switches |
| 1–0 / mouse wheel | Choose a weapon |
| Q | Return to the previous weapon |
| Tab / N | Open or close NETRICSA |
| Esc / P | Pause or resume |
| R | Restart the level |

## Settings and extras

Open **Options → Settings** to choose fullscreen or windowed play, change the
graphics mode, and adjust music and sound volume. Set a volume to 0% to mute it.

Music choices are **Original**, **SoundFont** and **MIDI**. SoundFont uses an
optional instrument file; MIDI uses the Windows GS synthesizer. You’ll only see choices
that work on your computer. See the
[SoundFont setup guide](soundfonts/README.md) if you want to try it.

Under **Options → Extras**, you can turn jumping, third-person view and in-game
NETRICSA on or off. **Alternate weapons** is off by default. Turn it on to use
single/dual Colts with reloads and a different double-shotgun spread. Once you
have both Colts, press **2** again to switch between one and two guns.

## Saves and problems

Beating a level unlocks the next one in **Select Level**. New Game keeps your
unlocks. Passwords start you with the original weapons, ammo and difficulty for
that level. The game saves your unlocks and settings, but not your place halfway through a level.

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

## Looking through the code

The game source is in `src/`, `include/ssa/` and `platform/`. The website is in
`site/`. Tests and research notes explain how the port compares with the original.

- [Build setup and earlier development notes](DEVELOPMENT.md)
- [Current porting notes](analysis/porting.md)
- [Code recovered from the GBA game](recovered/snapshot/README.md)
- [Website editing guide](site/README.md)

Serious Sam and its artwork belong to their original owners. ROMs, personal
saves and SoundFont files are not included in this repository.
