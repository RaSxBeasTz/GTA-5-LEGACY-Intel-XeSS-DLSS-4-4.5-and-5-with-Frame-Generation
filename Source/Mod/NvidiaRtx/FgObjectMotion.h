#pragma once
#include "OpticalMotion.h"
#include "MotionPairCapture.h"
// This pass runs AFTER the real frame's SR and NR command list is submitted.
// It replaces only this frame's FG tag. It never replaces taggedMotion, the
// original resource passed to SR/NR, and never modifies real-frame colour.
namespace fgmotion {
opticalmotion::Engine engine;
ID3D12Resource* scene{};unsigned slot{};uint32_t preparedFrame=~0u;
rtx::ComPtr<ID3D12CommandAllocator> allocators[16];
rtx::ComPtr<ID3D12GraphicsCommandList> lists[16];
rtx::ComPtr<ID3D12Fence> fence;UINT64 marker{},retire[16]{};
unsigned previousEstimationFrame{};LARGE_INTEGER previousEstimationTime{};
HRESULT prepare(){
 if(!enabled){engine.history=engine.hints=false;return S_OK;}
 if(engine.failed||!scene||!taggedMotion||!taggedDepth||slot>=16||preparedFrame==frameId)return S_OK;
 preparedFrame=frameId;auto motionDesc=taggedMotion->GetDesc();ID3D12Resource* estimate{};bool captureQueued=false;
 try{
  if(!engine.device)engine.init(nativeDevice.Get(),proxyQueue.Get(),(unsigned)motionDesc.Width,motionDesc.Height);
  if(!fence)rtx::check(nativeDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"FG motion retirement fence");
  if(fence->GetCompletedValue()<retire[slot]){engine.history=engine.hints=false;return S_OK;}
  if(!allocators[slot]){rtx::check(nativeDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocators[slot])),"FG motion allocator");rtx::check(nativeDevice->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators[slot].Get(),nullptr,IID_PPV_ARGS(&lists[slot])),"FG motion list");lists[slot]->Close();}
  rtx::check(allocators[slot]->Reset(),"FG motion allocator reset");rtx::check(lists[slot]->Reset(allocators[slot].Get(),nullptr),"FG motion list reset");
  auto previousIndex=engine.index;
  estimate=engine.record(lists[slot].Get(),scene,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,taggedMotion,taggedDepth,reset,slot);
  if(previousIndex!=engine.index){
   LARGE_INTEGER now{},frequency{};QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
   const double interval=previousEstimationTime.QuadPart?1000.0*(now.QuadPart-previousEstimationTime.QuadPart)/frequency.QuadPart:0;
   const bool wasPending=motionpaircapture::pending;
   motionpaircapture::record(lists[slot].Get(),engine,taggedMotion,taggedDepth,estimate,previousEstimationFrame,interval);
   captureQueued=!wasPending&&motionpaircapture::pending;
   previousEstimationFrame=frameId;previousEstimationTime=now;
  }
 }catch(const std::exception&e){engine.failed=true;if(lists[slot])lists[slot]->Close();log(std::string("R637: FG_OBJECT_MOTION_DISABLED real_frame_inputs_unchanged=1, reason=")+e.what());return S_OK;}
 rtx::check(lists[slot]->Close(),"FG motion list close");
 if(estimate||captureQueued){
  ID3D12CommandList* work[]{lists[slot].Get()};proxyQueue->ExecuteCommandLists(1,work);rtx::check(proxyQueue->Signal(fence.Get(),++marker),"FG motion retirement signal");retire[slot]=marker;
  if(captureQueued)motionpaircapture::signal();
 }
 if(estimate){
  sl::Resource resource(sl::ResourceType::eTex2d,estimate,D3D12_RESOURCE_STATE_COMMON);sl::Extent extent{0,0,(uint32_t)motionDesc.Width,motionDesc.Height};sl::ResourceTag tag(&resource,sl::kBufferTypeMotionVectors,sl::ResourceLifecycle::eValidUntilPresent,&extent);
  // No evaluation of SR or NR occurs after this frame's FG-only override.
  rtx::check(sdk.tags(*frame,viewport,&tag,1,nullptr),"FG-only object motion tag");
  static unsigned count=0;if(++count<=3||count%120==0)log("R637: FG_OBJECT_MOTION active=1, frame="+std::to_string(frameId)+", applied="+std::to_string(count)+", source=post_SR_NR, SR_motion_unchanged=1, NR_motion_unchanged=1");
 }else{static unsigned seeds=0;if(++seeds<=3||seeds%120==0)log("R637: FG_OBJECT_MOTION active=0, seed_or_reset=1, frame="+std::to_string(frameId));}
 return S_OK;
}
}
API HRESULT WINAPI NvidiaBeforePresent(){try{return fgmotion::prepare();}catch(const std::exception&e){log(e.what());return E_FAIL;}}
