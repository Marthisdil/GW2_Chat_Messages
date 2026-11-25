# Draws the Chat Messages Quick Access icons (64x64 PNG, transparent): icon.png + icon_hover.png.
# A quick bubble: a parchment speech bubble with a lightning bolt in it (a chat message sent in
# one press; chosen 2026-11-25, replacing the scroll). Palette matches Nexus/Raidcore icons
# (normal ~#D0D0A0 parchment, hover ~#F0F0E0 cream), same as GW2_WvW_Info.
# Other concepts that were considered: docs/icon-suggestions/.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\draw_chat_icon.ps1 -OutDir resources\icons
param([string]$OutDir)
Add-Type -AssemblyName System.Drawing

# The bubble: a rounded rectangle (8,10)-(56,44), corner radius 8, with a tail down to (20,54).
function New-Bubble {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $p.AddArc(8, 10, 16, 16, 180, 90)
    $p.AddArc(40, 10, 16, 16, 270, 90)
    $p.AddArc(40, 28, 16, 16, 0, 90)
    $p.AddLine(48, 44, 30, 44)
    $p.AddLine(30, 44, 20, 54)
    $p.AddLine(20, 54, 20, 44)
    $p.AddArc(8, 28, 16, 16, 90, 90)
    $p.CloseFigure()
    return $p
}

function Draw-Icon([string]$path, [System.Drawing.Color]$top, [System.Drawing.Color]$bottom, [System.Drawing.Color]$ink) {
    $bmp = New-Object System.Drawing.Bitmap(64, 64, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $pen = New-Object System.Drawing.Pen($ink, 2.4)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    $bubble = New-Bubble
    $fill = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.PointF(0, 10)), (New-Object System.Drawing.PointF(0, 54)), $top, $bottom)
    $g.FillPath($fill, $bubble)
    $g.DrawPath($pen, $bubble)

    # The lightning bolt, solid ink.
    [System.Drawing.PointF[]]$bolt = @(
        (New-Object System.Drawing.PointF(35, 14)), (New-Object System.Drawing.PointF(23, 30)),
        (New-Object System.Drawing.PointF(31, 30)), (New-Object System.Drawing.PointF(27, 41)),
        (New-Object System.Drawing.PointF(41, 24)), (New-Object System.Drawing.PointF(33, 24)))
    $inkBrush = New-Object System.Drawing.SolidBrush($ink)
    $g.FillPolygon($inkBrush, $bolt)

    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$ink = [System.Drawing.Color]::FromArgb(255, 0x1E, 0x1E, 0x1A)
# Normal: muted parchment gold.
Draw-Icon (Join-Path $OutDir "icon.png") `
    ([System.Drawing.Color]::FromArgb(255, 0xE2, 0xE0, 0xB4)) ([System.Drawing.Color]::FromArgb(255, 0xC8, 0xC6, 0x9C)) $ink
# Hover: bright cream.
Draw-Icon (Join-Path $OutDir "icon_hover.png") `
    ([System.Drawing.Color]::FromArgb(255, 0xFC, 0xFC, 0xF4)) ([System.Drawing.Color]::FromArgb(255, 0xEC, 0xEC, 0xDE)) $ink
"done"
