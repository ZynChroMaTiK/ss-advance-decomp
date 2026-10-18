param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$menuChecker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_menu_check.exe'
$menu=& $menuChecker (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $menu.result -ne 'pass'){throw 'Windows typed-password/menu regression failed.'}
$menuReference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/menu-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','frames','new_games','languages','difficulties','music_modes')){
    if($menu.$field -ne $menuReference.$field){throw "MSVC/GCC typed-password menu differs: $field"}
}
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_password_check.exe'
$native=& $checker (Join-Path $projectRoot 'orig/SSA.gba') | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $native.result -ne 'pass'){throw 'Windows password loadout validation failed.'}
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/password-start-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','password_starts','frames','entry_restarts','level_starts','carry_starts','starter_restarts','advancement_starts','progress_captures','levels','difficulties')){
    if($native.$field -ne $reference.$field){throw "MSVC/GCC password start differs: $field"}
}
$testRoot=Join-Path $projectRoot 'build/password-start';New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$oldPause=$env:SSA_LAUNCH_NO_PAUSE;$oldLog=$env:SSA_LAUNCH_LOG
$env:SSA_LAUNCH_NO_PAUSE='1';$env:SSA_LAUNCH_LOG=Join-Path $testRoot 'launch.log';$cases=@()
try{
    $span=$reference.levels-1
    # Keep all three languages represented as the admitted map count grows.
    # Selection0 is language0 and the final selector is always language2.
    $middle=$span+1
    while($middle%3 -ne 1){++$middle}
    foreach($selection in @(0,$middle,(3*$span-1))){foreach($renderer in 0..1){
        $arguments=@('--startup-test','--password-test',"$selection",'--mute')
        if($renderer){$arguments+='--original-renderer'}else{$arguments+='--no-music'}
        $output=@(& (Join-Path $projectRoot 'play-native.cmd') @arguments);$code=$LASTEXITCODE
        $output | Set-Content -LiteralPath (Join-Path $testRoot ("case-$selection-$renderer.json"))
        if($code -ne 0){throw "Password launcher failed: $selection/$renderer"}
        $item=$output -join [Environment]::NewLine | ConvertFrom-Json
        $level=$selection%$span+1;$difficulty=[Math]::Floor($selection/$span)
        if($item.result -ne 'pass' -or $item.level -ne $level -or $item.difficulty -ne $difficulty -or
           $item.language -ne $selection%3 -or $item.password_loadout -ne ($difficulty*12+$level+1) -or
           $item.password_edges -lt 6 -or $item.menu_starts -ne 1 -or $item.restarts -ne 1 -or
           $item.reveal_entry_checks -ne 2 -or $item.map_views -ne 2 -or $item.map_starts -ne 2 -or
           $item.map_frozen_ticks -ne 140 -or $item.map_music_checks -ne 18 -or
           $item.level_music_checks -ne 2 -or $item.audio_device -ne 1 -or $item.audio_errors -ne 0){
            throw "Password menu/map/loadout/restart failed: $selection/$renderer"
        }
        $cases+=@{selection=$selection;original_renderer=[bool]$renderer;startup=$item}
        Write-Output "Passed password map $level, difficulty $difficulty, renderer $renderer and entry restart."
    }}
    foreach($level in @(0,3,($reference.levels-1))){foreach($renderer in 0..1){
        $arguments=@('--startup-test','--level',"$level",'--mute')
        if($renderer){$arguments+='--original-renderer'}
        $output=@(& (Join-Path $projectRoot 'play-native.cmd') @arguments);$code=$LASTEXITCODE
        $output | Set-Content -LiteralPath (Join-Path $testRoot ("direct-$level-$renderer.json"))
        if($code -ne 0){throw "Fresh level launcher failed: $level/$renderer"}
        $item=$output -join [Environment]::NewLine | ConvertFrom-Json
        if($item.result -ne 'pass' -or $item.level -ne $level -or $item.level_starter_checks -ne 1 -or
           $item.menu_starts -ne 0 -or $item.menu_frozen_ticks -ne 0 -or $item.restarts -ne 1 -or
           $item.map_views -ne 2 -or $item.reveal_entry_checks -ne 2){throw "Fresh level loadout/restart failed: $level/$renderer"}
        $cases+=@{direct_level=$level;original_renderer=[bool]$renderer;startup=$item}
        Write-Output "Passed fresh level $level, renderer $renderer and original-loadout restart."
    }}
}finally{$env:SSA_LAUNCH_NO_PAUSE=$oldPause;$env:SSA_LAUNCH_LOG=$oldLog}
$report=[ordered]@{result='pass';native=$native;menu=$menu;windows_cases=$cases;scope=$reference.scope
    physical_input_tested=$false;physical_listening_tested=$false
    executable_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    checker_sha256=(Get-FileHash -LiteralPath $checker -Algorithm SHA256).Hash.ToLowerInvariant()
    menu_checker_sha256=(Get-FileHash -LiteralPath $menuChecker -Algorithm SHA256).Hash.ToLowerInvariant();source_sha256=[ordered]@{}}
foreach($source in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$source){throw "Stale portable password start report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in $menuReference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($hash -ne $menuReference.source_sha256.$source){throw "Stale portable typed-password report: $source"}
    $report.source_sha256[$source]=$hash
}
foreach($source in @('CMakeLists.txt','scripts/test-password-start.ps1','platform/win32/game.c','platform/win32/audio_win32.c','platform/win32/audio_win32.h')){
    $report.source_sha256[$source]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-password-start-validation.json'),($report | ConvertTo-Json -Depth 8)+[Environment]::NewLine)
