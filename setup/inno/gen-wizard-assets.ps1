#Requires -Version 5.1
# Regenerates setup/inno/assets/wizard-*.bmp from resources/icons/png/256.png

Add-Type -AssemblyName System.Drawing

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$outDir = Join-Path $PSScriptRoot "assets"
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

$logo = [System.Drawing.Image]::FromFile((Join-Path $root "resources\icons\png\256.png"))

function Draw-Web(
    [System.Drawing.Graphics]$g,
    [int]$cx,
    [int]$cy,
    [int]$maxR,
    [System.Drawing.Color]$color
) {
    $pen = New-Object System.Drawing.Pen $color, 1.6
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $spokes = 8
    $rings = 4
    for ($s = 0; $s -lt $spokes; $s++) {
        $a = [Math]::PI * 2 * $s / $spokes - [Math]::PI / 2
        $g.DrawLine($pen, $cx, $cy,
            [int]($cx + [Math]::Cos($a) * $maxR),
            [int]($cy + [Math]::Sin($a) * $maxR))
    }
    for ($r = 1; $r -le $rings; $r++) {
        $rr = [int]($maxR * $r / $rings)
        $g.DrawEllipse($pen, $cx - $rr, $cy - $rr, $rr * 2, $rr * 2)
    }
    $pen.Dispose()
}

$surface = [System.Drawing.Color]::FromArgb(255, 28, 28, 30)
$web = [System.Drawing.Color]::FromArgb(28, 142, 142, 147)
$onSurface = [System.Drawing.Color]::FromArgb(255, 230, 230, 235)
$muted = [System.Drawing.Color]::FromArgb(255, 142, 142, 147)

$bmp = New-Object System.Drawing.Bitmap 360, 720
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.Clear($surface)
Draw-Web $g 280 360 220 $web
$g.DrawImage($logo, 110, 200, 140, 140)
$sf = New-Object System.Drawing.StringFormat
$sf.Alignment = [System.Drawing.StringAlignment]::Center
$font = New-Object System.Drawing.Font "Segoe UI Semibold", 26
$brush = New-Object System.Drawing.SolidBrush $onSurface
$g.DrawString("Arachnel", $font, $brush, (New-Object System.Drawing.RectangleF 0, 360, 360, 48), $sf)
$sub = New-Object System.Drawing.Font "Segoe UI", 12
$brush2 = New-Object System.Drawing.SolidBrush $muted
$g.DrawString("Setup", $sub, $brush2, (New-Object System.Drawing.RectangleF 0, 408, 360, 28), $sf)
$bmp.Save((Join-Path $outDir "wizard-image.bmp"), [System.Drawing.Imaging.ImageFormat]::Bmp)
$g.Dispose(); $bmp.Dispose(); $font.Dispose(); $sub.Dispose(); $brush.Dispose(); $brush2.Dispose(); $sf.Dispose()

$sb = New-Object System.Drawing.Bitmap 128, 128
$sg = [System.Drawing.Graphics]::FromImage($sb)
$sg.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$sg.Clear($surface)
Draw-Web $sg 64 64 58 ([System.Drawing.Color]::FromArgb(50, 142, 142, 147))
$sg.DrawImage($logo, 24, 24, 80, 80)
$sb.Save((Join-Path $outDir "wizard-small.bmp"), [System.Drawing.Imaging.ImageFormat]::Bmp)
$sg.Dispose(); $sb.Dispose(); $logo.Dispose()

Write-Host "Wrote $outDir\wizard-image.bmp and wizard-small.bmp"
