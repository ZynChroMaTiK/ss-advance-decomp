param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$binary = Join-Path $projectRoot 'build\native-windows\Release\ssa_native_viewer.exe'
$rom = Join-Path $projectRoot 'orig\SSA.gba'
$buildRoot = Join-Path $projectRoot 'build\native-viewer-smoke'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$buildReport = Get-Content -LiteralPath (Join-Path $projectRoot 'analysis\windows-native-build-validation.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$viewerHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'platform\win32\viewer.c')).Hash.ToLowerInvariant()
if ($buildReport.source_sha256.'platform/win32/viewer.c' -ne $viewerHash) { throw 'The viewer source changed after the recorded build.' }
foreach ($entry in $buildReport.source_sha256.PSObject.Properties) {
    $current = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $entry.Name)).Hash.ToLowerInvariant()
    if ($current -ne $entry.Value) { throw ('Source changed after the recorded build: ' + $entry.Name) }
}
$levels = @()
foreach ($level in 0..15) {
    $snapshot = Join-Path $buildRoot ('level-' + $level.ToString('00') + '.bmp')
    $chaseSnapshot = Join-Path $buildRoot ('chase-' + $level.ToString('00') + '.bmp')
    $result = & $binary $rom --level $level --smoke --snapshot $snapshot --snapshot-chase $chaseSnapshot
    if ($LASTEXITCODE -ne 0) { throw ('Native viewer smoke failed on level ' + $level) }
    $item = $result | ConvertFrom-Json
    if ($item.result -ne 'pass' -or $item.frames -ne 240 -or -not $item.hidden_window -or $item.input_injected -or
        $item.display_width -ne 240 -or $item.display_height -ne 160 -or $item.hud_draws -le 0 -or
        $item.idle_weapon_draws -le 0) { throw 'Unexpected viewer validation output.' }
    $levels += $item
}
if (($levels | Measure-Object -Property body_pixels -Sum).Sum -le 0 -or
    ($levels | Measure-Object -Property actor_changed_pixels -Sum).Sum -le 0 -or
    ($levels | Measure-Object -Property map_objects -Sum).Sum -ne 385) { throw 'Native actors/body were not exercised.' }
$invalidRom = Join-Path $buildRoot 'wrong-identity.gba'
[System.IO.File]::WriteAllBytes($invalidRom, (New-Object byte[] (8 * 1024 * 1024)))
$rejectCases = @(
    @{ name = 'missing-rom'; arguments = ''; expected = 2 },
    @{ name = 'invalid-level'; arguments = ('"' + $rom + '" --level 16 --smoke'); expected = 2 },
    @{ name = 'missing-file'; arguments = ('"' + (Join-Path $buildRoot 'nonexistent.gba') + '" --smoke'); expected = 1 },
    @{ name = 'wrong-sha1'; arguments = ('"' + $invalidRom + '" --smoke'); expected = 1 }
)
foreach ($case in $rejectCases) {
    $launch = @{
        FilePath = $binary
        WindowStyle = 'Hidden'
        Wait = $true
        PassThru = $true
        RedirectStandardOutput = (Join-Path $buildRoot ($case.name + '.stdout.log'))
        RedirectStandardError = (Join-Path $buildRoot ($case.name + '.stderr.log'))
    }
    if ($case.arguments) { $launch.ArgumentList = $case.arguments }
    $process = Start-Process @launch
    if ($process.ExitCode -ne $case.expected) { throw ('Viewer rejection failed: ' + $case.name + ' exit ' + $process.ExitCode) }
}
$report = [ordered]@{
    result = 'pass'
    scope = 'Real native MSVC x64 viewer; 16 hidden Win32 windows, native movement/collision/jump/H camera, original animated map objects and Sam body. Original1240C starting weapon,101F4 HUD, native cache/OAM and verified240x160 hardware sampling feed actual GDI. Weapon is idle-only, score/time remain initialized; no AI/firing/combat/pickup execution/audio or fullgame claim. Portable input fixtures do not validate physical devices or visible-window interaction.'
    maps = 16
    frames = ($levels | Measure-Object -Property frames -Sum).Sum
    body_frames = ($levels | Measure-Object -Property body_frames -Sum).Sum
    body_pixels = ($levels | Measure-Object -Property body_pixels -Sum).Sum
    actor_commands = ($levels | Measure-Object -Property actor_commands -Sum).Sum
    actor_changed_pixels = ($levels | Measure-Object -Property actor_changed_pixels -Sum).Sum
    hud_draws = ($levels | Measure-Object -Property hud_draws -Sum).Sum
    idle_weapon_draws = ($levels | Measure-Object -Property idle_weapon_draws -Sum).Sum
    presentation_pixels = ($levels | Measure-Object -Property presentation_pixels -Sum).Sum
    levels = $levels
    cli_rejections = $rejectCases.Count
    source_sha256 = $viewerHash
    binary_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash.ToLowerInvariant()
    rom_sha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath $rom).Hash.ToLowerInvariant()
    native_sources = $buildReport.source_sha256
}
$json = $report | ConvertTo-Json -Depth 7
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis\native-viewer-validation.json'), $json + [Environment]::NewLine)
Write-Output ('Native viewer passed ' + $report.frames + ' frames on all 16 maps; screenshots: ' + $buildRoot)
