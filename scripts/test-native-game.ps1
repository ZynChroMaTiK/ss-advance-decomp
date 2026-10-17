param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$binary = Join-Path $projectRoot 'build\native-windows\Release\ssa_native_game.exe'
$checker = Join-Path $projectRoot 'build\native-windows\Release\ssa_native_playability_check.exe'
$rom = Join-Path $projectRoot 'orig\SSA.gba'
$buildRoot = Join-Path $projectRoot 'build\native-game-smoke'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
Push-Location $projectRoot
try {
    $completionPath = Join-Path $buildRoot 'completion.json'
    $completionProcess = Start-Process -FilePath $checker -WorkingDirectory $projectRoot -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput $completionPath -RedirectStandardError (Join-Path $buildRoot 'completion.log')
    if ($completionProcess.ExitCode -ne 0) { throw 'Normal-input level completion failed.' }
    $completion = Get-Content -LiteralPath $completionPath -Raw -Encoding UTF8
    $item = $completion | ConvertFrom-Json
    $reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/native-playability-validation.json') -Raw | ConvertFrom-Json
    foreach($source in $reference.source_sha256.PSObject.Properties.Name){
        if((Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $reference.source_sha256.$source){throw "Stale portable completion evidence: $source"}
    }
    foreach($field in @('result','finished','frames','score','health','collected','spawned','pistol_shots','shotgun_shots','live_enemies','open_groups')){
        if($item.$field -ne $reference.$field){throw "Windows completion diverged from independently tested native run: $field"}
    }
    $snapshot = Join-Path $buildRoot 'gameplay.bmp'
    $smoke = & $binary $rom --smoke --snapshot $snapshot | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or $smoke.result -ne 'pass' -or $smoke.frames -ne 600 -or
        -not $smoke.hidden_window) { throw 'Windows frontend smoke failed.' }
    & (Join-Path $PSScriptRoot 'test-native-startup.ps1') -SkipBuild
    $report = [ordered]@{
        result = 'pass'; completion = $item; frontend = $smoke
        normal_startup = (Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/windows-native-startup-validation.json') -Raw | ConvertFrom-Json).cases
        physical_devices_tested = $false
        executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash.ToLowerInvariant()
        completion_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $checker).Hash.ToLowerInvariant()
        source_sha256 = [ordered]@{}
    }
    foreach ($source in @('src/native_game.c','platform/win32/game.c','tests/test_native_playability.c')) {
        $report.source_sha256[$source] = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
    }
    foreach($source in $reference.source_sha256.PSObject.Properties.Name){$report.source_sha256[$source]=$reference.source_sha256.$source}
    $report.source_sha256['scripts/test-native-game.ps1']=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
    [System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis\windows-native-game-validation.json'),
        ($report | ConvertTo-Json -Depth 6) + [Environment]::NewLine)
    Write-Output ('Passed native level completion (' + $item.frames + ' frames) and Windows gameplay smoke.')
} finally { Pop-Location }
