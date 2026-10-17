param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$testRoot = Join-Path $projectRoot 'build/native-audio'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$checker = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_audio_check.exe'
$binary = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$rom = Join-Path $projectRoot 'orig/SSA.gba'
$oldPause = $env:SSA_LAUNCH_NO_PAUSE
$oldLog = $env:SSA_LAUNCH_LOG
$env:SSA_LAUNCH_NO_PAUSE = '1'
$env:SSA_LAUNCH_LOG = Join-Path $testRoot 'audio-launch.log'
Push-Location $testRoot
try {
    $process = Start-Process -FilePath $checker -ArgumentList @($rom) -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait `
        -RedirectStandardOutput (Join-Path $testRoot 'catalog.json') -RedirectStandardError (Join-Path $testRoot 'catalog.log')
    if ($process.ExitCode -ne 0) { Get-Content (Join-Path $testRoot 'catalog.log'); throw 'MSVC audio decoder/mixer validation failed.' }
    $catalog = Get-Content (Join-Path $testRoot 'catalog.json') -Raw | ConvertFrom-Json
    if ($catalog.result -ne 'pass' -or $catalog.songs -ne 131 -or $catalog.audible_songs -ne 131 -or
        $catalog.pcm_bus_gain -ne 2 -or $catalog.effects_bus_gain -ne 1.5 -or $catalog.music_bus_gain -ne 0.5 -or $catalog.effects_scope_cases -ne 2310 -or
        $catalog.priority_cases -ne 7 -or $catalog.concurrency_cases -ne 6 -or $catalog.effect_instances -ne 32 -or $catalog.effect_voice_capacity -ne 64 -or
        $catalog.world_audio_cases -ne 6 -or
        $catalog.catalog_clipped_samples -ne 0 -or $catalog.decoder_errors -ne 0) { throw 'Audio catalogue differs from the sanitizer result.' }
    $cases = @()
    foreach ($music in @($true,$false)) {
        $arguments = @('--startup-test','--audio-test','--mute')
        if (-not $music) { $arguments += '--no-music' }
        $launchOutput = @(& (Join-Path $projectRoot 'play-native.cmd') @arguments)
        $launchExit = $LASTEXITCODE
        $caseName = if ($music) { 'device-music' } else { 'device-effects' }
        $launchOutput | Set-Content -LiteralPath (Join-Path $testRoot ($caseName + '.txt'))
        if ($launchExit -ne 0) { throw ('Windows audio startup failed with exit ' + $launchExit + ': ' + ($launchOutput -join ' ')) }
        $item = $launchOutput -join [Environment]::NewLine | ConvertFrom-Json
        if ($item.result -ne 'pass' -or $item.audio_device -ne 1 -or
            $item.audio_buffers -lt 6 -or $item.audio_recycled -lt 1 -or $item.audio_position -lt 1 -or
            $item.audio_notes -lt 1 -or $item.audio_errors -ne 0 -or $item.restarts -ne 1 -or
            $item.audio_worker_progress -ne 1 -or $item.audio_mute_toggle -ne 1 -or $item.audio_footsteps -ne 2 -or $item.audio_spatial -ne 2 -or $item.audio_music -ne 3 -or $item.audio_level_starts -ne 2 -or $item.audio_fade -ne 2 -or
            $item.map_views -ne 2 -or $item.map_starts -ne 2 -or $item.map_frozen_ticks -ne 140 -or $item.map_music_checks -ne 18 -or $item.level_music_checks -ne 2 -or
            $item.menu_starts -ne 1 -or $item.menu_frozen_ticks -ne 187) {
            throw ('Windows audio startup/output/restart validation failed: ' + ($item | ConvertTo-Json -Compress))
        }
        $cases += @{ music_enabled=$music; output_muted=$true; startup=$item }
    }
    $report = [ordered]@{ result='pass'; catalog=$catalog; cases=$cases
        physical_listening_tested=$false
        executable_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash.ToLowerInvariant()
        checker_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $checker).Hash.ToLowerInvariant()
        source_sha256=[ordered]@{} }
    foreach ($source in @('src/audio.c','include/ssa/audio.h','src/native_game.c','src/native_audio.inc','include/ssa/native_game.h','src/map_view.c','include/ssa/map_view.h',
        'src/menu.c','include/ssa/menu.h','platform/win32/game.c','platform/win32/audio_win32.c','platform/win32/audio_win32.h','tests/test_audio.c','tests/audio_concurrency.inc','tests/audio_world.inc','tests/audio_priority.inc','scripts/test-native-audio.ps1')) {
        $report.source_sha256[$source]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
    }
    [System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-native-audio-validation.json'),
        ($report | ConvertTo-Json -Depth 8) + [Environment]::NewLine)
    Write-Output 'Passed MSVC audio catalogue and actual Windows audio device buffering/restart, with music enabled/disabled and output muted.'
} finally { Pop-Location; $env:SSA_LAUNCH_NO_PAUSE = $oldPause; $env:SSA_LAUNCH_LOG = $oldLog }
