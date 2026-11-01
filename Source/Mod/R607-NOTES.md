# R607 comparison frame-ID sequencing

User R606 comparison39423718: F9off125.5FPS; capture-only125.667FPS, depthGPU0.0390774ms and colourGPU0.0389629ms,1508samples each. The full phase never measured: XeLL sleep returned INVALID_ARGUMENT(-4) on PresentID2260, then required markers failed with-8. No GPU-device removal was logged for this comparison failure.

Capture-only reused the normal queue consumer, allocating real Present IDs for1885inputs without any corresponding XeLL/Present calls. R607 lets the consumer dequeue capture-only packets with ID reservation disabled. Actual FG consumes the next real ID; refresh retains it as before. Both completed-first and legacy queue paths honor this option. Retirement and camera snapshots are unchanged.

Production-extracted regression processes10000capture-only packets without advancing NextPresentId, then verifies the first real FG job gets the immediately next ID. Existing refresh, retirement, epoch and disposition assertions also pass. Release build and DLL integrity pass. Runtime installation SHA256 verified. Existing C4244 warnings remain.

The completed off/capture measurements support native capture being negligible in this scene; full FG GPU cost remains unmeasured. Restart GTA completely because the previous test marked the pipeline failed, then F9 followed by F8 and repeat the stationary approximately1minute comparison. Normal R605 pacing and uncapped source settings retained.
