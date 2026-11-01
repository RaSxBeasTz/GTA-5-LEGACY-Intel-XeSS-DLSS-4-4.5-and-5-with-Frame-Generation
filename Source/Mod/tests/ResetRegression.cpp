#define GHOSTING_RUNTIME_TEST 1
#include "../ProtectedFixes/TemporalRuntime.h"
#include <d3dcompiler.h>
#include <iostream>
#include <stdexcept>
static void check(bool value,const char* name){if(!value)throw std::runtime_error(name);}
static void same(const float*a,const float*b,const char*name){check(std::memcmp(a,b,64)==0,name);}
int main(){try{
 GhostingBindProtectedModuleCheck(+[](){return true;});
 std::ofstream log("reset-regression.log");float identity[16]{};for(int i=0;i<16;i+=5)identity[i]=1;
 GhostingCameraPair old{};old.b437MotionCameraValid=true;std::memcpy(old.previousViewProjection,identity,64);std::memcpy(old.currentViewProjection,identity,64);std::memcpy(old.currentInverseViewProjection,identity,64);
 auto capture=[&](unsigned frame){g_b434ValidatedBaselineCommitPending.store(true);GhostingTemporalFrame f{};GhostingValidate(frame,identity,identity,identity,identity,"reset-regression.log",f);GhostingPrepareMotion(frame,identity,"reset-regression.log",f);GhostingCameraPair p{};GhostingFinalizePair(frame,identity,"reset-regression.log",f,p);return p;};
 auto recovered=capture(10);check(recovered.cameraEpoch!=0,"recovery epoch created");
 check(GhostingWorkerReset(old,false,10,log)==0,"older queued frame must not consume recovery reset");
 check(GhostingWorkerReset(recovered,false,10,log)==1,"recovered frame resets history");
 check(GhostingWorkerReset(recovered,false,10,log)==0,"epoch reset is one shot");
 auto dropped=capture(11);auto next=dropped;next.sourceFrame=12;
 check(GhostingWorkerReset(next,false,10,log)==1,"reset survives dropped recovery frame via epoch");
 check(GhostingWorkerReset(next,false,10,log)==0,"retained epoch resumes history");
 next.forceReset=true;check(GhostingWorkerReset(next,false,10,log)==1,"queue collapse reset preserved");next.forceReset=false;
 check(GhostingWorkerReset(next,false,10,log)==0,"queue reset does not stick");
 auto future=capture(20);check(GhostingWorkerReset(next,false,10,log)==0,"later capture cannot reset older worker");check(GhostingWorkerReset(future,false,10,log)==1,"second recovery resets correct epoch");


 // Homogeneous VP scale changes raw coefficients without moving the image.
 // They must preserve both motion histories and avoid resetting either consumer.
 float view[16]{},inverse[16]{};GhostingTakeXessReset();
 for(unsigned n=0;n<120;++n){const float scale=1000.0f+n;for(unsigned i=0;i<16;++i){view[i]=identity[i]*scale;inverse[i]=identity[i]/scale;}
  GhostingTemporalFrame f{};GhostingValidate(100+n,view,inverse,view,identity,"reset-regression.log",f);GhostingPrepareMotion(100+n,view,"reset-regression.log",f);
  check(f.valid&&!f.rebase,"homogeneous scale is not a camera cut");check(!GhostingTakeXessReset(),"continuous image does not reset XeSS");
  GhostingCameraPair pair{};GhostingFinalizePair(100+n,view,"reset-regression.log",f,pair);check(GhostingWorkerReset(pair,false,100+n,log)==0,"continuous image does not reset XeFG");
  GhostingCommitPresented(pair.currentViewProjection,pair.cameraEpoch);GhostingCommitXess(pair.currentViewProjection);
 }
 // Ordinary pans preserve nonzero motion; a real large jump rebases and resets.
 auto translated=[&](unsigned frame,float shift){std::memcpy(view,identity,64);std::memcpy(inverse,identity,64);view[12]=shift;inverse[12]=-shift;GhostingTemporalFrame f{};GhostingValidate(frame,view,inverse,view,identity,"reset-regression.log",f);GhostingPrepareMotion(frame,view,"reset-regression.log",f);GhostingCameraPair pair{};GhostingFinalizePair(frame,view,"reset-regression.log",f,pair);return std::make_pair(f,pair);};
 auto pan=translated(230,.02f);check(!pan.first.rebase,"small pan retains history");check(std::memcmp(pan.first.motion.previousViewProjection,pan.first.validated,64)!=0,"small pan retains real motion");
 auto cut=translated(231,3.0f);check(cut.first.rebase,"screen-space camera cut rebases");check(GhostingWorkerReset(cut.second,false,231,log)==1,"camera cut resets matching XeFG history");check(GhostingTakeXessReset(),"camera cut resets XeSS too");
 // Queue alignment uses successfully presented history, ignoring capture-side
 // progress and unpresented/dropped jobs.
 GhostingClearConsumed();GhostingCommitPresented(identity,55);
 GhostingCameraPair queued{};queued.cameraEpoch=55;queued.b437MotionCameraValid=true;std::memcpy(queued.currentViewProjection,identity,64);std::memcpy(queued.currentInverseViewProjection,identity,64);queued.currentViewProjection[12]=.1f;queued.currentInverseViewProjection[12]=-.1f;
 GhostingCommitXess(cut.second.currentViewProjection);GhostingAlignWorkerPair(queued);same(queued.previousViewProjection,identity,"capture progress cannot advance worker history");check(!queued.forceReset,"adjacent small motion keeps worker history");
 GhostingCommitPresented(queued.currentViewProjection,55);float lastPresented[16]{};std::memcpy(lastPresented,queued.currentViewProjection,64);
 queued.currentViewProjection[12]=.3f;queued.currentInverseViewProjection[12]=-.3f;GhostingAlignWorkerPair(queued);same(queued.previousViewProjection,lastPresented,"dropped intermediate source uses last presented camera");
 queued.cameraEpoch=56;GhostingAlignWorkerPair(queued);same(queued.previousViewProjection,queued.currentViewProjection,"new epoch does not reuse old camera history");
 queued.cameraEpoch=55;queued.forceReset=true;GhostingAlignWorkerPair(queued);same(queued.previousViewProjection,queued.currentViewProjection,"queue reset aligns motion with reset history");
 // Replay actual B546 cameras through the compiled B547 capture and worker policy.
 GhostingClearConsumed();GhostingInvalidateXess();GhostingTakeXessReset();
 std::ifstream fixture("tests/b547-camera-fixture.txt");check(fixture.good(),"recorded camera fixture available");
 unsigned replayed=0,rebases=0,workerResets=0,nonzero=0;
 while(fixture>>view[0]){for(unsigned i=1;i<16;++i)fixture>>view[i];for(float&v:inverse)fixture>>v;check(!fixture.fail(),"complete camera fixture row");
  GhostingTemporalFrame f{};f.valid=true;f.transition=replayed==0;std::memcpy(f.validated,view,64);f.effectiveInverse=inverse;f.effectiveView=identity;f.effectiveProjection=identity;
  GhostingPrepareMotion(1000+replayed,view,"reset-regression.log",f);rebases+=f.rebase;
  GhostingCameraPair pair{};GhostingFinalizePair(1000+replayed,view,"reset-regression.log",f,pair);GhostingAlignWorkerPair(pair);
  workerResets+=GhostingWorkerReset(pair,false,1000+replayed,log)!=0;
  nonzero+=std::memcmp(pair.previousViewProjection,pair.currentViewProjection,64)!=0;
  GhostingCommitXess(view);GhostingCommitPresented(view,pair.cameraEpoch);++replayed;
 }
 check(replayed>4000&&rebases<20&&workerResets<20&&nonzero>4000,"recorded normal movement preserves nonzero motion without reset storm");
 std::cout<<"Replay rows="<<replayed<<" capture rebases="<<rebases<<" worker resets="<<workerResets<<" nonzero motion="<<nonzero<<"\n";
 // B554: sustained queue collapse skips captures but not presented history.
 // Each newest camera must retain nonzero motion and generate without reset.
 GhostingClearConsumed();GhostingCameraPair newest{};newest.b437MotionCameraValid=true;newest.cameraEpoch=GhostingTemporal().cameraEpoch.load()+1;
 std::memcpy(newest.currentViewProjection,identity,64);std::memcpy(newest.currentInverseViewProjection,identity,64);
 GhostingAlignWorkerPair(newest);check(GhostingWorkerReset(newest,false,20000,log)==1,"initial epoch remains reset");GhostingCommitPresented(identity,newest.cameraEpoch);
 float last[16];std::memcpy(last,identity,64);
 for(unsigned n=1;n<=10000;++n){
  std::memcpy(newest.currentViewProjection,identity,64);std::memcpy(newest.currentInverseViewProjection,identity,64);newest.currentViewProjection[12]=n*.006f;newest.currentInverseViewProjection[12]=-(n*.006f);newest.forceReset=false;
  GhostingAlignWorkerPair(newest);same(newest.previousViewProjection,last,"newest queued frame follows last presented frame");check(!newest.forceReset&&GhostingWorkerReset(newest,false,20000+n,log)==0,"ordinary skipped sources preserve interpolation");check(std::memcmp(newest.previousViewProjection,newest.currentViewProjection,64)!=0,"skipped sources retain nonzero motion");
  GhostingCommitPresented(newest.currentViewProjection,newest.cameraEpoch);std::memcpy(last,newest.currentViewProjection,64);
 }
 newest.forceReset=true;GhostingAlignWorkerPair(newest);check(GhostingWorkerReset(newest,false,40001,log)==1,"explicit reset survives collapse");same(newest.previousViewProjection,newest.currentViewProjection,"explicit reset stays zero-motion aligned");newest.forceReset=false;
 ++newest.cameraEpoch;GhostingAlignWorkerPair(newest);check(GhostingWorkerReset(newest,false,40002,log)==1,"dropped recovery epoch still resets newest job");same(newest.previousViewProjection,newest.currentViewProjection,"recovery epoch aligns current camera");
 std::cout<<"PASS: 10000 collapsed source batches keep real presented motion, explicit resets and recovery epochs preserved\n";
 const char* shaders[]={GhostingMotionShader0(),GhostingMotionShader1(),GhostingMotionShader2(),GhostingMotionShader3()};
 for(auto source:shaders){ComPtr<ID3DBlob> code,error;check(SUCCEEDED(D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error)),"motion shader compiles");}
 std::cout<<"PASS: queued recovery epochs, dropped recovery, one-shot resets, queue reset preservation, homogeneous-scale continuity, real pan/cut, per-consumer queued history, four motion shaders\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}


