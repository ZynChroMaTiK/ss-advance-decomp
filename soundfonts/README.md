# External SoundFonts

The default SoundFont is `SC-55.sf2` in this directory. Install the requested
[SC-55 from Musical Artifacts 1228](https://musical-artifacts.com/artifacts/1228/SC-55.sf2):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/install-soundfont.ps1
```

If the site requires a browser challenge, download the linked file manually to
`soundfonts/SC-55.sf2`, then run the script to verify it. Restart the game after
replacing the file so it loads the new bank.

Then choose **Options > Settings > Music style > SoundFont**. Original remains
the default. Other GM SoundFonts can be supplied with
`play-native.cmd --soundfont "C:\Music\my-bank.sf2"`; the bank must include
melodic presets and a standard drum kit (bank128, program0). Files larger than
64 MiB are rejected. The path is a launch option; style and volumes are saved.

The port supplies its own instrument arrangement; it is not an SC-55 hardware
emulation. Only PSG music uses SoundFont instruments; all ROM sampled music
instruments remain native, including sampled drums. SoundFont music does not
inherit Original-mode fixed volume boosts/cuts. Its synth layer has a 15 dB trim
calibrated against retained ROM samples for this bank. A shared +3 dB music gain
then raises both layers together. Final output gains raise both music modes to
full peaks (Original +9.54 dB, SoundFont +6.02 dB) with stereo-linked peak
protection. SoundFont binaries stay local
and are excluded from Git.

Expected SHA-256:
`ae2e56711dc40835d3f97d975d80eadb2abdc76dc9da8b055f7a3c18dab20974`
(47,324,042 bytes; verified against the user's manually downloaded file).
