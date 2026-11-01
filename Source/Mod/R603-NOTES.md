# R603 final-use resource lifetime

R602 failed with DEVICE_HUNG during XeFG Present after8.797seconds. The log has no DRED breadcrumbs and does not prove a specific faulting command.

Source audit found a resource-lifetime defect: startup/recovery seed copies are submitted after the Present fence, but the job originally retired against the earlier Present marker. Native colour handback also preceded the seed copies. R603 retains the source slot against the latest stage marker and signals the native return fence after all colour consumers. Seed allocator Reset now requires its last submission fence to complete; it defers without blocking when busy. A removed-device fence sentinel is not accepted as completion. Failed seed fence signals retain ownership rather than retiring against an earlier marker.

Kept uncapped source FPS, direct shared colour, separate preparation queue and post-sleep refresh. Protected ghosting/camera/freezing sources unchanged.

Tests: production-extracted retirement regression includes incomplete seed use, Present-before-seed completion, removed device and10000reuse cycles. Arc A770 GPU stress covers256full-HD frames with8in flight and an additional late copy before handback, with exact pixel comparison. DLL integrity and Release build pass. Existing C4244 warning remains. No claim of GTA stability until gameplay verification.
