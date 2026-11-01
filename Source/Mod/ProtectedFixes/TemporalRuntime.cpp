#include "TemporalRuntime.h"
#include "TemporalContinuity.h"
static TemporalState ownedTemporal;
static bool (*protectedModuleCheck)()=nullptr;
GHOST_API void GhostingBindProtectedModuleCheck(bool(*check)()){protectedModuleCheck=check;}
GHOST_API TemporalState& GhostingTemporal(){return ownedTemporal;}
extern "C" GHOST_API void WINAPI B407MarkUnsafeCameraFrame(){
 g_b407LastUnsafeCameraTick.store(GetTickCount64(),std::memory_order_release);
 g_b408UnsafeEpisodeActive.store(true,std::memory_order_release);
 g_b408StableSafeFrames.store(0,std::memory_order_release);
}
extern "C" GHOST_API void WINAPI B408MarkSafeCameraFrame(){
 unsigned n=g_b408StableSafeFrames.load(std::memory_order_acquire);
 while(n<30&&!g_b408StableSafeFrames.compare_exchange_weak(n,n+1,std::memory_order_acq_rel,std::memory_order_acquire)){}
 if((n<30?n+1:n)>=30){g_b408UnsafeEpisodeActive.store(false,std::memory_order_release);g_b407UnsafeResetConsumed.store(false,std::memory_order_release);}
}
GHOST_API // B461: B437 is the final camera authority. Once a recovered authoritative camera has
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
extern "C" GHOST_API void WINAPI B345ApplyCameraZeroMotionSafety(BOOL resetTemporalHistory){
 // B406: retain the protected zero-motion ghosting safety on every unsafe frame, but temporal
 // history is invalidated only on entry to an unsafe-camera episode. Re-clearing it every frame
 // prevented XeFG from accumulating temporal history and produced long result=3/source-only runs.
 if(resetTemporalHistory){
  AcquireSRWLockExclusive(&g_b175MotionHistoryLock);
  g_b175HaveConsumedViewProjection=false;
  g_b175bHaveXessPreviousViewProjection=false;
  ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 }
 const BOOL b384Reset=(protectedModuleCheck&&protectedModuleCheck())?GTAXeSSGhostingUnsafeFramePolicy(resetTemporalHistory):FALSE;
 if(resetTemporalHistory&&b384Reset){
  g_b288CameraResetPending.store(true,std::memory_order_release);
  g_b175bXessReset.store(true,std::memory_order_release);
 }
}
GHOST_API void b436_mul4x4(const float*a,const float*b,double*out){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double v=0.0;for(unsigned k=0;k<4;++k)v+=(double)a[r*4+k]*(double)b[k*4+c];out[r*4+c]=v;}}
GHOST_API double b436_identity_residual(const double*m){double worst=0.0;for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){const double target=r==c?1.0:0.0;worst=(std::max)(worst,std::fabs(m[r*4+c]-target));}return worst;}
GHOST_API double b437_view_projection_residual(const float*invVp,const float*view,const float*projection,float*outViewProjection){
 if(!invVp||!view||!projection)return 960.0;
 double viewProj[16]{},vpInv[16]{},invVpVp[16]{};b436_mul4x4(view,projection,viewProj);
 float vpF[16]{};for(unsigned i=0;i<16;++i){vpF[i]=(float)viewProj[i];if(outViewProjection)outViewProjection[i]=vpF[i];}
 b436_mul4x4(vpF,invVp,vpInv);b436_mul4x4(invVp,vpF,invVpVp);
 const double residual=(std::min)(b436_identity_residual(vpInv),b436_identity_residual(invVpVp));
 return std::isfinite(residual)?residual:960.0;
}
GHOST_API void b436_motion_transform_diagnostic(const float*invVp,const float*view,const float*projection,unsigned long long frame,const char*path){
 if(!invVp||!view||!projection||!path||frame%120)return;
 double viewProj[16]{},projView[16]{},vpInv[16]{},invVpVp[16]{},pvInv[16]{},invPv[16]{};
 b436_mul4x4(view,projection,viewProj);b436_mul4x4(projection,view,projView);
 float vpF[16]{},pvF[16]{};for(unsigned i=0;i<16;++i){vpF[i]=(float)viewProj[i];pvF[i]=(float)projView[i];}
 b436_mul4x4(vpF,invVp,vpInv);b436_mul4x4(invVp,vpF,invVpVp);b436_mul4x4(pvF,invVp,pvInv);b436_mul4x4(invVp,pvF,invPv);
 const double rowOrder=(std::min)(b436_identity_residual(vpInv),b436_identity_residual(invVpVp));
 const double columnOrder=(std::min)(b436_identity_residual(pvInv),b436_identity_residual(invPv));
 const char*best=rowOrder<columnOrder?"view_times_projection":"projection_times_view";
 std::ofstream d(path,std::ios::app);d<<"B436: MOTION_TRANSFORM_CONVENTION_PROBE source_frame_id="<<frame<<", view_projection_inverse_residual="<<rowOrder<<", projection_view_inverse_residual="<<columnOrder<<", best_order="<<best<<", shader_decl=row_major, shader_mul=vector_times_matrix, depth_ndc_range_assumed=zero_to_one, runtime_motion_unchanged=1, diagnostic_only=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";d.flush();
}
extern "C" GHOST_API unsigned WINAPI B440InvalidMotionCameraStreak(){return g_b440InvalidMotionCameraStreak.load(std::memory_order_acquire);}
GHOST_API void GhostingValidate(unsigned g_b172GtaFrame,const float*previousViewProjection,const float*currentInverseViewProjection,const float*currentView,const float*currentProjection,const char*path,GhostingTemporalFrame&f){
 const bool b434AlignValidatedBaseline=B434ConsumeValidatedBaselineCommit()!=FALSE;
 auto b447TupleFingerprint=[](const float*vp,const float*inv,const float*view,const float*proj)->unsigned long long{unsigned long long h=1469598103934665603ULL;const float*parts[4]={vp,inv,view,proj};for(const float*part:parts){const unsigned char*b=(const unsigned char*)part;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}}return h;};
 if((g_b172GtaFrame<=6)||(g_b172GtaFrame%120)==0){std::ofstream b447(path,std::ios::app);b447<<"B447: CAMERA_TUPLE_BOUNDARY source_frame_id="<<g_b172GtaFrame<<", boundary=B437_ENTRY, tuple_fingerprint="<<b447TupleFingerprint(previousViewProjection,currentInverseViewProjection,currentView,currentProjection)<<", vp_component=caller_supplied_first_tuple_component, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 b436_motion_transform_diagnostic(currentInverseViewProjection,currentView,currentProjection,g_b172GtaFrame,path);
 float b437ValidatedCurrentVp[16]{};const double b437VpResidual=b437_view_projection_residual(currentInverseViewProjection,currentView,currentProjection,b437ValidatedCurrentVp);
 const bool b437MotionCameraValid=GhostingCaptureCameraSafe()&&b437VpResidual<=0.01;if(b437MotionCameraValid)g_b440InvalidMotionCameraStreak.store(0,std::memory_order_release);else g_b440InvalidMotionCameraStreak.fetch_add(1,std::memory_order_acq_rel);static std::atomic<bool> b437LastMotionCameraValid{true};const bool b437PriorValid=b437LastMotionCameraValid.exchange(b437MotionCameraValid,std::memory_order_acq_rel);const bool b437ValidityTransition=b437PriorValid!=b437MotionCameraValid;
 // B458: B437 is the final authority for camera validity. Capture the exact tuple only after
 // this final mathematical gate succeeds. If a later candidate fails, keep the frame unsafe/zero-motion
 // but use the last B437-valid inverse/view/projection as the stable reconstruction basis.
 static float b458LastValidatedVp[16]{},b458LastValidatedInvVp[16]{},b458LastValidatedView[16]{},b458LastValidatedProjection[16]{};static bool b458HaveValidatedCamera=false;static unsigned long long b458ValidatedFrame=0;
 if(b437MotionCameraValid){std::memcpy(b458LastValidatedVp,b437ValidatedCurrentVp,64);std::memcpy(b458LastValidatedInvVp,currentInverseViewProjection,64);std::memcpy(b458LastValidatedView,currentView,64);std::memcpy(b458LastValidatedProjection,currentProjection,64);b458HaveValidatedCamera=true;b458ValidatedFrame=g_b172GtaFrame;}
 const float*b458EffectiveInvVp=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedInvVp:currentInverseViewProjection;const float*b458EffectiveView=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedView:currentView;const float*b458EffectiveProjection=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedProjection:currentProjection;
 std::ofstream out(path,std::ios::app);
 if(!b437MotionCameraValid&&b458HaveValidatedCamera&&(b437ValidityTransition||(g_b172GtaFrame%120)==0))out<<"B458: B437_VALIDATED_AUTHORITATIVE_CAMERA_HOLD source_frame_id="<<g_b172GtaFrame<<", last_validated_source_frame_id="<<b458ValidatedFrame<<", rejected_inverse_residual="<<b437VpResidual<<", action=retain_B437_valid_reconstruction_basis_plus_existing_zero_motion_unsafe_state, b453_rebase_on_fresh_validity=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
 if(b437ValidityTransition||(g_b172GtaFrame%120)==0)out<<"B437: VALIDATED_MOTION_CAMERA_GATE source_frame_id="<<g_b172GtaFrame<<", view_projection_inverse_residual="<<b437VpResidual<<", valid="<<(b437MotionCameraValid?1:0)<<", prior_valid="<<(b437PriorValid?1:0)<<", transition="<<(b437ValidityTransition?1:0)<<", valid_order=view_times_projection, invalid_action=zero_motion_preserve_last_valid_history, return_action=rebase_then_resume, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
f.align=b434AlignValidatedBaseline;f.valid=b437MotionCameraValid;f.transition=b437ValidityTransition;
std::memcpy(f.validated,b437ValidatedCurrentVp,64);f.effectiveInverse=b458EffectiveInvVp;f.effectiveView=b458EffectiveView;f.effectiveProjection=b458EffectiveProjection;
}
GHOST_API void GhostingPrepareMotion(unsigned g_b172GtaFrame,const float*previousViewProjection,const char*path,GhostingTemporalFrame&f){
std::ofstream out(path,std::ios::app);
const bool b434AlignValidatedBaseline=f.align,b437MotionCameraValid=f.valid,b437ValidityTransition=f.transition;
const auto& b437ValidatedCurrentVp=f.validated;
const float*b458EffectiveInvVp=f.effectiveInverse,*b458EffectiveView=f.effectiveView,*b458EffectiveProjection=f.effectiveProjection;
 // B453: the authoritative B451 camera can become valid before worker-side B329 sees the discontinuity.
 // Rebase every capture-side temporal baseline atomically before reconstructed motion is dispatched,
 // so the transition frame is zero-motion instead of a full-screen stale-history vector.
 bool b453AtomicTemporalRebase=false;double b453ConsumedMax=0.0,b453ConsumedRms=0.0;bool b453HadConsumed=false;bool b452HadXessPrevious=false;
 AcquireSRWLockExclusive(&g_b175MotionHistoryLock);
 b453HadConsumed=g_b175HaveConsumedViewProjection;b452HadXessPrevious=g_b175bHaveXessPreviousViewProjection;
 if(b437MotionCameraValid&&b453HadConsumed){for(unsigned i=0;i<16;++i){const double d=(double)b437ValidatedCurrentVp[i]-(double)g_b175ConsumedViewProjection[i];b453ConsumedMax=(std::max)(b453ConsumedMax,std::fabs(d));b453ConsumedRms+=d*d;}b453ConsumedRms=std::sqrt(b453ConsumedRms/16.0);}
 // B472: B471 runtime evidence showed the B453 0.08/0.030 thresholds fire during ordinary live camera motion, repeatedly forcing previous=current and XeFG history resets. Preserve transition/baseline rebases, but reserve the numeric fallback for a genuine large camera discontinuity.
 b453AtomicTemporalRebase=b437MotionCameraValid&&(b437ValidityTransition||b434AlignValidatedBaseline||(b453HadConsumed&&ReprojectionCut(g_b175ConsumedViewProjection,b458EffectiveInvVp)));
 const bool requestXefgRebase=b453AtomicTemporalRebase;
 if(b437MotionCameraValid&&b453HadConsumed&&!b453AtomicTemporalRebase&&(b453ConsumedMax>=2.0||b453ConsumedRms>=0.500)){static unsigned continuityLog=0;if(++continuityLog<=6||continuityLog%120==0)out<<"B547: REVERSED_DEPTH_CAMERA_CONTINUITY source_frame_id="<<g_b172GtaFrame<<", raw_vp_delta="<<b453ConsumedMax<<", screen_space_cut=0, motion_history_preserved=1, xess_reset=0, xefg_reset=0\n";}

 if(b453AtomicTemporalRebase){std::memcpy(g_b175ConsumedViewProjection,b437ValidatedCurrentVp,64);g_b175HaveConsumedViewProjection=true;std::memcpy(g_b175bXessPreviousViewProjection,b437ValidatedCurrentVp,64);g_b175bHaveXessPreviousViewProjection=true;}
 B173GpuConstants xessMotionConstants{};std::memcpy(xessMotionConstants.previousViewProjection,b453AtomicTemporalRebase?b437ValidatedCurrentVp:(b434AlignValidatedBaseline?previousViewProjection:(b452HadXessPrevious?g_b175bXessPreviousViewProjection:previousViewProjection)),64);
 ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 if(b437MotionCameraValid&&b453HadConsumed&&!b453AtomicTemporalRebase&&(b453ConsumedMax>=0.08||b453ConsumedRms>=0.030)){static unsigned b472Preserved=0;if((++b472Preserved<=12)||(b472Preserved%120)==0)out<<"B472: NORMAL_CAMERA_MOTION_HISTORY_PRESERVED source_frame_id="<<g_b172GtaFrame<<", vp_max_abs_delta="<<b453ConsumedMax<<", vp_rms_delta="<<b453ConsumedRms<<", old_b453_threshold_would_reset=1, previous_history_preserved=1, xefg_reset_history_requested=0, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, motion_scale_unchanged=1, motion_direction_unchanged=1\n";}
 if(b453AtomicTemporalRebase){if(requestXefgRebase)GhostingTemporal().cameraEpoch.fetch_add(1,std::memory_order_acq_rel);g_b175bXessReset.store(true,std::memory_order_release);out<<"B453: AUTHORITATIVE_CAMERA_ATOMIC_TEMPORAL_REBASE source_frame_id="<<g_b172GtaFrame<<", trigger="<<(b437ValidityTransition?"validity_transition":(b434AlignValidatedBaseline?"validated_baseline_alignment":"abrupt_authoritative_camera_delta"))<<", prior_consumed_available="<<(b453HadConsumed?1:0)<<", prior_xess_available="<<(b452HadXessPrevious?1:0)<<", vp_max_abs_delta="<<b453ConsumedMax<<", vp_rms_delta="<<b453ConsumedRms<<", reconstructed_motion_previous_equals_current=1, xess_previous_equals_current=1, consumed_history_rebased=1, xess_history_rebased=1, xefg_reset_history_requested="<<(requestXefgRebase?1:0)<<", numeric_reset_burst_coalesced="<<(requestXefgRebase?0:1)<<", motion_scale_unchanged=1, motion_direction_unchanged=1, b451_camera_handoff_unchanged=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}

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

GhostingBuildMotionTransform(xessMotionConstants,b437ValidatedCurrentVp);
f.cameraEpoch=GhostingTemporal().cameraEpoch.load(std::memory_order_acquire);f.rebase=b453AtomicTemporalRebase;f.hadConsumed=b453HadConsumed;f.hadXess=b452HadXessPrevious;f.motion=xessMotionConstants;
}
GHOST_API bool GhostingFinalizePair(unsigned g_b172GtaFrame,const float*previousViewProjection,const char*path,const GhostingTemporalFrame&f,GhostingCameraPair&job){
std::ofstream out(path,std::ios::app);
const bool b434AlignValidatedBaseline=f.align,b437MotionCameraValid=f.valid,b437ValidityTransition=f.transition;
const auto& b437ValidatedCurrentVp=f.validated;
const float*b458EffectiveInvVp=f.effectiveInverse,*b458EffectiveView=f.effectiveView,*b458EffectiveProjection=f.effectiveProjection;
const bool b453AtomicTemporalRebase=f.rebase;
 AcquireSRWLockShared(&g_b175MotionHistoryLock);std::memcpy(job.previousViewProjection,b453AtomicTemporalRebase?b437ValidatedCurrentVp:(b434AlignValidatedBaseline?previousViewProjection:(g_b175HaveConsumedViewProjection?g_b175ConsumedViewProjection:previousViewProjection)),64);const bool b279HadConsumedVp=g_b175HaveConsumedViewProjection;ReleaseSRWLockShared(&g_b175MotionHistoryLock);if(b434AlignValidatedBaseline)out<<"B434: VALIDATED_REACQUIRE_CONSUMED_BASELINE_ALIGNED source_frame_id="<<g_b172GtaFrame<<", action=align_motion_previous_and_b329_comparison_to_validated_current_camera, xefg_reset_history=0, zero_motion=0, ordinary_b329_guard_retained=1, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";if(b437MotionCameraValid)std::memcpy(job.currentViewProjection,b437ValidatedCurrentVp,64);else std::memcpy(job.currentViewProjection,job.previousViewProjection,64);if(b437MotionCameraValid&&b437ValidityTransition)std::memcpy(job.previousViewProjection,job.currentViewProjection,64);std::memcpy(job.currentInverseViewProjection,b458EffectiveInvVp,64);std::memcpy(job.currentView,b458EffectiveView,64);std::memcpy(job.currentProjection,b458EffectiveProjection,64);
job.cameraEpoch=f.cameraEpoch;job.sourceFrame=g_b172GtaFrame;job.b437MotionCameraValid=f.valid;job.b437MotionValidityTransition=f.transition;return b279HadConsumedVp;
}
GHOST_API unsigned GhostingWorkerReset(const GhostingCameraPair&job,bool resumeNeedsFresh,unsigned submitted,std::ofstream&out){
 const bool b407RawCameraReset=g_b288CameraResetPending.exchange(false,std::memory_order_acq_rel);
 const bool b535AuthoritativeRebaseReset=job.cameraEpoch>GhostingTemporal().workerCameraEpoch;
 if(b535AuthoritativeRebaseReset)GhostingTemporal().workerCameraEpoch=job.cameraEpoch;
 const ULONGLONG b407Now=GetTickCount64();
 const ULONGLONG b407LastUnsafe=g_b407LastUnsafeCameraTick.load(std::memory_order_acquire);
 const bool b407UnsafeEpisode=g_b408UnsafeEpisodeActive.load(std::memory_order_acquire);
 const unsigned b408StableSafeFrames=g_b408StableSafeFrames.load(std::memory_order_acquire);
 bool b288CameraReset=b407RawCameraReset;
 if(b407RawCameraReset&&b407UnsafeEpisode&&!b535AuthoritativeRebaseReset){
  const bool b407AlreadyConsumed=g_b407UnsafeResetConsumed.exchange(true,std::memory_order_acq_rel);
  if(b407AlreadyConsumed){
   b288CameraReset=false;
   if(!resumeNeedsFresh)g_b175bXessReset.store(false,std::memory_order_release);
   out<<"B407: FINAL_CAMERA_RESET_COALESCED source_frame_id="<<job.sourceFrame<<", raw_reset=1, final_reset=0, unsafe_episode=1, unsafe_age_ms="<<(b407LastUnsafe&&b407Now>=b407LastUnsafe?b407Now-b407LastUnsafe:0)<<", stable_safe_frames="<<b408StableSafeFrames<<", rearm_required_safe_frames=30, classifier_flicker_tolerated=1, menu_transition_coalesced=1, zero_motion_retained=1, xess_history_preserved=1, xefg_history_preserved=1\n";
  }else out<<"B407: FINAL_CAMERA_RESET_ACCEPTED source_frame_id="<<job.sourceFrame<<", raw_reset=1, final_reset=1, unsafe_episode=1, episode_entry=1, stable_safe_frames="<<b408StableSafeFrames<<", rearm_required_safe_frames=30, zero_motion_retained=1\n";
 }
 if(b535AuthoritativeRebaseReset){b288CameraReset=true;g_b175bXessReset.store(true,std::memory_order_release);out<<"B535: AUTHORITATIVE_REBASE_XEFG_RESET_FORCED source_frame_id="<<job.sourceFrame<<", raw_reset="<<(b407RawCameraReset?1:0)<<", unsafe_episode="<<(b407UnsafeEpisode?1:0)<<", prior_unsafe_reset_consumed="<<(g_b407UnsafeResetConsumed.load(std::memory_order_acquire)?1:0)<<", final_reset=1, reason=new_validated_camera_temporal_epoch, b407_duplicate_unsafe_coalescing_bypassed=1, xess_reset_preserved=1, xefg_reset_preserved=1, b423_freezing_unchanged=1, b501_f9_unchanged=1, xefg_x4_presentation_unchanged=1\n";}
 // B329: Ghosting-only temporal guard.  B328 already measured camera VP deltas but only logged them.
 // Reset XeFG history on abrupt camera transitions so stale temporal history cannot smear high-contrast
 // world details (signs/edges) across a rapid camera change.  Sustained ordinary pans are left alone.
 double b329VpMax=0.0,b329VpRms=0.0;
 for(unsigned i=0;i<16;++i){const double d=(double)job.currentViewProjection[i]-(double)job.previousViewProjection[i];const double ad=std::fabs(d);if(ad>b329VpMax)b329VpMax=ad;b329VpRms+=d*d;}
 b329VpRms=std::sqrt(b329VpRms/16.0);
 static double b329LastVpMax=0.0;
 const bool b329AbruptCamera=job.b437MotionCameraValid&&ReprojectionCut(job.previousViewProjection,job.currentInverseViewProjection);
 b329LastVpMax=b329VpMax;
 if(!b329AbruptCamera&&b329VpRms>=0.030){static unsigned b473Preserved=0;if((++b473Preserved<=12)||(b473Preserved%120)==0)out<<"B473: B329_NORMAL_CAMERA_HISTORY_PRESERVED source_frame_id="<<job.sourceFrame<<", vp_max_abs_delta="<<b329VpMax<<", vp_rms_delta="<<b329VpRms<<", old_b329_rms_threshold_would_reset=1, xefg_reset_history_requested=0, b472_history_policy_aligned=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, motion_scale_unchanged=1, motion_direction_unchanged=1\n";}
 const unsigned resetHistory=(job.forceReset||submitted==0||resumeNeedsFresh||b288CameraReset||b329AbruptCamera||job.b437MotionValidityTransition)?1:0;if(job.b437MotionValidityTransition)out<<"B437: MOTION_VALIDITY_TRANSITION source_frame_id="<<job.sourceFrame<<", camera_valid="<<(job.b437MotionCameraValid?1:0)<<", zero_motion="<<(job.b437MotionCameraValid?0:1)<<", xefg_reset_history=1, previous_equals_current_on_valid_return="<<(job.b437MotionCameraValid?1:0)<<", freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";
 if(b288CameraReset)out<<"B297: CAMERA_HISTORY_RESET source_frame_id="<<job.sourceFrame<<", reason=first_valid_or_genuine_later_discontinuity, xefg_reset_history=1, motion_previous_equals_current=1, b276_presentation_frozen=1\n";
 if(b329AbruptCamera)out<<"B329: GHOSTING_TEMPORAL_HISTORY_GUARD source_frame_id="<<job.sourceFrame<<", vp_max_abs_delta="<<b329VpMax<<", vp_rms_delta="<<b329VpRms<<", action=xefg_reset_history, threshold_policy=B473_genuine_discontinuity_only, discontinuity_max_threshold=2.0, discontinuity_rms_threshold=0.500, reconstructed_motion_unchanged=1, motion_scale_unchanged=1, jitter_unchanged=1, queue_unchanged=1, x4_unchanged=1\n";

return resetHistory;
}
GHOST_API void GhostingCommitPresented(const float*vp,unsigned long long epoch){AcquireSRWLockExclusive(&g_b175MotionHistoryLock);std::memcpy(g_b175ConsumedViewProjection,vp,64);g_b175HaveConsumedViewProjection=true;std::memcpy(GhostingTemporal().workerPreviousVp,vp,64);GhostingTemporal().workerHasPrevious=true;GhostingTemporal().workerPresentedEpoch=epoch;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);}
GHOST_API void GhostingCommitXess(const float*vp){AcquireSRWLockExclusive(&g_b175MotionHistoryLock);std::memcpy(g_b175bXessPreviousViewProjection,vp,64);g_b175bHaveXessPreviousViewProjection=true;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);}
GHOST_API void GhostingInvalidateXess(){AcquireSRWLockExclusive(&g_b175MotionHistoryLock);g_b175bHaveXessPreviousViewProjection=false;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);}
GHOST_API void GhostingClearConsumed(){AcquireSRWLockExclusive(&g_b175MotionHistoryLock);g_b175HaveConsumedViewProjection=false;GhostingTemporal().workerHasPrevious=false;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);}

