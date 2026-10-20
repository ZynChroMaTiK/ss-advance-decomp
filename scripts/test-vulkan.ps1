param([switch]$SkipBuild,[switch]$Validation)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'build-windows.ps1') -StaticRuntime}
$output=Join-Path $root 'build/native-windows/test-output/vulkan'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$previous=$env:SSA_VULKAN_VALIDATION
try {
    if($Validation){$env:SSA_VULKAN_VALIDATION='1'}
    $exe=Join-Path $root 'build/native-windows/Release/ssa_native_vulkan_check.exe'
    $arguments=@((Join-Path $root 'orig/SSA.gba'),(Join-Path $output 'scene.bmp'),(Join-Path $output 'water.bmp'))
    $quoted=($arguments | ForEach-Object { '"{0}"' -f $_ }) -join ' '
    $stdout=Join-Path $output 'result.json';$stderr=Join-Path $output 'validation.log'
    $process=Start-Process -FilePath $exe -ArgumentList $quoted -WorkingDirectory $root -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if($process.ExitCode -ne 0){Get-Content -LiteralPath $stderr;throw "Vulkan regression failed: $($process.ExitCode)"}
    $result=Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json
    if($result.result -ne 'pass' -or $result.validation_errors -ne 0 -or (Select-String -LiteralPath $stderr -Pattern 'Vulkan validation:' -Quiet)){throw 'Vulkan validation reported errors.'}
    $game=Join-Path $root 'build/native-windows/Release/ssa_native_game.exe'
    $gameArgs='"{0}" --smoke --smoke-frames 180 --smoke-look --smoke-resize --mute --snapshot "{1}"' -f (Join-Path $root 'orig/SSA.gba'),(Join-Path $output 'frontend.bmp')
    $gameOut=Join-Path $output 'frontend.json';$gameLog=Join-Path $output 'frontend.log'
    $process=Start-Process -FilePath $game -ArgumentList $gameArgs -WorkingDirectory $root -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $gameOut -RedirectStandardError $gameLog
    if($process.ExitCode -ne 0){Get-Content -LiteralPath $gameLog;throw "Vulkan frontend failed: $($process.ExitCode)"}
    $frontend=Get-Content -LiteralPath $gameOut -Raw | ConvertFrom-Json
    if($frontend.result -ne 'pass' -or $frontend.world_rays -ne 0 -or
        ($frontend.initial_width -eq $frontend.render_width -and $frontend.initial_height -eq $frontend.render_height) -or
        (Select-String -LiteralPath $gameLog -Pattern 'Vulkan validation:|using Software|returning to software' -Quiet)){
        throw 'Vulkan frontend did not complete hardware rendering/resizing cleanly.'
    }
    $report=[ordered]@{result='pass';validation_layers=[bool]$Validation;native=$result;frontend=$frontend;source_sha256=[ordered]@{}}
    foreach($path in @('CMakeLists.txt','src/pc_scene.c','src/pc_renderer_pitch.inc','src/pc_renderer.c','include/ssa/pc_renderer.h','include/ssa/pc_hardware.h',
        'platform/win32/vulkan_renderer.c','platform/win32/vulkan_renderer.h','platform/vulkan/scene.vert','platform/vulkan/scene.frag',
        'platform/win32/game.c','platform/win32/game_screens.inc','src/settings.c','include/ssa/settings.h','src/menu.c','include/ssa/menu.h',
        'tests/test_vulkan_renderer.c','tests/test_settings_fields.c','scripts/test-vulkan.ps1')){
        $report.source_sha256[$path]=(Get-FileHash -LiteralPath (Join-Path $root $path) -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    [IO.File]::WriteAllText((Join-Path $root 'analysis/vulkan-validation.json'),($report | ConvertTo-Json -Depth 6)+[Environment]::NewLine)
    Write-Output "Passed Vulkan on $($result.device): $($result.checks) checks, $($result.draws) paired scene draws, no validation errors."
} finally {
    $env:SSA_VULKAN_VALIDATION=$previous
}
