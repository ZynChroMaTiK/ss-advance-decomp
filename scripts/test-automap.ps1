param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $root 'scripts/build-windows.ps1')}
$directory=Join-Path $root 'build/automap-tests'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$core=& (Join-Path $root 'build/native-windows/Release/ssa_native_automap_check.exe') (Join-Path $root 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $core.result -ne 'pass'){throw 'Native automap check failed.'}
$reference=Get-Content (Join-Path $root 'analysis/automap-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','native_frames','draws','maps')){
    if($core.$field -ne $reference.$field){throw "Automap MSVC/GCC differs: $field"}
}
$output=Join-Path $directory 'windows.json'
$process=Start-Process -FilePath (Join-Path $root 'build/native-windows/Release/ssa_native_automap_win32_check.exe') `
    -ArgumentList @('orig/SSA.gba','build/automap-tests') -WorkingDirectory $root -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput $output -RedirectStandardError (Join-Path $directory 'windows.log')
if($process.ExitCode -ne 0){Get-Content (Join-Path $directory 'windows.log');throw 'Windows automap check failed.'}
$windows=Get-Content $output -Raw | ConvertFrom-Json
if($windows.result -ne 'pass' -or $windows.paths -ne 4){throw 'Automap route checks incomplete.'}
$report=[ordered]@{result='pass';core=$core;windows=$windows;physical_input_tested=$false;source_sha256=[ordered]@{}
    executable_sha256=(Get-FileHash -Algorithm SHA256 (Join-Path $root 'build/native-windows/Release/ssa_native_game.exe')).Hash.ToLowerInvariant()}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -Algorithm SHA256 (Join-Path $root $source)).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale automap report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($file in @('platform/win32/game.c','tests/test_automap_win32.c','scripts/test-automap.ps1','CMakeLists.txt')){
    $report.source_sha256[$file]=(Get-FileHash -Algorithm SHA256 (Join-Path $root $file)).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-automap-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
Write-Output 'Passed native automap, all-map draw purity, M/Extras gating/wheel/held zoom, both renderers, pause/resume, focus cleanup and death handling.'
