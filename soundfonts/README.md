# Optional SoundFont music

Want to try a different sound for the music? A SoundFont supplies a different
set of instruments. It’s optional—**Original** music works without one.

**To play, you’ll need your own European version of the Serious Sam Advance ROM**, saved
as `orig/SSA.gba`. Other regional versions won’t work. No ROM is included.

## Set it up

From the main game folder, run this in PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/install-soundfont.ps1
```

This downloads the optional **SC-55** instrument file. If the download fails, [download it in your browser](https://musical-artifacts.com/artifacts/1228/SC-55.sf2)
and save it as `soundfonts/SC-55.sf2`. Run the script again to check the file.

Restart the game, then choose **Options → Settings → Music style → SoundFont**.
SoundFont will appear in the menu once the game can load the file.

## Use another instrument file

You can try another General MIDI SoundFont with:

```bat
play-native.cmd --soundfont "C:\Music\my-bank.sf2"
```

Choose a file no larger than 64 MiB with instruments and a standard drum kit. The game remembers your music choice and volume, but you’ll need to use
this command each time you launch with a custom file.

The original recorded instruments and drums remain part of the music. SoundFont
changes the synthesized instruments. If you’re curious about how the two are mixed, see
[audio notes](../analysis/audio-settings.md).

The SC-55 file is not bundled with the game. Its expected SHA-256 fingerprint is
`ae2e56711dc40835d3f97d975d80eadb2abdc76dc9da8b055f7a3c18dab20974`
(47,324,042 bytes), if you need to verify your download.
