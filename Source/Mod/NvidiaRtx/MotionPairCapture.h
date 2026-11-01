#pragma once
#include <array>
// One F11 packet. Copy only after estimation, restore every resource state, and
// read back only after a GPU fence completes. No readback work in normal play.
namespace motionpaircapture {
struct Plane {
 rtx::ComPtr<ID3D12Resource> readback;
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
 UINT64 bytes{},rowBytes{};
 UINT rows{};
};
std::array<Plane,5> planes;
rtx::ComPtr<ID3D12Fence> completion;
bool used{},pending{};
unsigned serial{},currentFrame{},previousFrame{};
unsigned previousPresentedFrame{};
sl::float4x4 capturedTransform{};
bool estimated{};double intervalMs{};
void record(ID3D12GraphicsCommandList* list,opticalmotion::Engine& engine,
 ID3D12Resource* camera,ID3D12Resource* depth,ID3D12Resource* estimate,
 unsigned previous,double interval){
 if(used||!nrcomparison::recorded||nrcomparison::captureFrame!=frameId||!engine.device)return;
 used=true;serial=nrcomparison::serial;currentFrame=frameId;previousFrame=previous;
 estimated=estimate!=nullptr;intervalMs=interval;previousPresentedFrame=lastPresentedFrame;capturedTransform=clipToPrevious;
 ID3D12Resource* sources[]{engine.images[engine.index^1].Get(),engine.images[engine.index].Get(),camera,estimate?estimate:camera,depth};
 try{
  for(unsigned i=0;i<planes.size();++i){
   auto& p=planes[i];auto desc=sources[i]->GetDesc();
   nativeDevice->GetCopyableFootprints(&desc,0,1,0,&p.footprint,&p.rows,&p.rowBytes,&p.bytes);
   D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
   D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
   buffer.Width=p.bytes;buffer.Height=1;buffer.DepthOrArraySize=buffer.MipLevels=1;
   buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   rtx::check(nativeDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&p.readback)),"motion pair readback");
  }
  rtx::check(nativeDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&completion)),"motion pair fence");
 }catch(const std::exception&e){for(auto& p:planes)p.readback.Reset();log(std::string("R639: MOTION_PAIR_CAPTURE_DISABLED reason=")+e.what());return;}
 for(unsigned i=0;i<planes.size();++i){
  auto state=i<2||(i==3&&estimate)?D3D12_RESOURCE_STATE_COMMON:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  transition(list,sources[i],state,D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=sources[i];source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  destination.pResource=planes[i].readback.Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=planes[i].footprint;
  list->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
  transition(list,sources[i],D3D12_RESOURCE_STATE_COPY_SOURCE,state);
 }
 pending=true;
}
void signal(){if(pending&&completion)rtx::check(proxyQueue->Signal(completion.Get(),1),"motion pair completion signal");}
void poll(){
 if(!pending||!completion)return;auto value=completion->GetCompletedValue();if(value<1)return;
 if(value!=UINT64_MAX){
  auto folder=moduleDirectory()+L"\\GTAXeSS-BlurCapture-"+std::to_wstring(serial);
  std::filesystem::create_directories(folder);std::ofstream meta(folder+L"\\motion-pair.txt");
  meta<<"build=R639\ncurrent_frame="<<currentFrame<<"\nprevious_estimation_frame="<<previousFrame
      <<"\nestimation_interval_cpu_ms="<<intervalMs<<"\noptical_override_applied="<<estimated
      <<"\nprevious_luma_valid="<<(estimated&&previousFrame!=0)
      <<"\nprevious_presented_frame="<<previousPresentedFrame
      <<"\nestimation_matches_presented_history="<<(previousPresentedFrame==previousFrame)
      <<"\nvector_units=input_render_pixels; direction=current_to_previous\n"
      <<"Images are estimation inputs, not generated output. Interval is CPU submission time, not source capture time.\n";
  meta<<"clip_to_previous=";auto transform=reinterpret_cast<const float*>(&capturedTransform);for(unsigned i=0;i<16;++i)meta<<(i?",":"")<<transform[i];meta<<"\n";
  const char* names[]{"04-previous-estimation-luma.bin","05-current-estimation-luma.bin","06-camera-motion.bin","07-FG-motion.bin","08-depth.bin"};
  for(unsigned i=0;i<planes.size();++i){auto& p=planes[i];unsigned char* data{};D3D12_RANGE range{0,(SIZE_T)p.bytes};
   rtx::check(p.readback->Map(0,&range,(void**)&data),"motion pair map");
   std::ofstream out(folder+L"\\"+std::filesystem::path(names[i]).wstring(),std::ios::binary);
   for(unsigned y=0;y<p.rows;++y)out.write((char*)data+p.footprint.Offset+y*p.footprint.Footprint.RowPitch,p.rowBytes);
   D3D12_RANGE written{0,0};p.readback->Unmap(0,&written);
   meta<<names[i]<<" "<<p.footprint.Footprint.Width<<" "<<p.footprint.Footprint.Height<<" "<<unsigned(p.footprint.Footprint.Format)<<" "<<p.rowBytes<<"\n";
  }
  log("R639: MOTION_PAIR_CAPTURE_SAVED frame="+std::to_string(currentFrame)+", previous="+std::to_string(previousFrame)+", override="+std::to_string(estimated));
 }
 for(auto& p:planes)p.readback.Reset();completion.Reset();pending=false;
}
}
