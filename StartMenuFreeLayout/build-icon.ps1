# Generate native multi-size icon resources from the simple SVG mark.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$iconFolder=Join-Path $PSScriptRoot 'assets'
[xml]$iconSource=[IO.File]::ReadAllText((Join-Path $iconFolder 'AppIcon.svg'))
$iconRects=$iconSource.SelectNodes("//*[local-name()='rect']")
if($iconRects.Count -ne 8){throw 'Expected eight icon tiles'}
$iconSizes=@(16,20,24,32,40,48,64,96,128,256)
$iconImages=@()
foreach($iconSize in $iconSizes) {
    $large=New-Object Drawing.Bitmap ($iconSize*4),($iconSize*4)
    $graphics=[Drawing.Graphics]::FromImage($large)
    try {
        $graphics.Clear([Drawing.Color]::Transparent)
        $graphics.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.ScaleTransform([single]($iconSize*4/103.0),[single]($iconSize*4/102.0))
        $brush=New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#0070f5'))
        try {
            foreach($rect in $iconRects) {
                $x=[single]$rect.x;$y=[single]$rect.y;$w=[single]$rect.width;$h=[single]$rect.height;$d=2*[single]$rect.rx
                $path=New-Object Drawing.Drawing2D.GraphicsPath
                try {
                    $path.AddArc($x,$y,$d,$d,180,90);$path.AddArc(($x+$w-$d),$y,$d,$d,270,90)
                    $path.AddArc(($x+$w-$d),($y+$h-$d),$d,$d,0,90);$path.AddArc($x,($y+$h-$d),$d,$d,90,90)
                    $path.CloseFigure();$graphics.FillPath($brush,$path)
                }finally{$path.Dispose()}
            }
        }finally{$brush.Dispose()}
        $small=New-Object Drawing.Bitmap $iconSize,$iconSize
        $drawing=[Drawing.Graphics]::FromImage($small)
        $stream=New-Object IO.MemoryStream
        try {
            $drawing.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
            $drawing.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $drawing.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $drawing.DrawImage($large,(New-Object Drawing.Rectangle 0,0,$iconSize,$iconSize))
            $small.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
            $iconImages+=,[byte[]]$stream.ToArray()
            if($iconSize -eq 256){$small.Save((Join-Path $iconFolder 'AppIcon.png'),[Drawing.Imaging.ImageFormat]::Png)}
        }finally{$stream.Dispose();$drawing.Dispose();$small.Dispose()}
    }finally{$graphics.Dispose();$large.Dispose()}
}
$file=[IO.File]::Create((Join-Path $iconFolder 'AppIcon.ico'))
$writer=New-Object IO.BinaryWriter $file
try {
    $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$iconSizes.Count)
    $offset=6+16*$iconSizes.Count
    for($i=0;$i -lt $iconSizes.Count;$i++) {
        $sizeByte=if($iconSizes[$i] -eq 256){0}else{$iconSizes[$i]}
        $writer.Write([byte]$sizeByte);$writer.Write([byte]$sizeByte);$writer.Write([byte]0);$writer.Write([byte]0)
        $writer.Write([uint16]1);$writer.Write([uint16]32);$writer.Write([uint32]$iconImages[$i].Length);$writer.Write([uint32]$offset)
        $offset+=$iconImages[$i].Length
    }
    foreach($bytes in $iconImages){$writer.Write([byte[]]$bytes)}
}finally{$writer.Dispose();$file.Dispose()}
Write-Output 'Created transparent icon resources: 16-256 px.'
