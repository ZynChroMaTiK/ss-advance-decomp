param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$testRoot=Join-Path $projectRoot 'build/netricsa-tests'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_netricsa_check.exe'
$nativePath=Join-Path $testRoot 'native.json'
$p=Start-Process -FilePath $checker -ArgumentList @((Join-Path $projectRoot 'orig/SSA.gba'),(Join-Path $testRoot 'screen.bmp')) -WorkingDirectory $projectRoot -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $nativePath -RedirectStandardError (Join-Path $testRoot 'native.log')
if($p.ExitCode -ne 0){throw 'MSVC NETRICSA screen check failed.'}
$native=Get-Content -LiteralPath $nativePath -Raw | ConvertFrom-Json
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/netricsa-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','runs','frames','languages','levels')){if($native.$field -ne $reference.$field){throw "MSVC/GCC NETRICSA differs: $field"}}
$oldPause=$env:SSA_LAUNCH_NO_PAUSE;$env:SSA_LAUNCH_NO_PAUSE='1'
$oldCapture=$env:SSA_NETRICSA_CAPTURE_DIR;$env:SSA_NETRICSA_CAPTURE_DIR=$testRoot
$cases=@()
try{
    foreach($level in @(0,6)){
        $arguments=@('--startup-test','--netricsa-test','--level',"$level",'--mute')
        if($level -eq 6){$arguments+='--original-renderer'}
        else{$arguments+=@('--resolution','854x480')}
        $output=@(& (Join-Path $projectRoot 'play-native.cmd') @arguments)
        $output | Set-Content -LiteralPath (Join-Path $testRoot "startup-$level.json")
        if($LASTEXITCODE -ne 0){throw "NETRICSA Windows open/resume failed on map$level"}
        $item=$output -join [Environment]::NewLine | ConvertFrom-Json
        if($item.result -ne 'pass' -or $item.netricsa_ticks -ne 110 -or $item.pointer_checks -ne 7 -or $item.snapshot_checks -ne 1 -or $item.audio_errors -ne 0 -or $item.music_checks -ne 70 -or $item.resumed_game_counter -le $item.frozen_game_counter -or $item.hud_pixels_removed -le 0 -or $item.weapon_pieces -le 0){throw ('Incomplete NETRICSA frontend proof: '+($item|ConvertTo-Json -Compress))}
        if($item.briefings -ne 1 -or $item.briefing_ticks -ne 127 -or $item.overlay_checks -ne 1 -or
           $item.overlay_width -ne $(if($level -eq 6){240}else{854}) -or
           $item.overlay_height -ne $(if($level -eq 6){160}else{480})){throw 'NETRICSA entry/overlay dimensions differ'}
        $cases+=@{level=$level;original_renderer=($level -eq 6);startup=$item}
    }
}finally{
    if($null -eq $oldPause){Remove-Item Env:\SSA_LAUNCH_NO_PAUSE -ErrorAction SilentlyContinue}else{$env:SSA_LAUNCH_NO_PAUSE=$oldPause}
    if($null -eq $oldCapture){Remove-Item Env:\SSA_NETRICSA_CAPTURE_DIR -ErrorAction SilentlyContinue}else{$env:SSA_NETRICSA_CAPTURE_DIR=$oldCapture}
}
$report=[ordered]@{result='pass';native=$native;cases=$cases;physical_input_tested=$false
    scope='Native NETRICSA controls/snapshot/pause/music/resume; no campaign playthrough.'
    executable_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}
}
foreach($path in @('src/netricsa.c','include/ssa/netricsa.h','tests/test_netricsa.c','platform/win32/game.c','platform/win32/audio_win32.c','platform/win32/audio_win32.h','scripts/test-netricsa.ps1')){
    $report.source_sha256[$path]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $path) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-netricsa-validation.json'),($report|ConvertTo-Json -Depth 6)+[Environment]::NewLine)
$report|ConvertTo-Json -Depth 6
