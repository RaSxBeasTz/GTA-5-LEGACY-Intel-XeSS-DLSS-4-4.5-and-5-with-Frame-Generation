param([switch]$StageOnly,[string]$Dlss5RuntimePath='C:/Users/Franc/OneDrive/DLSS 5/nvngx_dlssnr.dll')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(-not(Test-Path -LiteralPath $Dlss5RuntimePath)){$Dlss5RuntimePath=Join-Path $root '..\..\Backend\nvngx_dlssnr.dll'}
$Dlss5Rtx40RuntimePath=Join-Path $root '..\..\Backend\nvngx_dlssnr_rtx40.dll'
$lock=Get-Content (Join-Path $root 'ProtectedFixes/B535-source-lock.json') -Raw | ConvertFrom-Json
foreach($entry in $lock.PSObject.Properties){
 $actual=(Get-FileHash -LiteralPath (Join-Path $root $entry.Name) -Algorithm SHA256).Hash.ToLowerInvariant()
 if($actual -ne $entry.Value){throw "Protected source/interface changed: $($entry.Name). Review the intentional change; do not silently regenerate this lock."}
}
# B535 Fixed-v2: restore missing official Intel XeSS SDK automatically.
# Version is pinned to v3.0.2 to avoid silently tracking an incompatible main branch.
$cache=Join-Path $env:LOCALAPPDATA 'GTAXeSS/xess'
$local=Join-Path $root '.deps/xess'
function Test-XeSSSdk([string]$folder){
 if(-not $folder){return $false}
 foreach($f in @('inc/xess/xess_d3d11.h','lib/libxess_dx11.lib','lib/libxess_fg.lib','lib/libxell.lib')){
  if(-not(Test-Path -LiteralPath (Join-Path $folder $f) -PathType Leaf)){return $false}
 }
 return $true
}
$xess=$null
if(Test-XeSSSdk $cache){$xess=$cache}
elseif(Test-XeSSSdk $local){$xess=$local}
if(-not $xess){
 $sdkVersion='v3.0.2'
 $sdkUrl='https://github.com/intel/xess/archive/refs/tags/'+$sdkVersion+'.zip'
 $sdkTemp=Join-Path ([IO.Path]::GetTempPath()) ('GTAXeSS-XeSS-SDK-'+[Guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $sdkTemp -Force | Out-Null
 try {
  $archive=Join-Path $sdkTemp 'intel-xess.zip'
  Write-Host ('Downloading official Intel XeSS SDK '+$sdkVersion+' (first build only)...') -ForegroundColor Cyan
  $ProgressPreference='SilentlyContinue'
  try {Invoke-WebRequest -Uri $sdkUrl -OutFile $archive -UseBasicParsing -MaximumRedirection 8 -ErrorAction Stop}
  catch {
   $fallback='https://codeload.github.com/intel/xess/zip/refs/tags/'+$sdkVersion
   Write-Host ('Primary download unavailable; retrying '+$fallback) -ForegroundColor Yellow
   Invoke-WebRequest -Uri $fallback -OutFile $archive -UseBasicParsing -ErrorAction Stop
  }
  Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $sdkTemp 'expanded') -Force
  $candidate=Get-ChildItem -LiteralPath (Join-Path $sdkTemp 'expanded') -Directory | Where-Object {Test-XeSSSdk $_.FullName} | Select-Object -First 1
  if(-not $candidate){throw 'Downloaded XeSS SDK does not contain the required headers and x64 import libraries; refusing to build against an incomplete SDK.'}
  $parent=Split-Path $cache -Parent
  New-Item -ItemType Directory -Path $parent -Force | Out-Null
  $incoming=Join-Path $parent ('xess-new-'+[Guid]::NewGuid().ToString('N'))
  Copy-Item -LiteralPath $candidate.FullName -Destination $incoming -Recurse -Force
  if(-not(Test-XeSSSdk $incoming)){throw 'Downloaded SDK cache copy failed validation.'}
  if(Test-Path -LiteralPath $cache){Remove-Item -LiteralPath $cache -Recurse -Force}
  Move-Item -LiteralPath $incoming -Destination $cache
  $xess=$cache
  Write-Host ('XeSS SDK '+$sdkVersion+' cached at '+$cache) -ForegroundColor Green
 } finally {Remove-Item -LiteralPath $sdkTemp -Recurse -Force -ErrorAction SilentlyContinue}
}
if(-not(Test-XeSSSdk $xess)){throw 'Intel XeSS SDK validation failed.'}
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if(-not $msbuild){throw 'Visual Studio C++ Build Tools required'}
# Fixed-v3: select the MSVC toolset actually installed with the selected Visual Studio.
# Do not force VS 2022 v143 on a VS 2026 v145-only installation.
$vsInstall=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if(-not $vsInstall){throw 'Visual Studio C++ x64 build tools are not installed. Install Desktop development with C++.'}
$toolsetFolders=Get-ChildItem -Path (Join-Path $vsInstall 'MSBuild/Microsoft/VC') -Directory -ErrorAction SilentlyContinue | ForEach-Object {
 Get-ChildItem -Path (Join-Path $_.FullName 'Platforms/x64/PlatformToolsets') -Directory -ErrorAction SilentlyContinue
} | Where-Object { $_.Name -match '^v\d{3}$' } | Sort-Object { [int]($_.Name.Substring(1)) } -Descending
if(-not $toolsetFolders){throw ('No x64 MSVC PlatformToolset found under '+$vsInstall)}
$platformToolset=$toolsetFolders[0].Name
Write-Host ('Using installed MSVC PlatformToolset '+$platformToolset+' from '+$vsInstall) -ForegroundColor Cyan
function Build-Project([string]$relative,[string]$targetName=""){
 $launch=[Diagnostics.ProcessStartInfo]::new();$launch.FileName=$msbuild;$launch.UseShellExecute=$false
 $launch.Arguments='"'+(Join-Path $root $relative)+'" /t:Build /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset='+$platformToolset+' "/p:XeSSRoot='+$xess+'"'
 if($targetName){$launch.Arguments+=' /p:TargetName='+$targetName}
 $clean=@{};foreach($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()){$clean[$entry.Key]=$entry.Value}
 $launch.EnvironmentVariables.Clear();foreach($entry in $clean.GetEnumerator()){$launch.EnvironmentVariables[$entry.Key]=$entry.Value}
 $proc=[Diagnostics.Process]::Start($launch);$proc.WaitForExit();if($proc.ExitCode){throw "Build failed: $relative"}
}
Build-Project 'ProtectedFixes/FreezingFix.vcxproj' 'GTAXeSS.FreezingFix'
Build-Project 'IntelArc/IntelArc.vcxproj'
Build-Project 'NvidiaRtx/Dlss5Runtime.vcxproj'
Build-Project 'NvidiaRtx/NvidiaBackend.vcxproj'
Build-Project 'NvidiaRtx/NvidiaProbe.vcxproj'
Build-Project 'ProtectedFixes/GhostingFix.vcxproj'
$ghost=Join-Path $root 'ProtectedFixes/x64/Release/GTAXeSS.GhostingFix.dll'
$hash=(Get-FileHash $ghost -Algorithm SHA256).Hash.ToLowerInvariant()
$expected=Join-Path $root 'B25DepthHelper/GhostingExpected.h'
$text=[IO.File]::ReadAllText($expected)
$text=[regex]::Replace($text,'GHOSTING_EXPECTED_SHA256 "[^"]+"',('GHOSTING_EXPECTED_SHA256 "'+$hash+'"'))
[IO.File]::WriteAllText($expected,$text,[Text.UTF8Encoding]::new($false))
Build-Project 'B25DepthHelper/B25DepthHelper.vcxproj'
Build-Project 'ScriptHookProbe/ScriptHookProbe.vcxproj'
$out=Join-Path $root 'GTA-UPDATE';New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'NvidiaRtx/x64/Release/GTAXeSS.DLSS5_nvngx.dll') -Destination $out -Force
Copy-Item -LiteralPath (Join-Path $root 'NvidiaRtx/DLSS5-reference-LICENSE.txt') -Destination $out -Force
if(-not(Test-Path -LiteralPath $Dlss5RuntimePath)){throw 'Supply your local nvngx_dlssnr.dll with -Dlss5RuntimePath.'}
Copy-Item -LiteralPath $Dlss5RuntimePath -Destination (Join-Path $out 'nvngx_dlssnr.dll') -Force
if(-not(Test-Path -LiteralPath $Dlss5Rtx40RuntimePath)){throw 'Backend\nvngx_dlssnr_rtx40.dll is missing.'}
Copy-Item -LiteralPath $Dlss5Rtx40RuntimePath -Destination (Join-Path $out 'nvngx_dlssnr_rtx40.dll') -Force
$pairs=@(@('NvidiaRtx/x64/Release/NVIDIA RTX.dll','NVIDIA RTX.dll'),@('IntelArc/x64/Release/Intel ARC.dll','Intel ARC.dll'),@('ProtectedFixes/x64/Release/GTAXeSS.GhostingFix.dll','GTAXeSS.GhostingFix.dll'),@('B25DepthHelper/x64/Release/B25DepthHelper.dll','B25DepthHelper.dll'),@('ScriptHookProbe/x64/Release/ScriptHookProbe.asi','ScriptHookProbe.asi'))
foreach($pair in $pairs){$source=Join-Path $root $pair[0];$dest=Join-Path $out $pair[1];Copy-Item -LiteralPath $source -Destination $dest -Force;if((Get-FileHash $source).Hash -ne (Get-FileHash $dest).Hash){throw "Staging mismatch: $dest"}}
$nvidiaSdk=Join-Path $env:LOCALAPPDATA 'GTAXeSS/streamline-v2.14.16'
foreach($name in @('sl.interposer.dll','sl.common.dll','sl.dlss.dll','sl.dlss_g.dll','sl.reflex.dll','sl.pcl.dll','nvngx_dlss.dll','nvngx_dlssg.dll')){Copy-Item -LiteralPath (Join-Path $nvidiaSdk ('bin/x64/'+$name)) -Destination (Join-Path $out $name) -Force}
Copy-Item -LiteralPath (Join-Path $root 'NvidiaRtx/x64/Release/NvidiaProbe.exe') -Destination $out -Force
foreach($name in @('nvngx_dlss.license.txt','reflex.license.txt')){Copy-Item -LiteralPath (Join-Path $nvidiaSdk ('bin/x64/'+$name)) -Destination (Join-Path $out $name) -Force}
Copy-Item -LiteralPath (Join-Path $nvidiaSdk 'license.txt') -Destination (Join-Path $out 'NVIDIA-Streamline-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $nvidiaSdk '3rd-party-licenses.md') -Destination (Join-Path $out 'NVIDIA-ThirdParty-LICENSES.md') -Force
Copy-Item -LiteralPath (Join-Path $root 'ProtectedFixes/x64/Release/GTAXeSS.FreezingFix.dll') -Destination (Join-Path $out 'GTAXeSS.FreezingFix.dll') -Force
Get-ChildItem $out -File | Where-Object {$_.Extension -in '.dll','.asi'} | Get-FileHash -Algorithm SHA256 | ForEach-Object {$_.Hash+'  '+[IO.Path]::GetFileName($_.Path)} | Set-Content (Join-Path $out 'SHA256.txt')
Copy-Item -LiteralPath (Join-Path $root 'GTAXeSS.ini') -Destination (Join-Path $out 'GTAXeSS.ini') -Force
if($StageOnly){Write-Host 'R639 staged in GTA-UPDATE.';return}
if(Get-Process GTA5 -ErrorAction SilentlyContinue){throw 'GTA5.exe is running. Close GTA V completely, then run this builder again.'}
function Test-GtaRoot([string]$p){return $p -and (Test-Path -LiteralPath (Join-Path $p 'GTA5.exe'))}
$remember=Join-Path $root '.gta-path.txt';$GtaRoot=$null
if(Test-Path $remember){$candidate=(Get-Content $remember -Raw).Trim();if(Test-GtaRoot $candidate){$GtaRoot=$candidate}}
if(-not $GtaRoot){
 $candidates=New-Object System.Collections.Generic.List[string]
 foreach($reg in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam','HKLM:\SOFTWARE\Valve\Steam')){try{$sp=(Get-ItemProperty $reg -ErrorAction Stop).SteamPath;if($sp){$candidates.Add((Join-Path $sp 'steamapps\common\Grand Theft Auto V'))}}catch{}}
 foreach($drive in [IO.DriveInfo]::GetDrives()|Where-Object {$_.DriveType -eq 'Fixed'}){$candidates.Add((Join-Path $drive.RootDirectory.FullName 'Program Files (x86)\Steam\steamapps\common\Grand Theft Auto V'));$candidates.Add((Join-Path $drive.RootDirectory.FullName 'SteamLibrary\steamapps\common\Grand Theft Auto V'))}
 foreach($candidate in $candidates){if(Test-GtaRoot $candidate){$GtaRoot=$candidate;break}}
}
while(-not $GtaRoot){$candidate=Read-Host 'GTA V Legacy folder not auto-detected. Paste the folder containing GTA5.exe';$candidate=$candidate.Trim('"');if(Test-GtaRoot $candidate){$GtaRoot=$candidate}else{Write-Host 'GTA5.exe was not found in that folder.' -ForegroundColor Yellow}}
Set-Content -LiteralPath $remember -Value $GtaRoot -Encoding UTF8
Write-Host ('Installing R639 into: '+$GtaRoot)
foreach($name in @('GTAXeSS.DLSS5_nvngx.dll','nvngx_dlssnr.dll','nvngx_dlssnr_rtx40.dll','DLSS5-reference-LICENSE.txt')){Copy-Item -LiteralPath (Join-Path $out $name) -Destination (Join-Path $GtaRoot $name) -Force;if((Get-FileHash (Join-Path $out $name)).Hash -ne (Get-FileHash (Join-Path $GtaRoot $name)).Hash){throw "Installation mismatch: $name"}}
foreach($pair in $pairs){$built=Join-Path $out $pair[1];$installed=Join-Path $GtaRoot $pair[1];Copy-Item -LiteralPath $built -Destination $installed -Force;$bh=(Get-FileHash $built -Algorithm SHA256).Hash;$ih=(Get-FileHash $installed -Algorithm SHA256).Hash;if($bh -ne $ih){throw "Installed SHA256 mismatch: $installed"};Write-Host ('Verified '+$pair[1]+'  '+$ih)}
foreach($name in @('sl.interposer.dll','sl.common.dll','sl.dlss.dll','sl.dlss_g.dll','sl.reflex.dll','sl.pcl.dll','nvngx_dlss.dll','nvngx_dlssg.dll','NvidiaProbe.exe','NVIDIA-Streamline-LICENSE.txt','NVIDIA-ThirdParty-LICENSES.md','nvngx_dlss.license.txt','reflex.license.txt')){Copy-Item -LiteralPath (Join-Path $out $name) -Destination (Join-Path $GtaRoot $name) -Force;if((Get-FileHash -LiteralPath (Join-Path $out $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $GtaRoot $name)).Hash){throw ('NVIDIA installation mismatch: '+$name)}}
$freezeDestination=Join-Path $GtaRoot 'GTAXeSS.FreezingFix.dll'
if(-not(Test-Path -LiteralPath $freezeDestination)){Copy-Item -LiteralPath (Join-Path $out 'GTAXeSS.FreezingFix.dll') -Destination $freezeDestination; if((Get-FileHash $freezeDestination).Hash -ne (Get-FileHash (Join-Path $out 'GTAXeSS.FreezingFix.dll')).Hash){throw 'Freezing DLL installation mismatch'}}
$installedIni=Join-Path $GtaRoot 'GTAXeSS.ini'
if(-not(Test-Path $installedIni)){Copy-Item -LiteralPath (Join-Path $root 'GTAXeSS.ini') -Destination $installedIni}else{if((Get-Content $installedIni -Raw) -notmatch '(?im)^\[Intel ARC\]'){Add-Content -LiteralPath $installedIni -Value "`r`n[Intel ARC]`r`nEnabled=1"}}
if((Get-Content $installedIni -Raw) -notmatch '(?im)^\[Diagnostics\]'){Add-Content -LiteralPath $installedIni -Value "`r`n[Diagnostics]`r`nEnabled=1"}
if((Get-Content $installedIni -Raw) -notmatch '(?im)^\[Pacing\]'){Add-Content -LiteralPath $installedIni -Value "`r`n[Pacing]`r`nSourceFPS=30"}
$proof=Join-Path $GtaRoot 'GTAXeSS-R639-INSTALLED.txt';@('GTAXeSS R639 installed and SHA256 verified',('Installed: '+(Get-Date -Format o)),('GTA root: '+$GtaRoot),'Files: B25DepthHelper.dll, GTAXeSS.GhostingFix.dll, ScriptHookProbe.asi, Intel ARC.dll','GTAXeSS.FreezingFix.dll was not changed.') | Set-Content $proof
Write-Host 'R639 BUILD + INSTALL SUCCEEDED. All four matching R639 runtime files are in the GTA folder and verified.' -ForegroundColor Green
Write-Host 'Existing freezing DLL preserved; missing freezing DLL supplied.'

