param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$test=Join-Path $root 'build/audio-settings'
New-Item -ItemType Directory -Force -Path $test | Out-Null
$cases=@{}
foreach($name in @('core','windows')){
    $exe=if($name -eq 'core'){'ssa_native_audio_settings_check.exe'}else{'ssa_native_settings_win32_check.exe'}
    $extra=if($name -eq 'core'){'build/audio-settings'}else{'build/audio-settings/settings-test.dat'}
    $output=Join-Path $test "$name.json"
    $process=Start-Process -FilePath (Join-Path $root "build/native-windows/Release/$exe") -ArgumentList @('orig/SSA.gba','soundfonts/SC-55.sf2',$extra) `
        -WorkingDirectory $root -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $output -RedirectStandardError (Join-Path $test "$name.log")
    if($process.ExitCode -ne 0){Get-Content (Join-Path $test "$name.log");throw "Settings $name check failed."}
    $cases[$name]=Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
}
$report=[ordered]@{result='pass';core=$cases.core;windows=$cases.windows;physical_listening_tested=$false;source_sha256=[ordered]@{}}
$soundfont=Join-Path $root 'soundfonts/SC-55.sf2'
$report.soundfont_sha256=(Get-FileHash -LiteralPath $soundfont -Algorithm SHA256).Hash.ToLowerInvariant()
$report.soundfont_bytes=(Get-Item -LiteralPath $soundfont).Length
foreach($source in @('src/audio.c','src/audio/soundfont.c','src/menu.c','src/settings.c','include/ssa/audio.h','include/ssa/soundfont.h','include/ssa/menu.h','include/ssa/settings.h',
    'platform/win32/audio_win32.c','platform/win32/audio_win32.h','platform/win32/midi_win32.c','platform/win32/midi_win32.h','include/ssa/music_instrument.h',
    'platform/win32/game.c','platform/win32/profile_win32.c','tests/test_audio_settings.c','tests/test_settings_win32.c','third_party/TinySoundFont/tsf.h')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $root $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-audio-settings-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
Write-Output "Passed SoundFont availability/catalogue, Microsoft GS MIDI, Settings UI, native Windows input/audio and preference persistence."
