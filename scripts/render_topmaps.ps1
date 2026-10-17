param([string]$Root = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$atlasDir = Join-Path $Root 'build/exports/ssa-topmaps'
$pngDir = Join-Path $atlasDir 'png'
[IO.Directory]::CreateDirectory($pngDir) | Out-Null
$scenes = Get-Content -Raw (Join-Path $atlasDir 'render-scenes.json') | ConvertFrom-Json
$fonts = @{}; $brushes=@{}; $pens=@{}; $images=@{}
function Brush($hex) {
 if (-not $brushes.ContainsKey($hex)) {$brushes[$hex]=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($hex))}
 return $brushes[$hex]
}
function Pen($hex,$width) {
 $key=$hex+':'+$width
 if (-not $pens.ContainsKey($key)) {$pens[$key]=[Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml($hex),[single]$width)}
 return $pens[$key]
}
function Font($size) {
 if (-not $fonts.ContainsKey([int]$size)) {$fonts[[int]$size]=[Drawing.Font]::new('Arial',[single]$size,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)}
 return $fonts[[int]$size]
}
$number=0
foreach ($scene in $scenes) {
 $number++
 $bmp=[Drawing.Bitmap]::new([int]$scene.width,[int]$scene.height)
 $g=[Drawing.Graphics]::FromImage($bmp)
 $g.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
 $g.TextRenderingHint=[Drawing.Text.TextRenderingHint]::AntiAliasGridFit
 foreach ($p in $scene.art) {
  if ($p.layer -in @('triggers','enemies','scripted-enemies')) { continue }
  switch ($p.kind) {
   'image' {
    if (-not $images.ContainsKey($p.path)) {$images[$p.path]=[Drawing.Image]::FromFile((Join-Path $atlasDir $p.path))}
    $g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::Half
    $g.DrawImage($images[$p.path],[single]$p.x,[single]$p.y,[single]$p.w,[single]$p.h)
    $g.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::Default
   }
   'rect' {$g.FillRectangle((Brush $p.fill),[single]$p.x,[single]$p.y,[single]$p.w,[single]$p.h)}
   'circle' {
    if ($p.fill -ne 'none') {$g.FillEllipse((Brush $p.fill),[single]($p.x-$p.r),[single]($p.y-$p.r),[single](2*$p.r),[single](2*$p.r))}
    if ($p.stroke) {$g.DrawEllipse((Pen $p.color $p.stroke),[single]($p.x-$p.r),[single]($p.y-$p.r),[single](2*$p.r),[single](2*$p.r))}
   }
   'line' {$g.DrawLine((Pen $p.color $p.stroke),[single]$p.points[0][0],[single]$p.points[0][1],[single]$p.points[1][0],[single]$p.points[1][1])}
   'path' {
    $path=[Drawing.Drawing2D.GraphicsPath]::new([Drawing.Drawing2D.FillMode]::Alternate)
    foreach ($loop in $p.loops) {
     if ($loop.Count -lt 3) {continue}
     $pts=[Drawing.PointF[]]@($loop | ForEach-Object {[Drawing.PointF]::new([single]$_[0],[single]$_[1])})
     $path.AddPolygon($pts)
    }
    if ($p.fill -ne 'none') {$g.FillPath((Brush $p.fill),$path)}
    if ($p.stroke) {$g.DrawPath((Pen $p.color $p.stroke),$path)}
    $path.Dispose()
   }
   'text' {$g.DrawString([string]$p.text,(Font $p.size),(Brush $p.color),[single]$p.x,[single]($p.y-$p.size))}
  }
 }
 $bmp.Save((Join-Path $pngDir ('level-{0:00}.png' -f $number)),[Drawing.Imaging.ImageFormat]::Png)
 $g.Dispose();$bmp.Dispose()
}
$tileWidth=[int]($scenes[0].width/5);$tileHeight=[int]($scenes[0].height/5)
$sheet=[Drawing.Bitmap]::new(($tileWidth*4),($tileHeight*4))
$g=[Drawing.Graphics]::FromImage($sheet);$g.Clear([Drawing.Color]::FromArgb(16,24,34))
$g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
for ($i=0;$i -lt 16;$i++) {
 $img=[Drawing.Image]::FromFile((Join-Path $pngDir ('level-{0:00}.png' -f ($i+1))))
 $g.DrawImage($img,[int](($i%4)*$tileWidth),[int]([math]::Floor($i/4)*$tileHeight),$tileWidth,$tileHeight);$img.Dispose()
}
$sheet.Save((Join-Path $atlasDir 'all-levels.png'),[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$sheet.Dispose()
foreach ($f in $fonts.Values) {$f.Dispose()};foreach ($b in $brushes.Values) {$b.Dispose()};foreach ($p in $pens.Values) {$p.Dispose()}
foreach ($img in $images.Values) {$img.Dispose()}
Write-Output 'Rendered 16 PNG maps and all-levels.png.'
