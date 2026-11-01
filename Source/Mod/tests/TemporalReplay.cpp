#define GHOSTING_RUNTIME_TEST 1
#include "../ProtectedFixes/TemporalRuntime.h"
#include <iostream>
#include <d3dcompiler.h>
#include <stdexcept>
#undef g_b175ConsumedViewProjection
#undef g_b175HaveConsumedViewProjection
#undef g_b175MotionHistoryLock
#undef g_b175bHaveXessPreviousViewProjection
#undef g_b175bXessPreviousViewProjection
#undef g_b175bXessReset
#undef g_b288CameraResetPending
#undef g_b407LastUnsafeCameraTick
#undef g_b407UnsafeResetConsumed
#undef g_b408StableSafeFrames
#undef g_b408UnsafeEpisodeActive
#undef g_b440InvalidMotionCameraStreak
#undef g_b175FastConstants
#undef g_b280CandidateLock
#undef g_b280CandidateUpdateSerial
#undef g_b280CandidateWindows
#undef g_b280PreviousByteCount
#undef g_b280PreviousBytes
#undef g_b280PreviousValid
#undef g_b282LastBindSlot
#undef g_b282LastBindStage
#undef g_b282TargetWriteCount
#undef g_b286IndexedDraws
#undef g_b286LastBaseVertex
#undef g_b286LastCbBytes
#undef g_b286LastCbMask
#undef g_b286LastDsvFormat
#undef g_b286LastDsvH
#undef g_b286LastDsvW
#undef g_b286LastFastColorMatch
#undef g_b286LastFastDepthMatch
#undef g_b286LastIndexCount
#undef g_b286LastRtvFormat
#undef g_b286LastRtvH
#undef g_b286LastRtvW
#undef g_b286LastStartIndex
#undef g_b286LastVs
#undef g_b286ProbeLock
#undef g_b286ProbeSamples
#undef g_b287FrameLock
#undef g_b287LastCbIndex
#undef g_b287LastIndexCount
#undef g_b287LastVs
#undef g_b287LastWriteSerial
#undef g_b287Latest256
#undef g_b287LatestLock
#undef g_b287LatestValid
#undef g_b287LatestWriteSerial
#undef g_b287MainPassDraws
#undef g_b287QualifyingDraws
#undef g_b287RejectedScreenDraws
#undef g_b287SumViewInverse
#undef g_b287SumWorld
#undef g_b287SumWorldView
#undef g_b287SumWorldViewProj
#undef g_b291DirectDeriveDraws
#undef g_b291DirectRejectDraws
#undef g_b291SumDirectProjection
#undef g_b291SumDirectView
#undef g_b292Families
#undef g_b292FamilyCount
#undef g_b292FamilyOverflow
#undef g_b300DirectCbIndex
#undef g_b300DirectMode
#undef g_b300DirectWriteCount
#undef g_b300DirectWriteHead
#undef g_b300DirectWriteLock
#undef g_b300DirectWrites
#undef g_b300DrawHooksReleased
#undef g_b301DirectCbResource
#undef g_b301DirectWriteSerial
#undef g_b303StaleDiscoveryConstantBypassLogged
#undef g_b308AbStartTick
#undef g_b308DirectLockTick
#undef g_b309CandidatesCaptured
#undef g_b309DirectGeneration
#undef g_b309DuplicateSkips
#undef g_b309SelectedUnmaps
#undef g_b321ProfilerActive
#undef g_b322ProfilerActive
#undef g_b324DuplicateSelectedUnmapBypasses
#undef g_b324FirstEligibleCaptures
#undef g_b324SourceFrameEpoch
#undef g_b339SecondChanceBypasses
#undef g_b339SecondChanceCaptures
#undef g_b430RejectedStaticCameraResource
#undef g_b430ResponsiveReacquireActive
#undef g_b431RejectedClassifierDraws
#undef g_b432ValidatedReacquireBaselinePending
#undef g_b434ValidatedBaselineCommitPending
#undef g_b445LatestSelectedResource
#undef g_b445LatestSelectedSerial
#undef g_b445LatestSelectedSnapshot
#undef g_b456FullTupleChanges
#undef g_b456PublishedWrites
#undef g_b456SelectedMaps
#undef g_b456SelectedUnmaps
#undef g_b456ViewInverseChanges
#undef g_b475ChangedLaneMask
#undef g_b475ChangedWrites
#undef g_b475LatestChangedSnapshotFp
#undef g_b479LatestLiveSnapshotFp
#undef g_b479LiveChangedSnapshots
#undef g_b479LiveSelectedUnmaps
#undef g_b479PreviousLiveSnapshotFp
#undef g_b484WatchBytes
#undef g_b484WatchChanges
#undef g_b484WatchLatest
#undef g_b484WatchLatestFp
#undef g_b484WatchLock
#undef g_b484WatchResource
#undef g_b484WatchUnmaps
#undef g_b484WatchValid
#undef g_b489Authoritative
#undef g_b489AuthoritativeLock
#undef g_cb_next
#undef g_cb_overflow
#undef g_known_cb_bytes
#undef g_known_cb_count
#undef g_known_cbs
#undef g_map_next
#undef g_map_overflow
#undef g_snapshot_next
#undef g_snapshot_overflow
#undef t_b309Generation
#undef t_b309LastViewInverseSignature
#undef t_b309LastViewInverseValid
#undef t_b309SelectedOrdinal
#undef t_b324CapturedEpoch
#undef t_b339FirstViewInverseValid
#undef t_b339SecondChanceCaptured
#undef t_b339SecondChanceEpoch
#undef t_b446LastFullSnapshot
#undef t_b446LastFullSnapshotValid
#undef t_b339FirstViewInverse
namespace reference {
static TemporalState state;
static bool align=false;
#define g_b175ConsumedViewProjection state.g_b175ConsumedViewProjection
#define g_b175HaveConsumedViewProjection state.g_b175HaveConsumedViewProjection
#define g_b175MotionHistoryLock state.g_b175MotionHistoryLock
#define g_b175bHaveXessPreviousViewProjection state.g_b175bHaveXessPreviousViewProjection
#define g_b175bXessPreviousViewProjection state.g_b175bXessPreviousViewProjection
#define g_b175bXessReset state.g_b175bXessReset
#define g_b288CameraResetPending state.g_b288CameraResetPending
#define g_b407LastUnsafeCameraTick state.g_b407LastUnsafeCameraTick
#define g_b407UnsafeResetConsumed state.g_b407UnsafeResetConsumed
#define g_b408StableSafeFrames state.g_b408StableSafeFrames
#define g_b408UnsafeEpisodeActive state.g_b408UnsafeEpisodeActive
#define g_b440InvalidMotionCameraStreak state.g_b440InvalidMotionCameraStreak
bool B434ConsumeValidatedBaselineCommit(){bool v=align;align=false;return v;}


static void b436_mul4x4(const float*a,const float*b,double*out){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double v=0.0;for(unsigned k=0;k<4;++k)v+=(double)a[r*4+k]*(double)b[k*4+c];out[r*4+c]=v;}}

