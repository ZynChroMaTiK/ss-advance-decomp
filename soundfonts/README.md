# Optional SoundFont music

A SoundFont is an instrument file that gives some of the music a different
sound. You can skip this entirely and use **Original** music instead.

## Set it up

From the main game folder, run this in PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/install-soundfont.ps1
```

This downloads the optional **SC-55** instrument file. If the download does not
work, [download it in your browser](https://musical-artifacts.com/artifacts/1228/SC-55.sf2)
and save it as `soundfonts/SC-55.sf2`. Run the script again to check the file.

Restart the game, then choose **Options → Settings → Music style → SoundFont**.
The choice only appears if the file loads successfully.

## Use another instrument file

You can try another General MIDI SoundFont with:

```bat
play-native.cmd --soundfont "C:\Music\my-bank.sf2"
```

The file must be no larger than 64 MiB and include instruments and a standard drum
kit. The game remembers your music choice and volume, but you need to supply
this path again when using a custom file.

The original recorded instruments and drums remain part of the music. SoundFont
changes the synthesized instruments. For the detailed mixing settings, see
[audio notes](../analysis/audio-settings.md).

The SC-55 file is not bundled with the game. Its expected SHA-256 fingerprint is
`ae2e56711dc40835d3f97d975d80eadb2abdc76dc9da8b055f7a3c18dab20974`
(47,324,042 bytes), if you need to verify your download.