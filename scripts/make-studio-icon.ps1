# IPKG Studio 앱 아이콘 생성기 (Windows / System.Drawing).
# 사용: powershell -ExecutionPolicy Bypass -File scripts\make-studio-icon.ps1
# 결과: repository/apps-src/com.illiertion.ipkgstudio/metadata/icon.png (+ preview.png)

Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root "repository\apps-src\com.illiertion.ipkgstudio\metadata"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$size = 256
$bitmap = New-Object System.Drawing.Bitmap $size, $size
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([System.Drawing.Color]::FromArgb(255, 9, 12, 18))

$plate = New-Object System.Drawing.Drawing2D.GraphicsPath
$plate.AddArc(6, 6, 60, 60, 180, 90)
$plate.AddArc(190, 6, 60, 60, 270, 90)
$plate.AddArc(190, 190, 60, 60, 0, 90)
$plate.AddArc(6, 190, 60, 60, 90, 90)
$plate.CloseFigure()
$plateBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Rectangle 0, 0, $size, $size),
    [System.Drawing.Color]::FromArgb(255, 24, 34, 58),
    [System.Drawing.Color]::FromArgb(255, 7, 10, 16),
    90.0)
$graphics.FillPath($plateBrush, $plate)
$platePen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(120, 61, 125, 240)), 3
$graphics.DrawPath($platePen, $plate)

function New-Polygon([System.Drawing.Point[]]$points, [System.Drawing.Color]$color) {
    $brush = New-Object System.Drawing.SolidBrush $color
    $graphics.FillPolygon($brush, $points)
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(210, 240, 246, 255)), 2.5
    $graphics.DrawPolygon($pen, $points)
}

# 등각 상자(패키지) — 윗면 → 왼면 → 오른면
$top = [System.Drawing.Point[]]@(
    (New-Object System.Drawing.Point 128, 56),
    (New-Object System.Drawing.Point 198, 94),
    (New-Object System.Drawing.Point 128, 132),
    (New-Object System.Drawing.Point 58, 94)
)
$left = [System.Drawing.Point[]]@(
    (New-Object System.Drawing.Point 58, 94),
    (New-Object System.Drawing.Point 128, 132),
    (New-Object System.Drawing.Point 128, 202),
    (New-Object System.Drawing.Point 58, 164)
)
$right = [System.Drawing.Point[]]@(
    (New-Object System.Drawing.Point 198, 94),
    (New-Object System.Drawing.Point 128, 132),
    (New-Object System.Drawing.Point 128, 202),
    (New-Object System.Drawing.Point 198, 164)
)

New-Polygon $left ([System.Drawing.Color]::FromArgb(255, 34, 74, 156))
New-Polygon $right ([System.Drawing.Color]::FromArgb(255, 26, 56, 120))
New-Polygon $top ([System.Drawing.Color]::FromArgb(255, 74, 134, 245))

# 접힌 띠
$strip = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 238, 244, 255))
$graphics.FillRectangle($strip, 118, 96, 20, 84)

$graphics.Dispose()
$path = Join-Path $outDir "icon.png"
$bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Save((Join-Path $outDir "preview.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
Write-Output ("wrote {0}" -f $path)
