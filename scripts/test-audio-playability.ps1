param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$testRoot = Join-Path $projectRoot 'build/native-audio'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$checker = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_audio_playability_check.exe'
$output = Join-Path $testRoot 'completion.jsonl'
$process = Start-Process -FilePath $checker -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput $output -RedirectStandardError (Join-Path $testRoot 'completion.log')
if ($process.ExitCode -ne 0) { throw 'Windows audio-enabled level completion failed.' }
$lines = @(Get-Content -LiteralPath $output | Where-Object { $_.Trim() })
if ($lines.Count -ne 2) { throw 'Missing completion/audio results.' }
$completion = $lines[0] | ConvertFrom-Json
$audio = $lines[1] | ConvertFrom-Json
$reference = Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/audio-playability-validation.json') -Raw | ConvertFrom-Json
foreach ($field in @('frames','finished','score','collected','spawned','health','pistol_shots','shotgun_shots')) {
    if ($completion.$field -ne $reference.completion.$field) { throw ('MSVC/GCC completion differs: ' + $field) }
}
if ($completion.result -ne 'pass' -or $audio.errors -ne 0 -or $audio.clipped_samples -ne 0 -or $audio.effects_limited_frames -lt 1 -or $audio.audio_frames -ne $completion.frames -or
    $audio.mixed_frames -ne $completion.frames*800 -or $audio.notes -ne $reference.audio.notes -or $audio.nonzero_samples -lt 1 -or
    $audio.footstep_events -lt 1 -or $audio.footstep_events -ne $reference.audio.footstep_events -or $audio.music_preserved -ne 1) {
    throw 'Audio-enabled completion/mixing diverged.'
}
foreach ($field in @('spatial_events','listener_updates','source_updates')) {
    if ($audio.$field -ne $reference.audio.$field) { throw ('MSVC/GCC audio scene differs: ' + $field) }
}
if ($audio.spatial_events -lt 1 -or $audio.listener_updates -lt $completion.frames) { throw 'Missing spatial playback/listener updates.' }
foreach ($field in @('max_effect_instances','max_effect_voices','max_music_voices','effect_recycles','voice_recycles')) {
    if ($audio.$field -ne $reference.audio.$field) { throw ('MSVC/GCC audio concurrency differs: ' + $field) }
}
foreach ($field in @('door_moving_frames','door_stopped_loop_frames','door_unowned_loop_frames')) {
    if ($audio.$field -ne $reference.audio.$field) { throw ('MSVC/GCC door audio differs: ' + $field) }
}
if ($audio.door_moving_frames -lt 1 -or $audio.door_stopped_loop_frames -ne 0 -or $audio.door_unowned_loop_frames -ne 0) {
    throw 'Door movement loop survived its mover or was routed to another source.'
}
if ($audio.max_effect_instances -lt 2 -or $audio.max_effect_voices -lt 2 -or $audio.max_music_voices -lt 1 -or
    $audio.effect_recycles -ne 0 -or $audio.voice_recycles -ne 0) { throw 'Music/effects lost voices during normal gameplay.' }
if ($audio.music_events -ne 1 -or $audio.completion_song -ne 0x78) { throw 'Missing one-shot original level-completion music transition.' }
if ($audio.level_seen -ne 1) { throw 'Missing gameplay level music.' }
$report = [ordered]@{ result='pass'; completion=$completion; audio=$audio
    checker_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $checker).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{} }
foreach ($source in @('src/audio.c','include/ssa/audio.h','src/native_game.c','src/native_audio.inc','include/ssa/native_game.h',
    'tests/test_audio_playability.c','tests/test_native_playability.c','scripts/test-audio-playability.ps1')) {
    $report.source_sha256[$source]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
}
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-audio-playability-validation.json'),
    ($report | ConvertTo-Json -Depth 6) + [Environment]::NewLine)
Write-Output ('Passed Windows audio-enabled level completion: ' + $completion.frames + ' ticks, ' + $audio.mixed_frames + ' mixed frames, ' + $audio.notes + ' notes, no audio errors.')
