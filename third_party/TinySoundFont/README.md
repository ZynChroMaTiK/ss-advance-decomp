# TinySoundFont

Unmodified `tsf.h` and MIT license from
https://github.com/schellingb/TinySoundFont at commit
`853a0a171759f1ddba0de1442133a75912bbeffa`.

The port's adapter is `src/audio/soundfont.c`. This library is compiled locally;
no DLL, external synthesizer process, MIDI device, or runtime network is needed.
Its sample renderer does not implement all SoundFont modulators, reverb or chorus.
The adapter serializes use with the existing audio mutex and preallocates voices
and channels. Scene resets clear voices immediately using this pinned version's
voice state, leaving channels allocated for the audio worker.
