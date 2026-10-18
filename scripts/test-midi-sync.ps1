param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$directory=Join-Path $root 'build/midi-sync'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$results=@()
foreach($delay in @(0,185)){
    $output=Join-Path $directory "sync-$delay.json"
    $errorLog=Join-Path $directory "sync-$delay.log"
    $process=Start-Process -FilePath (Join-Path $root 'build/native-windows/Release/ssa_native_midi_sync_check.exe') `
        -ArgumentList @($delay) -WorkingDirectory $root -WindowStyle Hidden -PassThru -Wait `
        -RedirectStandardOutput $output -RedirectStandardError $errorLog
    if($process.ExitCode -ne 0){Get-Content $output;Get-Content $errorLog;throw 'MIDI timing check failed.'}
    $results+=Get-Content $output -Raw | ConvertFrom-Json
}
$report=[ordered]@{result='pass';device='Microsoft GS Wavetable Synth';audible_markers=$true;microphone_used=$false;cases=$results;source_sha256=[ordered]@{}}
foreach($file in @('src/audio.c','include/ssa/audio.h','platform/win32/midi_win32.c','platform/win32/midi_win32.h','tests/test_midi_sync_win32.cpp')){
    $report.source_sha256[$file]=(Get-FileHash -LiteralPath (Join-Path $root $file) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-midi-sync-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
Write-Output ('MIDI timing: baseline '+$results[0].max_error_ms+' ms; corrected maximum error '+$results[1].max_error_ms+' ms.')
