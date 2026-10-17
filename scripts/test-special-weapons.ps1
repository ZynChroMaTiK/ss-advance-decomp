param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_special_weapons_check.exe'
$native=& $binary (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows special weapon integration failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/special-weapons-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','maps','blast_cases','fall_cases','pool_full_cases','native_input_cases','requests','fall_particles','timed_cases','denied_cases','timed_frames','smooth_fall_frames','empty_animation_frames','prion_speed_cases','prion_speed_frames','frozen_render_cases')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC special weapons differ: $field"}
}
$report=[ordered]@{result='pass';native=$native
    checker_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale special weapon report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('CMakeLists.txt','scripts/test-special-weapons.ps1')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-special-weapons-validation.json'),($report | ConvertTo-Json -Depth 5)+[Environment]::NewLine)
Write-Output 'Passed Time Strike empty gestures, smooth falling/original impacts, weapons8/9 on16maps and Prion2x behavior on all10eligible types; Windows/GCC agree.'
