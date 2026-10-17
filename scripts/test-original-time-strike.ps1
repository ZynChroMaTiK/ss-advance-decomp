param()
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$oracleRoot=Join-Path $projectRoot 'tools/mgba-timing'
$lock=Get-Content -LiteralPath (Join-Path $projectRoot 'tools/mgba-timing.lock.json') -Raw | ConvertFrom-Json
$revision=(& git -c "safe.directory=$oracleRoot" -C $oracleRoot rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $revision -ne $lock.revision){throw 'Expected pinned timing oracle.'}
$sourceRoot=Join-Path $projectRoot 'build/mgba-time-strike-src'
$buildRoot=Join-Path $projectRoot 'build/mgba-time-strike'
New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null
$cmakeText=@'
cmake_minimum_required(VERSION 3.20)
project(ssa_time_strike_timing C)
set(LIBMGBA_ONLY ON)
set(M_CORE_GB OFF)
set(M_CORE_GBA ON)
add_subdirectory(../../tools/mgba-timing mgba)
add_executable(ssa_time_strike_timing ../../tests/probe_original_time_strike.c)
target_include_directories(ssa_time_strike_timing PRIVATE ../../tools/mgba-timing/include ${CMAKE_CURRENT_BINARY_DIR}/mgba/include)
target_link_libraries(ssa_time_strike_timing PRIVATE mgba)
'@
[IO.File]::WriteAllText((Join-Path $sourceRoot 'CMakeLists.txt'),$cmakeText)
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake=Join-Path $vsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ErrorActionPreference='Continue'
& $cmake -Wno-dev -Wno-deprecated -S $sourceRoot -B $buildRoot -G 'Visual Studio 17 2022' -A x64 *> (Join-Path $sourceRoot 'configure.log')
$configureExit=$LASTEXITCODE
if($configureExit -eq 0){& $cmake --build $buildRoot --config Release --target ssa_time_strike_timing --parallel 4 *> (Join-Path $sourceRoot 'build.log')}
$buildExit=$LASTEXITCODE;$ErrorActionPreference='Stop'
if($configureExit -ne 0 -or $buildExit -ne 0){Get-Content (Join-Path $sourceRoot 'build.log') -Tail 20;throw 'Timing oracle build failed.'}
$checker=Join-Path $buildRoot 'Release/ssa_time_strike_timing.exe'
$output=Join-Path $buildRoot 'trace.jsonl'
$p=Start-Process -FilePath $checker -ArgumentList @((Join-Path $projectRoot 'orig/SSA.gba')) -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $output -RedirectStandardError (Join-Path $buildRoot 'trace.log')
if($p.ExitCode -ne 0){throw 'Original Time Strike trace failed.'}
$trace=@(Get-Content -LiteralPath $output | ForEach-Object {$_ | ConvertFrom-Json})
if($trace.Count -ne 11 -or $trace[9].state -ne 1 -or $trace[10].type -ne 0){throw 'Unexpected original E3 lifetime.'}
$six=0;$seven=0;$refreshes=0
for($i=2;$i -lt $trace.Count;++$i){
    $interval=$trace[$i].vblank-$trace[$i-1].vblank
    if($interval -eq 6){++$six}elseif($interval -eq 7){++$seven}else{throw 'Unexpected original E3 update interval.'}
    $refreshes+=$interval
}
if($refreshes -ne 60 -or $six -ne 3 -or $seven -ne 6){throw 'Original E3 cadence changed.'}
for($i=1;$i -lt 9;++$i){
    if($trace[$i].vz -ne -2*65536*$i -or $trace[$i-1].z-$trace[$i].z -ne -$trace[$i].vz*$trace[$i].ticks){throw 'Original E3 gravity changed.'}
}
$report=[ordered]@{result='pass';oracle='mGBA0.10.5, built-in BIOS, original first-level scene';oracle_revision=$revision
    scope='RAM fixture converts an already linked pickup into E3 at its existing XY/floor,520units above the floor, using weapon9 artwork. No ROM instructions are patched. Original controller, frame timing and rendering execute. This is a measured scene, not campaign-wide hardware timing.'
    gba_cpu_hz=16777216;gba_frame_cycles=280896;steady_intervals=9;steady_refreshes=$refreshes
    six_refresh_intervals=$six;seven_refresh_intervals=$seven;native_average_intervals=3;native_average_refreshes=20;trace=$trace
    rom_sha1=(Get-FileHash -Algorithm SHA1 -LiteralPath (Join-Path $projectRoot 'orig/SSA.gba')).Hash.ToLowerInvariant()
    source_sha256=[ordered]@{}}
foreach($source in @('tests/probe_original_time_strike.c','scripts/test-original-time-strike.ps1','tools/mgba-timing.lock.json')){
    $report.source_sha256[$source]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot $source)).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'analysis/original-time-strike-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
$trace | Format-Table
