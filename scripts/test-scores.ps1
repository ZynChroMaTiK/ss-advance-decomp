param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$directory=Join-Path $root 'build/scores-tests'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$core=& (Join-Path $root 'build/native-windows/Release/ssa_native_scores_check.exe') (Join-Path $root 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $core.result -ne 'pass'){throw 'Native Scores check failed.'}
$reference=Get-Content (Join-Path $root 'analysis/scores-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','typed_submissions','codec_bytes','languages')){
    if($core.$field -ne $reference.$field){throw "Scores MSVC/GCC differs: $field"}
}
$output=Join-Path $directory 'windows.json'
$process=Start-Process -FilePath (Join-Path $root 'build/native-windows/Release/ssa_native_scores_win32_check.exe') `
    -ArgumentList @('orig/SSA.gba','build/scores-tests') -WorkingDirectory $root -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput $output -RedirectStandardError (Join-Path $directory 'windows.log')
if($process.ExitCode -ne 0){Get-Content (Join-Path $directory 'windows.log');throw 'Windows Scores check failed.'}
$windows=Get-Content $output -Raw | ConvertFrom-Json
if($windows.result -ne 'pass' -or $windows.name_paths -ne 6 -or $windows.ending_paths -ne 6 -or
    $windows.quit_run_paths -ne 6 -or $windows.new_run_paths -ne 6){throw 'Scores route checks incomplete.'}
$report=[ordered]@{result='pass';core=$core;windows=$windows;physical_input_tested=$false;source_sha256=[ordered]@{}
    executable_sha256=(Get-FileHash -Algorithm SHA256 (Join-Path $root 'build/native-windows/Release/ssa_native_game.exe')).Hash.ToLowerInvariant()}
foreach($file in @('src/scores.c','include/ssa/scores.h','src/menu.c','include/ssa/menu.h','platform/win32/game.c',
    'platform/win32/profile_win32.c','platform/win32/profile_win32.h','tests/test_scores.c','tests/test_scores_win32.c','scripts/test-scores.ps1','CMakeLists.txt')){
    $report.source_sha256[$file]=(Get-FileHash -Algorithm SHA256 (Join-Path $root $file)).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-scores-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
Write-Output 'Passed Scores codec, original ranking, keyboard/mouse name entry, ending handoff, file merge/failure/recovery and Clear Data.'
