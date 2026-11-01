# R636 — F11 evidence capture for unresolved blur

Diagnostic-only change to the accepted R635 rendering path. No optical motion integration, feature/preset/multiplier changes or protected module edits. Previous R634 motion-estimation experiment remains rejected.

With Diagnostics/BlurCaptureOnF11=1, F11 queues a single same-processed-frame GPU capture per process: original game source before SR, SR output before NR, NR output before FG. Three BMPs plus frame metadata are saved in GTA/GTAXeSS-BlurCapture-<serial>/. User must mark visible blur while moving, then allow several frames for asynchronous completion. No alt-tab or external recorder required. This does NOT capture actual interpolated frames; if the before-FG image is clean, FG remains suspect, not automatically proven faulty by this capture alone.

Readback resources are allocated only on request. Allocation failures are caught before recording barriers. Copies preserve resource states. CPU Map only occurs after GPU completion; no new CPU fence waits. Export runs once and may cause a brief hitch while writing images. Device-removal completion value cancels capture. Legacy NRComparison mode retained.

Validation: native protected-source build passed, WinUI 0 warnings/errors. Standalone 4K Quality FP16 game source + NR/x6, real F11 key at frame 11, all three BMPs exported and inspected: source 2560x1440, SR/NR 3840x2160, correct varied colours and moving rectangle. Generation completed: 100 NR evaluations, 100/100 full multiplier, 600 presents. Evidence: profiling/R636-smoke.txt, NvidiaRtx/x64/Release/GTAXeSS-BlurCapture-2461109/.

Installed app/runtime R636, all 27 manifest entries verified in source bundle, installed bundle and GTA. Current DLSS5/4K Quality/x6 settings kept, NRComparison=0, BlurCaptureOnF11=1, temporary native Probe=0. No GTA automatic launch. In-game marked capture is required before claiming a blur fix.
