# R631 — visible SR/NR output with frame generation disabled

User FG-off test log 20261029-025051 shows NR evaluates three frames and NVIDIA reports actual=1/status=0, but no output is revealed. Startup resets the hidden warmup when resetHistory is set, then treats E_PENDING from the still-busy seed allocator as fatal, resulting in startup_timeout and normal GTA fallback.

FG-off startup now accepts the successful single-frame result without requiring FG's history-reset gate to clear. A pending seed allocator is retried without falsely marking buffers seeded or stopping the pipeline. The FG input-processing retirement wait is skipped when FG is disabled; ordinary ordered queue fence retirement remains. The HUD now says UPSCALING / FG OFF instead of showing the configured x6 multiplier while disabled. Protected camera/ghosting/freezing sources unchanged.

Native integrity build passed; WinUI build passed with no warnings/errors. Real GTA 4K Quality DLSS5/FG-off output visually confirmed. Logs reached over 840 successful NR evaluations; actual=1/status=0; telemetry at 61 seconds reports visible=1, generated=0, no xess/xefg/device errors. Evidence: profiling/R631-FG-off-success/. This validates the isolation-test startup, not a fix for the reported moving-object trails.

Installed app/runtime updated together. DLSS5 and 4K Quality remain selected; FrameGeneration remains disabled. User should repeat the same movement to determine whether trailing remains without generated frames.