static double b436_identity_residual(const double*m){double worst=0.0;for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){const double target=r==c?1.0:0.0;worst=(std::max)(worst,std::fabs(m[r*4+c]-target));}return worst;}

static double b437_view_projection_residual(const float*invVp,const float*view,const float*projection,float*outViewProjection){
 if(!invVp||!view||!projection)return 960.0;
 double viewProj[16]{},vpInv[16]{},invVpVp[16]{};b436_mul4x4(view,projection,viewProj);
 float vpF[16]{};for(unsigned i=0;i<16;++i){vpF[i]=(float)viewProj[i];if(outViewProjection)outViewProjection[i]=vpF[i];}
 b436_mul4x4(vpF,invVp,vpInv);b436_mul4x4(invVp,vpF,invVpVp);
 const double residual=(std::min)(b436_identity_residual(vpInv),b436_identity_residual(invVpVp));
 return std::isfinite(residual)?residual:960.0;
}

static void b436_motion_transform_diagnostic(const float*invVp,const float*view,const float*projection,unsigned long long frame,const char*path){
 if(!invVp||!view||!projection||!path||frame%120)return;
 double viewProj[16]{},projView[16]{},vpInv[16]{},invVpVp[16]{},pvInv[16]{},invPv[16]{};
 b436_mul4x4(view,projection,viewProj);b436_mul4x4(projection,view,projView);
 float vpF[16]{},pvF[16]{};for(unsigned i=0;i<16;++i){vpF[i]=(float)viewProj[i];pvF[i]=(float)projView[i];}
 b436_mul4x4(vpF,invVp,vpInv);b436_mul4x4(invVp,vpF,invVpVp);b436_mul4x4(pvF,invVp,pvInv);b436_mul4x4(invVp,pvF,invPv);
 const double rowOrder=(std::min)(b436_identity_residual(vpInv),b436_identity_residual(invVpVp));
 const double columnOrder=(std::min)(b436_identity_residual(pvInv),b436_identity_residual(invPv));
 const char*best=rowOrder<columnOrder?"view_times_projection":"projection_times_view";
 std::ofstream d(path,std::ios::app);d<<"B436: MOTION_TRANSFORM_CONVENTION_PROBE source_frame_id="<<frame<<", view_projection_inverse_residual="<<rowOrder<<", projection_view_inverse_residual="<<columnOrder<<", best_order="<<best<<", shader_decl=row_major, shader_mul=matrix_times_vector, depth_ndc_range_assumed=zero_to_one, runtime_motion_unchanged=1, diagnostic_only=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";d.flush();
}
void validate(unsigned g_b172GtaFrame,const float*previousViewProjection,const float*currentInverseViewProjection,const float*currentView,const float*currentProjection,const char*path,GhostingTemporalFrame&f){
 const bool b434AlignValidatedBaseline=B434ConsumeValidatedBaselineCommit()!=FALSE;
 auto b447TupleFingerprint=[](const float*vp,const float*inv,const float*view,const float*proj)->unsigned long long{unsigned long long h=1469598103934665603ULL;const float*parts[4]={vp,inv,view,proj};for(const float*part:parts){const unsigned char*b=(const unsigned char*)part;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}}return h;};
 if((g_b172GtaFrame<=6)||(g_b172GtaFrame%120)==0){std::ofstream b447(path,std::ios::app);b447<<"B447: CAMERA_TUPLE_BOUNDARY source_frame_id="<<g_b172GtaFrame<<", boundary=B437_ENTRY, tuple_fingerprint="<<b447TupleFingerprint(previousViewProjection,currentInverseViewProjection,currentView,currentProjection)<<", vp_component=caller_supplied_first_tuple_component, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 b436_motion_transform_diagnostic(currentInverseViewProjection,currentView,currentProjection,g_b172GtaFrame,path);
 float b437ValidatedCurrentVp[16]{};const double b437VpResidual=b437_view_projection_residual(currentInverseViewProjection,currentView,currentProjection,b437ValidatedCurrentVp);
 const bool b437MotionCameraValid=b437VpResidual<=0.01;if(b437MotionCameraValid)g_b440InvalidMotionCameraStreak.store(0,std::memory_order_release);else g_b440InvalidMotionCameraStreak.fetch_add(1,std::memory_order_acq_rel);static std::atomic<bool> b437LastMotionCameraValid{true};const bool b437PriorValid=b437LastMotionCameraValid.exchange(b437MotionCameraValid,std::memory_order_acq_rel);const bool b437ValidityTransition=b437PriorValid!=b437MotionCameraValid;
 // B458: B437 is the final authority for camera validity. Capture the exact tuple only after
 // this final mathematical gate succeeds. If a later candidate fails, keep the frame unsafe/zero-motion
 // but use the last B437-valid inverse/view/projection as the stable reconstruction basis.
 static float b458LastValidatedVp[16]{},b458LastValidatedInvVp[16]{},b458LastValidatedView[16]{},b458LastValidatedProjection[16]{};static bool b458HaveValidatedCamera=false;static unsigned long long b458ValidatedFrame=0;
 if(b437MotionCameraValid){std::memcpy(b458LastValidatedVp,b437ValidatedCurrentVp,64);std::memcpy(b458LastValidatedInvVp,currentInverseViewProjection,64);std::memcpy(b458LastValidatedView,currentView,64);std::memcpy(b458LastValidatedProjection,currentProjection,64);b458HaveValidatedCamera=true;b458ValidatedFrame=g_b172GtaFrame;}
 const float*b458EffectiveInvVp=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedInvVp:currentInverseViewProjection;const float*b458EffectiveView=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedView:currentView;const float*b458EffectiveProjection=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedProjection:currentProjection;
 std::ofstream out(path,std::ios::app);
 if(!b437MotionCameraValid&&b458HaveValidatedCamera&&(b437ValidityTransition||(g_b172GtaFrame%120)==0))out<<"B458: B437_VALIDATED_AUTHORITATIVE_CAMERA_HOLD source_frame_id="<<g_b172GtaFrame<<", last_validated_source_frame_id="<<b458ValidatedFrame<<", rejected_inverse_residual="<<b437VpResidual<<", action=retain_B437_valid_reconstruction_basis_plus_existing_zero_motion_unsafe_state, b453_rebase_on_fresh_validity=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
 if(b437ValidityTransition||(g_b172GtaFrame%120)==0)out<<"B437: VALIDATED_MOTION_CAMERA_GATE source_frame_id="<<g_b172GtaFrame<<", view_projection_inverse_residual="<<b437VpResidual<<", valid="<<(b437MotionCameraValid?1:0)<<", prior_valid="<<(b437PriorValid?1:0)<<", transition="<<(b437ValidityTransition?1:0)<<", valid_order=view_times_projection, invalid_action=zero_motion_preserve_last_valid_history, return_action=rebase_then_resume, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
f.align=b434AlignValidatedBaseline;f.valid=b437MotionCameraValid;f.transition=b437ValidityTransition;std::memcpy(f.validated,b437ValidatedCurrentVp,64);f.effectiveInverse=b458EffectiveInvVp;f.effectiveView=b458EffectiveView;f.effectiveProjection=b458EffectiveProjection;}
void prepare(unsigned g_b172GtaFrame,const float*previousViewProjection,const char*path,GhostingTemporalFrame&f){std::ofstream out(path,std::ios::app);const bool b434AlignValidatedBaseline=f.align,b437MotionCameraValid=f.valid,b437ValidityTransition=f.transition;const auto&b437ValidatedCurrentVp=f.validated;const float*b458EffectiveInvVp=f.effectiveInverse,*b458EffectiveView=f.effectiveView,*b458EffectiveProjection=f.effectiveProjection;
 // B453: the authoritative B451 camera can become valid before worker-side B329 sees the discontinuity.
 // Rebase every capture-side temporal baseline atomically before reconstructed motion is dispatched,
 // so the transition frame is zero-motion instead of a full-screen stale-history vector.
 bool b453AtomicTemporalRebase=false;double b453ConsumedMax=0.0,b453ConsumedRms=0.0;bool b453HadConsumed=false;bool b452HadXessPrevious=false;
 AcquireSRWLockExclusive(&g_b175MotionHistoryLock);
 b453HadConsumed=g_b175HaveConsumedViewProjection;b452HadXessPrevious=g_b175bHaveXessPreviousViewProjection;
 if(b437MotionCameraValid&&b453HadConsumed){for(unsigned i=0;i<16;++i){const double d=(double)b437ValidatedCurrentVp[i]-(double)g_b175ConsumedViewProjection[i];b453ConsumedMax=(std::max)(b453ConsumedMax,std::fabs(d));b453ConsumedRms+=d*d;}b453ConsumedRms=std::sqrt(b453ConsumedRms/16.0);}
 // B472: B471 runtime evidence showed the B453 0.08/0.030 thresholds fire during ordinary live camera motion, repeatedly forcing previous=current and XeFG history resets. Preserve transition/baseline rebases, but reserve the numeric fallback for a genuine large camera discontinuity.
 b453AtomicTemporalRebase=b437MotionCameraValid&&(b437ValidityTransition||b434AlignValidatedBaseline||(b453HadConsumed&&(b453ConsumedMax>=2.0||b453ConsumedRms>=0.500)));
 if(b453AtomicTemporalRebase){std::memcpy(g_b175ConsumedViewProjection,b437ValidatedCurrentVp,64);g_b175HaveConsumedViewProjection=true;std::memcpy(g_b175bXessPreviousViewProjection,b437ValidatedCurrentVp,64);g_b175bHaveXessPreviousViewProjection=true;}
 B173GpuConstants xessMotionConstants{};std::memcpy(xessMotionConstants.previousViewProjection,b453AtomicTemporalRebase?b437ValidatedCurrentVp:(b434AlignValidatedBaseline?previousViewProjection:(b452HadXessPrevious?g_b175bXessPreviousViewProjection:previousViewProjection)),64);
 ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 if(b437MotionCameraValid&&b453HadConsumed&&!b453AtomicTemporalRebase&&(b453ConsumedMax>=0.08||b453ConsumedRms>=0.030)){static unsigned b472Preserved=0;if((++b472Preserved<=12)||(b472Preserved%120)==0)out<<"B472: NORMAL_CAMERA_MOTION_HISTORY_PRESERVED source_frame_id="<<g_b172GtaFrame<<", vp_max_abs_delta="<<b453ConsumedMax<<", vp_rms_delta="<<b453ConsumedRms<<", old_b453_threshold_would_reset=1, previous_history_preserved=1, xefg_reset_history_requested=0, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, motion_scale_unchanged=1, motion_direction_unchanged=1\n";}
 if(b453AtomicTemporalRebase){g_b288CameraResetPending.store(true,std::memory_order_release);g_b175bXessReset.store(true,std::memory_order_release);out<<"B453: AUTHORITATIVE_CAMERA_ATOMIC_TEMPORAL_REBASE source_frame_id="<<g_b172GtaFrame<<", trigger="<<(b437ValidityTransition?"validity_transition":(b434AlignValidatedBaseline?"validated_baseline_alignment":"abrupt_authoritative_camera_delta"))<<", prior_consumed_available="<<(b453HadConsumed?1:0)<<", prior_xess_available="<<(b452HadXessPrevious?1:0)<<", vp_max_abs_delta="<<b453ConsumedMax<<", vp_rms_delta="<<b453ConsumedRms<<", reconstructed_motion_previous_equals_current=1, xess_previous_equals_current=1, consumed_history_rebased=1, xess_history_rebased=1, xefg_reset_history_requested=1, motion_scale_unchanged=1, motion_direction_unchanged=1, b451_camera_handoff_unchanged=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}

 std::memcpy(xessMotionConstants.currentInverseViewProjection,b458EffectiveInvVp,64);
 // B495 runtime ghosting fix: restore the accepted B458/B453 temporal authority at the motion boundary.
 // Later B474 distinct-VP substitution and B493 synthetic exact-inverse replacement are bypassed here.
 // The current frame uses the B437/B458 validated camera tuple; previous motion comes only from the
 // accepted B453/XeSS history path above. B423 freezing and XeFG presentation are untouched.
 const bool b495AcceptedB458TemporalRuntime=true;
 static unsigned b495LogOrdinal=0;if((++b495LogOrdinal<=12)||(b495LogOrdinal%120)==0)out<<"B495: ACCEPTED_B458_GHOSTING_RUNTIME_RESTORED source_frame_id="<<g_b172GtaFrame<<", b474_distinct_vp_override=0, b493_synthetic_inverse_override=0, b458_camera_authority=1, b453_temporal_authority=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1, runtime_behavior_changed=1\n";
 float b474TemporalPreviousVp[16]{};bool b474UsedDistinctPrevious=false;double b474DistinctMax=0.0,b474DistinctRms=0.0;bool b474TupleAdvanced=false;
 std::memcpy(b474TemporalPreviousVp,xessMotionConstants.previousViewProjection,64);
 // B482 diagnostic-only: fingerprint the validated current/previous VP pair at the final B474 boundary.
 static unsigned b482MotionTraceOrdinal=0;const bool b482MotionTrace=((++b482MotionTraceOrdinal)<=24)||((b482MotionTraceOrdinal%120)==0);
 if(b482MotionTrace){auto b482Fp=[](const float*m)->unsigned long long{unsigned long long h=1469598103934665603ULL;const unsigned char*b=(const unsigned char*)m;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}return h;};out<<"B482: VALIDATED_VP_TO_MOTION_DISPATCH source_frame_id="<<g_b172GtaFrame<<", camera_valid="<<(b437MotionCameraValid?1:0)<<", validity_transition="<<(b437ValidityTransition?1:0)<<", b474_used_distinct_previous="<<(b474UsedDistinctPrevious?1:0)<<", b474_tuple_advanced="<<(b474TupleAdvanced?1:0)<<", validated_current_vp_fingerprint="<<b482Fp(b437ValidatedCurrentVp)<<", motion_previous_vp_fingerprint="<<b482Fp(xessMotionConstants.previousViewProjection)<<", vp_max_abs_delta="<<b474DistinctMax<<", vp_rms_delta="<<b474DistinctRms<<", motion_direction=current_to_previous_pixel_space, motion_scale={1,1}, b474_history_policy_unchanged=1, b437_gate_unchanged=1, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}

f.rebase=b453AtomicTemporalRebase;f.hadConsumed=b453HadConsumed;f.hadXess=b452HadXessPrevious;f.motion=xessMotionConstants;}
bool pair(unsigned g_b172GtaFrame,const float*previousViewProjection,const char*path,const GhostingTemporalFrame&f,GhostingCameraPair&g_b174CaptureJob){std::ofstream out(path,std::ios::app);const bool b434AlignValidatedBaseline=f.align,b437MotionCameraValid=f.valid,b437ValidityTransition=f.transition;const auto&b437ValidatedCurrentVp=f.validated;const float*b458EffectiveInvVp=f.effectiveInverse,*b458EffectiveView=f.effectiveView,*b458EffectiveProjection=f.effectiveProjection;
const bool b453AtomicTemporalRebase=f.rebase; AcquireSRWLockShared(&g_b175MotionHistoryLock);std::memcpy(g_b174CaptureJob.previousViewProjection,b453AtomicTemporalRebase?b437ValidatedCurrentVp:(b434AlignValidatedBaseline?previousViewProjection:(g_b175HaveConsumedViewProjection?g_b175ConsumedViewProjection:previousViewProjection)),64);const bool b279HadConsumedVp=g_b175HaveConsumedViewProjection;ReleaseSRWLockShared(&g_b175MotionHistoryLock);if(b434AlignValidatedBaseline)out<<"B434: VALIDATED_REACQUIRE_CONSUMED_BASELINE_ALIGNED source_frame_id="<<g_b172GtaFrame<<", action=align_motion_previous_and_b329_comparison_to_validated_current_camera, xefg_reset_history=0, zero_motion=0, ordinary_b329_guard_retained=1, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";if(b437MotionCameraValid)std::memcpy(g_b174CaptureJob.currentViewProjection,b437ValidatedCurrentVp,64);else std::memcpy(g_b174CaptureJob.currentViewProjection,g_b174CaptureJob.previousViewProjection,64);if(b437MotionCameraValid&&b437ValidityTransition)std::memcpy(g_b174CaptureJob.previousViewProjection,g_b174CaptureJob.currentViewProjection,64);std::memcpy(g_b174CaptureJob.currentInverseViewProjection,b458EffectiveInvVp,64);std::memcpy(g_b174CaptureJob.currentView,b458EffectiveView,64);std::memcpy(g_b174CaptureJob.currentProjection,b458EffectiveProjection,64);return b279HadConsumedVp;}
unsigned workerReset(const GhostingCameraPair&job,bool resumeNeedsFresh,unsigned submitted,std::ofstream&out){ const bool b407RawCameraReset=g_b288CameraResetPending.exchange(false,std::memory_order_acq_rel);
 const ULONGLONG b407Now=GetTickCount64();
 const ULONGLONG b407LastUnsafe=g_b407LastUnsafeCameraTick.load(std::memory_order_acquire);
 const bool b407UnsafeEpisode=g_b408UnsafeEpisodeActive.load(std::memory_order_acquire);
 const unsigned b408StableSafeFrames=g_b408StableSafeFrames.load(std::memory_order_acquire);
 bool b288CameraReset=b407RawCameraReset;
 if(b407RawCameraReset&&b407UnsafeEpisode){
  const bool b407AlreadyConsumed=g_b407UnsafeResetConsumed.exchange(true,std::memory_order_acq_rel);
  if(b407AlreadyConsumed){
   b288CameraReset=false;
   if(!resumeNeedsFresh)g_b175bXessReset.store(false,std::memory_order_release);
   out<<"B407: FINAL_CAMERA_RESET_COALESCED source_frame_id="<<job.sourceFrame<<", raw_reset=1, final_reset=0, unsafe_episode=1, unsafe_age_ms="<<(b407LastUnsafe&&b407Now>=b407LastUnsafe?b407Now-b407LastUnsafe:0)<<", stable_safe_frames="<<b408StableSafeFrames<<", rearm_required_safe_frames=30, classifier_flicker_tolerated=1, menu_transition_coalesced=1, zero_motion_retained=1, xess_history_preserved=1, xefg_history_preserved=1\n";
  }else out<<"B407: FINAL_CAMERA_RESET_ACCEPTED source_frame_id="<<job.sourceFrame<<", raw_reset=1, final_reset=1, unsafe_episode=1, episode_entry=1, stable_safe_frames="<<b408StableSafeFrames<<", rearm_required_safe_frames=30, zero_motion_retained=1\n";
 }
 // B329: Ghosting-only temporal guard.  B328 already measured camera VP deltas but only logged them.
 // Reset XeFG history on abrupt camera transitions so stale temporal history cannot smear high-contrast
 // world details (signs/edges) across a rapid camera change.  Sustained ordinary pans are left alone.
 double b329VpMax=0.0,b329VpRms=0.0;
 for(unsigned i=0;i<16;++i){const double d=(double)job.currentViewProjection[i]-(double)job.previousViewProjection[i];const double ad=std::fabs(d);if(ad>b329VpMax)b329VpMax=ad;b329VpRms+=d*d;}
 b329VpRms=std::sqrt(b329VpRms/16.0);
 static double b329LastVpMax=0.0;
 const bool b329AbruptCamera=(b329VpMax>=2.0&&b329VpMax>((std::max)(1.0,b329LastVpMax*1.80)))||b329VpRms>=0.500;
 b329LastVpMax=b329VpMax;
 if(!b329AbruptCamera&&b329VpRms>=0.030){static unsigned b473Preserved=0;if((++b473Preserved<=12)||(b473Preserved%120)==0)out<<"B473: B329_NORMAL_CAMERA_HISTORY_PRESERVED source_frame_id="<<job.sourceFrame<<", vp_max_abs_delta="<<b329VpMax<<", vp_rms_delta="<<b329VpRms<<", old_b329_rms_threshold_would_reset=1, xefg_reset_history_requested=0, b472_history_policy_aligned=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, motion_scale_unchanged=1, motion_direction_unchanged=1\n";}
 const unsigned resetHistory=(submitted==0||resumeNeedsFresh||b288CameraReset||b329AbruptCamera||job.b437MotionValidityTransition)?1:0;if(job.b437MotionValidityTransition)out<<"B437: MOTION_VALIDITY_TRANSITION source_frame_id="<<job.sourceFrame<<", camera_valid="<<(job.b437MotionCameraValid?1:0)<<", zero_motion="<<(job.b437MotionCameraValid?0:1)<<", xefg_reset_history=1, previous_equals_current_on_valid_return="<<(job.b437MotionCameraValid?1:0)<<", freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";
 if(b288CameraReset)out<<"B297: CAMERA_HISTORY_RESET source_frame_id="<<job.sourceFrame<<", reason=first_valid_or_genuine_later_discontinuity, xefg_reset_history=1, motion_previous_equals_current=1, b276_presentation_frozen=1\n";
 if(b329AbruptCamera)out<<"B329: GHOSTING_TEMPORAL_HISTORY_GUARD source_frame_id="<<job.sourceFrame<<", vp_max_abs_delta="<<b329VpMax<<", vp_rms_delta="<<b329VpRms<<", action=xefg_reset_history, threshold_policy=B473_genuine_discontinuity_only, discontinuity_max_threshold=2.0, discontinuity_rms_threshold=0.500, reconstructed_motion_unchanged=1, motion_scale_unchanged=1, jitter_unchanged=1, queue_unchanged=1, x4_unchanged=1\n";
return resetHistory;}

void WINAPI B407MarkUnsafeCameraFrame(){
 g_b407LastUnsafeCameraTick.store(GetTickCount64(),std::memory_order_release);
 g_b408UnsafeEpisodeActive.store(true,std::memory_order_release);
 g_b408StableSafeFrames.store(0,std::memory_order_release);
}

void WINAPI B408MarkSafeCameraFrame(){
 unsigned n=g_b408StableSafeFrames.load(std::memory_order_acquire);
 while(n<30&&!g_b408StableSafeFrames.compare_exchange_weak(n,n+1,std::memory_order_acq_rel,std::memory_order_acquire)){}
 if((n<30?n+1:n)>=30){g_b408UnsafeEpisodeActive.store(false,std::memory_order_release);g_b407UnsafeResetConsumed.store(false,std::memory_order_release);}
}

// B461: B437 is the final camera authority. Once a recovered authoritative camera has
// passed B437 and B453 has atomically rebased both motion histories, the older B408
// classifier episode must not keep the worker in its 30-safe-frame quarantine.


void WINAPI B297ResetCameraHistory(){
 AcquireSRWLockExclusive(&g_b175MotionHistoryLock);
 g_b175HaveConsumedViewProjection=false;
 g_b175bHaveXessPreviousViewProjection=false;
 ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 g_b288CameraResetPending.store(true,std::memory_order_release);
 g_b175bXessReset.store(true,std::memory_order_release);
}


void WINAPI B345ApplyCameraZeroMotionSafety(BOOL resetTemporalHistory){
 // B406: retain the protected zero-motion ghosting safety on every unsafe frame, but temporal
 // history is invalidated only on entry to an unsafe-camera episode. Re-clearing it every frame
 // prevented XeFG from accumulating temporal history and produced long result=3/source-only runs.
 if(resetTemporalHistory){
  AcquireSRWLockExclusive(&g_b175MotionHistoryLock);
  g_b175HaveConsumedViewProjection=false;
  g_b175bHaveXessPreviousViewProjection=false;
  ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 }
 const BOOL b384Reset=resetTemporalHistory ? TRUE : FALSE;
 if(resetTemporalHistory&&b384Reset){
  g_b288CameraResetPending.store(true,std::memory_order_release);
  g_b175bXessReset.store(true,std::memory_order_release);
 }
}
}
#undef g_b175ConsumedViewProjection
#undef g_b175HaveConsumedViewProjection
#undef g_b175MotionHistoryLock
#undef g_b175bHaveXessPreviousViewProjection
#undef g_b175bXessPreviousViewProjection
#undef g_b175bXessReset
#undef g_b288CameraResetPending
#undef g_b407LastUnsafeCameraTick
#undef g_b407UnsafeResetConsumed
#undef g_b408StableSafeFrames
#undef g_b408UnsafeEpisodeActive
#undef g_b440InvalidMotionCameraStreak
#undef g_b175FastConstants
#undef g_b280CandidateLock
#undef g_b280CandidateUpdateSerial
#undef g_b280CandidateWindows
#undef g_b280PreviousByteCount
#undef g_b280PreviousBytes
#undef g_b280PreviousValid
#undef g_b282LastBindSlot
#undef g_b282LastBindStage
#undef g_b282TargetWriteCount
#undef g_b286IndexedDraws
#undef g_b286LastBaseVertex
#undef g_b286LastCbBytes
#undef g_b286LastCbMask
#undef g_b286LastDsvFormat
#undef g_b286LastDsvH
#undef g_b286LastDsvW
#undef g_b286LastFastColorMatch
#undef g_b286LastFastDepthMatch
#undef g_b286LastIndexCount
#undef g_b286LastRtvFormat
#undef g_b286LastRtvH
#undef g_b286LastRtvW
#undef g_b286LastStartIndex
#undef g_b286LastVs
#undef g_b286ProbeLock
#undef g_b286ProbeSamples
#undef g_b287FrameLock
#undef g_b287LastCbIndex
#undef g_b287LastIndexCount
#undef g_b287LastVs
#undef g_b287LastWriteSerial
#undef g_b287Latest256
#undef g_b287LatestLock
#undef g_b287LatestValid
#undef g_b287LatestWriteSerial
#undef g_b287MainPassDraws
#undef g_b287QualifyingDraws
#undef g_b287RejectedScreenDraws
#undef g_b287SumViewInverse
#undef g_b287SumWorld
#undef g_b287SumWorldView
#undef g_b287SumWorldViewProj
#undef g_b291DirectDeriveDraws
#undef g_b291DirectRejectDraws
#undef g_b291SumDirectProjection
#undef g_b291SumDirectView
#undef g_b292Families
#undef g_b292FamilyCount
#undef g_b292FamilyOverflow
#undef g_b300DirectCbIndex
#undef g_b300DirectMode
#undef g_b300DirectWriteCount
#undef g_b300DirectWriteHead
#undef g_b300DirectWriteLock
#undef g_b300DirectWrites
#undef g_b300DrawHooksReleased
#undef g_b301DirectCbResource
#undef g_b301DirectWriteSerial
#undef g_b303StaleDiscoveryConstantBypassLogged
#undef g_b308AbStartTick
#undef g_b308DirectLockTick
#undef g_b309CandidatesCaptured
#undef g_b309DirectGeneration
#undef g_b309DuplicateSkips
#undef g_b309SelectedUnmaps
#undef g_b321ProfilerActive
#undef g_b322ProfilerActive
#undef g_b324DuplicateSelectedUnmapBypasses
#undef g_b324FirstEligibleCaptures
#undef g_b324SourceFrameEpoch
#undef g_b339SecondChanceBypasses
#undef g_b339SecondChanceCaptures
#undef g_b430RejectedStaticCameraResource
#undef g_b430ResponsiveReacquireActive
#undef g_b431RejectedClassifierDraws
#undef g_b432ValidatedReacquireBaselinePending
#undef g_b434ValidatedBaselineCommitPending
#undef g_b445LatestSelectedResource
#undef g_b445LatestSelectedSerial
#undef g_b445LatestSelectedSnapshot
#undef g_b456FullTupleChanges
#undef g_b456PublishedWrites
#undef g_b456SelectedMaps
#undef g_b456SelectedUnmaps
#undef g_b456ViewInverseChanges
#undef g_b475ChangedLaneMask
#undef g_b475ChangedWrites
#undef g_b475LatestChangedSnapshotFp
#undef g_b479LatestLiveSnapshotFp
#undef g_b479LiveChangedSnapshots
#undef g_b479LiveSelectedUnmaps
#undef g_b479PreviousLiveSnapshotFp
#undef g_b484WatchBytes
#undef g_b484WatchChanges
#undef g_b484WatchLatest
#undef g_b484WatchLatestFp
#undef g_b484WatchLock
#undef g_b484WatchResource
#undef g_b484WatchUnmaps
#undef g_b484WatchValid
#undef g_b489Authoritative
#undef g_b489AuthoritativeLock
#undef g_cb_next
#undef g_cb_overflow
#undef g_known_cb_bytes
#undef g_known_cb_count
#undef g_known_cbs
#undef g_map_next
#undef g_map_overflow
#undef g_snapshot_next
#undef g_snapshot_overflow
#undef t_b309Generation
#undef t_b309LastViewInverseSignature
#undef t_b309LastViewInverseValid
#undef t_b309SelectedOrdinal
#undef t_b324CapturedEpoch
#undef t_b339FirstViewInverseValid
#undef t_b339SecondChanceCaptured
#undef t_b339SecondChanceEpoch
#undef t_b446LastFullSnapshot
#undef t_b446LastFullSnapshotValid
#undef t_b339FirstViewInverse

