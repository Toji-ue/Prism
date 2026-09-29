# Prism installer (per-user, no admin needed).
#
# Copies the app to %LOCALAPPDATA%\Programs\Prism, creates Desktop and Start
# Menu shortcuts, and drops an Uninstall-Prism.cmd next to the exe.
#
# Run:  powershell -ExecutionPolicy Bypass -File Prism-Setup.ps1
# Optional: -Destination "D:\Prism" to install elsewhere.

param(
    [string]$Destination = "$env:LOCALAPPDATA\Programs\Prism"
)

$ErrorActionPreference = 'Stop'
$source = $PSScriptRoot

# --- close a running Prism first: its exe is locked otherwise ---------------
Get-Process prism -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600

# --- copy the app ------------------------------------------------------------
New-Item -ItemType Directory -Force -Path $Destination | Out-Null

# Everything in this folder except the installer itself.
$files = Get-ChildItem $source -File | Where-Object {
    $_.Name -notin @('Prism-Setup.ps1', 'Install-Prism.cmd', 'README-TESTER.txt')
}
$dirs = Get-ChildItem $source -Directory

$total = $files.Count + $dirs.Count
$i = 0
foreach ($f in $files) {
    $i++
    Write-Progress -Activity 'Cai dat Prism' -Status "$($f.Name) ($i/$total)" -PercentComplete ($i * 100 / $total)
    Copy-Item $f.FullName $Destination -Force
}
foreach ($d in $dirs) {
    $i++
    Write-Progress -Activity 'Cai dat Prism' -Status "$($d.Name) ($i/$total)" -PercentComplete ($i * 100 / $total)
    Copy-Item $d.FullName (Join-Path $Destination $d.Name) -Recurse -Force
}
Write-Progress -Activity 'Cai dat Prism' -Completed

# --- shortcuts ---------------------------------------------------------------
$wsh = New-Object -ComObject WScript.Shell
$desktop = [Environment]::GetFolderPath('Desktop')
$startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Prism'
New-Item -ItemType Directory -Force -Path $startMenu | Out-Null

foreach ($dir in @($desktop, $startMenu)) {
    $lnk = $wsh.CreateShortcut((Join-Path $dir 'Prism.lnk'))
    $lnk.TargetPath = Join-Path $Destination 'prism.exe'
    $lnk.WorkingDirectory = $Destination
    $lnk.Description = 'Prism - ung dung ve'
    $lnk.Save()
}

# --- uninstaller -------------------------------------------------------------
# A PowerShell uninstaller (wrapped by Uninstall-Prism.cmd): the Desktop can be
# redirected (OneDrive), so resolving it with the shell API beats the
# %USERPROFILE%\Desktop guess that a pure batch file would need.
$uninstallPs1 = @"
`$ErrorActionPreference = 'SilentlyContinue'
Get-Process prism | Stop-Process -Force
Start-Sleep -Milliseconds 600
Remove-Item '$Destination' -Recurse -Force
Remove-Item (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Prism.lnk') -Force
Remove-Item (Join-Path ([Environment]::GetFolderPath('Programs')) 'Prism') -Recurse -Force
Write-Output 'Da go Prism xong.'
"@
[System.IO.File]::WriteAllText((Join-Path $Destination 'Uninstall-Prism.ps1'),
    $uninstallPs1, [System.Text.ASCIIEncoding]::new())
[System.IO.File]::WriteAllText((Join-Path $Destination 'Uninstall-Prism.cmd'),
    "@echo off`r`npowershell -NoProfile -ExecutionPolicy Bypass -File `"%~dp0Uninstall-Prism.ps1`"`r`npause`r`n",
    [System.Text.ASCIIEncoding]::new())

Write-Output ''
Write-Output '=========================================='
Write-Output ' Cai dat Prism xong!'
Write-Output " Thu muc : $Destination"
Write-Output " Shortcut: Desktop + Start Menu > Prism"
Write-Output " Go bo   : $Destination\Uninstall-Prism.cmd"Write-Output '=========================================='
Write-Output ''
Read-Host 'Nhan Enter de dong'
