# R602 post-sleep input selection

R601 completed66.672seconds,2733jobs, no device/present/tag/status errors. Artifact rows show564history resets:561in the first10seconds,3later. Steady output was approximately140FPS. This distinguishes startup reset activity from persistent presentation cost.

R602 refreshes the provisional worker input after the required XeLL sleep and before simulation/render markers, input preparation or XeFG tags. Uses the newest completed pending snapshot, retaining the same Present ID and ordinal. Superseded provisional and queued packets enter existing native-fence retirement tracking. Incomplete tails remain queued; no extra history reset is injected. Queue delay now measures selection time, preventing negative ages when a refreshed frame arrived after worker entry. Refresh diagnostics are sparse.

Kept R601 explicit shared-colour handback, separate preparation queue and SourceFPS=0. Protected camera/history/freezing sources unchanged.

Verification: production-extracted scheduler regression validates empty/future-only queues, superseded retirement, Present ID and ordinal, camera epoch, no duplicate decisions, counters and device removal. ResetRegression, shared-ownership GPU stress and DLL integrity pass. Release build passes with existing C4244 warnings.

Installed with hash verification; gameplay latency and FPS improvement are not established until the next run. Newest-frame selection cannot itself eliminate the XeFG Present service cost.
