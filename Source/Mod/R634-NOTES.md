# R634 — NVIDIA object motion from image-pair optical flow

User supplied Optical Flow SDK 5 headers and samples in Temp. Retained NVIDIA header copyright notices in OpticalFlowSdk/. Only the installed driver's System32/nvofapi64.dll is loaded; no downloaded CUDA/FRUC/FreeImage binaries are used.

OpticalMotion.h is an independent NVIDIA module. It converts the exact pre-SR source into R8 luminance, estimates full-resolution forward and backward flow between consumed image pairs, decodes signed 10.5 vectors into current-to-previous render pixels, and resolves them against the camera field. Forward/backward consistency and a 3x3 photometric improvement gate reject unreliable estimates. Reversed-Z foreground dilation matches the existing full-resolution contract without altering depth. Validated vectors guide SR, NR and FG. Cuts, FG-off, unavailable allocators and mismatched extents invalidate history. Initialization/execution failure disables this module and retains camera guidance; SDK tagging failures remain real errors.

All GPU producer/consumer work is ordered with copy and optical-flow fences, including image reuse and flow-buffer reads. Copy allocators use a three-entry completion-checked ring. No new CPU fence waits/readback in gameplay. Resources stay owned through process lifetime. No protected camera/ghosting/freezing source changed.

Validation on RTX 5070:
- OpticalFlowProbe: API/create/caps/init all success, 640x360.
- OpticalMotionTest: rightward object -8 px, reverse/vertical object (+6,-4), camera-plus-object (-13,+3), reset/reseed. Each moving interior 9900/9900 within tolerance, stationary/camera-only background 36000/36000 correct.
- 4K Quality + NR x6 integration: 100/100 NR evaluations, full multiplier 100/100, actual presents 600. Logs confirm R634 OBJECT_MOTION active=1, guides=2560x1440. Evidence profiling/R634-smoke.txt and NvidiaRtx/x64/Release/GTAXeSS-NVIDIA.log. This is a short synthetic integration check, not proof of long GTA stability or complete visual remediation.
- Native protected-source build passed. WinUI build passed 0 warnings/errors. Source app, installed app and GTA payloads verified against all 27 manifest hashes, Build R634.

Recovery / corrections:
R632 incorrectly set motionVectorsDilated=false for the active full-resolution shader. That shader is already dilated; true is restored. R632 notes are marked superseded.
Temporary native-candidate capture removed from the live helper; helper source matches profiling/R632-B25DepthHelper.cpp. Probe=0. Evidence capture saved under GTA/GTAXeSS-MotionProbe-1084796, frames 2911 and 2912. Four candidates have strong depth-like signatures and are not promoted. Format-19 diagnostic depth dump was truncated (the probe assumed 4-byte pixels); use candidate/color samples as evidence, not that depth file for full-frame quantitative reconstruction.
Windows reported 0x116 VIDEO_TDR_FAILURE naming nvlddmkm.sys_Blackwell after the prior GTA run. WER is readable; kernel minidump debugging was denied by Windows file permissions. No cause is assigned to hardware or a specific mod operation. No GTA auto-launch or long GPU stress run after that crash.

Installed 4K Quality, DLSS5 and saved x6 FG settings retained; Fluent glass UI retained. In-game moving-character/vehicle comparison and longer-run stability are still required. Runtime backup: LocalAppData/ZinkGTAUI/backups/20261029-154622-423-runtime-e0e4b6.

REJECTED: user in-game test reported worse blur/ghost trails. Integration removed in R635. Standalone tests are not proof of visual improvement.
