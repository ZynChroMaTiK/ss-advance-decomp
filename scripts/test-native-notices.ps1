param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_notices_check.exe'
Push-Location $projectRoot
try{$native=& $checker (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json}
finally{Pop-Location}
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows native notice validation failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/native-notices-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','starts','maps','languages','frames','actual_pickups','ready_notices','death_cases','render_pairs','projected_notices')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC native notices differ: $field"}
}
$report=[ordered]@{result='pass';native=$native;scope=$reference.scope
    checker_sha256=(Get-FileHash -LiteralPath $checker -Algorithm SHA256).Hash.ToLowerInvariant()
    executable_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($path in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $path) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$path){throw "Stale portable notice report: $path"}
    $report.source_sha256[$path]=$hash
}
foreach($path in @('scripts/test-native-notices.ps1','CMakeLists.txt','platform/win32/game.c')){
    $report.source_sha256[$path]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $path) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-native-notices-validation.json'),($report | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
Write-Output "Passed Windows original localized notices: $($native.checks) checks, $($native.actual_pickups) actual pickups, $($native.death_cases) deaths and both renderers."
