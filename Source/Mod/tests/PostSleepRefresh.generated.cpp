
#include "../B25DepthHelper/CompletedFramePolicy.h"
#include <atomic>
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <mutex>
#include <set>
using UINT64=std::uint64_t;
struct Job{unsigned sourceFrame{},slotIndex{};UINT64 nativeFenceValue{};unsigned presentId{},ordinal{},cameraEpoch{};};using B173hLiveJob=Job;
struct B174ReplacedPending{UINT64 nativeFenceValue{};unsigned slotIndex{},sourceFrame{};};
constexpr unsigned kB174oPendingCapacity=8,kB173iColorSlotCount=8;
struct Fence{UINT64 completed{};UINT64 GetCompletedValue(){return completed;}} fence;
Fence*g_b173fSharedFence12=&fence;
std::mutex g_b173hJobLock;
void AcquireSRWLockExclusive(std::mutex*m){m->lock();}void ReleaseSRWLockExclusive(std::mutex*m){m->unlock();}
Job g_b174oPendingJobs[8];unsigned g_b174oPendingSize{},g_b174oPendingHead{};
std::atomic<unsigned>g_b174ColorSlotState[8],g_b174DepthSlotState[8];
std::atomic<unsigned>g_b174Dropped{},g_b174TotalDropped{},g_b174NextPresentId{1},g_b174Accepted{},g_b174TotalAccepted{},g_b174PendingCount{};
std::atomic<bool>g_b173hJobProcessing{},g_b173hJobPending{};
std::vector<B174ReplacedPending>g_b174ReplacedPending;int stopped{};
void b174_request_stop(int i){stopped=i;}bool b240_log_sample(unsigned){return false;}void log_line(const std::string&){}
std::set<unsigned> decisions;
void b175_log_disposition(unsigned id,const char*,const char*,unsigned){if(!decisions.insert(id).second)throw std::runtime_error("duplicate disposition");}
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
static bool take_latest_completed_job(B173hLiveJob&job,bool refresh=false,bool reservePresentId=true){
 if(!g_b173fSharedFence12)return false;
 const UINT64 complete=g_b173fSharedFence12->GetCompletedValue();
 if(complete==UINT64_MAX){b174_request_stop(4);return false;}
 const auto retainedPresentId=job.presentId;const auto retainedOrdinal=job.ordinal;const unsigned supersededSource=job.sourceFrame;
 unsigned droppedFrames[kB174oPendingCapacity]{},droppedSlots[kB174oPendingCapacity]{},dropped=0;
 AcquireSRWLockExclusive(&g_b173hJobLock);
 std::uint64_t fences[kB174oPendingCapacity]{};
 for(unsigned i=0;i<g_b174oPendingSize;++i)fences[i]=g_b174oPendingJobs[(g_b174oPendingHead+i)%kB174oPendingCapacity].nativeFenceValue;
 auto selected=gta::LatestCompletedFrame(fences,g_b174oPendingSize,complete);
 if(selected==gta::NoReadyFrame){ReleaseSRWLockExclusive(&g_b173hJobLock);return false;}
 if(refresh){
  // No preparation commands or XeFG tags exist for this provisional job yet.
  B174ReplacedPending retired{};retired.nativeFenceValue=job.nativeFenceValue;retired.slotIndex=job.slotIndex;retired.sourceFrame=job.sourceFrame;
  g_b174ReplacedPending.push_back(retired);
  if(retired.slotIndex<kB173iColorSlotCount){g_b174ColorSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174DepthSlotState[retired.slotIndex].store(4,std::memory_order_release);}
  g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);
 }
 while(selected--){
  auto& stale=g_b174oPendingJobs[g_b174oPendingHead];
  if(stale.slotIndex<kB173iColorSlotCount){
   droppedFrames[dropped]=stale.sourceFrame;droppedSlots[dropped]=stale.slotIndex;++dropped;
   B174ReplacedPending retired{};retired.nativeFenceValue=stale.nativeFenceValue;retired.slotIndex=stale.slotIndex;retired.sourceFrame=stale.sourceFrame;g_b174ReplacedPending.push_back(retired);
   g_b174ColorSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174DepthSlotState[retired.slotIndex].store(4,std::memory_order_release);
   g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);
  }
  stale=B173hLiveJob{};g_b174oPendingHead=(g_b174oPendingHead+1)%kB174oPendingCapacity;--g_b174oPendingSize;
 }
 const bool available=true;
 if(available){job=g_b174oPendingJobs[g_b174oPendingHead];g_b174oPendingJobs[g_b174oPendingHead]=B173hLiveJob{};g_b174oPendingHead=(g_b174oPendingHead+1)%kB174oPendingCapacity;--g_b174oPendingSize;if(refresh){job.presentId=retainedPresentId;job.ordinal=retainedOrdinal;}else{job.presentId=reservePresentId?g_b174NextPresentId.fetch_add(1,std::memory_order_acq_rel):0;job.ordinal=g_b174Accepted.fetch_add(1,std::memory_order_acq_rel);g_b174TotalAccepted.fetch_add(1,std::memory_order_acq_rel);}g_b173hJobProcessing.store(true,std::memory_order_release);g_b173hJobPending.store(g_b174oPendingSize!=0,std::memory_order_release);g_b174PendingCount.store(g_b174oPendingSize,std::memory_order_release);if(job.slotIndex<kB173iColorSlotCount){g_b174ColorSlotState[job.slotIndex].store(3,std::memory_order_release);g_b174DepthSlotState[job.slotIndex].store(3,std::memory_order_release);}}
 ReleaseSRWLockExclusive(&g_b173hJobLock);
 for(unsigned i=0;i<dropped;++i)b175_log_disposition(droppedFrames[i],"dropped_completed_superseded","newer_completed_frame_ready",droppedSlots[i]);
 if(refresh&&(job.ordinal<8||b240_log_sample(job.sourceFrame)))log_line("R602: POST_SLEEP_INPUT_REFRESH superseded_source="+std::to_string(supersededSource)+", selected_source="+std::to_string(job.sourceFrame)+", present_id="+std::to_string(job.presentId)+", native_fence_completed="+std::to_string(complete)+", gpu_preparation_started=0, present_id_preserved=1, history_reset_added=0");
 b175_log_disposition(job.sourceFrame,"accepted_current_worker","newest_gpu_completed_frame",job.slotIndex);
 return true;
}
int main(){try{
 Job current{};g_b174oPendingJobs[0]={10,0,10,0,0,100};g_b174oPendingSize=1;fence.completed=10;
 check(take_latest_completed_job(current),"initial selection");auto present=current.presentId,ordinal=current.ordinal;
 check(!take_latest_completed_job(current,true),"empty refresh keeps original");check(current.sourceFrame==10,"original retained");
 g_b174oPendingJobs[1]={20,1,20,0,0,101};g_b174oPendingJobs[2]={30,2,30,0,0,102};g_b174oPendingJobs[3]={40,3,40,0,0,103};g_b174oPendingSize=3;fence.completed=30;
 check(take_latest_completed_job(current,true),"refresh after sleep");check(current.sourceFrame==30&&current.cameraEpoch==102,"newest complete snapshot preserved");check(current.presentId==present&&current.ordinal==ordinal,"one XeLL lifecycle and present ID");check(g_b174Accepted==1&&g_b174TotalAccepted==1&&g_b174NextPresentId==2,"no fabricated acceptance or present IDs");check(g_b174oPendingSize==1&&g_b174oPendingJobs[g_b174oPendingHead].sourceFrame==40,"incomplete tail preserved");check(g_b174ReplacedPending.size()==2&&g_b174TotalDropped==2,"provisional and superseded queued jobs retired");
 for(auto&r:g_b174ReplacedPending){check(r.nativeFenceValue<=fence.completed,"no incomplete retirement");check(g_b174ColorSlotState[r.slotIndex]==4,"retirement retains slot ownership");}
 check(!take_latest_completed_job(current,true)&&current.sourceFrame==30,"future-only queue retains prepared choice");
 fence.completed=UINT64_MAX;check(!take_latest_completed_job(current,true)&&stopped==4,"device removed stops selection");
 // Capture-only processing must never consume XeLL/Present IDs.
 stopped=0;g_b174oPendingHead=0;g_b174oPendingSize=0;const unsigned nextId=g_b174NextPresentId.load();
 for(unsigned n=0;n<10000;++n){unsigned source=100000+n;g_b174oPendingJobs[0]={source,0,source,0,0,200};g_b174oPendingHead=0;g_b174oPendingSize=1;fence.completed=source;Job capture{};check(take_latest_completed_job(capture,false,false),"capture-only dequeue");check(capture.presentId==0&&g_b174NextPresentId==nextId,"capture never consumes a real Present ID");}
 g_b174oPendingJobs[0]={200000,1,200000,0,0,201};g_b174oPendingHead=0;g_b174oPendingSize=1;fence.completed=200000;Job resumed{};check(take_latest_completed_job(resumed),"resume real generation");check(resumed.presentId==nextId&&g_b174NextPresentId==nextId+1,"resume has no frame-ID gap after10000captures");
 std::cout<<"PASS: production post-sleep refresh, preserved Present ID/ordinal/epoch, deferred retirement, incomplete tail, unique disposition, device removal\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
