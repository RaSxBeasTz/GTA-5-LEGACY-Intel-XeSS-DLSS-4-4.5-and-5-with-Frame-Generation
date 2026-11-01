# R618 direct programmatic PIX capture

R617 20261020-081753 log: profiling preload succeeded in GTA PID51904, one-shot flag disarmed, F11 triggered. External pixtool attach failed PIXTOOL17 Process not launched for GPU Capture. No wpix produced. Mod ran45s,86.5 source/165.1 output, no Present/tag/status errors; these instrumented numbers are not ordinary performance baseline.

Replaced external attach with the capture helper used by Microsoft's pix3_win.h: dynamically resolve SetGlobalTargetWindow(void WINAPI(HWND)) and CaptureNextFrame(HRESULT WINAPI(PCWSTR,UINT32)) from already-loaded WinPixGpuCapturer.dll. ABI verified from official Microsoft PixEvents main/include/pix3_win.h. No arbitrary undocumented function signatures.

F11 callback/fallback asks helper B618RequestGpuCapture. Only profiling-enabled process with live, visible FG accepts. Atomic one-shot prevents duplicate callback/fallback captures. Existing serial FG worker consumes request before next frame preparation, targets actual XeFG HWND and queues4 Present-to-Present frames. File saves directly in GTA root as GTAXeSS-PIX-<PID>.wpix. R618 PIX_GPU_CAPTURE_REQUEST and PIX_GPU_CAPTURE_ENQUEUED log HRESULTs. No external watcher, attach, remote monitor or alt-tab needed. Existing preloader remains one-shot PIXCapture=1->0; normal next launch uninstrumented.

Validation: Release build/integrity passes existing C4244 warnings. Actual D3D12 hidden-window PixGpuCaptureSmoke queues programmatic2frames, clears backbuffers, presents and produces5640-byte wpix. Capturer emitted diagnostic stack text during smoke; output capture was then reopened with installed pixtool, analysis succeeded, exit0, and event-list CSV includes ResourceBarrier/ClearRenderTargetView. This verifies end-to-end programmatic capture works independently of external attach. In-game XeFG/shared-resource capture still requires confirmation.

Four installed runtime files SHA256 verified. R616 rendering, one-frame default, R611 camera fix, protected shader/freezing unchanged. Backup pre-R618-* contains Restore.ps1/pre-opt-in INI. Game INI rearmed PIXCapture=1. Old external watcher exited; do not relaunch Capture-PixF11.ps1 for R618.

Test: restart GTA, F9, let gameplay settle and proxy appear, pressF11once. Allow capture to finish, tell agent. Agent finds GTA-root GTAXeSS-PIX-*.wpix locally. No F8. Capture can pause rendering, and capture-enabled run is not used for normal FPS comparisons. Restart afterward returns normal profile-off mode.

Sources: https://devblogs.microsoft.com/pix/programmatic-capture/ and https://github.com/microsoft/PixEvents/blob/main/include/pix3_win.h .
