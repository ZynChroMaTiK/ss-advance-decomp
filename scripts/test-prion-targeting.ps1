param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_prion_targeting_check.exe'
$native=& $checker (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows Prion targeting validation failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/prion-targeting-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','converted_types','frames','target_selections','melee_hits','ranged_shots','unconverted_cases','no_target_cases','boss_targets','damage_cases','range_holds','chase_cases','health_cases')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC Prion targeting differs: $field"}
}
$report=[ordered]@{result='pass';native=$native;scope=$reference.scope
    checker_sha256=(Get-FileHash -LiteralPath $checker -Algorithm SHA256).Hash.ToLowerInvariant()
    executable_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($path in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $path) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$path){throw "Stale portable Prion report: $path"}
    $report.source_sha256[$path]=$hash
}
foreach($path in @('scripts/test-prion-targeting.ps1','CMakeLists.txt')){
    $report.source_sha256[$path]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $path) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-prion-targeting-validation.json'),($report | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
Write-Output "Passed Windows Prion targeting: $($native.checks) checks, all ten convertible enemy types, melee/ranged attacks and nearest-player/enemy/boss selection."
