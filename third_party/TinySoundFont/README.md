# TinySoundFont

TinySoundFont plays the optional SoundFont instruments used by the PC port. It is built into the game, so it needs no separate program or internet connection while playing.

The unmodified `tsf.h` and [MIT license](LICENSE) come from [TinySoundFont](https://github.com/schellingb/TinySoundFont), commit `853a0a171759f1ddba0de1442133a75912bbeffa`.

See [the SoundFont guide](../../soundfonts/README.md) to choose an instrument pack. Contributors can find the game’s integration in `src/audio/soundfont.c` and [the audio notes](../../analysis/audio-settings.md). This library does not support every SoundFont feature, including reverb and chorus.
