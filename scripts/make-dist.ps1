# Stage a distributable NAPP build: launcher + Qt deployment + web runtime,
# then produce the setup executable and the portable ZIP from one payload.
#
# Output (dist/):
#   NAPP-<version>-Setup.exe   single-file installer (Install or Portable)
#   NAPP-<version>-portable.zip
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\make-dist.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\make-dist.ps1 -SkipBuild
#   scripts\build-cef.bat --dist        (build, then package)

param(
    [string]$BuildDirectory = 'build\cef',
    [string]$QtBin = 'C:\Qt\6.12.0\msvc2022_64\bin',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$BuildPath = Join-Path $Root $BuildDirectory
$RuntimeSource = Join-Path $BuildPath 'runtime-stage'

$cmake = Get-Content (Join-Path $Root 'CMakeLists.txt') -Raw
if ($cmake -notmatch 'project\(NAPP\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Could not read the project version from CMakeLists.txt'
}
$Version = $Matches[1]

if (-not $SkipBuild) {
    Write-Host "Building NAPP $Version (Release + CEF)..."
    Push-Location $Root
    try {
        & cmd.exe /c "scripts\build-cef.bat"
        if ($LASTEXITCODE -ne 0) { throw "build-cef.bat failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}

$launcher = Join-Path $BuildPath 'NAPP.exe'
if (-not (Test-Path $launcher)) { throw "Launcher not found: $launcher (run scripts\build-cef.bat first)" }
if (-not (Test-Path (Join-Path $RuntimeSource 'runtime.json'))) {
    throw "Runtime stage not found: $RuntimeSource (build with -DNAPP_ENABLE_CEF=ON)"
}

$windeployqt = Join-Path $QtBin 'windeployqt.exe'
if (-not (Test-Path $windeployqt)) {
    $command = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($command) { $windeployqt = $command.Source } else { throw "windeployqt not found (looked in $QtBin)" }
}

$dist = Join-Path $Root 'dist'
$packageRoot = Join-Path $dist "NAPP-$Version"
$payload = Join-Path $packageRoot 'payload'

if (Test-Path $packageRoot) { Remove-Item $packageRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $payload | Out-Null

Write-Host "Staging payload in $payload"
Copy-Item $launcher (Join-Path $payload 'NAPP.exe') -Force
Set-Content (Join-Path $payload 'version.txt') $Version -Encoding UTF8

Write-Host 'Deploying Qt runtime...'
& $windeployqt --release --no-translations --no-compiler-runtime `
    --qmldir (Join-Path $Root 'launcher\qml') (Join-Path $payload 'NAPP.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

Write-Host 'Copying the web runtime...'
# Read the version from the staged runtime so it can never drift from runtime.json.
$runtimeVersion = (Get-Content (Join-Path $RuntimeSource 'runtime.json') -Raw | ConvertFrom-Json).version
if (-not $runtimeVersion) { throw "runtime.json has no version" }
$runtimeTarget = Join-Path $payload "runtime\$runtimeVersion"
New-Item -ItemType Directory -Force -Path $runtimeTarget | Out-Null
Copy-Item (Join-Path $RuntimeSource '*') $runtimeTarget -Recurse -Force

# Ship the repository snapshot and the docs next to the app so a fresh install
# has a working store even when the GitHub raw URL is unreachable.
$docs = Join-Path $Root 'repository\docs'
if (Test-Path $docs) {
    Copy-Item $docs (Join-Path $payload 'docs') -Recurse -Force
}

Write-Host 'Adding setup scripts...'
Copy-Item (Join-Path $Root 'packaging\NAPP-Setup.ps1') $packageRoot -Force
Copy-Item (Join-Path $Root 'packaging\NAPP-Setup.cmd') $packageRoot -Force
Copy-Item (Join-Path $Root 'packaging\Uninstall.ps1') $packageRoot -Force
# The payload carries the uninstaller, so both the installer and the portable
# copy can remove themselves without the setup folder.
Copy-Item (Join-Path $Root 'packaging\Uninstall.ps1') $payload -Force

$portableZip = Join-Path $dist "NAPP-$Version-portable.zip"
if (Test-Path $portableZip) { Remove-Item $portableZip -Force }
Write-Host 'Creating the portable archive...'
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $portableZip -CompressionLevel Optimal

# --------------------------------------------------------------------------
# Setup executable: compile the bootstrapper, then append the payload ZIP and
# a 32-byte trailer (offset + version + magic) so the .exe is the whole installer.

Add-Type -AssemblyName System.IO.Compression.FileSystem

$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path $csc)) { $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe' }
if (-not (Test-Path $csc)) { throw 'csc.exe (the .NET Framework C# compiler) was not found.' }

$installerSource = Join-Path $Root 'packaging\Installer.cs'
$installerIcon = Join-Path $Root 'launcher\assets\napp.ico'
$setupExe = Join-Path $dist "NAPP-$Version-Setup.exe"
if (Test-Path $setupExe) { Remove-Item $setupExe -Force }

Write-Host 'Compiling the setup executable...'
Push-Location (Join-Path $Root 'packaging')
try {
    & $csc -nologo -target:winexe -optimize+ -platform:anycpu `
        "-win32icon:$installerIcon" "-out:$setupExe" `
        -r:System.Windows.Forms.dll -r:System.Drawing.dll `
        -r:System.IO.Compression.dll -r:System.IO.Compression.FileSystem.dll `
        'Installer.cs'
    if ($LASTEXITCODE -ne 0) { throw "csc.exe failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}

$payloadZip = Join-Path $dist "NAPP-$Version-payload.zip"
if (Test-Path $payloadZip) { Remove-Item $payloadZip -Force }
Write-Host 'Packing the setup payload...'
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $payload, $payloadZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

Write-Host 'Attaching the payload to the setup executable...'
$offset = (Get-Item $setupExe).Length
$output = [System.IO.File]::Open($setupExe, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write)
try {
    $input = [System.IO.File]::OpenRead($payloadZip)
    try { $input.CopyTo($output) } finally { $input.Dispose() }
    $writer = New-Object System.IO.BinaryWriter($output)
    $writer.Write([int64]$offset)
    # 16-byte version field: the installer shows it before unpacking anything.
    $versionBytes = New-Object byte[] 16
    $encoded = [System.Text.Encoding]::ASCII.GetBytes($Version)
    [Array]::Copy($encoded, $versionBytes, [Math]::Min($encoded.Length, 16))
    $writer.Write($versionBytes)
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes('NAPPPAY1'))
    $writer.Flush()
} finally {
    $output.Dispose()
}
Remove-Item $payloadZip -Force

$setupHash = (Get-FileHash $setupExe -Algorithm SHA256).Hash
Set-Content "$setupExe.sha256" "$setupHash  $(Split-Path -Leaf $setupExe)" -Encoding ASCII

Write-Host ''
Write-Host "Distribution ready in $packageRoot"
Write-Host "  setup     $setupExe"
Write-Host "  portable  $portableZip"
Write-Host "  sha256    $setupHash"
