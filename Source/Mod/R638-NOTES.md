# R638 — capture actual FG motion pairs

R637 video confirmed persistent doubled character/floor artifacts at 67 seconds. Existing sampled B486 records show matching colour/depth source IDs; GhostingAlignWorkerPair rebases the worker camera to the last successfully presented frame. Neither observation proves per-pixel motion correctness.

R638 changes diagnostics only: F11 keeps the three pre-FG images and now copies previous/current NVOFA luminance inputs, original camera vectors, actual FG vectors, and depth. motion-pair.txt records dimensions/formats/row bytes, processed-frame IDs, last successful-present ID, CPU estimation interval and clip-to-previous matrix. Previous luminance is explicitly marked invalid when no history estimate was applied. No generated frames are captured.

Copies run after motion resolve on the same queue, restore resource states, and use a completion fence. CPU maps only completed data. Approximately 52 MB extra readback at 2560x1440 allocated once on request, freed after export. F11 can cause a one-off capture/export hitch. No normal-play readback, interpolation tuning, camera/protected module changes, or settings changes. Existing one-capture-per-process policy retained.

Validated final build in standalone 4K Quality/x6: 100/100 NR evaluations,100/100 full-multiplier frames,600 actual presents. Frame11 packet previous estimate10/previous presented10 match. Raw sizes verified; no nonfinite camera/FG vectors. Changed-pixel warp error improved .2774 to .0589 on synthetic scene. This tests capture correctness/integration, not GTA visual quality. Parser profiling/analyze_fg_pair.py writes derived previews/metrics separately.

Native build and WinUI build passed (WinUI 0 warnings/errors). R638 source/installed payloads and GTA verified against manifest. Current NVIDIA DLSS5/4K Quality/x6 preserved. Backup 20261029-204612-178-runtime-dd9109. GTA not launched. Next user action: Load GTA + mod, reproduce blur, F11 once while moving. No additional video required for this packet.
