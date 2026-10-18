param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version = ([IO.File]::ReadAllText((Join-Path $projectRoot 'VERSION'))).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+(?:-[a-z0-9.]+)?$') { throw 'Invalid release version.' }
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-windows.ps1') -StaticRuntime }
$buildReport = Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/windows-native-build-validation.json') -Raw | ConvertFrom-Json
if (-not $buildReport.static_runtime -or $buildReport.version -ne $version) { throw 'Build this version with -StaticRuntime before packaging.' }
$output = Join-Path $projectRoot 'build/releases'
$name = 'ss-advance-v' + $version + '-windows-x64'
$stage = Join-Path $output ($name + '-stage-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
# Explicit allowlist: never copy a workspace/build tree or personal runtime data.
$files = @('play-native.cmd', 'VERSION', 'orig/README.md', 'soundfonts/README.md',
    'scripts/install-soundfont.ps1', 'third_party/TinySoundFont/LICENSE',
    'third_party/TinySoundFont/README.md', 'third_party/TinySoundFont/REVISION',
    'build/native-windows/Release/ssa_native_game.exe')
foreach ($relative in $files) {
    $destination = Join-Path $stage $relative
    New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot $relative) -Destination $destination
}
Copy-Item -LiteralPath (Join-Path $projectRoot ('releases/v' + $version + '.md')) -Destination (Join-Path $stage 'README.md')
$archive = Join-Path $output ($name + '.zip')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [IO.File]::Open($archive, [IO.FileMode]::Create)
$zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File) {
        $entry = $file.FullName.Substring($stage.Length + 1).Replace('\', '/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $entry) | Out-Null
    }
} finally { $zip.Dispose(); $stream.Dispose() }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $output ($name + '.sha256')), $hash + '  ' + $name + '.zip' + "`n")
Write-Output $archive
Write-Output ('SHA256 ' + $hash)
