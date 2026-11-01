# R608 interpolation/presentation control

Installed after the complete R607 ETW comparison. R607 main-engine DMA occupancy was99.18%in full FG, with approximately73.8%of the8-second window attributed to the mod worker and the explicitly named XeSS-FG Presenter Thread1. This identified an FG-side scheduling/workload cost, not individual shader cost or intrinsic hardware capability.

F8 now runs four phases:normal F9off, capture-only, full x4, visible proxy with interpolation suppressed. Each phase settles3seconds and measures12seconds. Normal x4 is restored on completion. Approximate gameplay test duration60seconds plus transitions. The added control follows full x4 so it starts with a visible, warmed proxy. Both rendered phases require actual proxy visibility before settling/measurement.

Control implementation:only while this benchmark phase is active, the tagged XeFG frameConstants.resetHistory is forced to1. Intel's SDK contract skips interpolation for that frame while showing the rendered source. Keeps capture, D3D12 input preparation, tags, proxy prefill, SDK-enabled state, stable XeLL lifecycle and visible Present. Expected status is existing reset-frame1/result3. This is an interpolation-suppressed/history-reset control, not a disabled or absent SDK. Its cost may include reset-path SDK work. Comparing it with full x4 isolates the extra history-dependent interpolation/pacing workload more closely; it does not provide shader-level profiling.

Defaults off. Phase exit requests one fresh temporal pair and resumes normal interpolation. F8/F9/focus cancellation clears the control; F9 user choice is respected. Existing protected camera/shader/freezing sources unchanged. Uncapped source and R607 normal renderer remain.

Source-only frames/resets throughout the added phase are intentional and must not be reported as new flashing/ghosting regressions from ArtifactFrames alone. CSV labels the row proxy_only_history_reset and the log records R608 PROXY_ONLY_CONTROL transitions.

Tests:production-header runner covers four results, visibility gating, normal completion, cancellation including during proxy control, F9 and focus loss. Trace companion/parser accepts legacy three-phase and new four-phase tests; mocked continuous-recording success/abort/start/save failures pass. Release build and DLL integrity pass; existing C4244 warnings remain. Installed runtime SHA256 verified. R607 backup includes Restore.ps1. No real R608 gameplay test has been performed by the agent.

Run:restart GTA, F9, remain stationary in the same3D scene, F8, wait about75seconds, send latest Comparison CSV and ScriptHook log. No ETW recorder or admin launch is required for this next timing comparison.
