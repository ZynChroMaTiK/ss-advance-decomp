param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$binary = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$before = Join-Path $projectRoot 'build/native-game-smoke/ssa_native_game_before.exe'
$rom = Join-Path $projectRoot 'orig/SSA.gba'
$results = @()
Push-Location $projectRoot
try {
    foreach ($look in @($false,$true)) {
        foreach ($version in @('before','after')) {
            $exe = if ($version -eq 'before') { $before } else { $binary }
            if (-not (Test-Path -LiteralPath $exe)) { continue }
            $arguments = @($rom,'--smoke','--smoke-frames','180','--resolution','1280x720')
            if ($look) { $arguments += '--smoke-look' }
            $watch = [System.Diagnostics.Stopwatch]::StartNew()
            $item = & $exe @arguments | ConvertFrom-Json
            $watch.Stop()
            if ($LASTEXITCODE -ne 0 -or $item.result -ne 'pass' -or $item.frames -ne 180) { throw 'Performance run failed.' }
            $results += [ordered]@{ version=$version; vertical_look=$look; frames=$item.frames
                elapsed_ms=$watch.Elapsed.TotalMilliseconds; simulated_frames_per_second=180/$watch.Elapsed.TotalSeconds }
        }
    }
    # Exercise the actual GDI memory-DIB composition and finished-frame blit
    # on every frame, including resize, jumping, H and changing pitch.
    $present = & $binary $rom --smoke --smoke-look --smoke-present-each --smoke-resize | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or $present.result -ne 'pass' -or $present.frames -ne 600 -or
        $present.buffered_presents -lt 600 -or $present.presentation_samples -lt 1800 -or
        $present.render_width -eq $present.initial_width -or $present.render_height -eq $present.initial_height) {
        throw 'Buffered presentation/resize checks failed.'
    }
    $report = [ordered]@{ result='pass'; timing_scope='Local hidden-window throughput, including simulation/rendering and process startup; real-time simulation remains fixed at60Hz.'
        physical_display_tested=$false; timings=$results; presentation=$present; source_sha256=[ordered]@{}
        executable_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant() }
    if (Test-Path -LiteralPath $before) { $report.before_executable_sha256=(Get-FileHash -LiteralPath $before -Algorithm SHA256).Hash.ToLowerInvariant() }
    foreach ($source in @('platform/win32/game.c','src/native_game.c','src/pc_renderer.c','src/pc_renderer_pitch.inc','src/weapon_runtime.c')) {
        $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    [System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-render-performance-validation.json'),
        ($report | ConvertTo-Json -Depth 7) + [Environment]::NewLine)
    Write-Output 'Passed 600 buffered presentations, 1800+ pixel checks, live resize and throughput measurements.'
} finally { Pop-Location }
