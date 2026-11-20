# Draws the Chat Messages Quick Access icons (64x64 PNG, transparent): icon.png + icon_hover.png.
# A scroll: a parchment sheet with three text lines between two rolls. Palette matches
# Nexus/Raidcore icons (normal ~#D0D0A0 parchment, hover ~#F0F0E0 cream), same as GW2_WvW_Info.
# Other concepts that were considered: docs/icon-suggestions/.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\draw_chat_icon.ps1 -OutDir resources\icons
param([string]$OutDir)
Add-Type -AssemblyName System.Drawing

function New-RoundedRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function Draw-Roll($g, [float]$y, $fill, $pen, $ink) {
    $roll = New-RoundedRect 7 $y 50 11 5.5
    $g.FillPath($fill, $roll)
    $g.DrawPath($pen, $roll)
    # End caps: short vertical marks so the rolls read as cylinders.
    $cap = New-Object System.Drawing.Pen($ink, 1.8)
    $g.DrawLine($cap, 13, ($y + 2.5), 13, ($y + 8.5))
    $g.DrawLine($cap, 51, ($y + 2.5), 51, ($y + 8.5))
}

function Draw-Icon([string]$path, [System.Drawing.Color]$top, [System.Drawing.Color]$bottom, [System.Drawing.Color]$rollColor, [System.Drawing.Color]$ink) {
    $bmp = New-Object System.Drawing.Bitmap(64, 64, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $pen = New-Object System.Drawing.Pen($ink, 2.2)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # Sheet between the rolls.
    $sheet = New-Object System.Drawing.RectangleF(14, 11, 36, 42)
    $fill = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.PointF(0, 11)), (New-Object System.Drawing.PointF(0, 53)), $top, $bottom)
    $g.FillRectangle($fill, $sheet)
    $g.DrawRectangle($pen, 14, 11, 36, 42)

    # Rolls on top of the sheet's ends.
    $rollBrush = New-Object System.Drawing.SolidBrush($rollColor)
    Draw-Roll $g 4 $rollBrush $pen $ink
    Draw-Roll $g 49 $rollBrush $pen $ink

    # Three "text" lines, the last one shorter.
    $line = New-Object System.Drawing.Pen($ink, 3.2)
    $line.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $line.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($line, 21, 23, 43, 23)
    $g.DrawLine($line, 21, 31, 43, 31)
    $g.DrawLine($line, 21, 39, 35, 39)

    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$ink = [System.Drawing.Color]::FromArgb(255, 0x1E, 0x1E, 0x1A)
# Normal: muted parchment gold, rolls a shade darker.
Draw-Icon (Join-Path $OutDir "icon.png") `
    ([System.Drawing.Color]::FromArgb(255, 0xE2, 0xE0, 0xB4)) ([System.Drawing.Color]::FromArgb(255, 0xC8, 0xC6, 0x9C)) `
    ([System.Drawing.Color]::FromArgb(255, 0xB4, 0xB2, 0x8C)) $ink
# Hover: bright cream.
Draw-Icon (Join-Path $OutDir "icon_hover.png") `
    ([System.Drawing.Color]::FromArgb(255, 0xFC, 0xFC, 0xF4)) ([System.Drawing.Color]::FromArgb(255, 0xEC, 0xEC, 0xDE)) `
    ([System.Drawing.Color]::FromArgb(255, 0xE2, 0xE2, 0xD4)) $ink
"done"
