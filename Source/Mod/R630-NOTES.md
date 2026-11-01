# R630 — postprocessed colour handoff to NVIDIA FG

Reviewed user recording Recording 2026-10-29 023435.mp4 (109.2 s). Full-resolution samples at 84, 94 and 98 s show translucent trailing around the character and doubled/distorted edges around the car; strong outdoor highlight clipping is also visible. This is not merely uniform upscaling softness. The recording alone cannot attribute all artifacts to NR, SR or generated frames.

Found a concrete FG contract mismatch: the helper's HUD-less scene tag referred to the earlier input-render-resolution colour, while the private backbuffer contained full-resolution SR and optional Neural Rendering output. FG now receives a matching output-resolution BGRA scene image rendered from the selected SR/NR result, before the mod HUD. That image is copied to the backbuffer and tagged for the same frame. It is retained per actual swapchain buffer until the existing input-retirement synchronization. Descriptors remain transport-slot-owned. The R629 shared SR/NR scratch images and warning handling are retained.

This corrects a verifiable handoff mismatch, not a guarantee that all motion artifacts are fixed. Independent object motion still is not fully described by the camera/depth reconstructed motion vectors. No camera or protected ghosting/freezing code was changed. Highlight clipping is not addressed by this change.

Validation: native protected-source build passed. GPU smoke test: 4K Quality input 2560x1440, x6 plus NR, 100/100 successful NR evaluations and 600 actual presents, full multiplier 100/100. App build passed with no warnings/errors. Real GTA startup verification follows in profiling/R630-in-game/.

Recording analysis frames: profiling/R629-video/. Original video was not modified.

Real GTA verification: FG_COLOUR_MATCH confirms post-SR/NR 3840x2160 scene tags, with successful NR evaluations and actual=6 requested=6 status=0 through at least 600 frames and 56 seconds. No freeze in this short validation. Moving-scene improvement remains unverified.
