param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $root 'scripts/build-windows.ps1') -StaticRuntime}
$directory=Join-Path $root 'build/surface-decals'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$native=& (Join-Path $root 'build/native-windows/Release/ssa_native_surface_decals_check.exe') (Join-Path $root 'orig/SSA.gba') $directory | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows surface decal check failed.'}
$reference=Get-Content (Join-Path $root 'analysis/surface-decals-validation.json') -Raw | ConvertFrom-Json
$settingsReference=Get-Content (Join-Path $root 'analysis/windows-audio-settings-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','draws','software_changed_pixels','renderers','pitch_views')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC surface decals differ: $field"}
}
$report=[ordered]@{result='pass';native=$native;default='off';settings_version=19;settings_bytes=16;physical_input_tested=$false;source_sha256=[ordered]@{}
    executable_sha256=(Get-FileHash (Join-Path $root 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()}
foreach($source in @('src/surface_decals.c','include/ssa/surface_decals.h','src/water.c','src/water_edges.inc','src/surface_edge_tiles.inc','scripts/generate-surface-edge-tiles.py','src/water_foam.inc','src/water_ripples.inc','include/ssa/water.h','src/pc_renderer.c','src/pc_scene.c','include/ssa/pc_scene.h','include/ssa/pc_renderer.h','src/pc_renderer_pitch.inc','src/native_game.c','include/ssa/native_game.h','src/menu.c','src/settings.c','include/ssa/settings.h','platform/win32/game.c','tests/test_surface_decals.c')){
    $hash=(Get-FileHash (Join-Path $root $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    # Menu presentation is validated by the independent Settings/UI suite;
    # renderer sources use the rendering fixture's sanitizer baseline.
    $sourceReference=if($source -eq 'src/menu.c'){$settingsReference}else{$reference}
    if($sourceReference.source_sha256.$source -and $hash -ne $sourceReference.source_sha256.$source){throw "Stale decal source: $source"}
    $report.source_sha256[$source]=$hash
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-surface-decals-validation.json'),($report|ConvertTo-Json -Depth 6)+[Environment]::NewLine)
Write-Output "Passed $($native.checks) surface decal checks in both renderers and three pitch views; MSVC/GCC agree."
