# R627 — NVIDIA startup and HUD crash fix

The 28 October 01:39 log never reached NVIDIA rendering: input discovery rejected GTA's 32-bit typeless depth format. Discovery and private depth transport now support R32G8X24_TYPELESS with its matching R32_FLOAT_X8X24 SRV, alongside the existing R24G8 path. The actual source format is passed to preparation before the private ring is created. The old camera/ghosting/freezing sources remain hash locked.

GTA clamps tiny windowed render heights. The app now moves to the next SDK quality preset when the recommended input height is below 560, rather than requesting a custom size that DLSS rejects. For 1440p output this changes Ultra Performance (853x480) to Performance (1280x720). Output and user-selected DLSS version/multiplier remain unchanged. Closed zombie processes no longer falsely trip the app's live-game guard.

After discovery succeeded, first-frame checkpoints showed DLSS evaluation and the full-screen output draw completed, but the HUD glyph ClearRenderTargetView crashed NVIDIA driver 32.0.16.2101 with C0000409. A batch of 64 rectangles also reproduced the crash. Limiting each call to 16 rectangles allowed the same GTA/DLSS/FG session to run beyond 2,800 submissions with actual=6, requested=6, status=0. The HUD's hard-coded XeFG x4 status now uses the selected provider and multiplier.

Validation:
- Native build and protected-source integrity check passed.
- GPU depth-format test passed both typeless depth families with known-depth readback.
- App backend regression suite passed, including quality fallback, settings round trips, race rollback, manifests and installation.
- Real GTA test: 1440p output, 1280x720 Performance input, requested DLSS 4, x6 FG; over one minute without the startup crash. Evidence in profiling/R627-hud16-success/.
- Heavy D3D12 validation disabled for the installed release; bounded first-frame checkpoints remain.

DLSS 5 Neural Rendering remains experimental. This real GTA verification used the user's selected DLSS 4 with NR disabled; it does not certify NR in-game. NVIDIA driver model overrides can change the actual SR model independently of the requested selector.

Final release check: installed app's Load GTA + mod launched the game and automatically activated the mod, with D3D12Validation=0. The final run passed 960 submissions with actual=6 requested=6 status=0; see profiling/R627-final-app-launch/. All 26 manifest files matched source bundle, installed app bundle and GTA runtime.

DLSS 5 follow-up: selected version 5/NR in installed app preferences and GTA INI, preserving 1440p Performance and x6. Real GTA initialization, parameter setup and feature creation each returned 0x1; NR evaluation passed beyond 480 frames, alongside actual=6 requested=6 status=0. Evidence: profiling/R627-DLSS5-in-game/. This verifies execution, not a visual-quality or performance guarantee.
