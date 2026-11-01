# R601 synchronization repair

Installed with matching binary hashes. SourceFPS=0, DirectSharedColor=1, LatestCompleted=1. No source FPS cap restored.

DirectSharedTexture now owns a D3D12 shared return fence opened on native D3D11. After the proxy colour copy the consumer signals this fence; before reusing that texture the native queue waits for that return value. This explicitly orders both directions without a CPU GPU-completion wait. Existing slot retirement remains. Colour incoming-state metadata matches COMMON for direct textures. Preparation now uses its own queue, retaining native-capture -> preparation -> XeFG fence dependencies; the shared preparation/presentation queue option is disabled.

Tests:2561920x1080 transfers with8 submissions in flight, every pixel of every output checked on Arc A770; all pass. CaptureRegression, ResetRegression, CompletedFramePolicy and DLL integrity pass. Release build passes with existing C4244 warnings.

The R600 log establishes DEVICE_HUNG during interpolated presentation but does not identify the precise offending command. This patch addresses ownership ordering and separates queue domains; it is not proof of the original root cause. Full GTA/XeFG stability and performance remain to be verified in gameplay. Protected camera/history and FreezingFix sources were not modified.