static float g_b474LastDistinctValidatedVp[16]{};static bool g_b474HaveLastDistinctValidatedVp=false;static SRWLOCK g_b474TemporalLock=SRWLOCK_INIT;

GHOST_API const char* GhostingMotionShader0(){static const char shader[]=R"(
cbuffer FrameData : register(b0){row_major float4x4 a;row_major float4x4 b;uint2 frameSize;float2 pad;}
Texture2D<float2> nativeMotion : register(t1);RWTexture2D<float2> motionOutput : register(u0);
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){if(id.x>=frameSize.x||id.y>=frameSize.y)return;uint sw,sh;nativeMotion.GetDimensions(sw,sh);float2 uv=(float2(id.xy)+0.5)/float2(frameSize);uint2 sp=min(uint2(uv*float2(sw,sh)),uint2(sw-1,sh-1));float2 mv=nativeMotion.Load(int3(sp,0));motionOutput[id.xy]=clamp(-mv,float2(-65504,-65504),float2(65504,65504));}
)";return shader;}

GHOST_API const char* GhostingMotionShader1(){static const char shader[]=R"(
cbuffer FrameData : register(b0) {
 row_major float4x4 previousViewProjection;
 row_major float4x4 currentInverseViewProjection;
 uint2 frameSize;
 float2 padding;
 row_major float4x4 clipToPrevious;
};
Texture2D<float4> depthInput : register(t0);
RWTexture2D<float2> motionOutput : register(u0);
RWTexture2D<float> depthOutput : register(u1);
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
 if(id.x>=frameSize.x||id.y>=frameSize.y)return;
 float4 packedDepth=depthInput.Load(int3(id.xy,0));
 uint3 packedBytes=(uint3)round(saturate(packedDepth.rgb)*255.0);
 float z=(float(packedBytes.x)+float(packedBytes.y)*256.0+float(packedBytes.z)*65536.0)/16777215.0;
 depthOutput[id.xy]=z; // Keep actual per-pixel depth; dilate only motion.
 // B549: XeFG full-resolution motion requires foremost-surface dilation.
 // GTA reversed Z: the largest depth is the closest surface. Start at the
 // centre and use strict greater-than so equal-depth surfaces do not wander.
 uint2 motionPixel=id.xy;
 for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox){
  int2 samplePixel=clamp(int2(id.xy)+int2(ox,oy),int2(0,0),int2(frameSize)-1);
  uint3 bytes=(uint3)round(saturate(depthInput.Load(int3(samplePixel,0)).rgb)*255.0);
  float candidate=(float(bytes.x)+float(bytes.y)*256.0+float(bytes.z)*65536.0)/16777215.0;
  if(candidate>z){z=candidate;motionPixel=uint2(samplePixel);}
 }
 float2 motion=float2(0.0,0.0);
 if(z>0.0&&z<1.0) {
  float4 clip=float4(2.0*((motionPixel.x+0.5)/frameSize.x)-1.0,1.0-2.0*((motionPixel.y+0.5)/frameSize.y),z,1.0);
  float4 world=clip;
  if(abs(world.w)>1.0e-6) {
   world/=world.w;
   float4 prior=mul(world,clipToPrevious);
   if(abs(prior.w)>1.0e-6) {
    float2 priorNdc=prior.xy/prior.w;
    motion=float2((priorNdc.x-clip.x)*(frameSize.x*0.5),(clip.y-priorNdc.y)*(frameSize.y*0.5));
    motion=clamp(motion,float2(-65504.0,-65504.0),float2(65504.0,65504.0));
   }
  }
 }
 motionOutput[id.xy]=motion;
}
)";return shader;}

