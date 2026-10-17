param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$testRoot=Join-Path $projectRoot ('build/profile/run-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_profile_disk_check.exe'
$save=Join-Path $testRoot 'progress.dat'
Push-Location $projectRoot
try{
 $portable=& (Join-Path $projectRoot 'build/native-windows/Release/ssa_native_profile_check.exe') (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
 if($LASTEXITCODE -ne 0){throw 'MSVC profile/menu checks failed'}
 $reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/profile-validation.json') -Raw | ConvertFrom-Json
 foreach($field in @('result','checks','frames','level_starts','campaign_names','languages')){if($portable.$field -ne $reference.$field){throw "MSVC/GCC profile differs: $field"}}
 $process=Start-Process -FilePath $checker -ArgumentList @((Join-Path $projectRoot 'orig/SSA.gba'),$save) -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait `
  -RedirectStandardOutput (Join-Path $testRoot 'disk.jsonl') -RedirectStandardError (Join-Path $testRoot 'disk.log')
 if($process.ExitCode -ne 0){throw 'Disk profile/normal-input completion failed'}
 $disk=@(Get-Content -LiteralPath (Join-Path $testRoot 'disk.jsonl') | ForEach-Object {$_ | ConvertFrom-Json})
 $completionReference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/native-playability-validation.json') -Raw | ConvertFrom-Json
 if($disk.Count -ne 2 -or $disk[0].result -ne 'pass' -or $disk[1].saved_completed -ne 1 -or $disk[1].saved_language -ne 1){throw 'Missing real completion/save evidence'}
 foreach($field in @('frames','finished','score','health','collected','spawned','pistol_shots','shotgun_shots','live_enemies','open_groups')){
  if($disk[0].$field -ne $completionReference.$field){throw "Saved completion differs from portable run: $field"}
 }
 $oldPause=$env:SSA_LAUNCH_NO_PAUSE;$oldLog=$env:SSA_LAUNCH_LOG
 $env:SSA_LAUNCH_NO_PAUSE='1';$env:SSA_LAUNCH_LOG=Join-Path $testRoot 'launch.log';$cases=@()
 try{
  foreach($renderer in 0..1){
   $arguments=@('--startup-test','--level-select-test','1','--save-file',$save,'--mute')
   if($renderer){$arguments+='--original-renderer'}
   $output=@(& (Join-Path $projectRoot 'play-native.cmd') @arguments);$code=$LASTEXITCODE
   $output | Set-Content -LiteralPath (Join-Path $testRoot "select-$renderer.json")
   if($code -ne 0){throw "Saved level selection failed: renderer $renderer"}
   $item=$output -join [Environment]::NewLine | ConvertFrom-Json
   if($item.result -ne 'pass' -or $item.language -ne 1 -or $item.level -ne 1 -or $item.level_starter_checks -ne 1 -or $item.reveal_entry_checks -ne 2 -or $item.restarts -ne 1){throw 'Saved language/unlock/loadout/restart failed'}
   $cases+=@{original_renderer=[bool]$renderer;startup=$item}
  }
 }finally{$env:SSA_LAUNCH_NO_PAUSE=$oldPause;$env:SSA_LAUNCH_LOG=$oldLog}
 $report=[ordered]@{result='pass';portable=$portable;disk=$disk;windows_cases=$cases
  scope='Isolated test files only: corruption/write failure recovery, concurrent writer merging/reset, real normal-input completion saved by the frontend, relaunch with saved language, unlocked level selection, original starter weapons and restart in both renderers.'
  physical_input_tested=$false;test_directory=$testRoot;source_sha256=[ordered]@{}
  executable_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
  checker_sha256=(Get-FileHash -LiteralPath $checker -Algorithm SHA256).Hash.ToLowerInvariant()}
 foreach($source in @('src/profile.c','include/ssa/profile.h','src/menu.c','include/ssa/menu.h','platform/win32/game.c','platform/win32/profile_win32.c','platform/win32/profile_win32.h','tests/test_profile.c','tests/test_profile.py','tests/test_profile_win32.c','tests/test_native_playability.c','scripts/test-profile.ps1','CMakeLists.txt')){
  $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
 }
 [IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-profile-validation.json'),($report | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
 Write-Output 'Passed saved progress/language, Clear Data, actual completion unlock and level-select launch/restart in both renderers.'
}finally{Pop-Location}
