#include "StreamlineRuntime.h"
#include <iostream>
#include <d3d11.h>
#include <xess/xess_d3d11.h>
int wmain(int argc,wchar_t**argv){try{
 if(argc!=2&&argc!=6)throw std::runtime_error("Pass the SDK bin directory, optionally provider width height quality");
 const bool query=argc==6;const uint32_t w=query?_wtoi(argv[3]):1920,h=query?_wtoi(argv[4]):1080;const std::wstring quality=query?argv[5]:L"AA";
 if(query&&std::wstring(argv[2])==L"Intel"){
  rtx::ComPtr<IDXGIFactory1> factory;rtx::check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Intel factory");rtx::ComPtr<IDXGIAdapter1> arc;for(UINT i=0;;i++){rtx::ComPtr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x8086&&wcsstr(d.Description,L"Arc")){arc=a;break;}}
  rtx::ComPtr<ID3D11Device> device;rtx::ComPtr<ID3D11DeviceContext> immediate;rtx::check(D3D11CreateDevice(arc.Get(),arc?D3D_DRIVER_TYPE_UNKNOWN:D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&immediate),"Intel sizing device");xess_context_handle_t context{};
  using Query=xess_result_t(*)(xess_context_handle_t,const xess_2d_t*,xess_quality_settings_t,xess_2d_t*);using Destroy=xess_result_t(*)(xess_context_handle_t);Query getResolution=&xessGetInputResolution;Destroy destroy=&xessDestroyContext;rtx::ComPtr<ID3D12Device> sizingDevice;HMODULE portable{};
  if(xessD3D11CreateContext(device.Get(),&context)!=XESS_RESULT_SUCCESS){wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::wstring path(exe);path=path.substr(0,path.find_last_of(L"\\/")+1)+L"libxess.dll";portable=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);using Create=xess_result_t(*)(ID3D12Device*,xess_context_handle_t*);auto create=portable?reinterpret_cast<Create>(GetProcAddress(portable,"xessD3D12CreateContext")):nullptr;getResolution=portable?reinterpret_cast<Query>(GetProcAddress(portable,"xessGetInputResolution")):nullptr;destroy=portable?reinterpret_cast<Destroy>(GetProcAddress(portable,"xessDestroyContext")):nullptr;rtx::check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&sizingDevice)),"XeSS sizing device");if(!create||!getResolution||!destroy||create(sizingDevice.Get(),&context)!=XESS_RESULT_SUCCESS)throw std::runtime_error("XeSS input sizing unavailable");}

  xess_quality_settings_t mode=quality==L"AA"?XESS_QUALITY_SETTING_AA:quality==L"ULTRA_QUALITY_PLUS"?XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS:quality==L"QUALITY"?XESS_QUALITY_SETTING_QUALITY:quality==L"BALANCED"?XESS_QUALITY_SETTING_BALANCED:quality==L"PERFORMANCE"?XESS_QUALITY_SETTING_PERFORMANCE:quality==L"ULTRA_PERFORMANCE"?XESS_QUALITY_SETTING_ULTRA_PERFORMANCE:XESS_QUALITY_SETTING_ULTRA_QUALITY;
  xess_2d_t output{w,h},input{};auto result=getResolution(context,&output,mode,&input);destroy(context);if(portable)FreeLibrary(portable);if(result!=XESS_RESULT_SUCCESS)throw std::runtime_error("XeSS quality query failed");std::cout<<"{\"inputWidth\":"<<input.x<<",\"inputHeight\":"<<input.y<<"}\n";return 0;
 }
 rtx::Streamline sl;sl.start(argv[1]);
 rtx::ComPtr<IDXGIFactory1> factory;rtx::check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");rtx::ComPtr<IDXGIAdapter1> adapter;
 for(UINT i=0;;++i){rtx::ComPtr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x10de){adapter=a;break;}}
 if(!adapter)throw std::runtime_error("No NVIDIA adapter");DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);sl::AdapterInfo info{};info.deviceLUID=(uint8_t*)&desc.AdapterLuid;info.deviceLUIDSizeInBytes=sizeof(LUID);
 auto fg=sl.supported(sl::kFeatureDLSS_G,info);auto sr=sl.supported(sl::kFeatureDLSS,info);
 rtx::ComPtr<ID3D12Device> device;rtx::check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)),"D3D12 device");sl.bind(device.Get());sl::DLSSGState state{};rtx::check(sl.fgState(sl::ViewportHandle(0),state,nullptr),"DLSSG capability state");
 if(query){sl::DLSSOptions options{};options.outputWidth=w;options.outputHeight=h;options.mode=quality==L"AA"?sl::DLSSMode::eDLAA:quality==L"QUALITY"?sl::DLSSMode::eMaxQuality:quality==L"BALANCED"?sl::DLSSMode::eBalanced:quality==L"PERFORMANCE"?sl::DLSSMode::eMaxPerformance:sl::DLSSMode::eUltraPerformance;sl::DLSSOptimalSettings optimal{};rtx::check(sl.optimal(options,optimal),"DLSS input resolution");std::cout<<"{\"inputWidth\":"<<optimal.optimalRenderWidth<<",\"inputHeight\":"<<optimal.optimalRenderHeight<<"}\n";}
 else std::cout<<"{\"dlssSupported\":"<<(sr==sl::Result::eOk?"true":"false")<<",\"fgSupported\":"<<(fg==sl::Result::eOk?"true":"false")<<",\"maxMultiplier\":"<<(state.numFramesToGenerateMax+1)<<",\"fgStatus\":"<<(unsigned)state.status<<"}\n";
 sl.stop();return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