GHOST_API const char* GhostingMotionShader2(){static const char shader[]=R"(
cbuffer FrameData : register(b0) {
 row_major float4x4 previousViewProjection;
 row_major float4x4 currentInverseViewProjection;
 uint2 frameSize;
 float2 padding;
 row_major float4x4 clipToPrevious;
};
Texture2D<float4> depthInput : register(t0);
RWTexture2D<float2> motionOutput : register(u0);
RWTexture2D<float> depthOutput : register(u1);
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
 if(id.x>=frameSize.x||id.y>=frameSize.y)return;
 float4 packedDepth=depthInput.Load(int3(id.xy,0));
 uint3 packedBytes=(uint3)round(saturate(packedDepth.rgb)*255.0);
 float z=(float(packedBytes.x)+float(packedBytes.y)*256.0+float(packedBytes.z)*65536.0)/16777215.0;
 depthOutput[id.xy]=z;
 float2 motion=float2(0.0,0.0);
 if(z>0.0&&z<1.0) {
  float4 clip=float4(2.0*((id.x+0.5)/frameSize.x)-1.0,1.0-2.0*((id.y+0.5)/frameSize.y),z,1.0);
  float4 world=clip;
  if(abs(world.w)>1.0e-6) {
   world/=world.w;
   float4 prior=mul(world,clipToPrevious);
   if(abs(prior.w)>1.0e-6) {
    float2 priorNdc=prior.xy/prior.w;
    motion=float2((priorNdc.x-clip.x)*(frameSize.x*0.5),(clip.y-priorNdc.y)*(frameSize.y*0.5));
    motion=clamp(motion,float2(-65504.0,-65504.0),float2(65504.0,65504.0));
   }
  }
 }
 motionOutput[id.xy]=motion;
}
)";return shader;}

