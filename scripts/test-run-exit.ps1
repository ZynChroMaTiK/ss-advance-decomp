param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $root 'scripts/build-windows.ps1')}
$directory=Join-Path $root 'build/run-exit-tests'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$core=& (Join-Path $root 'build/native-windows/Release/ssa_native_run_exit_check.exe') (Join-Path $root 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $core.result -ne 'pass'){throw 'Native death/retry check failed.'}
$reference=Get-Content (Join-Path $root 'analysis/run-exit-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','native_frames','deaths','retries','gameovers','death_voices','gameover_music_changes','languages','difficulties')){
    if($core.$field -ne $reference.$field){throw "Death/retry MSVC/GCC differs: $field"}
}
$output=Join-Path $directory 'windows.json'
$process=Start-Process -FilePath (Join-Path $root 'build/native-windows/Release/ssa_native_run_exit_win32_check.exe') `
    -ArgumentList @('orig/SSA.gba','build/run-exit-tests') -WorkingDirectory $root -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $output -RedirectStandardError (Join-Path $directory 'windows.log')
$null=$process.Handle
if(-not $process.WaitForExit(180000)){$process.Kill();throw 'Windows death/retry/menu loop timed out.'}
if($process.ExitCode -ne 0){Get-Content (Join-Path $directory 'windows.log');throw 'Windows death/retry check failed.'}
$windows=Get-Content $output -Raw | ConvertFrom-Json
if($windows.result -ne 'pass' -or $windows.deaths -ne 24 -or $windows.retries -ne 12 -or $windows.main_returns -ne 12 -or
    $windows.black_presentations -ne 24 -or $windows.clock_paths -ne 4 -or $windows.hud_styles -ne 2){throw 'Automatic death/menu checks incomplete.'}
$report=[ordered]@{result='pass';core=$core;windows=$windows;physical_input_tested=$false;physical_listening_tested=$false;source_sha256=[ordered]@{}
    executable_sha256=(Get-FileHash -Algorithm SHA256 (Join-Path $root 'build/native-windows/Release/ssa_native_game.exe')).Hash.ToLowerInvariant()}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -Algorithm SHA256 (Join-Path $root $source)).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale death/retry report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($file in @('platform/win32/game.c','platform/win32/audio_win32.c','platform/win32/profile_win32.c',
    'tests/test_run_exit_win32.c','scripts/test-run-exit.ps1','CMakeLists.txt')){
    $report.source_sha256[$file]=(Get-FileHash -Algorithm SHA256 (Join-Path $root $file)).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-run-exit-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
Write-Output 'Passed original native death/life controller, 12 retries, 12 final Game Over/Main returns and four real Windows retry-to-Game-Over loops in both HUD styles and renderers.'
