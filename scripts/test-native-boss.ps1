param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){ & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_boss_check.exe'
$result=& $binary (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $result.result -ne 'pass'){throw 'Native Windows boss validation failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/enemy-boss-runtime-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','assertions','maps','boss_types','controller_frames','projectiles','script_queues','death_cycles','events','reentry_guards','effect_failures','native_map_frames','native_boss_hud_frames')){
    if($result.$field -ne $reference.$field){throw "MSVC/GCC boss integration differs: $field"}
}
$report=[ordered]@{result='pass';native=$result;scope=$reference.scope
    checker_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale portable boss report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('CMakeLists.txt','scripts/test-native-boss.ps1')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-native-boss-validation.json'),($report | ConvertTo-Json -Depth 5)+[Environment]::NewLine)
Write-Output 'Passed both native boss controllers, five-shot cycles, real script queue bindings, health/death/release and callback failure recovery on all 16 map layouts; GCC/MSVC agree.'
