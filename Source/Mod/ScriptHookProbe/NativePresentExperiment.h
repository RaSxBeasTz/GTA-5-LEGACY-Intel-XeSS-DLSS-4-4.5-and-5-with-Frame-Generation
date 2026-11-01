#pragma once
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <atomic>
#include <cstring>

namespace gta {
// F8-only single-visible-presentation experiment. Explicitly run our capture
// callback once, then issue only a native visibility test while fresh FG is visible.
// One stable table per process, retained until shutdown, with no per-call CPU profiler.
class NativePresentExperiment {
 friend struct NativePresentExperimentTestAccess;
 using PresentFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
 inline static NativePresentExperiment* instance{};
 inline static thread_local bool retryWithoutCapture{};
 Microsoft::WRL::ComPtr<IDXGISwapChain> swap;
 Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context;
 Microsoft::WRL::ComPtr<ID3D11Fence> fence;
 void** originalTable{};void* table[41]{};PresentFn original{};
 HANDLE event{};UINT64 sequence{},pending{};bool(*ready)(){};void(*capture)(IDXGISwapChain*){};
 std::atomic<bool> requested{};
 std::atomic<unsigned long long> callbackSerial{};
 static HRESULT STDMETHODCALLTYPE hook(IDXGISwapChain* self,UINT sync,UINT flags){
  auto* p=instance;if(!p||!p->original)return E_UNEXPECTED;
  auto call=p->original;
  if(self!=p->swap.Get()||!p->requested.load()||flags!=0||sync!=0||!p->ready||!p->ready())return call(self,sync,flags);
  ++p->attempts;
  if(!p->admit()){p->requested=false;p->fallbackReason=1;++p->fallbacks;return call(self,sync,flags);}
  const auto before=p->callbackSerial.load();
  p->capture(self);
  const bool captured=p->callbackSerial.load()!=before;
  auto normal=[&](){retryWithoutCapture=captured;const HRESULT result=call(self,sync,flags);retryWithoutCapture=false;return result;};
  if(!captured){p->requested=false;p->fallbackReason=2;++p->fallbacks;return normal();}
  // F8/F9/focus changes can occur inside the capture callback.
  if(!p->requested.load()||!p->ready()){p->requested=false;p->fallbackReason=4;return normal();}
  retryWithoutCapture=true;HRESULT hr=call(self,0,DXGI_PRESENT_TEST);retryWithoutCapture=false;
  if(hr==DXGI_ERROR_INVALID_CALL||hr==E_INVALIDARG){p->requested=false;p->fallbackReason=3;++p->fallbacks;return normal();}
  if(FAILED(hr)){p->requested=false;p->fallbackReason=5;++p->failures;return hr;}
  if(!p->ready()){p->requested=false;p->fallbackReason=4;++p->fallbacks;return normal();}
  // Native occlusion is expected behind the FG window. Only mask that status
  // while fresh FG output exists; genuine device errors above remain visible.
  if(hr==S_OK||hr==DXGI_STATUS_OCCLUDED){++p->busy;hr=S_OK;}
  if(p->requested.load()){
   const auto value=++p->sequence;
   if(SUCCEEDED(p->context->Signal(p->fence.Get(),value))){p->pending=value;p->context->Flush();}
   else {p->requested=false;p->fallbackReason=6;++p->failures;}
  }
  return hr;
 }
 bool admit(){
  gateExit=0;gateHr=S_OK;gateWaitResult=WAIT_OBJECT_0;gatePending=pending;
  if(!pending)return true;
  UINT64 done=fence->GetCompletedValue();gateCompleted=done;
  if(done==UINT64_MAX){gateExit=2;return false;}if(done>=pending)return true;
  if(!ResetEvent(event)){gateHr=HRESULT_FROM_WIN32(GetLastError());gateExit=3;return false;}
  const HRESULT eventHr=fence->SetEventOnCompletion(pending,event);gateHr=eventHr;
  if(FAILED(eventHr)){gateExit=3;return false;}
  LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
  const ULONGLONG deadline=GetTickCount64()+20;
  while(ready&&ready()){
   const DWORD result=WaitForSingleObject(event,2);
   gateWaitResult=result;
   if(result==WAIT_OBJECT_0){done=fence->GetCompletedValue();break;}
   if(result!=WAIT_TIMEOUT){gateHr=HRESULT_FROM_WIN32(GetLastError());gateExit=4;break;}
   if(GetTickCount64()>=deadline)break;
  }
  QueryPerformanceCounter(&end);gateWaitUs.fetch_add((unsigned long long)(1.0e6*double(end.QuadPart-begin.QuadPart)/frequency.QuadPart));
  // A timeout can race fence completion. Read the fence again instead of
  // diagnosing failure from the value sampled before entering the wait.
  done=fence->GetCompletedValue();gateCompleted=done;
  if(done==UINT64_MAX){gateExit=2;return false;}
  if(!ready||!ready()){gateExit=5;return false;}
  if(gateExit.load()==4)return false;
  if(done<pending){gateExit=1;return false;}
  return true;
 }
public:
 std::atomic<unsigned long long> attempts{},busy{},fallbacks{},failures{},gateWaitUs{};
 std::atomic<unsigned> fallbackReason{};
 // Gate exit: 0=complete, 1=deadline, 2=device removed, 3=event setup,
 // 4=wait API failure, 5=FG output/focus no longer ready.
 std::atomic<unsigned> gateExit{},gateWaitResult{};
 std::atomic<HRESULT> gateHr{S_OK};
 std::atomic<UINT64> gatePending{},gateCompleted{};
 bool enable(IDXGISwapChain* source,ID3D11Device* device,bool(*isReady)(),void(*captureFrame)(IDXGISwapChain*)){
  if(!source||!device||!isReady||!captureFrame)return false;
  if(swap&&swap.Get()!=source)return false; // Do not retarget a live Present wrapper.
  if(!swap){
   Microsoft::WRL::ComPtr<IDXGISwapChain4> extended;
   Microsoft::WRL::ComPtr<ID3D11Device5> device5;
   Microsoft::WRL::ComPtr<ID3D11DeviceContext> baseContext;
   if(FAILED(source->QueryInterface(IID_PPV_ARGS(&extended)))||static_cast<IDXGISwapChain*>(extended.Get())!=source)return false;
   if(FAILED(device->QueryInterface(IID_PPV_ARGS(&device5))))return false;
   device->GetImmediateContext(&baseContext);
   if(!baseContext||FAILED(baseContext.As(&context))||FAILED(device5->CreateFence(0,D3D11_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))return false;
   event=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event)return false;
   auto*** object=reinterpret_cast<void***>(source);originalTable=*object;
   std::memcpy(table,originalTable,sizeof(table));original=reinterpret_cast<PresentFn>(table[8]);table[8]=reinterpret_cast<void*>(&hook);
   if(!original||original==&hook)return false;
   swap=source;ready=isReady;capture=captureFrame;instance=this;
   if(InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(object),table,originalTable)!=originalTable){swap.Reset();instance=nullptr;return false;}
  }
  if(*reinterpret_cast<void***>(source)!=table)return false;
  attempts=busy=fallbacks=failures=gateWaitUs=0;fallbackReason=0;requested=true;return true;
 }
 void disable(){requested=false;}
 bool enabled()const{return requested.load()&&swap&&*reinterpret_cast<void***>(swap.Get())==table;}
 bool targets(IDXGISwapChain* source)const{return swap.Get()==source;}
 void noteCallback(){callbackSerial.fetch_add(1);}
 static bool suppressRetryCapture(){return retryWithoutCapture;}
 void shutdown(){
  requested=false;
  if(swap){auto*** object=reinterpret_cast<void***>(swap.Get());InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(object),originalTable,table);}
  // Keep table and COM dependencies alive for any Present already in progress.
 }
};
}
