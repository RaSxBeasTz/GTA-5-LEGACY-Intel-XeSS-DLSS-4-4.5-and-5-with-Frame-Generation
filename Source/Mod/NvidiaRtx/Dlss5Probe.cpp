#include "StreamlineRuntime.h"
#include <iostream>
#include <iomanip>
#include "Dlss5NgxAbi.h"
bool ngxOk(unsigned result){return result==1;}
bool evaluateNr(ID3D12Device*device,HMODULE dll,void*parameters){
 auto*p=static_cast<FNGXParameter*>(parameters);
 using Populate=unsigned(__cdecl*)(FNGXParameter*);using Create=unsigned(__cdecl*)(ID3D12GraphicsCommandList*,int,FNGXParameter*,FNGXHandle**);using Evaluate=unsigned(__cdecl*)(ID3D12GraphicsCommandList*,const FNGXHandle*,const FNGXParameter*,void*);using Release=unsigned(__cdecl*)(FNGXHandle*);
 auto populate=reinterpret_cast<Populate>(GetProcAddress(dll,"NVSDK_NGX_D3D12_PopulateParameters_Impl"));auto create=reinterpret_cast<Create>(GetProcAddress(dll,"NVSDK_NGX_D3D12_CreateFeature"));auto evaluate=reinterpret_cast<Evaluate>(GetProcAddress(dll,"NVSDK_NGX_D3D12_EvaluateFeature"));auto release=reinterpret_cast<Release>(GetProcAddress(dll,"NVSDK_NGX_D3D12_ReleaseFeature"));if(!populate||!create||!evaluate||!release)return false;
 unsigned hr=populate(p);std::cout<<"nr_populate=0x"<<std::hex<<hr<<std::endl;if(!ngxOk(hr))return false;
 rtx::ComPtr<ID3D12CommandQueue>queue;D3D12_COMMAND_QUEUE_DESC q{};rtx::check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"probe queue");
 rtx::ComPtr<ID3D12CommandAllocator>allocator;rtx::ComPtr<ID3D12GraphicsCommandList>list;rtx::check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"probe allocator");rtx::check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"probe list");
 constexpr unsigned w=1280,h=720;rtx::ComPtr<ID3D12Resource>colour,output;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS|D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 rtx::check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&colour)),"probe colour");rtx::check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output)),"probe output");
 rtx::ComPtr<ID3D12DescriptorHeap>rtvs;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;rtx::check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtvs)),"probe RTV");auto rtv=rtvs->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(colour.Get(),nullptr,rtv);float clear[]{0.2f,0.3f,0.4f,1.f};list->ClearRenderTargetView(rtv,clear,0,nullptr);D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={colour.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};list->ResourceBarrier(1,&barrier);
