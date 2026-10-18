param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_weapon_fixes_check.exe'
$native=& $binary (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows weapon audio/projectile regression failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/weapon-fixes-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','weapon_loop_cases','rocket_trajectory_cases','rocket_frames','prion_trajectory_cases','prion_frames','cannon_trajectory_cases','cannon_frames','cannon_floor_cases','ceiling_cases')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC weapon regression differs: $field"}
}
$report=[ordered]@{result='pass';native=$native;scope=$reference.scope
    checker_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale portable weapon report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('CMakeLists.txt','scripts/test-native-weapon-fixes.ps1')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-weapon-fixes-validation.json'),($report | ConvertTo-Json -Depth 5)+[Environment]::NewLine)
Write-Output 'Passed 46 continuous-weapon sound cases, six rocket, six Prion and six floating cannon trajectories, plus 12 cannon floor cases and 12 ceiling contacts; Windows/GCC agree.'
