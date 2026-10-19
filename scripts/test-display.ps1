param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') -StaticRuntime }
$checks = [ordered]@{}
foreach ($name in @('display', 'pc_renderer')) {
    $exe = Join-Path $root "build/native-windows/Release/ssa_native_${name}_check.exe"
    $result = & $exe (Join-Path $root 'orig/SSA.gba')
    if ($LASTEXITCODE -ne 0) { throw "Windows $name check failed." }
    $checks[$name] = ($result | ConvertFrom-Json)
}
$report = [ordered]@{
    result = 'pass'
    scope = 'MSVC native display and full-resolution HUD integration. Synthetic mixed-priority OAM, original assets, seven output sizes and 24 live chase/jump frames; no campaign playthrough.'
    checks = $checks
    source_sha256 = [ordered]@{}
}
foreach ($path in @('src/display.c','include/ssa/display.h','tests/test_display.c',
    'src/pc_renderer.c','src/pc_renderer_pitch.inc','tests/test_pc_renderer.c',
    'scripts/test-display.ps1','CMakeLists.txt')) {
    $report.source_sha256[$path] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root $path)).Hash.ToLowerInvariant()
}
$report.rom_sha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath (Join-Path $root 'orig/SSA.gba')).Hash.ToLowerInvariant()
$report.executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root 'build/native-windows/Release/ssa_native_game.exe')).Hash.ToLowerInvariant()
$json = $report | ConvertTo-Json -Depth 6
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-display-validation.json'), $json + [Environment]::NewLine)
Write-Output $json