// Synthetic input tests API/GPU submission only, not image quality.
 for(auto key:{"DLSSNR.Width","DLSSNR.InputWidth","DLSSNR.OutputWidth","DLSSNR.Output.Width"})p->Set(key,w);for(auto key:{"DLSSNR.Height","DLSSNR.InputHeight","DLSSNR.OutputHeight","DLSSNR.Output.Height"})p->Set(key,h);
 p->Set("DLSSNR.Scale",1.f);p->Set("DLSSNR.ScalingRatio",1.f);p->Set("DLSSNR.Hint.Render.Preset",1);p->Set("CreationNodeMask",1u);p->Set("VisibilityNodeMask",1u);p->Set("DLSSNR.Color",colour.Get());p->Set("DLSSNR.Output",output.Get());
 for(auto prefix:{"DLSSNR.Color","DLSSNR.Output"}){std::string base(prefix);p->Set((base+"SubrectBaseX").c_str(),0u);p->Set((base+"SubrectBaseY").c_str(),0u);p->Set((base+"SubrectWidth").c_str(),w);p->Set((base+"SubrectHeight").c_str(),h);}
 p->Set("DLSSNR.Enabled",1);p->Set("DLSSNR.Reset",1);p->Set("Jitter.Offset.X",0.f);p->Set("Jitter.Offset.Y",0.f);p->Set("DLSSNR.Intensity",0.5f);p->Set("DLSSNR.LocalToneStrength",0.f);p->Set("DLSSNR.LocalStructureStrength",0.f);p->Set("DLSSNR.SkinStructureStrength",0.f);p->Set("DLSSNR.UseAutoMask",1);p->Set("DLSSNR.Style",0);p->Set("DLSSNR.UICorrection",1);
 FNGXHandle*handle{};hr=create(list.Get(),18,p,&handle);std::cout<<"nr_create=0x"<<std::hex<<hr<<" handle="<<(handle!=nullptr)<<std::endl;
 bool ok=ngxOk(hr)&&handle;if(ok){hr=evaluate(list.Get(),handle,p,nullptr);std::cout<<"nr_evaluate=0x"<<std::hex<<hr<<std::endl;ok=ngxOk(hr);}
 rtx::check(list->Close(),"close probe list");ID3D12CommandList*lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);rtx::ComPtr<ID3D12Fence>fence;rtx::check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"probe fence");rtx::check(queue->Signal(fence.Get(),1),"signal probe");auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);rtx::check(fence->SetEventOnCompletion(1,event),"probe event");auto wait=WaitForSingleObject(event,60000);CloseHandle(event);std::cout<<"gpu_wait="<<std::dec<<wait<<" removed=0x"<<std::hex<<device->GetDeviceRemovedReason()<<std::endl;
 if(wait!=WAIT_OBJECT_0||FAILED(device->GetDeviceRemovedReason())){std::cout<<"GPU completion failed; exiting without releasing in-flight NR resources"<<std::endl;ExitProcess(3);}if(handle)release(handle);return ok;
}
// Isolated experiment: no GTA injection, feature evaluation, or runtime deployment.
int wmain(int argc,wchar_t**argv){
 if(argc!=3){std::cerr<<"Usage: Dlss5Probe <Streamline folder> <NR DLL>\n";return 2;}
 try{
  rtx::ComPtr<IDXGIFactory6> factory;rtx::check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
  rtx::ComPtr<IDXGIAdapter1> adapter;rtx::ComPtr<ID3D12Device> device;
  for(UINT i=0;factory->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND;++i){DXGI_ADAPTER_DESC1 d{};adapter->GetDesc1(&d);if(d.VendorId==0x10de&&SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;adapter.Reset();}
  if(!device)throw std::runtime_error("No NVIDIA D3D12 device");
  rtx::Streamline sdk;sdk.start(argv[1]);sdk.bind(device.Get());
  auto core=GetModuleHandleW(L"_nvngx.dll");if(!core)throw std::runtime_error("Streamline did not load NGX driver core");
  using GetParams=unsigned(__cdecl*)(void**);using DestroyParams=unsigned(__cdecl*)(void*);
  auto get=reinterpret_cast<GetParams>(GetProcAddress(core,"NVSDK_NGX_D3D12_GetCapabilityParameters"));
  auto destroy=reinterpret_cast<DestroyParams>(GetProcAddress(core,"NVSDK_NGX_D3D12_DestroyParameters"));
  if(!get)throw std::runtime_error("Core capability parameter API missing");
  void*params{};auto result=get(&params);std::cout<<"core_parameters=0x"<<std::hex<<result<<" valid="<<(params!=nullptr)<<std::endl;
  if(!params)throw std::runtime_error("Core parameters unavailable");
  auto dll=LoadLibraryExW(argv[2],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!dll)throw std::runtime_error("NR LoadLibrary failed: "+std::to_string(GetLastError()));
  using Id=unsigned long long(__cdecl*)();using Version=unsigned(__cdecl*)();
  using Init=unsigned(__cdecl*)(unsigned long long,const wchar_t*,ID3D12Device*,unsigned,const void*);
  using Shutdown=unsigned(__cdecl*)(ID3D12Device*);
  auto id=reinterpret_cast<Id>(GetProcAddress(dll,"NVSDK_NGX_GetApplicationId"));auto version=reinterpret_cast<Version>(GetProcAddress(dll,"NVSDK_NGX_GetAPIVersion"));
  auto init=reinterpret_cast<Init>(GetProcAddress(dll,"NVSDK_NGX_D3D12_Init_Ext"));auto shutdown=reinterpret_cast<Shutdown>(GetProcAddress(dll,"NVSDK_NGX_D3D12_Shutdown1"));
  if(!id||!version||!init||!shutdown)throw std::runtime_error("Required NR exports missing");
  wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);std::wstring data=std::wstring(temp)+L"Zink-DLSS5-Probe";CreateDirectoryW(data.c_str(),nullptr);
  result=init(id(),data.c_str(),device.Get(),version(),params);
  std::cout<<"nr_init=0x"<<std::hex<<result<<" api=0x"<<version()<<std::endl;
  const bool initialized=ngxOk(result);bool ok=initialized&&evaluateNr(device.Get(),dll,params);
  if(initialized)shutdown(device.Get());if(destroy)destroy(params);sdk.stop();
  std::cout<<(ok?"EVALUATION_PASSED; synthetic frame only, image quality untested":"PROBE_FAILED; not ready for GTA integration")<<std::endl;return ok?0:1;
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
}
#ifdef DLSS5_PROBE_DLL
extern "C" __declspec(dllexport) int RunNrProbe(int argc,wchar_t**argv){return wmain(argc,argv);}
#endif

