# GTA 5 LEGACY — Intel XeSS, DLSS 4/4.5/5 and Frame Generation

Zink GTA Enhancement is a Windows 11 companion app and GTA V Legacy mod project for selecting and managing Intel XeSS or NVIDIA RTX rendering features from one Fluent UI interface.

Current release: **App 3.2.1 / Mod R656**.

## Download the ready-to-run app

Open the [`exe`](exe/) folder for the latest packaged Windows download. The full ZIP is published as a GitHub Release because its verified graphics runtimes exceed GitHub's 100 MB per-file repository limit.

## Features

- Intel XeSS upscaling and XeSS frame-generation controls.
- NVIDIA DLSS 4, DLSS 4.5 and experimental DLSS 5 paths.
- RTX 40-series experimental neural-rendering test with honest runtime reporting and fallback behaviour.
- Frame-generation controls, including supported multiplier selection.
- Automatic GTA V Legacy discovery and settings management.
- Manual rendering-GPU selection using the real DXGI adapter list.
- In-game performance overlay reporting the active provider, runtime versions and frame generation.
- Fullscreen/output-resolution management and automatic settings application.
- Optional NaturalVision Evolved installation automation and safe reversal.
- Backups and restoration of the GTA directory state from before mod installation.

## Requirements

- Windows 11 x64.
- GTA V Legacy/Story Mode.
- A supported Intel Arc or NVIDIA RTX GPU and current graphics driver.
- NaturalVision Evolved is optional and must be obtained separately from its authorized publisher.

## Important compatibility notes

DLSS 5 is officially hardware- and runtime-dependent. The RTX 40 option in this project is an experimental compatibility path, not official RTX 40 DLSS 5 support. The app reports verified runtime evidence rather than treating configuration alone as proof that a feature ran.

The Intel DLSS 5 experiment is a vendor-neutral DirectML post-processing route. Intel hardware does not gain native NVIDIA NGX support; XeSS remains the Intel upscaler.

## Building

The Windows app is under `Source/App`. Native mod components and tests are under `Source/Mod`.

```powershell
dotnet build Source\App\ZinkGTAUI.csproj -c Release -p:Platform=x64
dotnet run --project Source\Mod\tests\AppBackendRegression\AppBackendRegression.csproj -c Release
```

Some native components require Visual Studio 2022/2026 C++ build tools, the Windows SDK, and separately licensed vendor SDK files. See the project files and third-party notices for the expected paths and dependencies.

## Visual graphics mod

The source includes the NVE integration, progress UI, installer invocation, loader configuration, validation and rollback logic. The NaturalVision Evolved archive itself is not redistributed here because it is a large third-party download with separate distribution terms. Download it from the official source, then let the app detect and install it.

## Licences and third-party software

This repository contains project source and third-party notices. Vendor runtimes in the release package remain governed by their respective licences. See [`THIRD_PARTY.md`](THIRD_PARTY.md) and the notice files under `Backend` and `Source/Mod`.

No blanket open-source licence is granted for original project code unless a licence file is added later by the repository owner.

