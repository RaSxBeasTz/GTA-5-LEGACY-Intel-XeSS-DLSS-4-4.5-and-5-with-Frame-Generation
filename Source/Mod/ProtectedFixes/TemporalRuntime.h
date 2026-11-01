#pragma once
#include "CameraRuntime.h"
extern "C" GHOST_API unsigned __stdcall GTAXeSSGhostingApiVersion();
extern "C" GHOST_API BOOL __stdcall GTAXeSSGhostingUnsafeFramePolicy(BOOL);
#if defined(GHOSTING_RUNTIME_BUILD) || defined(GHOSTING_RUNTIME_TEST)
struct TemporalState {
std::atomic<bool> g_b175bXessReset{true};

// B407: final-consumption unsafe-camera episode guard. Camera classification can flicker safe for
// a frame while the underlying unsafe episode continues, so the final worker reset decision uses
// a short grace window instead of trusting a per-source-frame branch latch.
std::atomic<ULONGLONG> g_b407LastUnsafeCameraTick{0};

std::atomic<bool> g_b407UnsafeResetConsumed{false};

std::atomic<bool> g_b408UnsafeEpisodeActive{false};

std::atomic<unsigned> g_b408StableSafeFrames{30};

float g_b175ConsumedViewProjection[16]{};
bool g_b175HaveConsumedViewProjection=false;

float g_b175bXessPreviousViewProjection[16]{};
bool g_b175bHaveXessPreviousViewProjection=false;

SRWLOCK g_b175MotionHistoryLock=SRWLOCK_INIT;

std::atomic<bool> g_b288CameraResetPending{false};
// B535: a validated authoritative camera rebase is a new temporal epoch. This reset must not
// be swallowed by B407 unsafe-episode reset coalescing.
std::atomic<unsigned long long> cameraEpoch{0};
unsigned long long workerCameraEpoch=0;
float workerPreviousVp[16]{};
bool workerHasPrevious=false;
unsigned long long workerPresentedEpoch=0;


std::atomic<unsigned> g_b440InvalidMotionCameraStreak{0};
};
GHOST_API TemporalState& GhostingTemporal();
#endif
// Matrices are row-major with row-vector multiplication, matching GTA V * P.
struct B173GpuConstants{float previousViewProjection[16];float currentInverseViewProjection[16];UINT width,height;float padding[2];float clipToPrevious[16]{};float reserved[12]{};};
static_assert(sizeof(B173GpuConstants)==256,"GPU constant upload must match buffer allocation");
struct GhostingTemporalFrame {
 unsigned long long cameraEpoch{};
 bool align{},valid{},transition{},rebase{},hadConsumed{},hadXess{};
 float validated[16]{};
 const float*effectiveInverse{};const float*effectiveView{};const float*effectiveProjection{};
 B173GpuConstants motion{};
};
struct GhostingCameraPair {
 unsigned sourceFrame{};
 unsigned long long cameraEpoch{};
 bool forceReset{};
 bool b437MotionCameraValid{},b437MotionValidityTransition{};
 float previousViewProjection[16]{},currentViewProjection[16]{},currentInverseViewProjection[16]{},currentView[16]{},currentProjection[16]{};
};
GHOST_API void GhostingValidate(unsigned,const float*,const float*,const float*,const float*,const char*,GhostingTemporalFrame&);
GHOST_API void GhostingPrepareMotion(unsigned,const float*,const char*,GhostingTemporalFrame&);
GHOST_API bool GhostingFinalizePair(unsigned,const float*,const char*,const GhostingTemporalFrame&,GhostingCameraPair&);
GHOST_API unsigned GhostingWorkerReset(const GhostingCameraPair&,bool,unsigned,std::ofstream&);
GHOST_API void GhostingCommitPresented(const float*,unsigned long long epoch=0);
GHOST_API void GhostingAlignWorkerPair(GhostingCameraPair&);
GHOST_API bool GhostingBuildMotionTransform(B173GpuConstants&,const float* currentVp);
GHOST_API void GhostingCommitXess(const float*);
GHOST_API void GhostingInvalidateXess();
GHOST_API void GhostingClearConsumed();
extern "C" GHOST_API void WINAPI B407MarkUnsafeCameraFrame();
extern "C" GHOST_API void WINAPI B408MarkSafeCameraFrame();
GHOST_API // B461: B437 is the final camera authority. Once a recovered authoritative camera has
// passed B437 and B453 has atomically rebased both motion histories, the older B408
// classifier episode must not keep the worker in its 30-safe-frame quarantine.


void WINAPI B297ResetCameraHistory();
extern "C" GHOST_API void WINAPI B345ApplyCameraZeroMotionSafety(BOOL resetTemporalHistory);
GHOST_API void b436_mul4x4(const float*a,const float*b,double*out);
GHOST_API double b436_identity_residual(const double*m);
GHOST_API double b437_view_projection_residual(const float*invVp,const float*view,const float*projection,float*outViewProjection);
GHOST_API void b436_motion_transform_diagnostic(const float*invVp,const float*view,const float*projection,unsigned long long frame,const char*path);
extern "C" GHOST_API unsigned WINAPI B440InvalidMotionCameraStreak();
#if defined(GHOSTING_RUNTIME_BUILD)
#define g_b175ConsumedViewProjection (GhostingTemporal().g_b175ConsumedViewProjection)
#define g_b175HaveConsumedViewProjection (GhostingTemporal().g_b175HaveConsumedViewProjection)
#define g_b175MotionHistoryLock (GhostingTemporal().g_b175MotionHistoryLock)
#define g_b175bHaveXessPreviousViewProjection (GhostingTemporal().g_b175bHaveXessPreviousViewProjection)
#define g_b175bXessPreviousViewProjection (GhostingTemporal().g_b175bXessPreviousViewProjection)
#define g_b175bXessReset (GhostingTemporal().g_b175bXessReset)
#define g_b288CameraResetPending (GhostingTemporal().g_b288CameraResetPending)

#define g_b407LastUnsafeCameraTick (GhostingTemporal().g_b407LastUnsafeCameraTick)
#define g_b407UnsafeResetConsumed (GhostingTemporal().g_b407UnsafeResetConsumed)
#define g_b408StableSafeFrames (GhostingTemporal().g_b408StableSafeFrames)
#define g_b408UnsafeEpisodeActive (GhostingTemporal().g_b408UnsafeEpisodeActive)
#define g_b440InvalidMotionCameraStreak (GhostingTemporal().g_b440InvalidMotionCameraStreak)

#endif

GHOST_API void GhostingBindProtectedModuleCheck(bool(*)());

GHOST_API void GhostingRequestXessReset();
GHOST_API bool GhostingTakeXessReset();

GHOST_API const char* GhostingMotionShader0();
GHOST_API const char* GhostingMotionShader1();
GHOST_API const char* GhostingMotionShader2();
GHOST_API const char* GhostingMotionShader3();
