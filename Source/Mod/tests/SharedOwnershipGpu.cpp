#include "../B25DepthHelper/DirectSharedTexture.h"
#include "../B25DepthHelper/FgQueueTimer.h"
#include <dxgi1_2.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <vector>
using Microsoft::WRL::ComPtr;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static void hr(HRESULT h,const char*s){if(FAILED(h)){std::cerr<<s<<" HRESULT="<<std::hex<<h<<"\n";throw std::runtime_error(s);}}
int main(){try{
 ComPtr<IDXGIFactory1> factory;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");ComPtr<IDXGIAdapter1>arc;for(unsigned i=0;;++i){ComPtr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x8086&&std::wcsstr(d.Description,L"Arc")){arc=a;std::wcout<<L"Adapter: "<<d.Description<<L"\n";break;}}check(arc!=nullptr,"Arc required");
 ComPtr<ID3D11Device> native;ComPtr<ID3D11DeviceContext>ctx;hr(D3D11CreateDevice(arc.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&native,nullptr,&ctx),"D3D11");ComPtr<ID3D11Device5>n5;ComPtr<ID3D11DeviceContext4>c4;hr(native.As(&n5),"native5");hr(ctx.As(&c4),"context4");ComPtr<ID3D12Device>d12;hr(D3D12CreateDevice(arc.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d12)),"D3D12");D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue>queue;hr(d12->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"queue");
 ComPtr<ID3D11Fence>nf;hr(n5->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&nf)),"native fence");HANDLE fh{};hr(nf->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&fh),"fence share");ComPtr<ID3D12Fence>sharedFence;HRESULT opened=d12->OpenSharedHandle(fh,IID_PPV_ARGS(&sharedFence));CloseHandle(fh);hr(opened,"fence open");ComPtr<ID3D12Fence>done;hr(d12->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&done)),"done fence");HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(event!=nullptr,"event");

 const unsigned W=1920,H=1080,N=8;
 gta::DirectSharedTexture texture;hr(texture.create(native.Get(),d12.Get(),W,H,DXGI_FORMAT_B8G8R8A8_UNORM),"shared colour");
 D3D11_TEXTURE2D_DESC nd{};texture.native->GetDesc(&nd);nd.MiscFlags=0;ComPtr<ID3D11Texture2D>source;hr(native->CreateTexture2D(&nd,nullptr,&source),"native source");
 auto td=texture.gpu->GetDesc();std::cout<<"Native-owned shared D3D12 flags="<<unsigned(td.Flags)<<"\n";D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT rows;UINT64 rowBytes,bytes;d12->GetCopyableFootprints(&td,0,1,0,&fp,&rows,&rowBytes,&bytes);
 gta::FgQueueTimer timers[N];
 ComPtr<ID3D12Resource> readback[N];ComPtr<ID3D12CommandAllocator> alloc[N];ComPtr<ID3D12GraphicsCommandList> list[N];
 for(unsigned slot=0;slot<N;++slot){timers[slot].create(d12.Get(),queue.Get());D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=bytes;bd.Height=bd.DepthOrArraySize=bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;hr(d12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback[slot])),"readback");hr(d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc[slot])),"allocator");hr(d12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc[slot].Get(),nullptr,IID_PPV_ARGS(&list[slot])),"list");D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=texture.gpu.Get();b.Transition.StateBefore=D3D12_RESOURCE_STATE_COMMON;b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;list[slot]->ResourceBarrier(1,&b);D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=texture.gpu.Get();dst.pResource=readback[slot].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=fp;list[slot]->CopyTextureRegion(&dst,0,0,0,&src,nullptr);std::swap(b.Transition.StateBefore,b.Transition.StateAfter);list[slot]->ResourceBarrier(1,&b);timers[slot].start(list[slot].Get());hr(list[slot]->Close(),"close");}
 std::vector<unsigned> pixels((size_t)W*H);UINT64 frame=0;
 for(unsigned batch=0;batch<32;++batch){
  for(unsigned slot=0;slot<N;++slot){++frame;for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|((unsigned(frame)*17+unsigned(i)*3199)&0xffffff);
   texture.colourTimer.begin(ctx.Get());hr(texture.acquireForNative(c4.Get()),"native handback wait");ctx->UpdateSubresource(source.Get(),0,nullptr,pixels.data(),W*4,0);ctx->CopyResource(texture.native.Get(),source.Get());texture.colourTimer.end(ctx.Get());hr(c4->Signal(nf.Get(),frame),"capture signal");ctx->Flush();hr(queue->Wait(sharedFence.Get(),frame),"capture wait");/* Test replays a prerecorded begin query; production records it per frame. */timers[slot].armed=true;ID3D12CommandList*submit[]{list[slot].Get()};queue->ExecuteCommandLists(1,submit);/* Post-present seed-like use remains before handback. */queue->ExecuteCommandLists(1,submit);timers[slot].end(queue.Get());hr(texture.releaseToNative(queue.Get()),"consumer handback");
  }
  hr(queue->Signal(done.Get(),frame),"batch signal");hr(done->SetEventOnCompletion(frame,event),"batch event");check(WaitForSingleObject(event,10000)==WAIT_OBJECT_0,"batch timeout");
  for(unsigned slot=0;slot<N;++slot){UINT64 gpuNs{};check(timers[slot].poll(gpuNs),"GPU queue timing resolves after slot fence");check(!timers[slot].poll(gpuNs),"query sample counted once");void*data{};D3D12_RANGE range{0,(SIZE_T)bytes};hr(readback[slot]->Map(0,&range,&data),"map");unsigned id=unsigned(frame-N+slot+1);bool exact=true;for(unsigned y=0;y<H;++y){auto row=(unsigned*)((char*)data+(size_t)y*fp.Footprint.RowPitch);for(unsigned x=0;x<W;++x)exact&=row[x]==(0xff000000u|((id*17+(y*W+x)*3199)&0xffffff));}D3D12_RANGE none{};readback[slot]->Unmap(0,&none);check(exact,"GPU ownership must preserve every in-flight frame");}
 }
 check(texture.colourTimer.valid,"asynchronous native GPU timestamps available");std::cout<<"Last asynchronous native handback/copy GPU ms="<<texture.colourTimer.milliseconds<<"\n";check(SUCCEEDED(d12->GetDeviceRemovedReason()),"device remains healthy");CloseHandle(event);std::cout<<"PASS:256 full-HD transfers,8 in flight, GPU-only native handback, every pixel of every frame verified\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
