# R625 — local experimental DLSS 5 integration

The app and runtime are installed with NVIDIA, DLSS Quality, 1440p output, x2 FG, uncapped source FPS, and DLSS 5 enabled. GTA source size is 1707x960 in windowed mode; the existing mod supplies the reconstructed output. Load GTA + mod applies settings and requests Story Mode activation. Older app bridge builds are updated from the bundled backend automatically on a closed-game launch. Steam must already be signed in.

## Rendering

The separate GTAXeSS.DLSS5_nvngx.dll module owns the experimental NGX feature-18 integration. NVIDIA RTX.dll evaluates regular DLSS first, passes the resulting output plus existing depth/motion guides to NR, converts the NR image to the existing output backbuffer, and lets the existing FG path present it. Guide contract, reverse depth, pixel motion scaling, history reset and fixed 0.5 intensity are supplied. UI correction and automatic masking are requested. No claim of perfect HUD masking or visual stability is made before gameplay testing.

NR feature/resources are retained until process exit to avoid destroying in-flight GPU histories across F9 toggles. If initialization/evaluation fails or the contract changes, NR is disabled for the session and ordinary DLSS/FG continues. A restart is required to change NR, provider, render resolution or quality. Intel does not load NR. Native + NR is normalized to native AA so the reconstruction path and automatic activation remain available.

The user-supplied nvngx_dlssnr.dll is copied into this local app/game installation only. The source project's MIT license does not license NVIDIA's runtime. No external release or upload was performed. Original ABI reference attribution is included in DLSS5-reference-LICENSE.txt.

## Verification

- WinUI Release builds: zero warnings/errors.
- Native build passes protected source lock. Camera/temporal/ghosting/freezing sources not edited. Installed ghosting and freezing DLL hashes remain the original accepted values.
- Backend regression passes NR persistence, provider isolation, Native->AA normalization, deferred live changes, plus existing settings/backup/rollback/install tests.
- 1080p Quality + NR + x2: 100/100 full multiplier frames, 200 presentations.
- 1440p Quality + NR + x4 with FG on/off/on: NR evaluated 100/100 source frames; all 100 presentation counts matched, 295 total presentations including 35 reconstructed-only frames.
- App Play was clicked through the Windows UI. It applied 1707x960->2560x1440 Quality/NR/x2 and launched the Steam URI. Steam presented Sign in to Steam, preventing the GTA gameplay test. No authentication interaction was attempted.
- All 26 manifest files matched in the installed app and GTA. NR remains experimental: full GTA startup, actual camera motion, HUD, visual quality, sustained pacing and memory consumption have not been validated in gameplay.

Logs: GTAXeSS-DLSS5.log reports INIT, PARAMETERS, CREATE and successful EVALUATED counts; failures trigger ordinary DLSS/FG fallback. GTAXeSS-NVIDIA.log reports fallback and FG presentation counts.

## Recovery

Runtime backup: %LOCALAPPDATA%/ZinkGTAUI/backups/20261028-012945-098-runtime-f8d720.
Settings backup: %LOCALAPPDATA%/ZinkGTAUI/backups/20261028-012946-541-settings-202b1a.
Source backup: backups/pre-R625-*.
To disable just NR, turn off DLSS 5 in the app, save, quit GTA completely and relaunch. Other DLSS/FG controls remain usable.

Additional fallback GPU check: with GTAXeSS.DLSS5_nvngx.dll deliberately absent from the isolated test folder, NR evaluated 0 frames while ordinary DLSS Quality/x2 FG completed 100/100 matching source frames and 200 actual presentations. The test module was restored. An initial hidden-window run could not acquire foreground focus and did not execute the rendering test; the visible-window rerun passed.
