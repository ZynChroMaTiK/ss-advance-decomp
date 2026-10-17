param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$launcher = Join-Path $projectRoot 'play-native.cmd'
$testRoot = Join-Path $projectRoot 'build/native-startup-tests'
$log = Join-Path $testRoot 'startup-launch.log'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$savedPause = $env:SSA_LAUNCH_NO_PAUSE
$savedLog = $env:SSA_LAUNCH_LOG
$env:SSA_LAUNCH_NO_PAUSE = '1'
$env:SSA_LAUNCH_LOG = $log
$results = @()
# Normal startup registers raw input and paints a real window without taking
# focus. It auto-exits after120 ticks, exercising restart along the way.
Push-Location $testRoot
try {
    $failure = & $launcher --invalid-startup-test-option | Out-String
    if ($LASTEXITCODE -ne 2 -or $failure -notmatch 'exited with code 2' -or
        -not (Test-Path -LiteralPath $log) -or (Get-Content -LiteralPath $log -Raw) -notmatch 'Usage:') {
        throw 'Launcher did not preserve/report its failure.'
    }
    Copy-Item -LiteralPath $log -Destination (Join-Path $testRoot 'expected-failure.log')
    foreach ($original in @($false,$true)) {
        $arguments = @('--startup-test')
        if ($original) { $arguments += '--original-renderer' }
        $item = & $launcher @arguments | ConvertFrom-Json
        if ($LASTEXITCODE -ne 0 -or $item.result -ne 'pass' -or $item.startup_path -ne 'normal' -or
            $item.frames -lt 120 -or -not $item.raw_input_registered -or $item.buffered_presents -lt 2 -or
            $item.restarts -ne 1 -or $item.map_views -ne 2 -or $item.map_starts -ne 2 -or
            $item.map_frozen_ticks -ne 140 -or $item.map_black_presents -ne 2 -or $item.map_fade_presents -lt 2 -or
            $item.map_black_hold_ticks -ne 30 -or $item.level_fade_ticks -ne 120 -or $item.level_fade_starts -ne 2 -or
            $item.level_fade_presents -lt 2 -or $item.black_hold_min_us -lt 250000 -or $item.level_fade_min_us -lt 1000000 -or
            $item.menu_starts -ne 1 -or $item.menu_frozen_ticks -ne 187 -or
            $item.difficulty -ne 1 -or (Get-Item -LiteralPath $log).Length -ne 0) {
            throw 'Normal startup/restart through the launcher failed.'
        }
        $results += @{ original_renderer = $original; startup = $item }
    }
    # Default launches must coexist even if an older game still holds the
    # former shared last-launch.log open. Each child owns its stderr file.
    $beforeLogs = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'build/logs') -Directory -Filter 'launch-*' | ForEach-Object { $_.Name })
    $env:SSA_LAUNCH_LOG = $null
    $children = @()
    foreach ($index in 0..1) {
        $arguments = '""' + $launcher + '" --startup-test' + $(if ($index) { ' --original-renderer' } else { '' }) + '"'
        $child = Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',$arguments) -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $testRoot ('concurrent-' + $index + '.json')) -RedirectStandardError (Join-Path $testRoot ('concurrent-' + $index + '.log'))
        $null = $child.Handle
        $children += $child
    }
    foreach ($index in 0..1) {
        if (-not $children[$index].WaitForExit(30000)) { throw 'Concurrent launcher startup timed out.' }
        $children[$index].Refresh()
        $item = Get-Content -LiteralPath (Join-Path $testRoot ('concurrent-' + $index + '.json')) -Raw | ConvertFrom-Json
        if ($children[$index].ExitCode -ne 0 -or $item.result -ne 'pass' -or $item.restarts -ne 1) { throw 'Concurrent launcher startup failed.' }
    }
    $newLogs = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'build/logs') -Directory -Filter 'launch-*' | Where-Object { $_.Name -notin $beforeLogs })
    if ($newLogs.Count -lt 2) { throw 'Concurrent launches did not retain separate session logs.' }
    $report = [ordered]@{ result='pass'; cases=$results; launcher_error_exit=2
        concurrent_launches=2; distinct_session_logs=$true
        launched_from_different_directory=$true; physical_input_tested=$false
        executable_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
        source_sha256=[ordered]@{} }
    foreach ($source in @('play-native.cmd','platform/win32/game.c','src/map_view.c','include/ssa/map_view.h',
        'src/menu.c','include/ssa/menu.h','scripts/test-native-startup.ps1')) {
        $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    [System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-native-startup-validation.json'),
        ($report | ConvertTo-Json -Depth 7) + [Environment]::NewLine)
    Write-Output 'Passed normal PC/reference startup, raw-input registration, initial paint, restart and launcher error logging.'
} finally {
    Pop-Location
    $env:SSA_LAUNCH_NO_PAUSE = $savedPause
    $env:SSA_LAUNCH_LOG = $savedLog
}
