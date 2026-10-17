param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$checker = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_weapon_cadence_check.exe'
$testRoot = Join-Path $projectRoot 'build/native-tests'
$process = Start-Process -FilePath $checker -ArgumentList @((Join-Path $projectRoot 'orig/SSA.gba')) `
    -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput (Join-Path $testRoot 'windows-weapon-cadence.json') `
    -RedirectStandardError (Join-Path $testRoot 'windows-weapon-cadence.log')
if ($process.ExitCode -ne 0) {
    Get-Content -LiteralPath (Join-Path $testRoot 'windows-weapon-cadence.log')
    throw 'Windows automatic weapon cadence failed.'
}
$result = Get-Content -LiteralPath (Join-Path $testRoot 'windows-weapon-cadence.json') -Raw | ConvertFrom-Json
if ($result.result -ne 'pass' -or $result.cases -ne 21 -or $result.click_cases -ne 8 -or $result.cancel_cases -ne 12 -or $result.original_frame_vblanks -ne 7 -or
    $result.gba_cpu_hz -ne 16777216 -or $result.gba_frame_cycles -ne 280896 -or $result.simulation_hz -ne 60 -or
    $result.chainsaw_range -ne 192 -or [Math]::Abs($result.chainsaw_shots_per_second-8.693214980) -gt 0.000000001 -or
    [Math]::Abs($result.minigun_shots_per_second-8.532500081) -gt 0.000000001) {
    throw 'Unexpected weapon cadence result.'
}
$report = [ordered]@{ result='pass'; cadence=$result
    executable_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe')).Hash.ToLowerInvariant()
    checker_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $checker).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{} }
foreach ($source in @('src/weapon_controller.c','src/weapon_runtime.c','src/native_game.c','src/pc_input_runtime.c',
    'include/ssa/weapon_controller.h','include/ssa/weapon_runtime.h','tests/test_weapon_cadence.c',
    'scripts/test-weapon-cadence.ps1','CMakeLists.txt')) {
    $report.source_sha256[$source]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
}
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-weapon-cadence-validation.json'),
    ($report | ConvertTo-Json -Depth 6) + [Environment]::NewLine)
Write-Output ('Passed Windows weapon input/cadence: ' + $result.checks + ' checks, 21 timing/damage scenarios, 8 quick-click cases and 12 cancellation cases.')
