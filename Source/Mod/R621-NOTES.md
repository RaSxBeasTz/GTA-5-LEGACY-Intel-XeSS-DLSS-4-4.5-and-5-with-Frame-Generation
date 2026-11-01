# R621 fence gate diagnosis

R620 comparison aborted after four attempts, three suppressed native images, fallback reason 1, aggregate gate wait 42.542 ms. No experimental FPS window completed. Baseline 77.3333 source / 171 output FPS. Do not infer a performance improvement or blame the camera from this result.

The experiment adds a CPU wait on the prior native GPU frame. Existing sampled R604 handback counters report zero queued colour handback waits; this does not explain the gate failure or rule out unsampled waits.

R621 logs gate_exit (0 complete, 1 deadline, 2 device removed, 3 event setup failure, 4 wait API failure, 5 FG output/focus no longer ready), HRESULT, wait result, requested fence value and completed fence value. It also resamples completion after waiting, correcting a deadline/completion race that previously tested the pre-wait value. Timeout remains 20 ms and fallback remains normal native presentation. Camera, temporal, colour ownership and FG shaders are unchanged. This is diagnostic, not a demonstrated performance fix.

Validation: actual Arc smoke test (64 render/fence cycles, readback, capture exactly once, fallback paths and explicit deadline diagnostics) passes. Release build/protected source integrity passes; existing C4244 warnings remain. Four runtime files installed and hash verified with game closed; INI and installed freezing DLL unchanged. F9 then F8 performs the same comparison, now with an explicit gate diagnosis.
