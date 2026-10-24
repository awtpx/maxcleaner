Add-Type -AssemblyName System.Drawing

function Create-MaxCleanerBitmap([int]$size) {
    $bmp = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

    $scale = [float]$size / 256.0

    # Background rounded container / shield
    $rect = [System.Drawing.RectangleF]::new(10.0 * $scale, 10.0 * $scale, 236.0 * $scale, 236.0 * $scale)
    
    # Outer dark modern gradient (dark navy to obsidian)
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $radius = 48.0 * $scale
    $d = $radius * 2.0
    $r = $rect.X; $t = $rect.Y; $w = $rect.Width; $h = $rect.Height
    $path.AddArc($r, $t, $d, $d, 180.0, 90.0)
    $path.AddArc($r + $w - $d, $t, $d, $d, 270.0, 90.0)
    $path.AddArc($r + $w - $d, $t + $h - $d, $d, $d, 0.0, 90.0)
    $path.AddArc($r, $t + $h - $d, $d, $d, 90.0, 90.0)
    $path.CloseFigure()

    $bgBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
        $rect,
        [System.Drawing.Color]::FromArgb(255, 14, 20, 36),
        [System.Drawing.Color]::FromArgb(255, 6, 8, 14),
        [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal
    )
    $g.FillPath($bgBrush, $path)

    # Neon Cyan/Electric Blue Glowing Border
    $borderPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(240, 0, 229, 255), [float](5.0 * $scale))
    $g.DrawPath($borderPen, $path)

    # Inner subtle glow
    $innerPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $inR = $r + (7.0 * $scale); $inT = $t + (7.0 * $scale); $inW = $w - (14.0 * $scale); $inH = $h - (14.0 * $scale)
    $inD = ($radius - 7.0 * $scale) * 2.0
    if ($inD -gt 0) {
        $innerPath.AddArc($inR, $inT, $inD, $inD, 180.0, 90.0)
        $innerPath.AddArc($inR + $inW - $inD, $inT, $inD, $inD, 270.0, 90.0)
        $innerPath.AddArc($inR + $inW - $inD, $inT + $inH - $inD, $inD, $inD, 0.0, 90.0)
        $innerPath.AddArc($inR, $inT + $inH - $inD, $inD, $inD, 90.0, 90.0)
        $innerPath.CloseFigure()
        $innerPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(70, 0, 229, 255), [float](2.0 * $scale))
        $g.DrawPath($innerPen, $innerPath)
        $innerPath.Dispose()
        $innerPen.Dispose()
    }

    # Center Logo: Stylized "M" with High-Velocity Sharp Geometric Cuts
    $mPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $mPts = [System.Drawing.PointF[]]@(
        [System.Drawing.PointF]::new(48.0 * $scale,  190.0 * $scale),
        [System.Drawing.PointF]::new(48.0 * $scale,  72.0 * $scale),
        [System.Drawing.PointF]::new(84.0 * $scale,  72.0 * $scale),
        [System.Drawing.PointF]::new(128.0 * $scale, 138.0 * $scale),
        [System.Drawing.PointF]::new(172.0 * $scale, 72.0 * $scale),
        [System.Drawing.PointF]::new(208.0 * $scale, 72.0 * $scale),
        [System.Drawing.PointF]::new(208.0 * $scale, 190.0 * $scale),
        [System.Drawing.PointF]::new(176.0 * $scale, 190.0 * $scale),
        [System.Drawing.PointF]::new(176.0 * $scale, 116.0 * $scale),
        [System.Drawing.PointF]::new(138.0 * $scale, 172.0 * $scale),
        [System.Drawing.PointF]::new(118.0 * $scale, 172.0 * $scale),
        [System.Drawing.PointF]::new(80.0 * $scale,  116.0 * $scale),
        [System.Drawing.PointF]::new(80.0 * $scale,  190.0 * $scale)
    )
    $mPath.AddPolygon($mPts)

    # Gradient for 'M': Pure Brilliant White to Cyan/Electric Sky Blue
    $mRect = [System.Drawing.RectangleF]::new(48.0*$scale, 72.0*$scale, 160.0*$scale, 122.0*$scale)
    $mBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
        $mRect,
        [System.Drawing.Color]::FromArgb(255, 255, 255, 255),
        [System.Drawing.Color]::FromArgb(255, 0, 215, 255),
        [System.Drawing.Drawing2D.LinearGradientMode]::Vertical
    )
    $g.FillPath($mBrush, $mPath)

    # Clean Spark / Star in upper right
    $sX = 186.0 * $scale
    $sY = 60.0 * $scale
    $sR = 18.0 * $scale
    $starPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $starPts = [System.Drawing.PointF[]]@(
        [System.Drawing.PointF]::new($sX, $sY - $sR),
        [System.Drawing.PointF]::new($sX + $sR*0.25, $sY - $sR*0.25),
        [System.Drawing.PointF]::new($sX + $sR, $sY),
        [System.Drawing.PointF]::new($sX + $sR*0.25, $sY + $sR*0.25),
        [System.Drawing.PointF]::new($sX, $sY + $sR),
        [System.Drawing.PointF]::new($sX - $sR*0.25, $sY + $sR*0.25),
        [System.Drawing.PointF]::new($sX - $sR, $sY),
        [System.Drawing.PointF]::new($sX - $sR*0.25, $sY - $sR*0.25)
    )
    $starPath.AddPolygon($starPts)
    $starBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 255, 235, 59))
    $g.FillPath($starBrush, $starPath)

    # Clean Speed Sweep Accent Arc across bottom
    $swooshPath = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $swooshPts = [System.Drawing.PointF[]]@(
        [System.Drawing.PointF]::new(36.0 * $scale, 204.0 * $scale),
        [System.Drawing.PointF]::new(128.0 * $scale, 214.0 * $scale),
        [System.Drawing.PointF]::new(220.0 * $scale, 196.0 * $scale)
    )
    $swooshPath.AddCurve($swooshPts)
    $swooshPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(230, 0, 255, 170), [float](4.0 * $scale))
    $g.DrawPath($swooshPen, $swooshPath)

    $g.Dispose()
    $path.Dispose()
    $bgBrush.Dispose()
    $borderPen.Dispose()
    $mPath.Dispose()
    $mBrush.Dispose()
    $starPath.Dispose()
    $starBrush.Dispose()
    $swooshPath.Dispose()
    $swooshPen.Dispose()

    return $bmp
}

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngBytesList = @()

