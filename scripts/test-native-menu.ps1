param([switch]$SkipBuild,[ValidateRange(0,8)][int[]]$Selections=(0..8))
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$testRoot=Join-Path $projectRoot 'build/native-menu'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_menu_check.exe'
$process=Start-Process -FilePath $checker -ArgumentList @((Join-Path $projectRoot 'orig/SSA.gba')) -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput (Join-Path $testRoot 'core.json') -RedirectStandardError (Join-Path $testRoot 'core.log')
if ($process.ExitCode -ne 0) { throw 'MSVC original menu validation failed.' }
$core=Get-Content -LiteralPath (Join-Path $testRoot 'core.json') -Raw | ConvertFrom-Json
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/menu-validation.json') -Raw | ConvertFrom-Json
foreach ($field in @('result','checks','frames','new_games','languages','difficulties','music_modes')) {
    if ($core.$field -ne $reference.$field) { throw ('MSVC/GCC menu differs: '+$field) }
}
$oldPause=$env:SSA_LAUNCH_NO_PAUSE; $oldLog=$env:SSA_LAUNCH_LOG
$env:SSA_LAUNCH_NO_PAUSE='1';$env:SSA_LAUNCH_LOG=Join-Path $testRoot 'launch.log'
$cases=@()
try {
    foreach ($selection in $Selections) {
        $arguments=@('--startup-test','--menu-test',"$selection",'--mute')
        if ($selection%2) { $arguments+='--original-renderer' }
        if ($selection%3 -eq 0) { $arguments+='--no-music' }
        $output=@(& (Join-Path $projectRoot 'play-native.cmd') @arguments);$launchExit=$LASTEXITCODE
        $output | Set-Content -LiteralPath (Join-Path $testRoot ("case-$selection.json"))
        if ($launchExit -ne 0) { throw ('Menu startup failed for selection '+$selection) }
        $item=$output -join [Environment]::NewLine | ConvertFrom-Json
        if ($item.result -ne 'pass' -or $item.menu_starts -ne 1 -or $item.menu_frozen_ticks -ne 292 -or $item.menu_credits_checks -ne 72 -or
            $item.language -ne [Math]::Floor($selection/3) -or $item.difficulty -ne $selection%3 -or
            $item.menu_music_toggles -ne 0 -or $item.menu_mouse_checks -ne 2 -or
            $item.menu_music_continuity -ne $(if($selection%3 -eq 0){0}else{2}) -or
            $item.restarts -ne 1 -or $item.audio_device -ne 1 -or $item.audio_errors -ne 0 -or
            $item.map_views -ne 2 -or $item.map_starts -ne 2 -or $item.map_frozen_ticks -ne 140 -or
            $item.map_music_checks -ne 18 -or $item.level_music_checks -ne 2) { throw ('Menu selection/state/music did not persist: '+$selection) }
        $cases+=@{ selection=$selection; output_muted=$true; original_renderer=($selection%2 -eq 1); music_enabled=($selection%3 -ne 0); startup=$item }
        Write-Output ('Passed menu language '+$item.language+', difficulty '+$item.difficulty+', credits, options, map, level and restart.')
    }
} finally { $env:SSA_LAUNCH_NO_PAUSE=$oldPause; $env:SSA_LAUNCH_LOG=$oldLog }
$report=[ordered]@{ result='pass'; core=$core; windows_cases=$cases; physical_input_tested=$false; physical_listening_tested=$false
    executable_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    checker_sha256=(Get-FileHash -LiteralPath $checker -Algorithm SHA256).Hash.ToLowerInvariant(); source_sha256=[ordered]@{} }
foreach ($source in @('src/menu.c','include/ssa/menu.h','src/map_view.c','include/ssa/map_view.h','tests/test_menu.c',
    'platform/win32/game.c','platform/win32/audio_win32.c','platform/win32/audio_win32.h','scripts/test-native-menu.ps1','CMakeLists.txt')) {
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-native-menu-validation.json'),($report | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
Write-Output ('Passed '+$cases.Count+' language/difficulty combinations, credits controls, Options and level-entry restart.')
