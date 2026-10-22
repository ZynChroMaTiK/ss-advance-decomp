param([string]$Source=(Join-Path $PSScriptRoot '../assets/classic-hud/crosshair-reference.png'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$bitmap=[Drawing.Bitmap]::FromFile([IO.Path]::GetFullPath($Source))
try {
    if($bitmap.Width -ne 19 -or $bitmap.Height -ne 19){throw 'Expected the supplied19x19 crosshair.'}
    $lines=[Collections.Generic.List[string]]::new()
    $lines.Add('/* Imported from crosshair-reference.png by import-classic-crosshair.ps1.')
    $lines.Add(' * Exact19x19 artwork at(3,3) in24x24 storage; center dot at(12,12). */')
    $lines.Add('static const uint8_t classic_crosshair_mask[SSA_CLASSIC_CROSSHAIR_PIXELS]={')
    for($y=0;$y -lt 24;++$y){
        $row=for($x=0;$x -lt 24;++$x){
            if($x -lt 3 -or $x -gt 21 -or $y -lt 3 -or $y -gt 21){'0';continue}
            $c=$bitmap.GetPixel($x-3,$y-3)
            if(!$c.A){'0';continue}
            if($c.A -ne 255){throw 'Expected original binary transparency.'}
            if($c.R -eq 231 -and $c.G -eq 231 -and $c.B -eq 231){'31'}
            elseif($c.R -eq 148 -and $c.G -eq 148 -and $c.B -eq 140){'18'}
            else {throw "Unexpected color at$x,$y; source must use the original crosshair palette."}

        }
        $lines.Add('    '+($row -join ',')+',')
    }
    $lines.Add('};')
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot '../src/classic_crosshair.inc'),($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
} finally {$bitmap.Dispose()}