foreach ($sz in $sizes) {
    $bmp = Create-MaxCleanerBitmap $sz
    $ms = [System.IO.MemoryStream]::new()
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBytesList += ,@($sz, $ms.ToArray())
    if ($sz -eq 256) {
        $bmp.Save(".\assets\logo_256.png", [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $bmp.Dispose()
    $ms.Dispose()
}

$fs = [System.IO.File]::Create(".\assets\app.ico")
$bw = [System.IO.BinaryWriter]::new($fs)

$bw.Write([uint16]0)
$bw.Write([uint16]1)
$bw.Write([uint16]$pngBytesList.Count)

$offset = 6 + (16 * $pngBytesList.Count)

foreach ($item in $pngBytesList) {
    $sz = $item[0]
    $data = $item[1]
    $wByte = if ($sz -ge 256) { [byte]0 } else { [byte]$sz }
    $hByte = if ($sz -ge 256) { [byte]0 } else { [byte]$sz }
    
    $bw.Write($wByte)
    $bw.Write($hByte)
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]$data.Length)
    $bw.Write([uint32]$offset)
    $offset += $data.Length
}

foreach ($item in $pngBytesList) {
    $bw.Write($item[1])
}

$bw.Flush()
$bw.Close()
$fs.Close()
Write-Host "app.ico generated successfully without errors!"
