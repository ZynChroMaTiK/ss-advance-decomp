param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$directory=Join-Path $root 'build/intermission-tests'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$checker=Join-Path $root 'build/native-windows/Release/ssa_native_intermission_check.exe'
$core=& $checker (Join-Path $root 'orig/SSA.gba') (Join-Path $directory 'screen') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $core.result -ne 'pass'){throw 'Windows intermission core failed.'}
$reference=Get-Content -LiteralPath (Join-Path $root 'analysis/intermission-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','completion_starts','tally_frames','languages','pause_paths')){
    if($core.$field -ne $reference.$field){throw "MSVC/GCC intermission differs: $field"}
}
$frontend=Join-Path $root 'build/native-windows/Release/ssa_native_intermission_win32_check.exe'
$windows=& $frontend (Join-Path $root 'orig/SSA.gba') $directory | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $windows.result -ne 'pass' -or $windows.completion_routes -ne 6 -or $windows.pause_paths -ne 12 -or $windows.main_menu_quits -ne 6){throw 'Windows screen routes failed.'}
$report=[ordered]@{result='pass';core=$core;windows=$windows;physical_input_tested=$false;physical_listening_tested=$false
    executable_sha256=(Get-FileHash -LiteralPath (Join-Path $root 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $root $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale intermission source: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('platform/win32/game.c','platform/win32/audio_win32.c','tests/test_intermission_win32.c','scripts/test-intermission.ps1','CMakeLists.txt')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $root $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-intermission-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
Write-Output "Passed $($core.checks) native checks, 12 Windows pause paths, six Main-menu Quit paths and six completion routes in both renderers."
