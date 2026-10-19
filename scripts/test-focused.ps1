param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1') -StaticRuntime}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if(-not (Test-Path -LiteralPath $vswhere)){throw 'Visual Studio Build Tools are required.'}
$vsRoot=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$ctest=Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
if(-not (Test-Path -LiteralPath $ctest)){throw 'Install the Visual Studio CMake tools.'}
& $ctest --test-dir (Join-Path $root 'build/native-windows') -C Release -L focused --output-on-failure -j 2 --output-junit (Join-Path $root 'build/native-windows/focused-tests.xml')
if($LASTEXITCODE -ne 0){throw 'Focused native regression checks failed.'}
