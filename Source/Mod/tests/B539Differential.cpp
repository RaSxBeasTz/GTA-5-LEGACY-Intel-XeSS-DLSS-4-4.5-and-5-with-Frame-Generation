#define GHOSTING_RUNTIME_TEST 1
#include "../ProtectedFixes/TemporalRuntime.h"
#include <iostream>
#include <stdexcept>
#undef g_b434ValidatedBaselineCommitPending
static void check(bool value,const char*name){if(!value)throw std::runtime_error(name);}
static void same(const float*a,const float*b,const char*name){check(std::memcmp(a,b,64)==0,name);}
template<typename T>T load(HMODULE m,const char*name){auto p=GetProcAddress(m,name);check(p!=nullptr,name);return reinterpret_cast<T>(p);}
int main(int argc,char**argv){try{
 check(argc==2,"provide camera fixture");HMODULE module=LoadLibraryExW(L"B539Reference.dll",nullptr,LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);check(module!=nullptr,"load preserved B539 reference DLL");
  auto referenceGhostingValidate=load<decltype(&GhostingValidate)>(module,"?GhostingValidate@@YAXIPEBM000PEBDAEAUGhostingTemporalFrame@@@Z");
 auto referenceGhostingPrepareMotion=load<decltype(&GhostingPrepareMotion)>(module,"?GhostingPrepareMotion@@YAXIPEBMPEBDAEAUGhostingTemporalFrame@@@Z");
 auto referenceGhostingFinalizePair=load<decltype(&GhostingFinalizePair)>(module,"?GhostingFinalizePair@@YA_NIPEBMPEBDAEBUGhostingTemporalFrame@@AEAUGhostingCameraPair@@@Z");
 auto referenceGhostingWorkerReset=load<decltype(&GhostingWorkerReset)>(module,"?GhostingWorkerReset@@YAIAEBUGhostingCameraPair@@_NIAEAV?$basic_ofstream@DU?$char_traits@D@std@@@std@@@Z");
 auto referenceGhostingCollection=load<decltype(&GhostingCollection)>(module,"?GhostingCollection@@YAAEAUCameraCollectionState@@XZ");
 auto referenceGhostingCommitXess=load<decltype(&GhostingCommitXess)>(module,"?GhostingCommitXess@@YAXPEBM@Z");
 auto referenceGhostingTakeXessReset=load<decltype(&GhostingTakeXessReset)>(module,"?GhostingTakeXessReset@@YA_NXZ");
 auto referenceGhostingBindProtectedModuleCheck=load<decltype(&GhostingBindProtectedModuleCheck)>(module,"?GhostingBindProtectedModuleCheck@@YAXP6A_NXZ@Z");
 auto referenceCommit=load<void(*)(const float*)>(module,"?GhostingCommitPresented@@YAXPEBM@Z");
 referenceGhostingBindProtectedModuleCheck(+[](){return true;});GhostingBindProtectedModuleCheck(+[](){return true;});
 std::ifstream input(argv[1]);check(input.good(),"open captured GTA camera fixture");std::ofstream workerLog("b539-differential-worker.log");
 unsigned frame=0,count=0;float vp[16]{},inv[16]{},identity[16]{};for(unsigned i=0;i<16;i+=5)identity[i]=1;
 while(input>>frame){for(float&v:vp)check(bool(input>>v),"VP fixture");for(float&v:inv)check(bool(input>>v),"inverse fixture");++count;
  bool align=count%31==0;referenceGhostingCollection().g_b434ValidatedBaselineCommitPending.store(align);GhostingCollection().g_b434ValidatedBaselineCommitPending.store(align);
  GhostingTemporalFrame a{},b{};referenceGhostingValidate(frame,vp,inv,vp,identity,"b539-reference.log",a);GhostingValidate(frame,vp,inv,vp,identity,"b546-test.log",b);check(a.valid==b.valid&&a.transition==b.transition&&a.align==b.align,"validation/transition mismatch");
  referenceGhostingPrepareMotion(frame,vp,"b539-reference.log",a);GhostingPrepareMotion(frame,vp,"b546-test.log",b);check(a.rebase==b.rebase&&a.cameraEpoch==b.cameraEpoch,"B539 rebase/reset epoch mismatch");same(a.motion.previousViewProjection,b.motion.previousViewProjection,"B539 XeSS previous camera mismatch");same(a.motion.currentInverseViewProjection,b.motion.currentInverseViewProjection,"B539 inverse camera mismatch");
  GhostingCameraPair pa{},pb{};bool ha=referenceGhostingFinalizePair(frame,vp,"b539-reference.log",a,pa),hb=GhostingFinalizePair(frame,vp,"b546-test.log",b,pb);check(ha==hb,"consumed history mismatch");same(pa.previousViewProjection,pb.previousViewProjection,"B539 XeFG previous camera mismatch");same(pa.currentViewProjection,pb.currentViewProjection,"B539 current camera mismatch");
  pa.forceReset=pb.forceReset=count%43==0;check(referenceGhostingWorkerReset(pa,false,count,workerLog)==GhostingWorkerReset(pb,false,count,workerLog),"B539 XeFG reset decision mismatch");check(referenceGhostingTakeXessReset()==GhostingTakeXessReset(),"B539 XeSS reset decision mismatch");
  if(count%5){referenceCommit(pa.currentViewProjection);GhostingCommitPresented(pb.currentViewProjection,pb.cameraEpoch);}if(count%3){referenceGhostingCommitXess(pa.currentViewProjection);GhostingCommitXess(pb.currentViewProjection);}
 }
 check(count>100,"sufficient recorded camera coverage");std::cout<<"PASS: "<<count<<" captured GTA camera rows match saved B539 DLL validation, rebases, histories, epochs and reset decisions\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
