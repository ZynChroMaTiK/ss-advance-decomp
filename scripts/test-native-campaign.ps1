param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') }
$testRoot = Join-Path $projectRoot 'build/native-campaign'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$checker = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_campaign_check.exe'
$binary = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_game.exe'
$mapChecker = Join-Path $projectRoot 'build/native-windows/Release/ssa_native_map_view_check.exe'
$mapProcess = Start-Process -FilePath $mapChecker -ArgumentList @((Join-Path $projectRoot 'orig/SSA.gba')) -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput (Join-Path $testRoot 'map-view.json') -RedirectStandardError (Join-Path $testRoot 'map-view.log')
if ($mapProcess.ExitCode -ne 0) { throw 'MSVC map-view/music sequencing validation failed.' }
$mapResult = Get-Content -LiteralPath (Join-Path $testRoot 'map-view.json') -Raw | ConvertFrom-Json
$mapReference = Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/map-view-validation.json') -Raw | ConvertFrom-Json
foreach ($field in @('result','checks','map_song_terminal_wait_frames','music_modes','map_families')) {
    if ($mapResult.$field -ne $mapReference.$field) { throw ('MSVC/GCC map-view differs: ' + $field) }
}
$process = Start-Process -FilePath $checker -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait `
    -RedirectStandardOutput (Join-Path $testRoot 'completion.jsonl') -RedirectStandardError (Join-Path $testRoot 'completion.log')
if ($process.ExitCode -ne 0) { throw 'MSVC campaign normal-input completion failed.' }
$completion = @(Get-Content -LiteralPath (Join-Path $testRoot 'completion.jsonl') | ForEach-Object { $_ | ConvertFrom-Json })
$reference = (Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/campaign-validation.json') -Raw | ConvertFrom-Json).levels
$levelCount=$reference.Count
if ($levelCount -lt 3 -or $completion.Count -ne $levelCount) { throw 'Missing campaign completion results.' }
for ($i=0; $i -lt $levelCount; ++$i) {
    foreach ($field in @('result','frames','finished','score','collected','spawned','health','rendered_views','carry_guards',
        'airborne_frames','third_person_frames','pitched_frames','machinegun_shots','activated_switches','boss_frames','boss_hud_frames','boss_projectile_frames',
        'boss_hurt_frames','boss_wait_frames','boss_defeated','boss_music','post_boss_music','mixed_frames','audio_errors',
        'owned_weapons','conversion_shots','conversion_frames','navigation_recoveries','closing_door_wait_frames','meteor_shots','falling_effect_frames','cannon_shots','boss_launches')) {
        if ($completion[$i].$field -ne $reference[$i].$field) { throw "MSVC/GCC level $i differs: $field" }
    }
}
$oldPause = $env:SSA_LAUNCH_NO_PAUSE
$oldLog = $env:SSA_LAUNCH_LOG
$oldCapture = $env:SSA_TRANSITION_CAPTURE_DIR
$env:SSA_LAUNCH_NO_PAUSE = '1'
$env:SSA_LAUNCH_LOG = Join-Path $testRoot 'launch.log'
$env:SSA_TRANSITION_CAPTURE_DIR = $testRoot
if(-not ('SsaTransitionPixels' -as [type])){Add-Type -TypeDefinition @'
using System;
public static class SsaTransitionPixels {
    public static long[] Verify(byte[] black, byte[] mid, byte[] full, byte[] blackSource, byte[] midSource, byte[] fullSource) {
        if(black.Length!=mid.Length || black.Length!=full.Length || black.Length!=blackSource.Length || mid.Length!=midSource.Length || full.Length!=fullSource.Length || full.Length<54 ||
           BitConverter.ToInt16(full,28)!=32) throw new Exception("Invalid transition bitmaps.");
        int offset=BitConverter.ToInt32(full,10), width=BitConverter.ToInt32(full,18), height=BitConverter.ToInt32(full,22);
        if(offset<54 || width<=0 || height>=0 || (long)offset+(long)width*-height*4!=full.Length ||
           BitConverter.ToInt32(mid,18)!=width || BitConverter.ToInt32(mid,22)!=height ||
           BitConverter.ToInt32(black,18)!=width || BitConverter.ToInt32(black,22)!=height)
            throw new Exception("Transition dimensions differ.");
        long samples=0, middle=0, bright=0;
        for(int p=offset; p<full.Length; p+=4) for(int channel=0; channel<3; ++channel) {
            int i=p+channel;
            if(black[i]!=0 || mid[i]!=midSource[i]/2 || full[i]!=fullSource[i]) throw new Exception("Live black/midpoint/full reveal pixels differ.");
            middle+=mid[i]; bright+=full[i]; ++samples;
        }
        if(bright==0 || middle==0) throw new Exception("Level never became visible.");
        return new long[]{samples,middle,bright};
    }
}
'@}
$cases = @()
try {
    foreach ($original in @($false,$true)) {
        $arguments = @('--startup-test','--transition-test','--mute')
        if ($original) { $arguments += @('--original-renderer','--no-music') }
        $output = @(& (Join-Path $projectRoot 'play-native.cmd') @arguments)
        $launchExit = $LASTEXITCODE
        $name = if ($original) { 'reference' } else { 'pc' }
        $output | Set-Content -LiteralPath (Join-Path $testRoot ($name + '.txt'))
        if ($launchExit -ne 0) { throw "Windows campaign transition failed: $name" }
        $item = $output -join [Environment]::NewLine | ConvertFrom-Json
        $maps=2*$levelCount-1
        if ($item.result -ne 'pass' -or $item.level -ne ($levelCount-1) -or $item.transition_steps -ne 4*($levelCount-1) -or
            $item.restarts -ne 2*($levelCount-1) -or $item.audio_device -ne 1 -or $item.audio_errors -ne 0 -or
            $item.map_views -ne $maps -or $item.map_starts -ne $maps -or $item.map_frozen_ticks -ne 70*$maps -or
            $item.map_black_presents -ne $maps -or $item.map_fade_presents -lt $maps -or
            $item.map_black_hold_ticks -ne 15*$maps -or $item.level_fade_ticks -ne 60*$maps -or
            $item.level_fade_starts -ne $maps -or $item.level_fade_presents -lt $maps -or $item.reveal_weapon_checks -ne $maps -or
            $item.reveal_game_ticks -lt 60*$maps -or $item.reveal_move_checks -ne $maps -or
            $item.reveal_hud_checks -ne $maps -or $item.reveal_entry_checks -ne $maps -or
            $item.black_hold_min_us -lt 250000 -or $item.level_fade_min_us -lt 1000000 -or
            $item.map_music_checks -ne 9*$maps -or $item.level_music_checks -ne $maps -or
            $item.menu_starts -ne 1 -or $item.menu_frozen_ticks -ne 187) { throw 'Incomplete transition/restart/map-view validation.' }
        $blackImage=Join-Path $testRoot ($name+'-black.bmp')
        $middleImage=Join-Path $testRoot ($name+'-mid.bmp')
        $fullImage=Join-Path $testRoot ($name+'-full.bmp')
        $blackSource=Join-Path $testRoot ($name+'-black-source.bmp')
        $middleSource=Join-Path $testRoot ($name+'-mid-source.bmp')
        $fullSource=Join-Path $testRoot ($name+'-full-source.bmp')
        $pixels=[SsaTransitionPixels]::Verify([IO.File]::ReadAllBytes($blackImage),[IO.File]::ReadAllBytes($middleImage),[IO.File]::ReadAllBytes($fullImage),[IO.File]::ReadAllBytes($blackSource),[IO.File]::ReadAllBytes($middleSource),[IO.File]::ReadAllBytes($fullSource))
        $images=[ordered]@{rgb_samples=$pixels[0];midpoint_energy=$pixels[1];full_energy=$pixels[2];files=[ordered]@{}}
        foreach($imagePath in @($blackImage,$middleImage,$fullImage,$blackSource,$middleSource,$fullSource)){
            $images.files[[IO.Path]::GetFileName($imagePath)]=(Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        $cases += @{ original_renderer=$original; music_enabled=(-not $original); output_muted=$true; startup=$item; transition_images=$images }
    }
} finally { $env:SSA_LAUNCH_NO_PAUSE=$oldPause; $env:SSA_LAUNCH_LOG=$oldLog; $env:SSA_TRANSITION_CAPTURE_DIR=$oldCapture }
$report = [ordered]@{ result='pass'; levels=$completion; map_view=$mapResult; windows_cases=$cases
    physical_devices_tested=$false
    executable_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $binary).Hash.ToLowerInvariant()
    checker_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $checker).Hash.ToLowerInvariant()
    map_checker_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $mapChecker).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{} }
foreach ($source in @('src/native_game.c','src/native_audio.inc','include/ssa/native_game.h','src/weapon_progress.c','include/ssa/weapon_progress.h','src/palette_effect.c','include/ssa/palette_effect.h',
    'src/projectile_runtime.c','src/projectile_runtime_internal.h','src/weapon_runtime.c','src/audio.c','include/ssa/audio.h','platform/win32/game.c',
    'src/map_view.c','include/ssa/map_view.h','src/menu.c','include/ssa/menu.h','tests/test_map_view.c','platform/win32/audio_win32.c','platform/win32/audio_win32.h',
    'tests/test_campaign.c','tests/test_campaign.py','tests/test_second_level.c','tests/test_native_playability.c','scripts/test-native-campaign.ps1','CMakeLists.txt')) {
    $report.source_sha256[$source]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/windows-native-campaign-validation.json'),
    ($report | ConvertTo-Json -Depth 8) + [Environment]::NewLine)
Write-Output ('Passed '+$levelCount+' native levels, original map-view/music sequencing, every Enter advancement, level-entry restarts and both renderers.')
