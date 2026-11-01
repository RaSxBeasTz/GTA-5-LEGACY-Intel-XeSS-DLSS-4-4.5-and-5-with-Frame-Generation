R624 app startup correction — 2026-10-22

Observed GTAXeSS-B358-ScriptHook-20261022-140048.log rejects the initial 3840x2160 backbuffer against configured Intel Quality input 2259x1271, before selecting the GTA swapchain or polling auto-activation. ScriptHookV itself loaded and launched the readiness script. The current GTA XML had reverted to 3840x2160, Windowed=0. AutoStart=1 was already set.

ModBackend.Apply now writes Windowed=1 only when the render input is smaller than output. This avoids requesting an SDK-specific render size as an exclusive fullscreen display mode. Native and native-AA settings preserve their existing window mode. UI launch/application messages explain the windowed source. Runtime DLLs, source validation gates, camera, ghosting and freezing code were not changed.

Regression reproduces 4K Quality 2259x1271 with Windowed=0 and verifies Apply retains those dimensions and switches the source to windowed. Backend suite passed. WinUI Release passed with zero warnings/errors. App installed; saved Intel/4K/Quality settings reapplied and verified XML 2259x1271, Windowed=1. Settings backup: %LOCALAPPDATA%/ZinkGTAUI/backups/20261022-140513-117-settings-c36dc3.

Full GTA startup remains to be verified by gameplay. If GTA still overrides the source size, the existing UNSUPPORTED_OBSERVED_RESOLUTION_BEFORE_SOURCE_SELECTED diagnostic reports actual and required sizes. No source-size gate was weakened to mask that failure.
