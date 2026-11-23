# NAPP setup — choose Install or Portable.
#
# Install : copies the payload to a folder, adds Start Menu / desktop shortcuts
#           and an entry in Apps & features. Per-user, no administrator needed.
# Portable: copies the payload only; nothing is written to the registry.
#
# Usage:
#   NAPP-Setup.cmd                      (GUI)
#   powershell -File NAPP-Setup.ps1 -Mode Install -Target "C:\Apps\NAPP" -Shortcuts
#   powershell -File NAPP-Setup.ps1 -Mode Portable -Target "D:\NAPP"

param(
    [ValidateSet('Gui', 'Install', 'Portable')]
    [string]$Mode = 'Gui',
    [string]$Target = '',
    [switch]$Shortcuts,
    [switch]$LaunchAfter
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$SetupRoot = $PSScriptRoot
$Payload = Join-Path $SetupRoot 'payload'
$VersionFile = Join-Path $Payload 'version.txt'
$Version = if (Test-Path $VersionFile) { (Get-Content $VersionFile -Raw).Trim() } else { '0.1.0' }

function Get-Payload {
    if (-not (Test-Path (Join-Path $Payload 'NAPP.exe'))) {
        throw "Setup payload is missing. Expected $Payload\NAPP.exe. Run scripts\make-dist.ps1 first."
    }
    return $Payload
}

function Copy-Payload([string]$Destination, [string]$RunMode) {
    $source = Get-Payload
    if (Test-Path $Destination) {
        $existing = Get-ChildItem -LiteralPath $Destination -Force -ErrorAction SilentlyContinue
        if ($existing) {
            $answer = [System.Windows.Forms.MessageBox]::Show(
                "The folder already exists and is not empty:`n$Destination`n`nReplace its contents?",
                'NAPP Setup', 'YesNo', 'Warning')
            if ($answer -ne 'Yes') { throw 'cancelled' }
        }
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path (Join-Path $source '*') -Destination $Destination -Recurse -Force

    $uninstaller = Join-Path $SetupRoot 'Uninstall.ps1'
    if (Test-Path $uninstaller) {
        Copy-Item $uninstaller (Join-Path $Destination 'Uninstall.ps1') -Force
    }

    $record = [ordered]@{
        mode        = if ($RunMode -eq 'Portable') { 'portable' } else { 'install' }
        target      = $Destination
        version     = $Version
        installedAt = (Get-Date).ToString('s')
    }
    $record | ConvertTo-Json | Set-Content (Join-Path $Destination 'install.json') -Encoding UTF8
}

function New-Shortcut([string]$Path, [string]$Executable) {
    $parent = Split-Path -Parent $Path
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($Path)
    $shortcut.TargetPath = $Executable
    $shortcut.WorkingDirectory = Split-Path -Parent $Executable
    $shortcut.IconLocation = "$Executable,0"
    $shortcut.Description = 'NAPP — ILLIERTION web application platform'
    $shortcut.Save()
}

function Install-Napp([string]$Destination) {
    Copy-Payload $Destination 'Install'

    $executable = Join-Path $Destination 'NAPP.exe'
    $startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\NAPP.lnk'
    New-Shortcut $startMenu $executable

    if ($Shortcuts) {
        $desktop = [Environment]::GetFolderPath('Desktop')
        New-Shortcut (Join-Path $desktop 'NAPP.lnk') $executable
    }

    $uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\NAPP'
    New-Item -Path $uninstallKey -Force | Out-Null
    Set-ItemProperty $uninstallKey 'DisplayName' 'NAPP'
    Set-ItemProperty $uninstallKey 'DisplayVersion' $Version
    Set-ItemProperty $uninstallKey 'Publisher' 'ILLIERTION'
    Set-ItemProperty $uninstallKey 'DisplayIcon' $executable
    Set-ItemProperty $uninstallKey 'InstallLocation' $Destination
    Set-ItemProperty $uninstallKey 'NoModify' 1 -Type DWord
    Set-ItemProperty $uninstallKey 'NoRepair' 1 -Type DWord
    Set-ItemProperty $uninstallKey 'UninstallString' `
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$Destination\Uninstall.ps1`""

    # .ipkg files open with NAPP (per user, no administrator rights needed).
    # The launcher repairs this on start if the folder is moved.
    $classes = 'HKCU:\Software\Classes'
    New-Item -Path "$classes\.ipkg" -Force | Out-Null
    Set-ItemProperty "$classes\.ipkg" '(default)' 'NAPP.ipkg'
    New-Item -Path "$classes\NAPP.ipkg\DefaultIcon" -Force | Out-Null
    Set-ItemProperty "$classes\NAPP.ipkg" '(default)' 'NAPP Package'
    Set-ItemProperty "$classes\NAPP.ipkg\DefaultIcon" '(default)' "`"$executable`",0"
    New-Item -Path "$classes\NAPP.ipkg\shell\open\command" -Force | Out-Null
    Set-ItemProperty "$classes\NAPP.ipkg\shell\open\command" '(default)' "`"$executable`" `"%1`""

    return $executable
}

function Install-Portable([string]$Destination) {
    Copy-Payload $Destination 'Portable'
    # A visible marker so users can tell a portable copy from an installed one.
    Set-Content (Join-Path $Destination 'PORTABLE.txt') `
        "NAPP $Version portable`r`nSettings and app data live in `%LOCALAPPDATA%\illiertion\napp.`r`n" -Encoding UTF8
    return (Join-Path $Destination 'NAPP.exe')
}

function Start-Napp([string]$Executable) {
    Start-Process -FilePath $Executable -WorkingDirectory (Split-Path -Parent $Executable)
}

# --------------------------------------------------------------------------
# Mode dispatch

if ($Mode -eq 'Install') {
    if (-not $Target) { $Target = Join-Path $env:LOCALAPPDATA 'Programs\NAPP' }
    $exe = Install-Napp $Target
    Write-Host "Installed NAPP $Version to $Target"
    if ($LaunchAfter) { Start-Napp $exe }
    exit 0
}

if ($Mode -eq 'Portable') {
    if (-not $Target) { $Target = Join-Path ([Environment]::GetFolderPath('Desktop')) 'NAPP-Portable' }
    $exe = Install-Portable $Target
    Write-Host "Portable NAPP $Version copied to $Target"
    if ($LaunchAfter) { Start-Napp $exe }
    exit 0
}

# --------------------------------------------------------------------------
# GUI (default)

[System.Windows.Forms.Application]::EnableVisualStyles()

$form = New-Object System.Windows.Forms.Form
$form.Text = "NAPP $Version Setup"
$form.ClientSize = New-Object System.Drawing.Size(560, 366)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.MinimizeBox = $false
$form.Font = New-Object System.Drawing.Font('Segoe UI', 9)

$heading = New-Object System.Windows.Forms.Label
$heading.Text = 'Choose how to set up NAPP'
$heading.Font = New-Object System.Drawing.Font('Segoe UI', 14, [System.Drawing.FontStyle]::Bold)
$heading.AutoSize = $true
$heading.Location = New-Object System.Drawing.Point(22, 18)
$form.Controls.Add($heading)

$subtitle = New-Object System.Windows.Forms.Label
$subtitle.Text = "Install for this Windows account, or keep everything in a folder you can move around."
$subtitle.AutoSize = $false
$subtitle.Size = New-Object System.Drawing.Size(512, 34)
$subtitle.Location = New-Object System.Drawing.Point(24, 52)
$subtitle.ForeColor = [System.Drawing.Color]::FromArgb(90, 96, 106)
$form.Controls.Add($subtitle)

$installRadio = New-Object System.Windows.Forms.RadioButton
$installRadio.Text = 'Install (recommended)'
$installRadio.Checked = $true
$installRadio.AutoSize = $true
$installRadio.Location = New-Object System.Drawing.Point(26, 98)
$form.Controls.Add($installRadio)

$installHint = New-Object System.Windows.Forms.Label
$installHint.Text = 'Start Menu shortcut, optional desktop shortcut and an uninstall entry. No administrator rights.'
$installHint.AutoSize = $false
$installHint.Size = New-Object System.Drawing.Size(480, 20)
$installHint.Location = New-Object System.Drawing.Point(46, 120)
$installHint.ForeColor = [System.Drawing.Color]::FromArgb(110, 116, 126)
$form.Controls.Add($installHint)

$portableRadio = New-Object System.Windows.Forms.RadioButton
$portableRadio.Text = 'Portable'
$portableRadio.AutoSize = $true
$portableRadio.Location = New-Object System.Drawing.Point(26, 156)
$form.Controls.Add($portableRadio)

$portableHint = New-Object System.Windows.Forms.Label
$portableHint.Text = 'Copy the program files only. Nothing is added to the registry or Start Menu.'
$portableHint.AutoSize = $false
$portableHint.Size = New-Object System.Drawing.Size(480, 20)
$portableHint.Location = New-Object System.Drawing.Point(46, 178)
$portableHint.ForeColor = [System.Drawing.Color]::FromArgb(110, 116, 126)
$form.Controls.Add($portableHint)

$pathLabel = New-Object System.Windows.Forms.Label
$pathLabel.Text = 'Folder'
$pathLabel.AutoSize = $true
$pathLabel.Location = New-Object System.Drawing.Point(26, 214)
$form.Controls.Add($pathLabel)

$pathBox = New-Object System.Windows.Forms.TextBox
$pathBox.Location = New-Object System.Drawing.Point(74, 211)
$pathBox.Size = New-Object System.Drawing.Size(380, 24)
$pathBox.Text = Join-Path $env:LOCALAPPDATA 'Programs\NAPP'
$form.Controls.Add($pathBox)

$browse = New-Object System.Windows.Forms.Button
$browse.Text = 'Browse...'
$browse.Location = New-Object System.Drawing.Point(462, 210)
$browse.Size = New-Object System.Drawing.Size(72, 26)
$form.Controls.Add($browse)

$desktopCheck = New-Object System.Windows.Forms.CheckBox
$desktopCheck.Text = 'Create a desktop shortcut'
$desktopCheck.AutoSize = $true
$desktopCheck.Location = New-Object System.Drawing.Point(26, 244)
$form.Controls.Add($desktopCheck)

$launchCheck = New-Object System.Windows.Forms.CheckBox
$launchCheck.Text = 'Launch NAPP when setup finishes'
$launchCheck.AutoSize = $true
$launchCheck.Checked = $true
$launchCheck.Location = New-Object System.Drawing.Point(26, 268)
$form.Controls.Add($launchCheck)

$installButton = New-Object System.Windows.Forms.Button
$installButton.Text = 'Install'
$installButton.Location = New-Object System.Drawing.Point(330, 310)
$installButton.Size = New-Object System.Drawing.Size(100, 32)
$installButton.DialogResult = [System.Windows.Forms.DialogResult]::OK
$form.Controls.Add($installButton)

$cancelButton = New-Object System.Windows.Forms.Button
$cancelButton.Text = 'Cancel'
$cancelButton.Location = New-Object System.Drawing.Point(438, 310)
$cancelButton.Size = New-Object System.Drawing.Size(96, 32)
$cancelButton.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
$form.Controls.Add($cancelButton)
$form.CancelButton = $cancelButton

$browse.add_Click({
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Select the NAPP install folder'
    $dialog.SelectedPath = $pathBox.Text
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $pathBox.Text = $dialog.SelectedPath
    }
})

$installRadio.add_CheckedChanged({
    $isInstall = $installRadio.Checked
    $desktopCheck.Enabled = $isInstall
    if ($isInstall) {
        $pathBox.Text = Join-Path $env:LOCALAPPDATA 'Programs\NAPP'
        $installButton.Text = 'Install'
    } else {
        $pathBox.Text = Join-Path ([Environment]::GetFolderPath('Desktop')) 'NAPP-Portable'
        $installButton.Text = 'Copy'
    }
})

$installButton.add_Click({
    if (-not (Test-Path (Join-Path $Payload 'NAPP.exe'))) {
        [System.Windows.Forms.MessageBox]::Show(
            "The payload folder is missing.`n`nExpected: $Payload\NAPP.exe",
            'NAPP Setup', 'OK', 'Error') | Out-Null
        $form.DialogResult = [System.Windows.Forms.DialogResult]::None
        return
    }

    $installButton.Enabled = $false
    $cancelButton.Enabled = $false
    $installButton.Text = 'Working...'
    $form.Refresh()

    try {
        if ($installRadio.Checked) {
            $exe = Install-Napp $pathBox.Text
        } else {
            $exe = Install-Portable $pathBox.Text
        }
        $form.Hide()
        [System.Windows.Forms.MessageBox]::Show(
            "NAPP $Version is ready.`n`n$($pathBox.Text)",
            'NAPP Setup', 'OK', 'Information') | Out-Null
        if ($launchCheck.Checked) { Start-Napp $exe }
        $form.Close()
    } catch {
        if ("$($_.Exception.Message)" -ne 'cancelled') {
            [System.Windows.Forms.MessageBox]::Show(
                "Setup could not finish.`n`n$($_.Exception.Message)",
                'NAPP Setup', 'OK', 'Error') | Out-Null
        }
        $installButton.Enabled = $true
        $cancelButton.Enabled = $true
        $installButton.Text = if ($installRadio.Checked) { 'Install' } else { 'Copy' }
    }
})

[void]$form.ShowDialog()
