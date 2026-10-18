param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$testRoot=Join-Path $projectRoot 'build/umt-audio'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_umt_audio_check.exe'
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$launch=@{FilePath=$checker;ArgumentList=@((Join-Path $projectRoot 'orig/SSA.gba'));WorkingDirectory=$projectRoot;WindowStyle='Hidden';Wait=$true;PassThru=$true
    RedirectStandardOutput=(Join-Path $testRoot 'native.json');RedirectStandardError=(Join-Path $testRoot 'native.log')}
$p=Start-Process @launch
if($p.ExitCode -ne 0){throw 'Windows U.M.T. audio regression failed.'}
$native=Get-Content -LiteralPath (Join-Path $testRoot 'native.json') -Raw | ConvertFrom-Json
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/umt-audio-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','runs','actual_pickups','decoder_errors')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC U.M.T. audio differs: $field"}
}
$oldPause=$env:SSA_LAUNCH_NO_PAUSE
$env:SSA_LAUNCH_NO_PAUSE='1'
try{
    $startup=@(& (Join-Path $projectRoot 'play-native.cmd') --startup-test --audio-test --mute)
    if($LASTEXITCODE -ne 0){throw 'Rebuilt audio frontend launch failed.'}
}finally{
    if($null -eq $oldPause){Remove-Item Env:\SSA_LAUNCH_NO_PAUSE -ErrorAction SilentlyContinue}
    else{$env:SSA_LAUNCH_NO_PAUSE=$oldPause}
}
$startup | Set-Content -LiteralPath (Join-Path $testRoot 'startup.json')
$item=$startup -join [Environment]::NewLine | ConvertFrom-Json
if($item.result -ne 'pass' -or $item.audio_errors -ne 0 -or $item.audio_device -ne 1){throw 'Audio frontend launch check incomplete.'}
$report=[ordered]@{result='pass';scope='U.M.T. pickup mixer lifetime and audio frontend startup/restart; no campaign playthrough.'
    native=$native;startup=$item;physical_listening_tested=$false
    executable_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}
}
foreach($path in @('src/audio.c','tests/test_umt_audio.c','scripts/test-umt-audio.ps1','platform/win32/game.c','platform/win32/audio_win32.c')){
    $report.source_sha256[$path]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $path) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-umt-audio-validation.json'),($report|ConvertTo-Json -Depth 5)+[Environment]::NewLine)
$report|ConvertTo-Json -Depth 5
