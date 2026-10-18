param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_statistics_check.exe'
$native=& $checker (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows native statistics validation failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/native-statistics-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','starts','maps','difficulties','frames','snapshots','guards')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC native statistics differs: $field"}
}
$report=[ordered]@{result='pass';native=$native;scope=$reference.scope
    checker_sha256=(Get-FileHash -LiteralPath $checker -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
$inputs=@('tests/test_native_statistics.c','scripts/test-statistics.ps1','CMakeLists.txt','src/native_audio.inc')
$inputs+=@(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'src') -Filter '*.c' | ForEach-Object {'src/'+$_.Name})
$inputs+=@(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'include/ssa') -Filter '*.h' | ForEach-Object {'include/ssa/'+$_.Name})
foreach($inputPath in $inputs){$report.source_sha256[$inputPath]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $inputPath) -Algorithm SHA256).Hash.ToLowerInvariant()}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-native-statistics-validation.json'),($report | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
Write-Output "Passed Windows native statistics: $($native.checks) checks, $($native.frames) frames, all $($native.maps) maps and $($native.difficulties) difficulties."
