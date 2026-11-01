# Zink GTA Enhancement R658 — one-command NVIDIA native fix updater.
# Run from Windows PowerShell: irm 'https://raw.githubusercontent.com/RaSxBeasTz/GTA-5-LEGACY-Intel-XeSS-DLSS-4-4.5-and-5-with-Frame-Generation/fix/r658-nvidia-vram-warning-startup/Update-R658-v2.ps1' | iex
# Requires installed Visual Studio C++ build tools + Windows SDK. Downloads the
# exact R658 source and pinned third-party SDK headers/import libs when missing.
# Only installs a rebuilt NVIDIA RTX.dll and updates the app bundle manifest.
[CmdletBinding()]
param([string]$AppFolder = '')
$ErrorActionPreference = 'Stop'
Write-Host '[Zink R658] FIXED UPDATER v2: SHORT NON-TEMP BUILD PATH' -ForegroundColor Green
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

function Status([string]$message) { Write-Host ('[Zink R658] ' + $message) -ForegroundColor Cyan }
function Sha([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToUpperInvariant() }
function Download([string]$url,[string]$destination) {
    Status ('Downloading ' + [IO.Path]::GetFileName($destination))
    Invoke-WebRequest -Uri $url -OutFile $destination -UseBasicParsing -MaximumRedirection 10
    if (-not (Test-Path -LiteralPath $destination -PathType Leaf) -or (Get-Item -LiteralPath $destination).Length -lt 512) {
        throw ('Download failed or was incomplete: ' + $url)
    }
}
function Meets([string]$folder,[string[]]$required) {
    if (-not (Test-Path -LiteralPath $folder -PathType Container)) { return $false }
    foreach ($name in $required) { if (-not (Test-Path -LiteralPath (Join-Path $folder $name) -PathType Leaf)) { return $false } }
    return $true
}
function EnsureSdk([string]$label,[string]$cache,[string]$url,[string[]]$required,[string]$work) {
    if (Meets $cache $required) { Status ($label + ' SDK: already installed'); return $cache }
    $zip = Join-Path $work ($label + '.zip')
    $unpack = Join-Path $work ($label + '-expanded')
    Download $url $zip
    Expand-Archive -LiteralPath $zip -DestinationPath $unpack -Force
    $candidate = Get-ChildItem -LiteralPath $unpack -Directory | Where-Object { Meets $_.FullName $required } | Select-Object -First 1
    if (-not $candidate) { throw ($label + ' archive does not contain the pinned SDK headers and x64 import libraries.') }
    New-Item -ItemType Directory -Path (Split-Path -Parent $cache) -Force | Out-Null
    if (Test-Path -LiteralPath $cache) {
        $old = $cache + '.incomplete-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
        Move-Item -LiteralPath $cache -Destination $old
        Status ($label + ' incomplete SDK backed up: ' + $old)
    }
    Move-Item -LiteralPath $candidate.FullName -Destination $cache
    if (-not (Meets $cache $required)) { throw ($label + ' SDK setup failed.') }
    return $cache
}
function FindApp([string]$requested) {
    $candidates = New-Object 'System.Collections.Generic.List[string]'
    if ($requested) { $candidates.Add($requested) }
    Get-Process ZinkGTAUI -ErrorAction SilentlyContinue | ForEach-Object {
        try { $candidates.Add((Split-Path -Parent $_.Path)) } catch {}
    }
    $candidates.Add((Join-Path $env:LOCALAPPDATA 'Programs\Zink GTA Enhancement'))
    $candidates.Add((Join-Path $env:LOCALAPPDATA 'Zink GTA Enhancement'))
    $candidates.Add((Join-Path $env:USERPROFILE 'Desktop\Zink GTA Enhancement'))
    $candidates.Add((Join-Path $env:USERPROFILE 'Downloads\Zink GTA Enhancement'))
    foreach ($shortcut in @((Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Zink GTA Enhancement.lnk'), (Join-Path $env:USERPROFILE 'Desktop\Zink GTA Enhancement.lnk'))) {
        if (Test-Path -LiteralPath $shortcut) {
            try {
                $shell = New-Object -ComObject WScript.Shell
                $target = $shell.CreateShortcut($shortcut).TargetPath
                if ($target) { $candidates.Add((Split-Path -Parent $target)) }
            } catch {}
        }
    }
    foreach ($path in $candidates) {
        if ($path -and (Test-Path -LiteralPath (Join-Path $path 'ZinkGTAUI.exe') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $path 'Backend\manifest.json') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $path 'Backend\NVIDIA RTX.dll') -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }
    throw 'Installed Zink app folder was not found. It must contain ZinkGTAUI.exe and Backend\manifest.json. Run Zink once, then retry while it is open, or save the script and pass -AppFolder.'
}
function FindGame() {
    $list = New-Object 'System.Collections.Generic.List[string]'
    $settings = Join-Path $env:LOCALAPPDATA 'ZinkGTAUI\preferences.json'
    if (Test-Path -LiteralPath $settings) {
        try { $p = (Get-Content -LiteralPath $settings -Raw | ConvertFrom-Json).GameDirectory; if ($p) { $list.Add([string]$p) } } catch {}
    }
    $steam = @()
    try { $steam += (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath } catch {}
    try { $steam += (Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam' -ErrorAction Stop).InstallPath } catch {}
    foreach ($s in $steam) { if ($s) { $list.Add((Join-Path $s 'steamapps\common\Grand Theft Auto V')) } }
    Get-PSDrive -PSProvider FileSystem | ForEach-Object {
        $root = $_.Root
        $list.Add((Join-Path $root 'SteamLibrary\steamapps\common\Grand Theft Auto V'))
        $list.Add((Join-Path $root 'Program Files (x86)\Steam\steamapps\common\Grand Theft Auto V'))
    }
    foreach ($p in $list) { if ($p -and (Test-Path -LiteralPath (Join-Path $p 'GTA5.exe') -PathType Leaf)) { return $p } }
    return $null
}

if (Get-Process GTA5 -ErrorAction SilentlyContinue) { throw 'Close GTA V before updating its NVIDIA mod DLL, then run the same command again.' }
$app = FindApp $AppFolder
$backend = Join-Path $app 'Backend'
$manifestPath = Join-Path $backend 'manifest.json'
$oldDll = Join-Path $backend 'NVIDIA RTX.dll'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if (-not $manifest.Files -or -not $manifest.Files.'NVIDIA RTX.dll') { throw 'The installed app manifest has no NVIDIA RTX.dll entry.' }
$game = FindGame
Status ('App: ' + $app)
if ($game) { Status ('GTA: ' + $game) } else { Write-Warning 'GTA folder not found: app will be updated; run the Zink installer for the game afterward.' }

$time = Get-Date -Format 'yyyyMMdd-HHmmss'
$downloads = Join-Path $env:USERPROFILE 'Downloads'
if (-not (Test-Path -LiteralPath $downloads)) { New-Item -ItemType Directory -Path $downloads -Force | Out-Null }
# MSVC's legacy intermediate-path limit requires both a short source root and
# a non-TEMP output path (MSB3191/MSB8029 on Visual Studio 2026).
$work = Join-Path $env:USERPROFILE ('ZR658-v2-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $work -Force | Out-Null
$buildLog = Join-Path $downloads ('Zink-R658-build-' + $time + '.log')
$errLog = Join-Path $work 'msbuild-errors.txt'
try {
    $repoName = 'GTA-5-LEGACY-Intel-XeSS-DLSS-4-4.5-and-5-with-Frame-Generation'
    $repoUrl = 'https://codeload.github.com/RaSxBeasTz/' + $repoName + '/zip/refs/heads/fix/r658-nvidia-vram-warning-startup'
    $zip = Join-Path $work 'Zink-R658-source.zip'
    Download $repoUrl $zip
    $srcUnpack = Join-Path $work 'source'
    Expand-Archive -LiteralPath $zip -DestinationPath $srcUnpack -Force
    $source = Get-ChildItem -LiteralPath $srcUnpack -Directory | Where-Object {
        Test-Path -LiteralPath (Join-Path $_.FullName 'Source\Mod\NvidiaRtx\NvidiaBackend.vcxproj') -PathType Leaf
    } | Select-Object -First 1
    if (-not $source) { throw 'Downloaded source archive is missing the NVIDIA native project.' }
    # GitHub expands a very long repository/branch directory name. Move its
    # CONTENTS to a short root before building (path length must stay < 260).
    $shortSource = Join-Path $work 'src'
    Move-Item -LiteralPath $source.FullName -Destination $shortSource
    $source = Get-Item -LiteralPath $shortSource
    Status ('Short Windows source path: ' + $source.FullName)
    $header = Join-Path $source.FullName 'Source\Mod\NvidiaRtx\NvidiaDlaa.h'
    if (-not ([IO.File]::ReadAllText($header).Contains('R658: DLSS_EVALUATE_VRAM_WARNING'))) {
        throw 'Downloaded source does not contain the R658 hotfix. No files were changed.'
    }
    Copy-Item -LiteralPath $zip -Destination (Join-Path $downloads ('Zink-GTA-R658-source-' + $time + '.zip'))

    $localSdk = Join-Path $env:LOCALAPPDATA 'GTAXeSS'
    $xess = EnsureSdk 'XeSS' (Join-Path $localSdk 'xess') 'https://codeload.github.com/intel/xess/zip/refs/tags/v3.0.2' @('inc\xess\xess_d3d11.h','lib\libxess_dx11.lib','lib\libxess_fg.lib','lib\libxell.lib') $work
    $streamline = EnsureSdk 'Streamline' (Join-Path $localSdk 'streamline-v2.14.16') 'https://codeload.github.com/NVIDIA-RTX/Streamline/zip/refs/tags/v2.14.16' @('include\sl.h','include\sl_dlss.h','include\sl_dlss_g.h') $work

    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ Build Tools are required for rebuilding NVIDIA RTX.dll. No app files were changed.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
    if (-not $vs) { throw 'Visual Studio Desktop development with C++ is not installed. No app files were changed.' }
    $msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild)) { throw 'MSBuild.exe could not be located. No app files were changed.' }
    $vc = Join-Path $vs 'MSBuild\Microsoft\VC'
    $toolsets = @(Get-ChildItem -LiteralPath $vc -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        Get-ChildItem -LiteralPath (Join-Path $_.FullName 'Platforms\x64\PlatformToolsets') -Directory -ErrorAction SilentlyContinue
    } | Where-Object { $_.Name -match '^v\d{3}$' } | Sort-Object { [int]$_.Name.Substring(1) } -Descending)
    if ($toolsets.Count -eq 0) { throw 'The Visual Studio x64 C++ compiler toolset is missing. No app files were changed.' }
    $toolset = $toolsets[0].Name
    Status ('Building NVIDIA RTX.dll using ' + $toolset + ' (other mod DLLs are not touched)')
    $project = Join-Path $source.FullName 'Source\Mod\NvidiaRtx\NvidiaBackend.vcxproj'
    if ($project.Length -ge 220 -or $project -match '(?i)\\Temp\\') { throw ('Unexpected long/TEMP build path: ' + $project) }
    Status ('MSBuild project path length: ' + $project.Length)
    $arguments = '"' + $project + '" /t:Rebuild /m:1 /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=' + $toolset + ' "/p:XeSSRoot=' + $xess + '"'
    $result = Start-Process -FilePath $msbuild -ArgumentList $arguments -Wait -PassThru -RedirectStandardOutput $buildLog -RedirectStandardError $errLog
    if ($result.ExitCode -ne 0) {
        if (Test-Path -LiteralPath $buildLog) { Get-Content -LiteralPath $buildLog -Tail 30 }
        if (Test-Path -LiteralPath $errLog) { Get-Content -LiteralPath $errLog -Tail 30 }
        throw ('Native build failed, exit code ' + $result.ExitCode + '. See ' + $buildLog)
    }
    $newDll = Join-Path $source.FullName 'Source\Mod\NvidiaRtx\x64\Release\NVIDIA RTX.dll'
    if (-not (Test-Path -LiteralPath $newDll -PathType Leaf)) { throw 'MSBuild completed without producing NVIDIA RTX.dll.' }
    $newHash = Sha $newDll
    if ($newHash -eq (Sha $oldDll)) { throw 'The rebuilt DLL is identical to the existing one. Refusing to claim the fix was installed.' }

    $backup = Join-Path $env:LOCALAPPDATA ('ZinkGTAUI\backups\R658-nvidia-' + $time)
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    Copy-Item -LiteralPath $oldDll -Destination (Join-Path $backup 'NVIDIA RTX.dll')
    Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $backup 'manifest.json')
    $gameDll = $null
    if ($game -and (Test-Path -LiteralPath (Join-Path $game 'NVIDIA RTX.dll') -PathType Leaf)) {
        $gameDll = Join-Path $game 'NVIDIA RTX.dll'
        Copy-Item -LiteralPath $gameDll -Destination (Join-Path $backup 'NVIDIA RTX.GTA-original.dll')
    }
    $manifest.Files.'NVIDIA RTX.dll' = $newHash
    $manifest.Build = 'R658'
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    $manifestText = $manifest | ConvertTo-Json -Depth 20
    try {
        # No changes to any other graphics binaries or game settings.
        Get-Process ZinkGTAUI -ErrorAction SilentlyContinue | Where-Object {
            try { (Split-Path -Parent $_.Path) -ieq $app } catch { $false }
        } | Stop-Process -Force -ErrorAction Stop
        Copy-Item -LiteralPath $newDll -Destination $oldDll -Force
        [IO.File]::WriteAllText($manifestPath,$manifestText,$utf8)
        if ($gameDll) { Copy-Item -LiteralPath $newDll -Destination $gameDll -Force }
        if ((Sha $oldDll) -ne $newHash) { throw 'App DLL hash verification failed.' }
        if ($gameDll -and (Sha $gameDll) -ne $newHash) { throw 'GTA DLL hash verification failed.' }
        $verify = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        if ($verify.Build -ne 'R658' -or $verify.Files.'NVIDIA RTX.dll' -ne $newHash) { throw 'App manifest verification failed.' }
    } catch {
        Copy-Item -LiteralPath (Join-Path $backup 'NVIDIA RTX.dll') -Destination $oldDll -Force
        Copy-Item -LiteralPath (Join-Path $backup 'manifest.json') -Destination $manifestPath -Force
        if ($gameDll) { Copy-Item -LiteralPath (Join-Path $backup 'NVIDIA RTX.GTA-original.dll') -Destination $gameDll -Force }
        throw
    }
    Status 'R658 installed and hashes verified.'
    Status ('App backend updated: ' + $oldDll)
    if ($gameDll) { Status ('GTA mod updated: ' + $gameDll) }
    Status ('Originals backed up to: ' + $backup)
    Status ('Exact R658 source ZIP saved to: ' + $downloads)
    Status ('Build log: ' + $buildLog)
    Write-Host 'Open Zink GTA Enhancement normally. Your existing DLSS, XeSS, FG, overlays, settings, ghosting/freezing fixes and other DLLs were preserved.' -ForegroundColor Green
    Write-Warning 'This fixes fatal handling of NVIDIA SL39 warnings. Actual VRAM exhaustion or a driver reset can still crash GTA; live gameplay verification is required.'
} finally {
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Force -Recurse -ErrorAction SilentlyContinue }
}
