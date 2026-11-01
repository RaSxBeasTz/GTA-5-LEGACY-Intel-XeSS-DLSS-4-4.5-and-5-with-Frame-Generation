# R658 candidate — NVIDIA DLSS VRAM-warning startup crash

## Observed crash evidence

The uploaded `GTAXeSS-NVIDIA.log` contains earlier successful NVIDIA Streamline 2.14.16 sessions with 4K DLSS + x6 FG. Its final session is 3840x2160, x2 FG, neural rendering enabled and visual enhancement enabled. At frame 21 the NVIDIA FG status reports `eWarnOutOfVRAM` (SL 39). In the same session, the DLSS evaluate call reports `DLSS DLAA evaluation: SL 39`, followed by another nonfatal FG-options VRAM warning at frame 22. The log then stops. No crash exception code or D3D device-removed reason is present in this log.

## Root cause found in source

`Source/Mod/NvidiaRtx/NvidiaDlaa.h` passed every DLSS evaluation result to `rtx::check`, whose implementation in `StreamlineRuntime.h` throws for any non-`eOk` value. This escalates SDK warning SL 39 to a failed NVIDIA draw, despite R629 already handling the identical warning nonfatally in DLSS-G state/options calls.

## Minimal change

At the DLSS evaluate call only, treat `sl::Result::eWarnOutOfVRAM` as a warning; rate-limit a specific `R658: DLSS_EVALUATE_VRAM_WARNING` log and retain the same rendering pipeline. Other evaluation errors still throw, and the existing source, DLSS, neural rendering, visual enhancement, FG, camera, ghosting, freezing, HUD and other paths are untouched.

## Validation boundary

- Static diff: one graphics source file changed, plus these notes; protected camera/ghosting/freezing files unchanged.
- This is a targeted code fix for fatal warning escalation, **not** proof that underlying GPU memory pressure has disappeared. Windows x64 native compilation, release packaging, and GTA gameplay testing have not been performed in this environment.
- If the game still fails after rebuilding, capture a new `GTAXeSS-NVIDIA.log`, Streamline log, ScriptHook log, and Windows exception/device-removed evidence to distinguish memory exhaustion from a separate GPU crash.
