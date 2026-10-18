param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$directory=Join-Path $root 'build/midi-transition'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$exe=Join-Path $root 'build/native-windows/Release/ssa_native_midi_transition_check.exe'
$output=@(& $exe (Join-Path $root 'orig/SSA.gba'))
$code=$LASTEXITCODE
$output | Set-Content -LiteralPath (Join-Path $directory 'capture.jsonl')
if($code -ne 0){$output;throw 'MIDI map/fade/level output check failed.'}
$rows=@($output | ForEach-Object {$_ | ConvertFrom-Json})
if($rows.Count -ne 3 -or $rows[2].result -ne 'pass' -or $rows[2].transitions -ne 2 -or -not $rows[2].pcm_music_excluded){throw 'Incomplete MIDI transition capture.'}
foreach($case in $rows[0..1]){
    if($case.map_peak -le 0.001 -or $case.level_peak -le 0.001 -or $case.faded_expression -ne 0 -or $case.level_expression -le 0 -or $case.errors -ne 0){throw 'Silent or invalid MIDI handoff.'}
}
$report=[ordered]@{result='pass';scope='Real Windows MIDI map/fade/level audio, Egypt and Rome. Native PCM music excluded from process-only loopback capture.';cases=@($rows[0..1]);microphone_used=$false;physical_listening_tested=$false;process_loopback_only=$true;executable_sha256=(Get-FileHash -LiteralPath (Join-Path $root 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant();source_sha256=[ordered]@{}}
foreach($path in @('src/audio.c','include/ssa/audio.h','platform/win32/midi_win32.c','platform/win32/midi_win32.h','platform/win32/audio_win32.c','platform/win32/audio_win32.h','tests/test_midi_transition_win32.cpp','tests/test_midi_sync_win32.cpp','CMakeLists.txt','scripts/test-midi-transition.ps1')){
    $report.source_sha256[$path]=(Get-FileHash -LiteralPath (Join-Path $root $path) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-midi-transition-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
Write-Output 'Passed actual MIDI output after full map fades in Egypt and Rome; no microphone or unrelated-process capture.'
