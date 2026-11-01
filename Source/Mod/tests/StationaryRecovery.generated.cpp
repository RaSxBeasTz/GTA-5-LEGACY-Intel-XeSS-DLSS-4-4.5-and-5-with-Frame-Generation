#include <windows.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
struct ID3D11Resource{};static ID3D11Resource resource;
static std::atomic<bool>g_b300DirectMode{true},g_b430ResponsiveReacquireActive{false};
static std::atomic<ID3D11Resource*>g_b301DirectCbResource{&resource},g_b430RejectedStaticCameraResource{nullptr};
static std::atomic<int>g_b300DirectCbIndex{1};
static SRWLOCK g_b300DirectWriteLock=SRWLOCK_INIT,g_b489AuthoritativeLock=SRWLOCK_INIT;
static unsigned g_b300DirectWriteHead=0,g_b300DirectWriteCount=0;
struct Snapshot{int dummy{};};static Snapshot g_b445LatestSelectedSnapshot,g_b489Authoritative;
static ID3D11Resource*g_b445LatestSelectedResource=nullptr;static unsigned long long g_b445LatestSelectedSerial=0;
static unsigned long long b441ReacquireStartSource=0;static ID3D11Resource*b441FallbackResource=nullptr;static int b441FallbackCbIndex=-1;
static unsigned restored=0;static void restore(void*,void*){++restored;}
struct Transport{void(*drawTransport1)(void*,void*)=restore;void(*drawTransport3)(void*,void*)=restore;};static Transport cameraTransport;
static void reset(){g_b300DirectMode=true;g_b301DirectCbResource=&resource;g_b430ResponsiveReacquireActive=false;g_b300DirectCbIndex=1;g_b430RejectedStaticCameraResource=nullptr;b441FallbackResource=nullptr;b441ReacquireStartSource=0;restored=0;}
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
void oldStationary(unsigned sourceId,unsigned b343FalseStaticFrames){const char*path="stationary-regression.log";void*context=nullptr;static bool b429StationaryRelockUsed=false;bool b344UnsafeCameraActive=false,b288HistoryReset=false;
    if(!b429StationaryRelockUsed&&b343FalseStaticFrames>=120){
     ID3D11Resource* b497RejectedResource=g_b301DirectCbResource.load(std::memory_order_acquire);
     const int b497RejectedCbIndex=g_b300DirectCbIndex.load(std::memory_order_acquire);
     b429StationaryRelockUsed=true;b441FallbackResource=b497RejectedResource;b441FallbackCbIndex=b497RejectedCbIndex;b441ReacquireStartSource=sourceId;
     cameraTransport.drawTransport3(context, nullptr);
     g_b430RejectedStaticCameraResource.store(b497RejectedResource,std::memory_order_release);g_b430ResponsiveReacquireActive.store(true,std::memory_order_release);
     g_b300DirectMode.store(false,std::memory_order_release);g_b301DirectCbResource.store(nullptr,std::memory_order_release);g_b300DirectCbIndex.store(-1,std::memory_order_release);
     AcquireSRWLockExclusive(&g_b300DirectWriteLock);g_b300DirectWriteHead=g_b300DirectWriteCount=0;g_b445LatestSelectedSnapshot={};g_b445LatestSelectedResource=nullptr;g_b445LatestSelectedSerial=0;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);
     AcquireSRWLockExclusive(&g_b489AuthoritativeLock);g_b489Authoritative={};ReleaseSRWLockExclusive(&g_b489AuthoritativeLock);
     b344UnsafeCameraActive=false;b288HistoryReset=false;
     std::ofstream b497(path,std::ios::app);b497<<"B497: RESPONSIVE_CAMERA_RELOCK_ARMED source_frame_id="<<sourceId<<", trigger=120_fresh_zero_delta_frames, rejected_resource="<<b497RejectedResource<<", rejected_cb_index="<<b497RejectedCbIndex<<", action=restore_B429_bounded_camera_reselection, selected_resource_cleared=1, draw_classifier_restored=1, direct_write_ring_cleared=1, rejected_resource_quarantined=1, temporal_history_reset=0, protected_b458_b453_unchanged=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
    }
}
void newStationary(unsigned sourceId,unsigned b343FalseStaticFrames){const char*path="stationary-regression.log";void*context=nullptr;
    // B555: a validated camera with fresh writes can legitimately stand still.
    // Do not quarantine it solely for 120 zero-delta frames. That used to
    // clear the lock and leave the expensive classifier running indefinitely.
    if(b343FalseStaticFrames==120){std::ofstream stable(path,std::ios::app);stable<<"B555: VALID_STATIONARY_LOCK_RETAINED source_frame_id="<<sourceId<<", stationary_frames=120, direct_camera_preserved=1, reacquire_armed=0\n";}

}
void fallback(unsigned sourceId,unsigned b343FalseStaticFrames){const char*path="stationary-regression.log";void*context=nullptr;
 if(g_b430ResponsiveReacquireActive.load(std::memory_order_acquire)&&!g_b300DirectMode.load(std::memory_order_acquire)&&b441FallbackResource&&b441ReacquireStartSource&&sourceId>=b441ReacquireStartSource+6){
  g_b430ResponsiveReacquireActive.store(false,std::memory_order_release);g_b430RejectedStaticCameraResource.store(nullptr,std::memory_order_release);
  g_b301DirectCbResource.store(b441FallbackResource,std::memory_order_release);g_b300DirectCbIndex.store(b441FallbackCbIndex,std::memory_order_release);g_b300DirectMode.store(true,std::memory_order_release);
  AcquireSRWLockExclusive(&g_b300DirectWriteLock);g_b300DirectWriteHead=g_b300DirectWriteCount=0;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);
  cameraTransport.drawTransport1(context, nullptr);
  std::ofstream b441(path,std::ios::app);b441<<"B441: BOUNDED_CAMERA_REACQUIRE_ABORTED source_frame_id="<<sourceId<<", search_frames="<<(sourceId-b441ReacquireStartSource)<<", fallback_resource="<<b441FallbackResource<<", fallback_cb_index="<<b441FallbackCbIndex<<", action=restore_prior_direct_camera_and_original_draw_calls, history_reset=0, b437_zero_motion_safety_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
  b441FallbackResource=nullptr;b441FallbackCbIndex=-1;b441ReacquireStartSource=0;
 }

}

int main(){try{reset();for(unsigned n=1;n<=120;++n)oldStationary(n,n);check(!g_b300DirectMode&&g_b301DirectCbResource==nullptr,"old branch reproduces stationary lock loss");
 reset();for(unsigned n=1;n<=10000;++n)newStationary(n,n);check(g_b300DirectMode&&g_b301DirectCbResource==&resource&&!g_b430ResponsiveReacquireActive&&restored==0,"validated stationary lock remains selected");
 g_b300DirectMode=false;g_b301DirectCbResource=nullptr;g_b430ResponsiveReacquireActive=true;g_b430RejectedStaticCameraResource=&resource;b441FallbackResource=&resource;b441FallbackCbIndex=1;b441ReacquireStartSource=100;
 fallback(105,0);check(!g_b300DirectMode&&restored==0,"recovery search is not prematurely restored");fallback(106,0);check(g_b300DirectMode&&g_b301DirectCbResource==&resource&&!g_b430ResponsiveReacquireActive&&restored==1,"search ends at six frames and restores original resource");fallback(107,0);check(restored==1,"fallback restoration is one shot");
 std::cout<<"PASS: old stationary lock loss reproduced;10000 stationary frames retain lock; six-frame recovery restores once\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