static void check(bool ok,const char*what){if(!ok)throw std::runtime_error(what);}
static void same(const float*a,const float*b,const char*what){check(std::memcmp(a,b,64)==0,what);}
int main(){try{
 GhostingBindProtectedModuleCheck(+[](){return true;});
 const char*log="temporal-replay.log";std::ofstream workerLog("worker-replay.log");
 float view[16]{},projection[16]{},inverse[16]{},incoming[16]{};
 for(unsigned frame=1;frame<=2400;++frame){
  for(unsigned i=0;i<16;++i){view[i]=projection[i]=inverse[i]=incoming[i]=(i%5==0?1.0f:0.0f);}
  // Stationary, small pans, threshold crossings, large cuts, invalid tuples and recovery.
  const unsigned phase=frame%24;float shift=phase<4?0.0f:phase<10?float(phase-4)*0.03f:phase<14?1.999f:phase<18?2.001f:25.0f;
  view[3]=shift;inverse[3]=-shift;incoming[3]=shift;
  if(phase==20||phase==21)inverse[0]=2.0f;
  const bool align=(frame%31)==0;reference::align=align;GhostingCollection().g_b434ValidatedBaselineCommitPending.store(align);
  GhostingTemporalFrame expected{},actual{};reference::validate(frame,incoming,inverse,view,projection,log,expected);GhostingValidate(frame,incoming,inverse,view,projection,log,actual);
  check(expected.valid==actual.valid&&expected.transition==actual.transition&&expected.align==actual.align,"validation flags");same(expected.validated,actual.validated,"validated VP");same(expected.effectiveInverse,actual.effectiveInverse,"inverse hold");
  reference::prepare(frame,incoming,log,expected);GhostingPrepareMotion(frame,incoming,log,actual);
  check(expected.rebase==actual.rebase&&expected.hadConsumed==actual.hadConsumed&&expected.hadXess==actual.hadXess,"rebase/history flags");same(expected.motion.previousViewProjection,actual.motion.previousViewProjection,"XeSS previous pair");same(expected.motion.currentInverseViewProjection,actual.motion.currentInverseViewProjection,"motion inverse");
  GhostingCameraPair ep{},ap{};bool eh=reference::pair(frame,incoming,log,expected,ep),ah=GhostingFinalizePair(frame,incoming,log,actual,ap);check(eh==ah,"consumed availability");
  same(ep.previousViewProjection,ap.previousViewProjection,"worker previous pair");same(ep.currentViewProjection,ap.currentViewProjection,"worker current pair");same(ep.currentInverseViewProjection,ap.currentInverseViewProjection,"worker inverse");same(ep.currentView,ap.currentView,"worker view");same(ep.currentProjection,ap.currentProjection,"worker projection");
  ep.sourceFrame=frame;ep.b437MotionCameraValid=expected.valid;ep.b437MotionValidityTransition=expected.transition;
  if(frame%13==0){reference::B407MarkUnsafeCameraFrame();B407MarkUnsafeCameraFrame();}else{reference::B408MarkSafeCameraFrame();B408MarkSafeCameraFrame();}
  if(frame%113==0){reference::B297ResetCameraHistory();B297ResetCameraHistory();}
  if(frame%83==0){reference::B345ApplyCameraZeroMotionSafety(TRUE);B345ApplyCameraZeroMotionSafety(TRUE);}
  auto er=reference::workerReset(ep,frame%47==0,frame-1,workerLog),ar=GhostingWorkerReset(ap,frame%47==0,frame-1,workerLog);check(er==ar,"worker reset decision");
  check(reference::state.g_b175bXessReset.load()==GhostingTemporal().g_b175bXessReset.load(),"XeSS reset state");
  // Independently model original success-only advancement and dropped frames.
  if(frame%5){std::memcpy(reference::state.g_b175ConsumedViewProjection,ep.currentViewProjection,64);reference::state.g_b175HaveConsumedViewProjection=true;GhostingCommitPresented(ap.currentViewProjection);}
  if(frame%3){std::memcpy(reference::state.g_b175bXessPreviousViewProjection,ep.currentViewProjection,64);reference::state.g_b175bHaveXessPreviousViewProjection=true;GhostingCommitXess(ap.currentViewProjection);}
  if(frame%17==0){reference::state.g_b175bHaveXessPreviousViewProjection=false;GhostingInvalidateXess();}
 }
 // The original B499 quarantine rule remains strict; no B527/B528 behavior is inherited.
 auto*resource=reinterpret_cast<ID3D11Resource*>(0x1000);GhostingCollection().g_b430ResponsiveReacquireActive=true;GhostingCollection().g_b430RejectedStaticCameraResource=resource;
 check(!GhostingClassifierSnapshotAllowed(resource,true),"B499 quarantine");check(GhostingClassifierStride()==2,"reacquire stride");GhostingCollection().g_b430ResponsiveReacquireActive=false;check(GhostingClassifierStride()==8,"normal stride");
 const char*shaders[]={GhostingMotionShader0(),GhostingMotionShader1(),GhostingMotionShader2(),GhostingMotionShader3()};
 for(auto source:shaders){ComPtr<ID3DBlob>code,error;HRESULT hr=D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);check(SUCCEEDED(hr),"relocated motion shader compilation");}
 std::cout<<"PASS: 2400 original-B499 differential temporal frames; worker reset/rearm, quarantine/stride checks and four motion shaders\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
