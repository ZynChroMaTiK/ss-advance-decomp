# Serious Sam Advance for PC

Play the original GBA game on Windows with keyboard and mouse controls, a sharper
picture and a few optional extras. All twelve campaign levels are available.

**[Download v0.8.0 Alpha 1](https://github.com/ZynChroMaTiK/ss-advance-decomp/releases/tag/v0.8.0-alpha.1)**
· [Website and roadmap](https://zynchromatik.github.io/ss-advance-decomp/)
· [Explore the maps](https://zynchromatik.github.io/ss-advance-decomp/#levels/1/2d)

This is an unofficial fan project and an early release. Expect bugs and some
unfinished features. It runs directly on Windows; you do not need an emulator.

## Download and play

**You need your own ROM of the European version of Serious Sam Advance.**
A ROM is a file containing the original GBA game. Other regional versions are
not supported, and no ROM is included in the download.

1. Download the **Windows x64 ZIP** from the release page.
2. Extract it to a folder where you can save files, such as your Games folder.
3. Name your European ROM **`SSA.gba`** and put it in the **`orig`** folder.
4. Double-click **`play-native.cmd`**.

You do not need to install build tools. SoundFont music is optional; the original
music works without any extra downloads. If you are updating, keep your
`save.dat` and `settings.dat` files to retain level unlocks and preferences.

If the game cannot load your ROM, check its region and filename. The supported
European ROM has SHA-1 `4ff48d8c18b70a5184758f1917343ae85a70858e`.
This fingerprint is only needed if you want to check the file you have.

## Current prototype

- All twelve campaign levels, from Egypt to Caesar's Palace.
- Original enemies, weapons, pickups, music and sound effects.
- Mouse look, optional jumping and a third-person camera.
- Graphics that fit your window, or the original GBA-resolution view.
- NETRICSA briefings and an in-game guide, chapter screens and the ending.
- Pause menus, end-of-level bonuses, saved level unlocks and original passwords.
- English, German and French menus.
- Optional dual Colts and alternate double-shotgun behavior.

High-score lists, name entry and multiplayer are still unfinished. We are also
working on bugs and differences from the original game. All levels are open for
testing, but that does not mean the whole campaign is bug-free.

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

## Make it yours

Open **Options → Settings** to choose fullscreen or windowed play, change the
graphics mode, and adjust music and sound volume. Set a volume to 0% to mute it.

Music choices are **Original**, **SoundFont** and **MIDI**. SoundFont uses an
optional instrument file; MIDI uses the Windows GS synthesizer. A choice appears
only when it is available on your computer. See the
[SoundFont setup guide](soundfonts/README.md) if you want to try it.

Under **Options → Extras**, you can turn jumping, third-person view and in-game
NETRICSA on or off. **Alternate weapons** is off by default. Turn it on to use
single/dual Colts with reloads and a different double-shotgun spread. Once you
have both Colts, press **2** again to switch between one and two guns.

## Progress and troubleshooting

Beating a level unlocks the next one in **Select Level**. New Game keeps your
unlocks. Passwords start you with the original weapons, ammo and difficulty for
that level. Saves track unlocks and preferences; they are not mid-level saves.

If the game closes with an error, the launcher shows it and saves a log in
`build/logs/`. When reporting a problem, include the level, what happened, your
graphics/music settings and that log if available.

**[Report a problem](https://github.com/ZynChroMaTiK/ss-advance-decomp/issues)**
· [See what is planned](https://zynchromatik.github.io/ss-advance-decomp/#features)

## Build from a fresh checkout

This is only needed if you want to compile the game yourself.

Install Git and Visual Studio 2022 Build Tools with the **C++ desktop workload**
and **CMake tools**. Clone this repository, open PowerShell in its folder and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1
```

You can build without a ROM. To play afterwards, put your own **European ROM** at
`orig/SSA.gba` and run `play-native.cmd`.

## For contributors

The game source is in `src/`, `include/ssa/` and `platform/`. The website is in
`site/`. Tests and research notes explain how the port compares with the original.

- [Development setup and research archive](DEVELOPMENT.md)
- [Current porting notes](analysis/porting.md)
- [Recovered source archive](recovered/snapshot/README.md)
- [Website editing guide](site/README.md)

The original game's artwork belongs to its respective owners. ROMs, personal
saves and SoundFont files are not included in this repository.