GHOST_API const char* GhostingMotionShader3(){static const char shader[]=R"(
 cbuffer FrameData : register(b0) {
  row_major float4x4 previousViewProjection;
  row_major float4x4 currentInverseViewProjection;
  uint2 frameSize;
  float2 padding;
 row_major float4x4 clipToPrevious;
 };
 Texture2D<float> depthInput : register(t0);
 RWTexture2D<float2> motionOutput : register(u0);
 [numthreads(8,8,1)]
 void main(uint3 id : SV_DispatchThreadID) {
  if(id.x>=frameSize.x||id.y>=frameSize.y)return;
  float z=depthInput.Load(int3(id.xy,0));float2 motion=float2(0.0,0.0);
  if(z>0.000001&&z<0.999999){
   float2 uv=(float2(id.xy)+0.5)/float2(frameSize);float4 clip=float4(uv.x*2.0-1.0,1.0-uv.y*2.0,z,1.0);
   float4 world=clip;
   if(abs(world.w)>0.000001){world/=world.w;float4 prior=mul(world,clipToPrevious);if(abs(prior.w)>0.000001){float2 priorNdc=prior.xy/prior.w;motion=float2((priorNdc.x-clip.x)*(frameSize.x*0.5),(clip.y-priorNdc.y)*(frameSize.y*0.5));}}
  }
  motionOutput[id.xy]=clamp(motion,float2(-65504.0,-65504.0),float2(65504.0,65504.0));
 })";return shader;}

