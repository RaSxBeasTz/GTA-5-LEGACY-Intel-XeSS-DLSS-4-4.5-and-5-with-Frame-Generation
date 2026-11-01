# R635 — rollback rejected R634 object motion

User reports R634 made blur/ghost trails worse (not primarily stutter). Latest run 20261029-154915 confirms R634 motion active at 2560x1440, SDK x6 status=0. Therefore synthetic optical-flow correctness and successful generated-frame counts did not establish scene quality.

Removed OpticalMotion integration entirely from NVIDIA runtime, restoring pre-R632 NvidiaBackend.cpp from profiling/R631-NvidiaBackend.cpp and the original SR/NR/FG path in NvidiaDlaa.h. Full-resolution dilated camera vectors retained, matching the actual worker shader. Temporary native-candidate probe remains removed and disabled. No protected camera/ghosting/freezing changes. SDK and standalone test sources retained for research; they are not invoked by the mod.

R634 changed guides for all SR, NR and FG stages; image-derived flow can follow illumination/occlusion rather than actual geometry. This is a plausible regression mechanism, not a proven unique cause. Do not redeploy optical motion based solely on moving-pattern tests. Original FG-enabled blur remains unresolved. Next investigation needs actual rendered-frame evidence before modifying motion inputs again.

Native and WinUI builds passed. All 27 manifest hashes verified in source app bundle, installed app bundle and GTA. App/bridge Build R635. Existing DLSS5/4K Quality/x6 selections and glass UI preserved. GTA was closed and was not relaunched. Rejected source/log evidence in profiling/R634-rejected/. Runtime backup in LocalAppData/ZinkGTAUI/backups/20261029-155213-195-runtime-c5fc19.
