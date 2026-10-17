param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_sector_clip_check.exe'
$native=& $binary (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows native sector clipping failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/sector-clip-runtime-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','maps','projectile_frames','list_cases')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC sector clipping differs: $field"}
}
$report=[ordered]@{result='pass';native=$native;scope=$reference.scope
    executable_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    checker_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale portable sector report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('CMakeLists.txt','platform/win32/game.c','scripts/test-sector-clip.ps1')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-sector-clip-validation.json'),($report | ConvertTo-Json -Depth 5)+[Environment]::NewLine)
Write-Output ('Passed native sector clipping: ' + $native.checks + ' checks on 16 maps; Windows/GCC agree.')
