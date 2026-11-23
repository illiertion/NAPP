# NAPP uninstaller (copied into the install folder by the setup).
# Removes shortcuts, the Apps & features entry and the program files. App data
# is only removed when the user asks for it.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms

$InstallDir = $PSScriptRoot
$RecordPath = Join-Path $InstallDir 'install.json'
$Record = @{}
if (Test-Path $RecordPath) {
    $json = Get-Content $RecordPath -Raw | ConvertFrom-Json
    foreach ($property in $json.PSObject.Properties) { $Record[$property.Name] = $property.Value }
}
$Mode = if ($Record['mode']) { $Record['mode'] } else { 'install' }

$answer = [System.Windows.Forms.MessageBox]::Show(
    "Remove NAPP from this computer?`n`nProgram files: $InstallDir",
    'NAPP Uninstall', 'YesNo', 'Question')
if ($answer -ne 'Yes') { exit 0 }

$dataQuestion = [System.Windows.Forms.MessageBox]::Show(
    "Delete app settings, installed apps and runtimes too?`n`n%LOCALAPPDATA%\illiertion\napp`n`nChoose No to keep them for a later reinstall.",
    'NAPP Uninstall', 'YesNo', 'Warning')

if ($Mode -eq 'install') {
    Remove-Item (Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\NAPP.lnk') -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path ([Environment]::GetFolderPath('Desktop')) 'NAPP.lnk') -Force -ErrorAction SilentlyContinue
    Remove-Item 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\NAPP' -Recurse -Force -ErrorAction SilentlyContinue
    # Remove the .ipkg association only if it still points at this installation.
    $command = (Get-ItemProperty 'HKCU:\Software\Classes\NAPP.ipkg\shell\open\command' -ErrorAction SilentlyContinue).'(default)'
    if ($command -and $command -like "*$InstallDir*") {
        Remove-Item 'HKCU:\Software\Classes\.ipkg' -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item 'HKCU:\Software\Classes\NAPP.ipkg' -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($dataQuestion -eq 'Yes') {
    $dataRoot = Join-Path $env:LOCALAPPDATA 'illiertion\napp'
    if (Test-Path $dataRoot) { Remove-Item $dataRoot -Recurse -Force -ErrorAction SilentlyContinue }
}

# The running script lives inside the folder being deleted, so hand the removal
# to a detached command that waits for this process to exit.
$cleanup = Join-Path $env:TEMP ("napp-uninstall-{0}.cmd" -f [guid]::NewGuid().ToString('N'))
@"
@echo off
timeout /t 2 /nobreak >nul
rmdir /s /q "$InstallDir"
del "%~f0"
"@ | Set-Content $cleanup -Encoding ASCII

Start-Process -FilePath $cleanup -WindowStyle Hidden
[System.Windows.Forms.MessageBox]::Show('NAPP has been removed.', 'NAPP Uninstall', 'OK', 'Information') | Out-Null
exit 0
