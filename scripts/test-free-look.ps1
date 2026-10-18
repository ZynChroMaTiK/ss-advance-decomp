param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$binary = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$rom = Join-Path $projectRoot 'orig/SSA.gba'
$outputRoot = Join-Path $projectRoot 'build/native-game-smoke'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$modes = @(
    @{ name = 'look-up'; frames = 30; size = '640x480'; pitch = 5440 },
    @{ name = 'look-down-chase'; frames = 120; size = '1280x720'; pitch = -5440 },
    @{ name = 'look-up-4k'; frames = 11; size = '3840x2160'; pitch = 5440 },
    @{ name = 'look-original'; frames = 30; size = '640x480'; pitch = 0; original = $true },
    @{ name = 'look-original-chase'; frames = 120; size = '640x480'; pitch = 0; original = $true }
)
$results = @()
Push-Location $projectRoot
try {
    foreach ($mode in $modes) {
        $shot = Join-Path $outputRoot ($mode.name + '.bmp')
        $arguments = @($rom, '--smoke', '--smoke-look', '--smoke-frames', $mode.frames,
            '--resolution', $mode.size, '--snapshot', $shot)
        if ($mode.original) { $arguments += '--original-renderer' }
        $item = & $binary @arguments | ConvertFrom-Json
        if ($LASTEXITCODE -ne 0 -or $item.result -ne 'pass' -or $item.frames -ne $mode.frames -or
            $item.pitch -ne $mode.pitch -or -not $item.hidden_window) { throw ('Free look failed: ' + $mode.name) }
        $data = [System.IO.File]::ReadAllBytes($shot)
        if ([System.BitConverter]::ToInt32($data,18) -ne $item.render_width -or
            -[System.BitConverter]::ToInt32($data,22) -ne $item.render_height) { throw 'Snapshot dimensions differ.' }
        $results += @{ name = $mode.name; frontend = $item; snapshot = $shot }
    }
    $gba = & (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_gba_aim_check.exe') $rom | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or $gba.result -ne 'pass') { throw 'GBA cursor projection failed.' }
    $chase = & (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_chase_aim_check.exe') $rom | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or $chase.result -ne 'pass') { throw 'Chase aim/death view failed.' }
    $report = [ordered]@{ result = 'pass'; modes = $results; gba_aim = $gba; chase_aim = $chase; physical_devices_tested = $false
        executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash.ToLowerInvariant()
        source_sha256 = [ordered]@{} }
    foreach ($source in @('platform/win32/game.c','src/native_game.c','src/pc_input.c','src/pc_input_runtime.c',
        'src/player_runtime.c','src/pc_renderer.c','src/pc_renderer_pitch.inc','src/weapon_hit.c',
        'src/weapon_controller.c','src/weapon_runtime.c','src/view.c','include/ssa/view.h','include/ssa/native_game.h','tests/test_gba_aim.c','tests/test_chase_aim.c')) {
        $report.source_sha256[$source] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
    }
    [System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-free-look-validation.json'),
        ($report | ConvertTo-Json -Depth 8) + [Environment]::NewLine)
    Write-Output 'Passed Windows up/down chase, 4K perspective and original-renderer checks.'
} finally { Pop-Location }
