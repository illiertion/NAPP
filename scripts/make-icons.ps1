# NAPP 런처 아이콘 생성기 (Windows / System.Drawing).
# 사용: powershell -ExecutionPolicy Bypass -File scripts\make-icons.ps1
# 결과: launcher/assets/napp-icon-{16,32,64,128,256}.png
#        (napp.ico 는 scripts/pack-ico.py 가 같은 PNG 로 만든다)

Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root "launcher\assets"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$accent = [System.Drawing.Color]::FromArgb(255, 61, 125, 240)

function New-RoundedPath([System.Drawing.Rectangle]$rect, [int]$radius) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $radius * 2
    $path.AddArc($rect.X, $rect.Y, $d, $d, 180, 90)
    $path.AddArc($rect.Right - $d, $rect.Y, $d, $d, 270, 90)
    $path.AddArc($rect.Right - $d, $rect.Bottom - $d, $d, $d, 0, 90)
    $path.AddArc($rect.X, $rect.Bottom - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

function New-NappIcon([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    # 판(plate): 어두운 그라데이션 + 둥근 모서리
    $pad = [Math]::Max(1, [int]($size * 0.03))
    $rect = New-Object System.Drawing.Rectangle $pad, $pad, ($size - 2 * $pad), ($size - 2 * $pad)
    $radius = [Math]::Max(3, [int]($size * 0.235))
    $path = New-RoundedPath $rect $radius

    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect,
        [System.Drawing.Color]::FromArgb(255, 24, 34, 58),
        [System.Drawing.Color]::FromArgb(255, 7, 10, 16),
        90.0)
    $g.FillPath($brush, $path)

    $edge = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(110, 61, 125, 240)), ([Math]::Max(1.0, $size * 0.014))
    $g.DrawPath($edge, $path)

    # 모노그램 N
    $fontSize = $size * 0.58
    $font = New-Object System.Drawing.Font(
        "Segoe UI",
        $fontSize,
        [System.Drawing.FontStyle]::Bold,
        [System.Drawing.GraphicsUnit]::Pixel)
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = [System.Drawing.StringAlignment]::Center
    $format.LineAlignment = [System.Drawing.StringAlignment]::Center
    $white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(254, 245, 249, 255))
    $textRect = New-Object System.Drawing.RectangleF 0, (-$size * 0.07), $size, $size
    $g.DrawString("N", $font, $white, $textRect, $format)

    # 액센트 바
    if ($size -ge 24) {
        $barWidth = $size * 0.30
        $barHeight = [Math]::Max(1.5, $size * 0.055)
        $barX = ($size - $barWidth) / 2
        $barY = $size * 0.765
        $accentBrush = New-Object System.Drawing.SolidBrush $accent
        $g.FillRectangle($accentBrush, [single]$barX, [single]$barY, [single]$barWidth, [single]$barHeight)
    }

    $g.Dispose()
    $path.Dispose()
    return $bmp
}

foreach ($size in 16, 32, 64, 128, 256) {
    $bitmap = New-NappIcon $size
    $target = Join-Path $outDir ("napp-icon-{0}.png" -f $size)
    $bitmap.Save($target, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    Write-Output ("wrote {0}" -f $target)
}
