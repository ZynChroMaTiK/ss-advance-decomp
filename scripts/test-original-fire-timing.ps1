param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=[System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$oracleRoot=Join-Path $projectRoot 'tools/mgba-timing'
$lock=Get-Content -LiteralPath (Join-Path $projectRoot 'tools/mgba-timing.lock.json') -Raw | ConvertFrom-Json
$revision=(& git -c "safe.directory=$oracleRoot" -C $oracleRoot rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $revision -ne $lock.revision){throw 'Expected pinned mGBA0.10.5 checkout in tools/mgba-timing.'}
$buildRoot=Join-Path $projectRoot 'build/mgba-timing'
if(-not $SkipBuild){
    $sourceRoot=Join-Path $projectRoot 'build/mgba-timing-src'
    New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null
    $cmakeText=@'
cmake_minimum_required(VERSION 3.20)
project(ssa_rom_timing C)
set(LIBMGBA_ONLY ON)
set(M_CORE_GB OFF)
set(M_CORE_GBA ON)
add_subdirectory(../../tools/mgba-timing mgba)
add_executable(ssa_rom_timing ../../tests/probe_original_frame_rate.c)
target_include_directories(ssa_rom_timing PRIVATE ../../tools/mgba-timing/include ${CMAKE_CURRENT_BINARY_DIR}/mgba/include)
target_link_libraries(ssa_rom_timing PRIVATE mgba)
'@
    [System.IO.File]::WriteAllText((Join-Path $sourceRoot 'CMakeLists.txt'),$cmakeText)
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vsRoot=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmake=Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    # Third-party compiler/CMake warnings go to logs; exit codes decide failure.
    $ErrorActionPreference='Continue'
    & $cmake -Wno-dev -Wno-deprecated -S $sourceRoot -B $buildRoot -G 'Visual Studio 17 2022' -A x64 *> (Join-Path $sourceRoot 'configure.log')
    $configureExit=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if($configureExit -ne 0){throw 'Timing oracle configuration failed.'}
    $ErrorActionPreference='Continue'
    & $cmake --build $buildRoot --config Release --target ssa_rom_timing --parallel 4 *> (Join-Path $sourceRoot 'build.log')
    $buildExit=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if($buildExit -ne 0){Get-Content (Join-Path $sourceRoot 'build.log') -Tail 20;throw 'Timing oracle build failed.'}
}
$checker=Join-Path $buildRoot 'Release/ssa_rom_timing.exe'
$cases=@()
foreach($weapon in @(1,4,5)){
    $output=Join-Path $buildRoot ('weapon'+$weapon+'.jsonl')
    $p=Start-Process -FilePath $checker -ArgumentList @((Join-Path $projectRoot 'orig/SSA.gba'),$weapon) `
        -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $output `
        -RedirectStandardError (Join-Path $buildRoot ('weapon'+$weapon+'.log'))
    if($p.ExitCode -ne 0){throw 'Original ROM timing trace failed.'}
    $frames=@(Get-Content -LiteralPath $output | ForEach-Object {$_ | ConvertFrom-Json})
    if($weapon -eq 1){
        if($frames.Count -ne 87 -or $frames[-1].shots -ne 86 -or $frames[-1].ammo -ne 999){throw 'Unexpected original chainsaw trace.'}
        $refreshes=0;$six=0;$seven=0
        for($i=2;$i -lt $frames.Count;$i++){
            $interval=$frames[$i].vblank-$frames[$i-1].vblank
            if($interval -eq 6){++$six}elseif($interval -eq 7){++$seven}else{throw 'Unexpected chainsaw interval.'}
            if($frames[$i].shots-$frames[$i-1].shots -ne 1){throw 'Chainsaw shot counter changed.'}
            $refreshes+=$interval
        }
        if($refreshes -ne 584 -or $six -ne 11 -or $seven -ne 74){throw 'Original chainsaw timing changed.'}
        $cases+=@{weapon=1;hardware_refreshes=600;shots=86;steady_intervals=85;steady_refreshes=$refreshes
            six_refresh_intervals=$six;seven_refresh_intervals=$seven
            shots_per_second=16777216.0/280896*85/$refreshes;rom_range_units=96;trace=$frames}
        continue
    }
    for($i=2;$i -lt $frames.Count;$i++){
        if($frames[$i].vblank-$frames[$i-1].vblank -ne 7 -or $frames[$i].shots-$frames[$i-1].shots -ne 1 -or
            $frames[$i-1].ammo-$frames[$i].ammo -ne 1){throw 'Original firing cadence changed.'}
    }
    $cases+=@{weapon=$weapon;hardware_refreshes=600;shots=85;steady_intervals=84;interval_refreshes=7;trace=$frames}
}
$report=[ordered]@{result='pass';oracle='mGBA0.10.5, built-in BIOS, original first-level start scene'
    oracle_revision=$revision;rom_sha1=(Get-FileHash -Algorithm SHA1 -LiteralPath (Join-Path $projectRoot 'orig/SSA.gba')).Hash.ToLowerInvariant()
    scope='Only RAM inventory/current weapon were changed to select each weapon. No ROM instructions were patched. Measures this scene; rendering cost and cadence can vary elsewhere. Chainsaw native cadence uses the measured steady average, not a replay of scene-dependent interval jitter.'
    gba_cpu_hz=16777216;gba_frame_cycles=280896;interval_refreshes=7
    shots_per_second=16777216.0/(280896.0*7);cases=$cases
    checker_sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $checker).Hash.ToLowerInvariant();source_sha256=[ordered]@{}}
foreach($source in @('tests/probe_original_frame_rate.c','scripts/test-original-fire-timing.ps1','tools/mgba-timing.lock.json')){
    $report.source_sha256[$source]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
}
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/original-fire-timing-validation.json'),($report|ConvertTo-Json -Depth 7)+[Environment]::NewLine)
Write-Output 'Original ROM: Tommy/minigun seven-refresh intervals; chainsaw 85 steady intervals over 584 refreshes (about8.69attacks/s).'
