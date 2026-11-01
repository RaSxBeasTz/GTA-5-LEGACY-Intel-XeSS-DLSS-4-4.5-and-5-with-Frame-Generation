# R623 — NVIDIA RTX backend and normal app launch

Installed app: `%LOCALAPPDATA%\Programs\Zink GTA Enhancement\ZinkGTAUI.exe`.
Desktop and Start menu entry: **Zink GTA Enhancement**. No terminal/BAT launcher is needed.

## NVIDIA implementation

`NvidiaRtx/NVIDIA RTX.dll` uses the official production NVIDIA Streamline 2.14.16 SDK. The Intel implementation is selected independently by `[Graphics] Provider=Intel`; NVIDIA uses `Provider=NVIDIA`. The NVIDIA DLL contains the SDK integration, Reflex markers/sleep, native-resolution DLSS DLAA, and DLSS Frame Generation. The main helper routes frame/resource data through a narrow DLL boundary.

The app asks the installed driver for actual capabilities in a separate probe process. It permits x2 on capable RTX 40-series and x2/x4/x6 on capable RTX 50-series drivers. Runtime initialization independently validates the requested multiplier. No inference from GPU naming alone, SDK interposer edits, borrowed application ID, fake generated-frame counters, or global GPU settings are used.

This build preserves matching GTA/output resolution (1080p, 1440p, 4K). Its optional DLSS mode is **DLAA at native resolution**, active with the FG pipeline. DLSS Quality/Balanced/Performance Super Resolution modes are not implemented. DLAA is not a base-FPS optimization and can increase GPU cost. It is off by default.

NVIDIA uses native D3D12 objects for D3D11On12/shared-resource interop, and Streamline proxies for hooked queue creation and swapchain presentation. Existing shared-colour, protected camera/motion transforms, source-window gates and frame retirement are reused. NVIDIA actual presentation counts and input-processing completion fences are collected before the helper emits its retirement marker. The Intel-only frame-latency mutations and F8 comparison are bypassed on NVIDIA.

Settings: `[NVIDIA] Multiplier=2|4|6`, `DLAA=0|1`. Restart GTA after changing provider/settings. GTA must render on the selected provider's GPU; this app does not change Windows GPU assignments or monitor connections. F9 still activates/stops the mod. Intel remains the saved provider after installation; select NVIDIA and Apply to use the new backend.

## Validation

- Native backend and WinUI Release builds pass; WinUI has zero warnings/errors. Existing C4244 helper conversion warnings remain.
- Protected source lock passes. Installed ghosting/freezing DLLs match their prior R622 binaries.
- RTX 5070 / current driver: DLSS supported, FG supported, maximum multiplier 6.
- GPU smoke test: x2, 100 source frames, 200 actual presentations.
- GPU smoke test with native DLSS DLAA and non-activating proxy-window style: x4, 100 source frames, 400 actual presentations.
- Final GPU smoke test with DLAA, proxy-window style and the mod's ALLOW_TEARING | DO_NOT_WAIT flags: x6, 100 source frames, 600 actual presentations. See `profiling/R623-nvidia-smoke-x6-final.txt`.
- Driver suppresses FG when the process loses foreground focus. A test run without focus correctly returned only originals; the focused final run passed. This is not hidden or counted as generated output.
- Backend regressions pass for native resolution agreement, provider/multiplier/DLAA round trips, Intel policy restoration, invalid multiplier rejection, comments/unknown settings, duplicate keys, backups, partial-apply rollback, running-game guards, profiles and manifest verification.
- Installed app launched through the Desktop shortcut. Provider controls and live SDK capability detection verified through native UI.
- All 23 payload files verified against SHA256 in the app bundle and GTA folder.

These are standalone GPU and application tests, **not GTA gameplay validation**. Ghosting, HUD interpolation, camera motion, pacing, focus recovery, and actual in-game performance need a gameplay test. NVIDIA DLAA/FG were exercised at 1080p; the native transport's 1440p/4K regression test was completed in R622. No improvement to the previously investigated base-FPS bottleneck is claimed.

## Backups and build

Pre-change sources: `backups/pre-R623-20261021-143733`.
Previous installed runtime: `%LOCALAPPDATA%\ZinkGTAUI\backups\20261022-085052-342-runtime-cba279`.
Previous GTA XML/INI: `%LOCALAPPDATA%\ZinkGTAUI\backups\20261022-085053-074-settings-d3a46c`.

`redesign/Fast-Build.ps1 -StageOnly` builds/stages both providers. NVIDIA headers and production binaries are pinned in `%LOCALAPPDATA%\GTAXeSS\streamline-v2.14.16`, obtained from https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.16 . NVIDIA DLSS/Reflex and Streamline license files are included with the payload.

`redesign/Install-App.ps1` copies the built WinUI application to the stable user install folder and creates its two shortcuts. Existing mod settings and profiles stay in `%LOCALAPPDATA%\ZinkGTAUI`.
