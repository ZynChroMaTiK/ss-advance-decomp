param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$binary = Join-Path $projectRoot 'build\native-windows\Release\ssa_native_game.exe'
$rom = Join-Path $projectRoot 'orig\SSA.gba'
$buildRoot = Join-Path $projectRoot 'build\render-resolution-tests'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$cases = @(
    @{ name='auto'; args=@('--smoke-resize'); width=0; height=0 },
    @{ name='vga'; args=@('--resolution','640x480'); width=640; height=480 },
    @{ name='full-hd'; args=@('--resolution','1920x1080'); width=1920; height=1080 },
    @{ name='4k'; args=@('--resolution','3840x2160'); width=3840; height=2160 },
    @{ name='wide'; args=@('--resolution','8193x17'); width=8193; height=17 },
    @{ name='thin-wide'; args=@('--resolution','65537x1'); width=65537; height=1 },
    @{ name='thin-tall'; args=@('--resolution','1x8193'); width=1; height=8193 },
    @{ name='original'; args=@('--original-renderer'); width=240; height=160 }
)
$results = @()
foreach ($case in $cases) {
    $outPath = Join-Path $buildRoot ($case.name + '.json')
    $snapshot = Join-Path $buildRoot ($case.name + '.bmp')
    $process = Start-Process -FilePath $binary -WorkingDirectory $projectRoot -WindowStyle Hidden -Wait -PassThru `
        -ArgumentList (@(('"' + $rom + '"'),'--smoke','--smoke-frames','4','--snapshot',('"' + $snapshot + '"')) + $case.args) `
        -RedirectStandardOutput $outPath -RedirectStandardError (Join-Path $buildRoot ($case.name + '.log'))
    if ($process.ExitCode -ne 0) { throw ('Render failed: ' + $case.name) }
    $item = Get-Content -LiteralPath $outPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($item.result -ne 'pass') { throw 'Invalid smoke output.' }
    if ($case.width) {
        if ($item.render_width -ne $case.width -or $item.render_height -ne $case.height) { throw 'Resolution was not applied.' }
    } elseif ($item.render_width -eq $item.initial_width -or $item.render_height -eq $item.initial_height) {
        throw 'Auto resolution did not follow the window resize.'
    }
    if ($case.name -ne 'original' -and $item.world_rays -ne $item.render_width) { throw 'World was not rendered per output column.' }
    $bitmap = [System.IO.File]::ReadAllBytes($snapshot)
    if ([BitConverter]::ToInt32($bitmap,18) -ne $item.render_width -or
        -[BitConverter]::ToInt32($bitmap,22) -ne $item.render_height -or
        $bitmap.Length -ne 54 + 4 * $item.render_width * $item.render_height) { throw 'Snapshot dimensions are incorrect.' }
    $results += $item
}
$rejects = @('0x1080','1920x0','nonsense','999999999999x1')
foreach ($size in $rejects) {
    $process = Start-Process -FilePath $binary -WorkingDirectory $projectRoot -WindowStyle Hidden -Wait -PassThru `
        -ArgumentList @(('"' + $rom + '"'),'--resolution',$size,'--smoke','--smoke-frames','1') `
        -RedirectStandardOutput (Join-Path $buildRoot 'reject-out.log') -RedirectStandardError (Join-Path $buildRoot 'reject-err.log')
    if ($process.ExitCode -ne 2) { throw ('Invalid resolution accepted: ' + $size) }
}
$report = [ordered]@{
    result='pass'; cases=$results; rejected_sizes=$rejects; source_sha256=[ordered]@{}
    executable_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash.ToLowerInvariant()
}
foreach ($source in @('src/pc_renderer.c','include/ssa/pc_renderer.h','platform/win32/game.c')) {
    $report.source_sha256[$source]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
}
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis\windows-render-resolution-validation.json'),
    ($report | ConvertTo-Json -Depth 6) + [Environment]::NewLine)
Write-Output 'Passed eight render sizes/modes, live auto resize, GDI presentation, snapshots and four rejected resolution strings.'
