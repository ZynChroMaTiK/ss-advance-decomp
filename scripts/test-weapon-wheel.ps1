param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$binary = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_weapon_wheel_check.exe'
$game = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$rom = Join-Path $projectRoot 'orig/SSA.gba'
$item = & $binary $rom | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or $item.result -ne 'pass' -or -not $item.real_window_messages) {
    throw 'Windows weapon wheel validation failed.'
}
$report = [ordered]@{
    result = 'pass'
    validation = $item
    scope = 'Hidden Win32 window using the actual frontend wheel/Q message handler and native weapon switching. No physical device delivery tested.'
    rom_sha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath $rom).Hash.ToLowerInvariant()
    executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $game).Hash.ToLowerInvariant()
    checker_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash.ToLowerInvariant()
    source_sha256 = [ordered]@{}
}
foreach ($source in @('src/native_game.c','include/ssa/native_game.h','platform/win32/game.c',
    'tests/test_weapon_wheel_win32.c','CMakeLists.txt','scripts/test-weapon-wheel.ps1')) {
    $report.source_sha256[$source] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
}
if ($report.rom_sha1 -ne '4ff48d8c18b70a5184758f1917343ae85a70858e') { throw 'Original ROM hash differs.' }
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-weapon-wheel-validation.json'),
    ($report | ConvertTo-Json -Depth 5) + [Environment]::NewLine)
Write-Output ('Passed Windows weapon wheel validation: ' + $item.checks + ' checks.')
