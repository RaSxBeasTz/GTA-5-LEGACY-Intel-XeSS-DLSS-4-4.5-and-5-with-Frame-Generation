#pragma once
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <atomic>
#include <cstring>

namespace gta {
// F8-only experiment. Keep the ScriptHook Present chain; never skip its callback.
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
 HANDLE event{};UINT64 sequence{},pending{};bool(*ready)(){};
 std::atomic<bool> requested{};
 std::atomic<unsigned long long> callbackSerial{};
 static HRESULT STDMETHODCALLTYPE hook(IDXGISwapChain* self,UINT sync,UINT flags){
  auto* p=instance;if(!p||!p->original)return E_UNEXPECTED;
  auto call=p->original;
  if(self!=p->swap.Get()||!p->requested.load()||(flags&DXGI_PRESENT_TEST)||sync!=0||!p->ready||!p->ready())return call(self,sync,flags);
  ++p->attempts;
  if(!p->admit()){p->requested=false;++p->fallbacks;return call(self,sync,flags);}
  const auto before=p->callbackSerial.load();
  HRESULT hr=call(self,sync,flags|DXGI_PRESENT_DO_NOT_WAIT);
  const bool captured=p->callbackSerial.load()!=before;
  if((!captured&&(SUCCEEDED(hr)||hr==DXGI_ERROR_WAS_STILL_DRAWING))||hr==DXGI_ERROR_INVALID_CALL||hr==E_INVALIDARG){
   p->requested=false;++p->fallbacks;
   // Retry only flag incompatibility or a chain that omitted our callback.
   // Suppress our own capture on retry if the first call already captured it.
   retryWithoutCapture=captured;hr=call(self,sync,flags);retryWithoutCapture=false;
  }else if(hr==DXGI_ERROR_WAS_STILL_DRAWING){
   if(p->ready()){++p->busy;hr=S_OK;}
   else {p->requested=false;++p->fallbacks;retryWithoutCapture=true;hr=call(self,sync,flags);retryWithoutCapture=false;}
  }else if(FAILED(hr)){p->requested=false;++p->failures;}
  if(p->requested.load()){
   const auto value=++p->sequence;
   if(SUCCEEDED(p->context->Signal(p->fence.Get(),value))){p->pending=value;p->context->Flush();}
   else {p->requested=false;++p->failures;}
  }
  return hr;
 }
 bool admit(){
  if(!pending)return true;
  UINT64 done=fence->GetCompletedValue();if(done==UINT64_MAX)return false;if(done>=pending)return true;
  ResetEvent(event);if(FAILED(fence->SetEventOnCompletion(pending,event)))return false;
  LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
  const ULONGLONG deadline=GetTickCount64()+20;
  while(ready&&ready()){
   const DWORD result=WaitForSingleObject(event,2);
   if(result==WAIT_OBJECT_0){done=fence->GetCompletedValue();break;}
   if(result!=WAIT_TIMEOUT||GetTickCount64()>=deadline)break;
  }
  QueryPerformanceCounter(&end);gateWaitUs.fetch_add((unsigned long long)(1.0e6*double(end.QuadPart-begin.QuadPart)/frequency.QuadPart));
  return done!=UINT64_MAX&&done>=pending&&ready&&ready();
 }
public:
 std::atomic<unsigned long long> attempts{},busy{},fallbacks{},failures{},gateWaitUs{};
 bool enable(IDXGISwapChain* source,ID3D11Device* device,bool(*isReady)()){
  if(!source||!device||!isReady)return false;
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
   swap=source;ready=isReady;instance=this;
   if(InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(object),table,originalTable)!=originalTable){swap.Reset();instance=nullptr;return false;}
  }
  if(*reinterpret_cast<void***>(source)!=table)return false;
  attempts=busy=fallbacks=failures=gateWaitUs=0;requested=true;return true;
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
