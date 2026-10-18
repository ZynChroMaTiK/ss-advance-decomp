param()
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$directory=Join-Path $root 'soundfonts'
$destination=Join-Path $directory 'SC-55.sf2'
$expected='ae2e56711dc40835d3f97d975d80eadb2abdc76dc9da8b055f7a3c18dab20974'
if(Test-Path -LiteralPath $destination){
    if((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -eq $expected){
        Write-Output 'SC-55 from Musical Artifacts 1228 is already installed.';exit 0
    }
    throw 'soundfonts/SC-55.sf2 already contains a different file. Move it aside or use --soundfont with its path.'
}
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$temporary=Join-Path $directory ('download-'+[Guid]::NewGuid().ToString('N')+'.tmp')
try{
    try{
        Invoke-WebRequest 'https://musical-artifacts.com/artifacts/1228/SC-55.sf2' -OutFile $temporary
    }catch{
        throw "SoundFont download failed. Download https://musical-artifacts.com/artifacts/1228/SC-55.sf2 in your browser and save it to $destination, then rerun this script to verify it. $($_.Exception.Message)"
    }
    if((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected){throw 'Downloaded SoundFont checksum does not match the verified Musical Artifacts 1228 file.'}
    Move-Item -LiteralPath $temporary -Destination $destination
    Write-Output 'Installed SC-55 from Musical Artifacts 1228. Select Options > Settings > Music style > SoundFont.'
}finally{if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}}
