param([switch]$SkipBuild,[int]$Frames=60,[int]$Width=1920,[int]$Height=1080,[switch]$Software)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if($Frames -lt 1 -or $Width -lt 1 -or $Height -lt 1){throw 'Frame count and dimensions must be positive.'}
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1') -StaticRuntime}
$output=Join-Path $root 'build/native-windows/test-output/vulkan'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$name=if($Software){'software-performance'}else{'performance'}
$stdout=Join-Path $output "$name.json";$stderr=Join-Path $output "$name.log"
$exe=Join-Path $root 'build/native-windows/Release/ssa_vulkan_bench.exe'
$arguments='"{0}" {1} {2} {3}' -f (Join-Path $root 'orig/SSA.gba'),$Frames,$Width,$Height
if($Software){$arguments+=' software'}
$previous=$env:SSA_VULKAN_VALIDATION
try {
    # Validation overhead is measured separately by test-vulkan.ps1.
    $env:SSA_VULKAN_VALIDATION=$null
    $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $root -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if($process.ExitCode -ne 0){Get-Content -LiteralPath $stderr;throw "Rendering benchmark failed: $($process.ExitCode)"}
    $result=Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json
    $report=[ordered]@{scope='Isolated render wall time; no gameplay/audio/presentation or campaign run';native=$result;source_sha256=[ordered]@{}}
    foreach($path in @('tests/bench_vulkan.c','platform/win32/vulkan_renderer.c','platform/win32/vulkan_renderer.h',
        'platform/vulkan/scene.vert','platform/vulkan/scene.frag','src/pc_scene.c','src/pc_renderer_pitch.inc','include/ssa/pc_hardware.h')){
        $report.source_sha256[$path]=(Get-FileHash -LiteralPath (Join-Path $root $path) -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    [IO.File]::WriteAllText((Join-Path $root "analysis/$name-vulkan.json"),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
    $result.cases | Format-Table scene,decals,reflections,ms,fps
} finally {$env:SSA_VULKAN_VALIDATION=$previous}
