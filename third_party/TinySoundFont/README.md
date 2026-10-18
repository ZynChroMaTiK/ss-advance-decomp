# TinySoundFont

This is the small library that plays SoundFont instruments. It’s built into
the game, so there’s no extra program to run or internet connection needed.

**To play, you’ll need your own European version of the Serious Sam Advance ROM** at `orig/SSA.gba`. Other regional versions won’t work. No ROM is included.

The unmodified `tsf.h` and [MIT license](LICENSE) come from [TinySoundFont](https://github.com/schellingb/TinySoundFont), commit `853a0a171759f1ddba0de1442133a75912bbeffa`.

See [the SoundFont guide](../../soundfonts/README.md) to choose an instrument pack. If you’re working on the audio code, start with `src/audio/soundfont.c` and [the audio notes](../../analysis/audio-settings.md). Some SoundFont features, such as reverb and chorus, aren’t supported.
