param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_enemy_crowd_check.exe'
$native=& $binary (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows enemy crowd movement failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/enemy-crowd-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','scenarios','crowd_frames','overlap_separation_checks','maps','real_map_walks','native_frames','native_enemy_moves','player_escape_frames')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC crowd movement differs: $field"}
}
$report=[ordered]@{result='pass';native=$native
    executable_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    checker_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant();source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale crowd movement report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('CMakeLists.txt','scripts/test-enemy-crowd.ps1')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-enemy-crowd-validation.json'),($report | ConvertTo-Json -Depth 5)+[Environment]::NewLine)
Write-Output 'Passed enemy crowd movement, separation, wall/player contacts and native AI integration; Windows/GCC agree.'
