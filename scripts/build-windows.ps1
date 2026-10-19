param([switch]$TestInput, [switch]$StaticRuntime)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = Join-Path $projectRoot 'build\native-windows'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio discovery is unavailable.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'An installed MSVC x64 toolchain is required.' }
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'Visual Studio CMake is unavailable.' }
$configureLog = Join-Path $buildRoot 'configure.log'
$compileLog = Join-Path $buildRoot 'compile.log'
$runtime = if ($StaticRuntime) { 'MultiThreaded$<$<CONFIG:Debug>:Debug>' } else { 'MultiThreaded$<$<CONFIG:Debug>:Debug>DLL' }
& $cmake -S $projectRoot -B $buildRoot -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_MSVC_RUNTIME_LIBRARY=$runtime" *> $configureLog
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $configureLog -Tail 35; throw 'Windows native configuration failed.' }
& $cmake --build $buildRoot --config Release --parallel 4 *> $compileLog
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $compileLog -Tail 45; throw 'Windows native compilation failed.' }
$libraries = @(Get-ChildItem -LiteralPath (Join-Path $buildRoot 'Release') -Filter 'ssa_*.lib' | Sort-Object Name)
$report = [ordered]@{
    result = 'pass'
    version = ([IO.File]::ReadAllText((Join-Path $projectRoot 'VERSION'))).Trim()
    static_runtime = [bool]$StaticRuntime
    toolchain = 'MSVC x64, Visual Studio 2022 BuildTools, C11 Release'
    scope = 'Portable native libraries, diagnostic viewer, native campaign Windows frontend with original language/title/new-game/difficulty/music menus and pre-level map, audio mixer and validation tools. Original feature parity remains incomplete.'
    campaign_level_count = [int]([regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot 'include/ssa/native_game.h')), 'SSA_NATIVE_CAMPAIGN_LEVELS=(\d+)').Groups[1].Value)
    libraries = @($libraries | ForEach-Object { $_.Name })
    library_count = $libraries.Count
    viewer = 'build/native-windows/Release/ssa_native_viewer.exe'
    game = 'build/native-windows/Release/ssa_native_game.exe'
    playability_check = 'build/native-windows/Release/ssa_native_playability_check.exe'
    campaign_check = 'build/native-windows/Release/ssa_native_campaign_check.exe'
    boss_check = 'build/native-windows/Release/ssa_native_boss_check.exe'
    gunrilla_check = 'build/native-windows/Release/ssa_native_gunrilla_check.exe'
    second_level_check = 'build/native-windows/Release/ssa_native_second_level_check.exe'
    third_level_check = 'build/native-windows/Release/ssa_native_third_level_check.exe'
    audio_check = 'build/native-windows/Release/ssa_native_audio_check.exe'
    map_view_check = 'build/native-windows/Release/ssa_native_map_view_check.exe'
    menu_check = 'build/native-windows/Release/ssa_native_menu_check.exe'
    source_sha256 = [ordered]@{}
}
foreach ($source in @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'src') -Filter '*.c')) {
    $report.source_sha256['src/' + $source.Name] = (Get-FileHash -Algorithm SHA256 -LiteralPath $source.FullName).Hash.ToLowerInvariant()
}
$report.source_sha256['src/pc_renderer_pitch.inc'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'src/pc_renderer_pitch.inc')).Hash.ToLowerInvariant()
$report.source_sha256['src/native_audio.inc'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'src/native_audio.inc')).Hash.ToLowerInvariant()
$report.source_sha256['src/native_alternates.inc'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'src/native_alternates.inc')).Hash.ToLowerInvariant()
$report.source_sha256['src/native_laurel.inc'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'src/native_laurel.inc')).Hash.ToLowerInvariant()
$report.source_sha256['platform/win32/input_win32.c'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'platform/win32/input_win32.c')).Hash.ToLowerInvariant()
$report.source_sha256['platform/win32/viewer.c'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'platform/win32/viewer.c')).Hash.ToLowerInvariant()
$report.source_sha256['platform/win32/game.c'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'platform/win32/game.c')).Hash.ToLowerInvariant()
$report.source_sha256['platform/win32/audio_win32.c'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'platform/win32/audio_win32.c')).Hash.ToLowerInvariant()
$report.source_sha256['platform/win32/audio_win32.h'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'platform/win32/audio_win32.h')).Hash.ToLowerInvariant()
$report.source_sha256['platform/win32/profile_win32.c'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'platform/win32/profile_win32.c')).Hash.ToLowerInvariant()
$report.source_sha256['platform/win32/profile_win32.h'] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'platform/win32/profile_win32.h')).Hash.ToLowerInvariant()
foreach ($source in @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'platform/win32') -Filter '*.inc')) {
    $report.source_sha256['platform/win32/' + $source.Name] = (Get-FileHash -Algorithm SHA256 -LiteralPath $source.FullName).Hash.ToLowerInvariant()
}
$json = $report | ConvertTo-Json -Depth 5
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis\windows-native-build-validation.json'), $json + [Environment]::NewLine)
Write-Output ('Built ' + $libraries.Count + ' native Windows libraries, playable frontend, completion check and viewer in ' + (Join-Path $buildRoot 'Release'))
if ($TestInput) { & (Join-Path $PSScriptRoot 'test-win32-input.ps1') }
