#Requires -Version 5.1
# Spider web like qml/components/SpiderWebMark.qml + monochrome pills

Add-Type -AssemblyName System.Drawing
$skin = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

function New-RoundRect([int]$x, [int]$y, [int]$w, [int]$h, [int]$r) {
    $gp = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = [Math]::Min($r * 2, [Math]::Min($w, $h))
    $gp.AddArc($x, $y, $d, $d, 180, 90)
    $gp.AddArc(($x + $w - $d), $y, $d, $d, 270, 90)
    $gp.AddArc(($x + $w - $d), ($y + $h - $d), $d, $d, 0, 90)
    $gp.AddArc($x, ($y + $h - $d), $d, $d, 90, 90)
    $gp.CloseFigure()
    return $gp
}

function Save-Pill([string]$outFile, [bool]$filled, [int]$bw, [int]$bh) {
    $bmp = New-Object System.Drawing.Bitmap $bw, ($bh * 4)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    $rad = [int]($bh / 2)
    for ($i = 0; $i -lt 4; $i++) {
        $yy = $i * $bh
        $gp = New-RoundRect 1 ($yy + 1) ($bw - 3) ($bh - 3) ($rad - 1)
        if ($filled) {
            $cols = @(
                [System.Drawing.Color]::FromArgb(255, 255, 255, 255),
                [System.Drawing.Color]::FromArgb(255, 235, 235, 235),
                [System.Drawing.Color]::FromArgb(255, 210, 210, 210),
                [System.Drawing.Color]::FromArgb(255, 70, 70, 74)
            )
            $g.FillPath((New-Object System.Drawing.SolidBrush $cols[$i]), $gp)
        } else {
            $bcols = @(
                [System.Drawing.Color]::FromArgb(255, 200, 200, 205),
                [System.Drawing.Color]::FromArgb(255, 255, 255, 255),
                [System.Drawing.Color]::FromArgb(255, 255, 255, 255),
                [System.Drawing.Color]::FromArgb(255, 80, 80, 84)
            )
            $g.DrawPath((New-Object System.Drawing.Pen $bcols[$i], 1.5), $gp)
        }
        $gp.Dispose()
    }
    $bmp.Save($outFile, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

function Save-CircleBrowse([string]$outFile, [int]$size = 48) {
    $bmp = New-Object System.Drawing.Bitmap $size, ($size * 4)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    for ($i = 0; $i -lt 4; $i++) {
        $yy = $i * $size
        $penC = switch ($i) {
            0 { [System.Drawing.Color]::FromArgb(255, 180, 180, 185) }
            1 { [System.Drawing.Color]::FromArgb(255, 255, 255, 255) }
            2 { [System.Drawing.Color]::FromArgb(255, 255, 255, 255) }
            default { [System.Drawing.Color]::FromArgb(255, 80, 80, 84) }
        }
        $g.DrawEllipse((New-Object System.Drawing.Pen $penC, 1.5), 3, ($yy + 3), ($size - 8), ($size - 8))
        $fx = 14; $fy = $yy + 17
        $g.DrawRectangle((New-Object System.Drawing.Pen $penC, 1.5), $fx, ($fy + 4), 18, 13)
        $g.DrawLine((New-Object System.Drawing.Pen $penC, 1.5), $fx, ($fy + 4), ($fx + 5), $fy)
        $g.DrawLine((New-Object System.Drawing.Pen $penC, 1.5), ($fx + 5), $fy, ($fx + 11), ($fy + 4))
    }
    $bmp.Save($outFile, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

function Save-TextGhost([string]$outFile) {
    # fully transparent strip - Botva2 draws caption only (Back as text button)
    $bw = 100; $bh = 36
    $bmp = New-Object System.Drawing.Bitmap $bw, ($bh * 4)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    $bmp.Save($outFile, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

# Background 560x460 - web like SpiderWebMark: polygonal rings, right-side, ~6% opacity
$w = 560; $h = 460
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.Clear([System.Drawing.Color]::FromArgb(255, 18, 18, 20))

# primary-ish monochrome gray at ~6% opacity (matches QML opacity 0.06)
$webColor = [System.Drawing.Color]::FromArgb(15, 200, 200, 205)
$pen = New-Object System.Drawing.Pen $webColor, 2.5
$pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
$pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

# QML: web 300x300, verticalCenter, right + margin -120 → center ~ right side
$cx = 430
$cy = 230
$maxR = 138
$spokes = 8
$rings = 4

for ($s = 0; $s -lt $spokes; $s++) {
    $a = ($s / [double]$spokes) * [Math]::PI * 2 - [Math]::PI / 2
    $g.DrawLine($pen, $cx, $cy, [int]($cx + [Math]::Cos($a) * $maxR), [int]($cy + [Math]::Sin($a) * $maxR))
}

for ($r = 1; $r -le $rings; $r++) {
    $rad = $maxR * ($r / [double]$rings)
    $pts = New-Object System.Drawing.PointF[] ($spokes + 1)
    for ($s = 0; $s -le $spokes; $s++) {
        $a = ($s / [double]$spokes) * [Math]::PI * 2 - [Math]::PI / 2
        $pts[$s] = New-Object System.Drawing.PointF ([float]($cx + [Math]::Cos($a) * $rad), [float]($cy + [Math]::Sin($a) * $rad))
    }
    $g.DrawPolygon($pen, $pts)
}

# center dot
$dotR = 4
$g.FillEllipse((New-Object System.Drawing.SolidBrush $webColor), ($cx - $dotR), ($cy - $dotR), ($dotR * 2), ($dotR * 2))
$pen.Dispose()

$bmp.Save((Join-Path $skin 'background.bmp'), [System.Drawing.Imaging.ImageFormat]::Bmp)
$g.Dispose(); $bmp.Dispose()

# Path field outline plate
$pb = New-Object System.Drawing.Bitmap 420, 44
$pg = [System.Drawing.Graphics]::FromImage($pb)
$pg.SmoothingMode = 'AntiAlias'
$pg.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$rr = New-RoundRect 1 1 417 41 8
$pg.DrawPath((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 180, 180, 185), 1.5)), $rr)
$rr.Dispose()
$pb.Save((Join-Path $skin 'path-frame.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$pg.Dispose(); $pb.Dispose()

Save-Pill (Join-Path $skin 'pill-filled-wide.png') $true 480 48
Save-Pill (Join-Path $skin 'pill-outline-wide.png') $false 480 48
Save-Pill (Join-Path $skin 'pill-filled.png') $true 128 40
Save-Pill (Join-Path $skin 'pill-outline.png') $false 100 36
Save-TextGhost (Join-Path $skin 'pill-ghost.png')
Save-CircleBrowse (Join-Path $skin 'btn-browse.png') 48

Write-Host "OK $skin"
Get-ChildItem $skin -Include *.bmp,*.png | Select-Object Name, Length
