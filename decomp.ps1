param(
    [ValidateSet('setup','import','status','autopilot','export','rebuild','gui','tool','test-input','test-movement','test-physics','test-maps','test-geometry','test-view','test-world','test-slide','test-horizontal','test-inventory','test-pickup','test-spawn','test-weapon-select','test-entity-timer','test-player-spawn','test-player-motion','test-player-frame','test-player-runtime','test-respawn-select','test-player-lifecycle','test-player-hazards','test-player-gameplay','test-switches','test-sector-movers','test-level-script','test-level-runtime','test-entity-pool','test-enemy-activation','test-script-spawn','test-enemy-motion','test-enemy-navigation','test-enemy-lifecycle','test-enemy-cd','test-enemy-cd-runtime','test-enemy-targeting','test-ray-traversal','test-ray-intercepts','test-enemy-ray','test-enemy-queries','test-combat-damage','test-combat-explosion','test-combat-rewards','test-combat-combined','test-combat-runtime','test-weapon-animation','test-weapon-controller','test-weapon-aim','test-weapon-hit','test-weapon-runtime','test-projectile-create','test-projectile-motion','test-projectile-controller','test-projectile-bounce','test-projectile-runtime','test-gunrilla','test-projectile-effect','test-projectile-conversion','test-entity-dispatch','test-entity-runtime','test-weapon-loadout','test-weapon-start','test-frame','test-sprite-dispatch','test-actor-animation','test-actor-animation-runtime','test-pickup-respawn','test-pickup-respawn-runtime','test-game-runtime','test-map-gameplay-ownership','test-native-game','test-native-playability','test-second-level','test-third-level','test-campaign','test-map-view','test-map-view-rom','test-menu','test-menu-rom','test-profile','test-chase-aim','test-unlimited-range','test-audio','test-audio-playability','test-free-look','test-pc-renderer','test-pc-renderer-playability','test-enemy-control','test-enemy-control-runtime','test-enemy-ranged','test-enemy-boss','test-enemy-boss-runtime','test-weapon-fixes','test-special-weapons','test-palette-effects','test-sector-clip','test-sector-clip-runtime','test-pickup-runtime','test-particle-create','test-particle-runtime','test-projectile-target','test-projectile-extended','test-input-dispatch','test-pc-input','test-sprite-assets','test-mode-control','test-raster-fill','test-texture-assets','test-map-texture-bindings','test-world-raster','test-render-view','test-plane-renderer','test-world-visibility','test-wall-renderer','test-scene-coverage','test-world-scene','test-world-scene-rom','test-map-actors','test-actor-assets','test-actor-projection','test-actor-queue','test-actor-scene-rom','test-actor-scene','test-sprite-cache','test-sprite-pool','test-sprite-oam','test-display','test-hud-colors','test-hud-scene','test-hud-scene-rom','test-native-viewer','test-win32-input')]
    [string]$Command = 'status',
    [switch]$OwnCopy,
    [string]$ToolName,
    [string]$ToolArgs = '{}'
)
$ErrorActionPreference = 'Stop'
if ($Command -eq 'test-native-viewer') {
    & (Join-Path $PSScriptRoot 'scripts/test-native-viewer.ps1')
    exit $LASTEXITCODE
}
if ($Command -eq 'test-win32-input') {
    & (Join-Path $PSScriptRoot 'scripts/test-win32-input.ps1')
    exit $LASTEXITCODE
}
$pathOutput = & wsl -d Ubuntu --exec wslpath -a $PSScriptRoot
if ($LASTEXITCODE -ne 0) { throw 'Could not resolve workspace path in WSL Ubuntu.' }
$linuxRoot = "$pathOutput".Trim()
$arguments = @('-d', 'Ubuntu', '--exec', "$linuxRoot/.venv-linux/bin/python", '-u', "$linuxRoot/scripts/decomp.py", $Command)
if ($OwnCopy) { $arguments += '--own-copy' }
$jsonFile = $null
try {
    if ($ToolName) {
        $cacheDir = Join-Path $PSScriptRoot '.cache'
        New-Item -ItemType Directory -Path $cacheDir -Force | Out-Null
        $jsonName = 'tool-args-' + [guid]::NewGuid().ToString('N') + '.json'
        $jsonFile = Join-Path $cacheDir $jsonName
        [System.IO.File]::WriteAllText($jsonFile, $ToolArgs, (New-Object System.Text.UTF8Encoding($false)))
        $arguments += @('--name', $ToolName, '--args-file', "$linuxRoot/.cache/$jsonName")
    }
    & wsl @arguments
    $resultCode = $LASTEXITCODE
} finally {
    if ($jsonFile -and (Test-Path -LiteralPath $jsonFile)) { Remove-Item -LiteralPath $jsonFile }
}
exit $resultCode
