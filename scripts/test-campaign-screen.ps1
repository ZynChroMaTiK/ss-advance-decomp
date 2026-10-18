param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1')}
$testRoot=Join-Path $projectRoot 'build/campaign-screen'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$checker=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_campaign_screen_check.exe'
$binary=Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$checkArgs=@{FilePath=$checker;ArgumentList=@((Join-Path $projectRoot 'orig/SSA.gba'),(Join-Path $testRoot 'story'));WorkingDirectory=$projectRoot;WindowStyle='Hidden';Wait=$true;PassThru=$true
    RedirectStandardOutput=(Join-Path $testRoot 'native.json');RedirectStandardError=(Join-Path $testRoot 'native.log')}
$p=Start-Process @checkArgs
if($p.ExitCode -ne 0){throw 'Native Windows story-screen check failed.'}
$native=Get-Content -LiteralPath (Join-Path $testRoot 'native.json') -Raw | ConvertFrom-Json
$reference=Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/campaign-screen-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('result','checks','screens','frames','commands','fades')){
    if($native.$field -ne $reference.native.$field){throw "MSVC/GCC story screen differs: $field"}
}
foreach($level in @(0,6,12)){
    $actualImage=(Get-FileHash -LiteralPath (Join-Path $testRoot "story-$level.bmp") -Algorithm SHA256).Hash
    $referenceImage=(Get-FileHash -LiteralPath (Join-Path $projectRoot "build/native-tests/story-$level.bmp") -Algorithm SHA256).Hash
    if($actualImage -ne $referenceImage){throw "MSVC/GCC story image differs: $level"}
}
$cases=@()
foreach($level in @(0,6,12)){
    $arguments=@((Join-Path $projectRoot 'orig/SSA.gba'),'--story',"$level",'--startup-test','--mute')
    if($level -eq 6){$arguments+=@('--no-music','--original-renderer')}
    $launchArgs=@{FilePath=$binary;ArgumentList=$arguments;WorkingDirectory=$projectRoot;WindowStyle='Hidden';Wait=$true;PassThru=$true
        RedirectStandardOutput=(Join-Path $testRoot "story-$level.json");RedirectStandardError=(Join-Path $testRoot "story-$level.log")}
    $p=Start-Process @launchArgs
    if($p.ExitCode -ne 0){throw "Windows story frontend failed: $level"}
    $item=Get-Content -LiteralPath (Join-Path $testRoot "story-$level.json") -Raw | ConvertFrom-Json
    if($item.result -ne 'pass' -or $item.story -ne $level -or $item.story_ticks -lt 200 -or
       $item.frozen_game_counter -ne 0 -or $item.music_checks -lt 100 -or $item.audio_device -ne 1 -or
       $item.audio_errors -ne 0 -or $item.map_active -ne 0 -or $item.briefing_active -ne [int]($level -ne 12) -or
       $item.menu_active -ne [int]($level -eq 12)){throw "Incomplete Windows story frontend: $level"}
    $cases+=$item
}
$report=[ordered]@{
    result='pass';scope='Native story presentation/Windows input, audio and exit destinations; no campaign playthrough.'
    native=$native;windows_cases=$cases;physical_devices_tested=$false
    executable_sha256=(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}
}
foreach($path in @('src/campaign_screen.c','include/ssa/campaign_screen.h','src/campaign_story.c',
    'platform/win32/game.c','platform/win32/audio_win32.c','platform/win32/audio_win32.h',
    'tests/test_campaign_screen.c','scripts/test-campaign-screen.ps1')){
    $report.source_sha256[$path]=(Get-FileHash -LiteralPath (Join-Path $projectRoot $path) -Algorithm SHA256).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-campaign-screen-validation.json'),($report|ConvertTo-Json -Depth 6)+[Environment]::NewLine)
$report|ConvertTo-Json -Depth 6
