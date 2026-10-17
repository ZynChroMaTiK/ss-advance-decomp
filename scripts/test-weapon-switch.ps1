param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_weapon_switch_check.exe'
$native=& $binary (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows completed-fire weapon switching failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/weapon-switch-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','switch_cases','waiting_frames','weapons','views')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC weapon switching differs: $field"}
}
$report=[ordered]@{result='pass';native=$native;scope=$reference.scope
    executable_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    checker_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale portable weapon-switch report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('CMakeLists.txt','scripts/test-weapon-switch.ps1')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-weapon-switch-validation.json'),($report | ConvertTo-Json -Depth 5)+[Environment]::NewLine)
Write-Output ('Passed completed-fire switching: ' + $native.checks + ' checks, all ten weapons in both views; Windows/GCC agree.')
