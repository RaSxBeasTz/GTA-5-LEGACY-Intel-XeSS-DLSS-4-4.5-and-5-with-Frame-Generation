param([string]$Source = 'C:\Users\Franc\OneDrive\GTA 5. XeSS and Frame Generation\APP\ZinkGTAUI-WinUI3-FullDesign-Fixed\ZinkGTAUI\bin\x64\Release\net8.0-windows10.0.19041.0\win-x64')
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath (Join-Path $Source 'ZinkGTAUI.exe'))){throw 'Build the WinUI app before installing it.'}
$destination=Join-Path $env:LOCALAPPDATA 'Programs\Zink GTA Enhancement'
if(Get-Process ZinkGTAUI -ErrorAction SilentlyContinue){throw 'Close Zink before updating the app.'}
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Get-ChildItem -LiteralPath $Source | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force}
$exe=Join-Path $destination 'ZinkGTAUI.exe'
if((Get-FileHash -LiteralPath $exe).Hash -ne (Get-FileHash -LiteralPath (Join-Path $Source 'ZinkGTAUI.exe')).Hash){throw 'App installation failed verification.'}
$shell=New-Object -ComObject WScript.Shell
$desktop=[Environment]::GetFolderPath('Desktop')
$programs=[Environment]::GetFolderPath('Programs')
foreach($folder in @($desktop,$programs)){
 $shortcut=$shell.CreateShortcut((Join-Path $folder 'Zink GTA Enhancement.lnk'))
 $shortcut.TargetPath=$exe;$shortcut.WorkingDirectory=$destination;$shortcut.Description='Zink GTA Enhancement — Intel XeSS and NVIDIA RTX';$shortcut.IconLocation=$exe+',0';$shortcut.Save()
}
Write-Output ('Installed app: '+$exe)
Write-Output 'Desktop and Start menu shortcuts created. No BAT file or terminal is needed to open Zink.'
