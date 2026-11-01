# R629 — NVIDIA warning handling and scratch image memory

Reproduced 4K Quality / DLSS5 / x6 stopping at approximately 47 seconds and 625 processed frames. NVIDIA log reported SL 39 from actual-present-state and FG-options calls. The pinned SDK defines 39 as eWarnOutOfVRAM. The wrapper previously threw on every nonzero result, turning the warning into a fatal stop. No new Windows GTA crash event was recorded.

NVIDIA FG options/state calls now distinguish that warning from genuine errors, rate-limit its diagnostic logging and retain the existing actual-frame-count/status checks. Other Streamline errors still throw. The suspected queue wait was not changed after reproduction identified the warning.

SR and NR scratch output images are now one pair rather than a pair for each of 16 transport slots. They are written/read on the same ordered graphics queue, consumed by the output draw into separate swapchain backbuffers, and returned to UAV state. Input guides and per-slot descriptors remain separate. At 3840x2160 FP16 this reduces maximum scratch allocation from 32 images to 2 (about 1.85 GiB saved when all transport slots were populated). Protected camera, ghosting and freezing sources remain unchanged.

Validation: native protected-source build passed. User played the test DLL for two minutes without a crash/freeze. Run 20261029-022615 reached over 2,280 successful NR evaluations and successful x6 status reports; no xess/xefg/device errors in telemetry. This is not a claim of indefinite stability. Reported character/object blur remains unresolved; camera/depth reconstructed vectors do not describe all independent object motion. Need an actual moving-scene before/after NR comparison to establish where blur originates.

Installed app and game runtime updated to R629. Current user settings preserved. Original failure evidence: profiling/R629-reproduced/. Native diagnostic thread snapshot: profiling/R629-freeze-stacks.txt.