GHOST_API void GhostingRequestXessReset(){g_b175bXessReset.store(true,std::memory_order_release);}
GHOST_API bool GhostingTakeXessReset(){return g_b175bXessReset.exchange(false,std::memory_order_acq_rel);}

GHOST_API void GhostingAlignWorkerPair(GhostingCameraPair& job){
 auto& state=GhostingTemporal();AcquireSRWLockShared(&g_b175MotionHistoryLock);
 const bool adjacentEpoch=state.workerHasPrevious&&state.workerPresentedEpoch==job.cameraEpoch;
 if(adjacentEpoch&&job.b437MotionCameraValid&&!job.b437MotionValidityTransition&&!job.forceReset){
  std::memcpy(job.previousViewProjection,state.workerPreviousVp,64);
  if(ReprojectionCut(job.previousViewProjection,job.currentInverseViewProjection)){job.forceReset=true;std::memcpy(job.previousViewProjection,job.currentViewProjection,64);}
 }else std::memcpy(job.previousViewProjection,job.currentViewProjection,64);
 ReleaseSRWLockShared(&g_b175MotionHistoryLock);
}

GHOST_API bool GhostingBuildMotionTransform(B173GpuConstants& constants,const float* currentVp){
 double current[16]{},inverse[16]{},previous[16]{},combined[16]{};
 for(unsigned i=0;i<16;++i){current[i]=currentVp[i];previous[i]=constants.previousViewProjection[i];}
 const bool valid=b287_inverse4(current,inverse);
 if(valid)b287_mul4(inverse,previous,combined);
 for(unsigned i=0;i<16;++i){if(!valid||!std::isfinite(combined[i])){for(unsigned j=0;j<16;++j)constants.clipToPrevious[j]=(j%5==0?1.f:0.f);return false;}constants.clipToPrevious[i]=(float)combined[i];}
 return true;
}
