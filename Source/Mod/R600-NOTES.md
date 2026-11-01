# WITHDRAWN: GPU hang in full GTA/XeFG run

2026-10-19 12:38 run: device removed reason 0x887A0006 (DEVICE_HUNG), interpolated Present returned 0x887A0005; stopped after2.64seconds and57completed jobs. Transfer tests did not cover this integrated failure. No attribution to an individual redesign feature is established. Installed exact pre-R600 B560 binaries and45FPS stable pacing. All experimental flags now default off in source and INI. The staged R600 binaries are withdrawn and must be rebuilt before any further test.

# R600 experimental build

Installed into GTA with SHA256 verification on 2026-10-19. Source lives in redesign/, separately from B560. Previous installation and Restore.ps1 are in the folder recorded in ROLLBACK-PATH.txt.

Changes:
- D3D12-owned shared BGRA colour allocation opened directly on native D3D11. Removes an intermediate On12 colour copy when compatible; retains fallback on unsupported resources. Native backbuffer capture and proxy prefill copies remain.
- Preparation and XeFG share one D3D12 queue, removing cross-queue preparation waits. Native capture fence ordering remains mandatory.
- Select newest GPU-completed queued input; retain incomplete tail. Bound admission before expensive capture. Discards retain retirement tracking, camera epochs and discontinuity protection.
- SourceFPS=0. Diagnostics report actual queue sharing and direct-colour use.
- Optional forensic capture defaults off, preventing F10 recorder collision. Opt-in capture saves original RG16F data alongside existing diagnostics. Native object-motion integration remains disabled: old false-colour packets do not validate vector units, direction, jitter or moving-object correspondence.

Verification:
- Full Release/x64 build passes (existing C4244 warnings remain).
- Intel Arc A770:128 exact BGRA producer/consumer transfer and reuse cycles pass.
- Direct D24 and R32 sharing rejected by driver; retained existing packed depth transport.
- Completed-frame policy:20000 asynchronous ticks pass, including incomplete tails, missing fence and removed-device handling.
- CaptureRegression passes.
- ResetRegression passes:4410 recorded frames,10000 collapsed source batches, recovery epochs, four shaders.
- DLL integrity accepts matching binary; rejects modified hash, legacy API, missing DLL.
- Locked FreezingFix.cpp SHA256 unchanged:528d8f37e0a46770bc689924bfbc2a10d2e17c01b3998678f8b6cb8e3c362448.

Remaining validation:
Actual GTA source/output FPS, frame pacing, stability and visual quality must be measured during gameplay. These tests do not establish native125 FPS or sustained x4. Native-object-vector integration needs new raw captures and validation before activation. R600 is an experimental transport/scheduling build, not a claim that every performance target is achieved.

Run GTA, activate with F9, drive and pan the camera for60-90 seconds, mark visible problems with F11, exit normally and supply the latest ScriptHook log plus performance/artifact logs. If it regresses, close GTA and run Restore.ps1 from the backup folder.
