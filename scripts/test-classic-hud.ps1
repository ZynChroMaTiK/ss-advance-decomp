param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $root 'scripts/build-windows.ps1') -StaticRuntime}
$directory=Join-Path $root 'build/classic-hud-tests'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$core=& (Join-Path $root 'build/native-windows/Release/ssa_native_classic_hud_check.exe') (Join-Path $root 'orig/SSA.gba') $directory | ConvertFrom-Json
if($LASTEXITCODE -ne 0 -or $core.result -ne 'pass'){throw 'Classic HUD core failed.'}
$reference=Get-Content (Join-Path $root 'analysis/classic-hud-validation.json') -Raw | ConvertFrom-Json
foreach($field in @('checks','draws','paths')){if($core.$field -ne $reference.$field){throw "Classic HUD MSVC/GCC mismatch: $field"}}
$output=Join-Path $directory 'windows.json'
$process=Start-Process -FilePath (Join-Path $root 'build/native-windows/Release/ssa_native_classic_hud_win32_check.exe') `
    -ArgumentList @('orig/SSA.gba','build/classic-hud-tests') -WorkingDirectory $root -WindowStyle Hidden -Wait -PassThru `
    -RedirectStandardOutput $output -RedirectStandardError (Join-Path $directory 'windows.log')
if($process.ExitCode -ne 0){Get-Content (Join-Path $directory 'windows.log');throw 'Classic HUD frontend checks failed.'}
$windows=Get-Content $output -Raw | ConvertFrom-Json
if($windows.result -ne 'pass' -or $windows.paths -ne 2){throw 'Incomplete Classic HUD frontend checks.'}
# Compare the rendered icon with the actual supplied PNG, independently of the
# generated C pixels. This catches accidental resizing, cropping or stale art.
Add-Type -AssemblyName System.Drawing
$referencePixels=0
$icon=[Drawing.Bitmap]::FromFile((Join-Path $root 'assets/classic-hud/time-strike-reference.png'))
$rom=[IO.File]::ReadAllBytes((Join-Path $root 'orig/SSA.gba'))
$greens=for($light=0;$light -lt 32;++$light){
    $best=0;$errorValue=100
    for($index=1;$index -lt 256;++$index){
        $offset=0x79effc+2*$index;$value=[int]$rom[$offset] -bor ([int]$rom[$offset+1] -shl 8)
        if($value -band 0x7c1f){continue}
        $level=($value -shr 5) -band 31;$delta=[math]::Abs($level-$light)
        if($delta -lt $errorValue){$errorValue=$delta;$best=$level}
    }
    ($best*8+[int][math]::Floor($best/4)) -shl 8
}
try {
    if($icon.Width -ne 16 -or $icon.Height -ne 24){throw 'Unexpected reference dimensions.'}
    foreach($level in @(0,6)){
        $capture=[Drawing.Bitmap]::FromFile((Join-Path $directory "classic-ammo-panel-$level.bmp"))
        try {
            for($y=0;$y -lt 24;++$y){for($x=0;$x -lt 16;++$x){
                $sourceColor=$icon.GetPixel($x,$y);++$referencePixels
                if($y -ge 17){if($sourceColor.A){throw 'Reference artwork extends below the HUD canvas.'};continue}
                $expected=0x202020
                if($sourceColor.A){
                    $light=[math]::Max([math]::Max([int]$sourceColor.R,[int]$sourceColor.G),[int]$sourceColor.B) -shr 3
                    if($sourceColor.G -and !$sourceColor.R -and !$sourceColor.B){$light=31}
                    $expected=$greens[$light]
                }
                $actual=$capture.GetPixel(58+$x,141+$y).ToArgb() -band 0xffffff
                if($actual -ne $expected){throw "Time Strike reference pixel mismatch at$x,$y (map$level)."}
            }}
        } finally {$capture.Dispose()}
    }
} finally {$icon.Dispose()}
# Verify the new crosshair against the supplied PNG, not the generated C table.
$crosshair=[Drawing.Bitmap]::FromFile((Join-Path $root 'assets/classic-hud/crosshair-reference.png'))
$capture=[Drawing.Bitmap]::FromFile((Join-Path $directory 'classic-crosshair-0.bmp'))
try {
    if($crosshair.Width -ne 19 -or $crosshair.Height -ne 19){throw 'Unexpected crosshair dimensions.'}
    for($y=0;$y -lt 19;++$y){for($x=0;$x -lt 19;++$x){
        $sourceColor=$crosshair.GetPixel($x,$y)
        if(!$sourceColor.A){continue}
        $actual=$capture.GetPixel(67+(111+$x)*3,(71+$y)*3).ToArgb() -band 0xffffff
        if($actual -ne ($sourceColor.ToArgb() -band 0xffffff)){throw "Crosshair source pixel mismatch at$x,$y."}
    }}
} finally {$capture.Dispose();$crosshair.Dispose()}
$report=[ordered]@{result='pass';core=$core;windows=$windows;physical_input_tested=$false;source_sha256=[ordered]@{}
    time_strike_reference_pixels=$referencePixels
    executable_sha256=(Get-FileHash -Algorithm SHA256 (Join-Path $root 'build/native-windows/Release/ssa_native_game.exe')).Hash.ToLowerInvariant()}
foreach($file in $reference.source_sha256.PSObject.Properties.Name){
    $hash=(Get-FileHash -Algorithm SHA256 (Join-Path $root $file)).Hash.ToLowerInvariant()
    if($hash -ne $reference.source_sha256.$file){throw "Stale Classic HUD baseline: $file"}
    $report.source_sha256[$file]=$hash
}
foreach($file in @('platform/win32/game.c','tests/test_classic_hud_win32.c','scripts/test-classic-hud.ps1','CMakeLists.txt')){
    $report.source_sha256[$file]=(Get-FileHash -Algorithm SHA256 (Join-Path $root $file)).Hash.ToLowerInvariant()
}
[IO.File]::WriteAllText((Join-Path $root 'analysis/windows-classic-hud-validation.json'),($report | ConvertTo-Json -Depth 5)+[Environment]::NewLine)
Write-Output 'Passed Classic/Original HUD, live statistics, both renderers, dual Colts and clean NETRICSA/pause/completion captures.'
