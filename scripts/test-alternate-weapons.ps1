param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$directory=Join-Path $projectRoot 'build/alternate-weapons'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$binaryRoot=Join-Path $projectRoot 'build/native-windows/Release'
$native=& (Join-Path $binaryRoot 'ssa_native_alternate_weapons_check.exe') (Join-Path $projectRoot 'orig/SSA.gba') $directory | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows alternate weapons check failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/alternate-weapons-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','frames','pickups','alternating_shots','left_hand_transitions','loadout_cases','reloads','reload_sounds','reequips','paired_clicks','shotgun_cases','pellets_checked','shotgun_timing_cases','shotgun_interval_ms','colt_shot_ms','colt_reload_ms','dual_reload_ms')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC alternate weapons differ: $field"}
}
$wheel=& (Join-Path $binaryRoot 'ssa_native_weapon_wheel_check.exe') (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $wheel.result -ne 'pass'){throw 'Windows number-key/wheel check failed.'}
$switch=& (Join-Path $binaryRoot 'ssa_native_weapon_switch_check.exe') (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $switch.result -ne 'pass'){throw 'Original weapon switch regression failed.'}
$report=[ordered]@{result='pass';native=$native;windows_input=$wheel;original_switch=$switch
    physical_input_tested=$false;executable_sha256=(Get-FileHash -LiteralPath (Join-Path $binaryRoot 'ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant();source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale alternate weapons report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('platform/win32/game.c','tests/test_weapon_wheel_win32.c','CMakeLists.txt','scripts/test-alternate-weapons.ps1')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-alternate-weapons-validation.json'),($report|ConvertTo-Json -Depth 5)+[Environment]::NewLine)
Write-Output "Passed $($native.checks) alternate weapons checks, actual Windows number-key/wheel routes and original weapon switching."
