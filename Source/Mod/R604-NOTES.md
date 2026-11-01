# R604 native-owned sharing and GPU capture timings

R603 failed with DEVICE_HUNG. Latest healthy telemetry source54.13FPS/output108.72FPS; final rates include failure/drain time. Sampled CPU colour/fence submission median0.805ms, depth capture0.145ms, worker preparation0.437ms, private Present9.067ms. CPU wall times do not measure GPU contention or identify the native-FPS bottleneck.

Create shared BGRA on native D3D11 using SHARED|SHARED_NTHANDLE, opening the NT handle directly in D3D12. Arc A770 exposes D3D12 flags33(ALLOW_RENDER_TARGET|ALLOW_SIMULTANEOUS_ACCESS); previous D3D12-created allocation requested flags1. This corrects an interop compatibility difference; it is not proof of the precise hung command. Keeps eliminated intermediate copy and explicit final-use ownership.

Skip native Wait submission when the return fence is already complete; an incomplete handback still waits on GPU. Removed-device sentinel returns failure.

NativeGpuTimer measures native depth packing and native handback/colour copy. Poll only on slot reuse with DONOTFLUSH. No query waits or overwriting unresolved samples. R604 NATIVE_GPU_CAPTURE_TIMING reports validity, prior same-slot timing and queued/skipped waits. These intervals exclude full GTA rendering and XeFG internal work.

Tests:256full-HD frames with8in flight and a late copy before handback; every pixel verified and asynchronous timing validated on Arc A770. Release build and DLL integrity pass with existing C4244 warnings. Runtime hashes verified on installation. Uncapped source retained. Protected ghosting/freezing code unchanged. In-game stability and FPS improvement remain unconfirmed.
