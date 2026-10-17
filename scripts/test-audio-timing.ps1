param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$testRoot = Join-Path $projectRoot 'build/native-audio'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$checker = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_audio_timing_check.exe'
$process = Start-Process -FilePath $checker -ArgumentList @((Join-Path $projectRoot 'orig/SSA.gba')) `
    -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput (Join-Path $testRoot 'timing.jsonl') -RedirectStandardError (Join-Path $testRoot 'timing.log')
if ($process.ExitCode -ne 0) {
    Get-Content -LiteralPath (Join-Path $testRoot 'timing.jsonl')
    Get-Content -LiteralPath (Join-Path $testRoot 'timing.log')
    throw 'Audio device timing failed with the UI idle.'
}
$cases = @(Get-Content -LiteralPath (Join-Path $testRoot 'timing.jsonl') | ForEach-Object { $_ | ConvertFrom-Json })
if ($cases.Count -ne 3) { throw 'Missing timing cases.' }
foreach ($item in $cases) {
    if ($item.result -ne 'pass' -or $item.seconds -lt 3 -or $item.speed_ratio -lt 0.98 -or
        $item.speed_ratio -gt 1.04 -or $item.errors -ne 0) { throw 'Audio playback drifted from wall time.' }
}
$report = [ordered]@{ result='pass'; output_muted=$true; physical_listening_tested=$false
    cases=$cases; checker_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $checker).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{} }
foreach ($source in @('src/audio.c','include/ssa/audio.h','platform/win32/audio_win32.c','platform/win32/audio_win32.h',
    'tests/test_audio_win32_timing.c','scripts/test-audio-timing.ps1','CMakeLists.txt')) {
    $report.source_sha256[$source]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
}
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-audio-timing-validation.json'),
    ($report | ConvertTo-Json -Depth 6) + [Environment]::NewLine)
Write-Output 'Passed wall-clock audio timing: idle music, assisted music and idle music with 16 concurrent effects.'
