# R617 opt-in PIX GPU capture

Installed tools: Microsoft PIX2603.25, Intel GPA and RenderDoc. Installed XeSS3.0.2 developer guide explicitly supports PIX/GPUView, and warns that RenderDoc capture with FG enabled may cause device reset. Chose PIX.

R617 adds PixCaptureOptIn.h: only Diagnostics/PIXCapture=1 loads the installed Microsoft-signed WinPixGpuCapturer.dll2603.25 before the first mod D3D12 API. Covers all four CreateDevice callsites and DRED GetDebugInterface. One-shot INI flag is reset to0 before loading so future launches use normal rendering after restart. Capturer remains loaded until game exit; never unload it from instrumented devices. Full fixed DLL path, safe DLL search flags, no downloaded binaries. Load status/PID goes to GTAXeSS-PIX.log. No change to R616 rendering, camera/temporal/freezing, queue1, x4 or input resolution.

F11 fallback polling now logs the existing visual-artifact marker to trigger external recorder even if ScriptHook keyboard callback misses it. Duplicate markers are harmless because watcher captures once and exits. Existing marker debounce remains.

profiling/Capture-PixF11.ps1 watches only new F11 log lines. Validates PIX preload PID matches current GTA. Starts Microsoft secure remote monitor hidden only if none exists, attaches pixtool, captures4 GPU frames and saves xefg-x4.wpix. Bounded timeout180sec. Copies logs/artifacts/hash and writes COMPLETE/ERROR. It stops only the client/monitor processes it created. No late injection and no F8 required. This is profiling instrumentation; capture pauses are not gameplay performance measurements. Restart GTA after capture before ordinary testing.

Validation: Release build/integrity pass existing C4244 warnings. Dedicated PixBootstrapSmoke creates an actual PIX-instrumented D3D12 device successfully and verifies the test INI flag changed1->0. Recorder syntax verified. First watcher launch under Windows PowerShell inherited incompatible module paths and stopped before READY; relaunched using installed pwsh, which is confirmed READY/live. End-to-end in-game GPU capture remains pending.

Four installed runtime files SHA256 verified; backup pre-R617-* includes pre-opt-in INI and Restore.ps1. Game INI armed PIXCapture=1 for next process. Protected ghosting/freezing unchanged. Recorder readiness path profiling/PIX-READY.txt. Current hidden watcher PID46648. Use pwsh.exe -NoProfile -ExecutionPolicy Bypass -File profiling/Capture-PixF11.ps1 when rearming a recorder; setting PIXCapture=1 requires GTA closed and restart.

Run restart GTA, F9, settle normal gameplay, F11 once. Tell agent when done. Do not use F8. No alt-tab or external command required. Do not infer an FPS improvement from this build.

Primary reference: https://devblogs.microsoft.com/pix/programmatic-capture/ . PIX capture DLL must be loaded before D3D12 APIs. Installed SDK guide: %LOCALAPPDATA%/GTAXeSS/xess/doc/xess_fg_developer_guide_english.md Compatibility with Performance Analysis and Debugging Tools.
