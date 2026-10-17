param()
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = Join-Path $projectRoot 'build\win32-input'
$generatedRoot = Join-Path $buildRoot 'project'
New-Item -ItemType Directory -Path $generatedRoot -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio discovery tool is not installed.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'An installed MSVC x64 toolchain is required.' }
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'Visual Studio CMake is unavailable.' }
$cmakeRoot = $projectRoot.Replace('\','/')
$cmakeText = @"
cmake_minimum_required(VERSION 3.20)
project(ssa_win32_input_validation LANGUAGES C)
add_subdirectory("$cmakeRoot" native)
add_library(ssa_win32_input_backend STATIC "$cmakeRoot/platform/win32/input_win32.c")
target_include_directories(ssa_win32_input_backend PUBLIC "$cmakeRoot/platform/win32")
target_compile_features(ssa_win32_input_backend PUBLIC c_std_11)
target_compile_options(ssa_win32_input_backend PRIVATE /W4 /WX)
target_link_libraries(ssa_win32_input_backend PUBLIC ssa_pc_input_runtime user32)
add_executable(ssa_win32_input_test "$cmakeRoot/tests/test_win32_input.c")
target_compile_options(ssa_win32_input_test PRIVATE /W4 /WX)
target_link_libraries(ssa_win32_input_test PRIVATE ssa_win32_input_backend)
"@
[System.IO.File]::WriteAllText((Join-Path $generatedRoot 'CMakeLists.txt'),$cmakeText)
$configureLog = Join-Path $buildRoot 'configure.log'
$compileLog = Join-Path $buildRoot 'compile.log'
& $cmake -S $generatedRoot -B $buildRoot -G 'Visual Studio 17 2022' -A x64 *> $configureLog
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $configureLog -Tail 35; throw 'Win32 input configuration failed.' }
& $cmake --build $buildRoot --config Release --target ssa_win32_input_test --parallel 4 *> $compileLog
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $compileLog -Tail 55; throw 'Win32 input compilation failed.' }
$binary = Join-Path $buildRoot 'Release\ssa_win32_input_test.exe'
$result = & $binary
if ($LASTEXITCODE -ne 0) { throw 'Win32 input hidden-window test failed.' }
$report = $result | ConvertFrom-Json
$report | Add-Member -NotePropertyName result -NotePropertyValue 'pass'
$report | Add-Member -NotePropertyName toolchain -NotePropertyValue 'MSVC x64, Visual Studio 2022 BuildTools, C11 Release; backend/tests /W4 /WX'
$report | Add-Member -NotePropertyName scope -NotePropertyValue 'Real Win32 message-only windows, keyboard/focus messages and raw-mouse payload parsing through the actual PC runtime event adapters. Registration takeover/restoration, unchanged capture/clipping, malformed input and wrong-thread guards. No physical device, successful WM_INPUT handle, game frame, renderer or ROM execution is claimed.'
$hashes = [ordered]@{}
foreach ($relative in @('platform/win32/input_win32.c','platform/win32/input_win32.h','tests/test_win32_input.c','src/pc_input.c','src/pc_input_runtime.c','src/input_dispatch.c','src/player_runtime.c','src/view.c')) {
    $hashes[$relative] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $relative)).Hash.ToLowerInvariant()
}
$report | Add-Member -NotePropertyName source_sha256 -NotePropertyValue $hashes
$json = $report | ConvertTo-Json -Depth 5
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis\win32-input-validation.json'),$json + [Environment]::NewLine)
$json
