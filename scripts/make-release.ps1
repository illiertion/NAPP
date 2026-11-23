# Gather everything that goes to GitHub:
#   release/repository/  the static store tree (raw.githubusercontent.com)
#   release/apps/        the built .ipkg files, by app and version
#
# The launcher distribution (dist/) is a build artifact and is deliberately not
# copied here; attach the portable ZIP to a GitHub release instead.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\make-release.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\make-release.ps1 -SkipBuild

param(
    [string]$BaseUrl = 'https://raw.githubusercontent.com/ILLIERTION/NAPP-Repository/main/',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Repository = Join-Path $Root 'repository'
$Release = Join-Path $Root 'release'

if (-not $SkipBuild) {
    Write-Host 'Rebuilding packages and the repository index...'
    & python (Join-Path $Repository 'scripts\build_repository.py') --base-url $BaseUrl
    if ($LASTEXITCODE -ne 0) { throw "build_repository.py failed with exit code $LASTEXITCODE" }
}

if (-not (Test-Path (Join-Path $Repository 'repository.json'))) {
    throw "repository.json is missing; run repository\scripts\build_repository.py first"
}

if (Test-Path $Release) { Remove-Item $Release -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Release | Out-Null

# ---- repository tree (upload the contents of release/repository to GitHub)
$repositoryTarget = Join-Path $Release 'repository'
New-Item -ItemType Directory -Force -Path $repositoryTarget | Out-Null
$entries = @(
    'repository.json',
    'repository-meta.json',
    'README.md',
    'apps',
    'packages',
    'icons',
    'categories',
    'docs',
    'apps-src',
    'scripts',
    '.github'
)
foreach ($entry in $entries) {
    $source = Join-Path $Repository $entry
    if (Test-Path $source) {
        Copy-Item $source (Join-Path $repositoryTarget $entry) -Recurse -Force
    }
}

# ---- packaged apps, easy to grab without walking packages/
$appsTarget = Join-Path $Release 'apps'
New-Item -ItemType Directory -Force -Path $appsTarget | Out-Null
$packages = Join-Path $Repository 'packages'
if (Test-Path $packages) {
    Get-ChildItem $packages -Directory | ForEach-Object {
        $appFolder = Join-Path $appsTarget $_.Name
        New-Item -ItemType Directory -Force -Path $appFolder | Out-Null
        Copy-Item (Join-Path $_.FullName '*.ipkg') $appFolder -Force
    }
}

$count = (Get-ChildItem $appsTarget -Recurse -Filter '*.ipkg' | Measure-Object).Count
$portable = Get-ChildItem (Join-Path $Root 'dist') -Filter '*-portable.zip' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$setup = Get-ChildItem (Join-Path $Root 'dist') -Filter '*-Setup.exe' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1

Write-Host "release\repository  ready for GitHub (base: $BaseUrl)"
Write-Host "release\apps        $count package(s)"
if ($setup) {
    Write-Host "installer            $($setup.FullName)"
} else {
    Write-Host 'installer            not built (run scripts\make-dist.ps1)'
}
if ($portable) {
    Write-Host "portable archive     $($portable.FullName)"
} else {
    Write-Host 'portable archive     not built (run scripts\make-dist.ps1)'
}
Write-Host ''
Write-Host 'Upload release\repository to the repository branch, and attach release\apps/*.ipkg,'
Write-Host 'dist\NAPP-*-Setup.exe and dist\NAPP-*-portable.zip to a GitHub release.'
