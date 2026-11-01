#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <iostream>
#include "OpticalFlowSdk/nvOpticalFlowD3D12.h"
using Microsoft::WRL::ComPtr;
int main(){
 ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return 1;
 ComPtr<IDXGIAdapter1> adapter;
 for(unsigned i=0;;++i){ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x10de){adapter=a;std::wcout<<L"GPU: "<<d.Description<<L"\n";break;}}
 if(!adapter)return 2;ComPtr<ID3D12Device> device;if(FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))return 3;
 HMODULE dll=LoadLibraryExW(L"nvofapi64.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll){std::cout<<"Driver optical-flow module missing\n";return 4;}
 using Entry=NV_OF_STATUS(NVOFAPI*)(uint32_t,NV_OF_D3D12_API_FUNCTION_LIST*);
 auto entry=reinterpret_cast<Entry>(GetProcAddress(dll,"NvOFAPICreateInstanceD3D12"));if(!entry)return 5;
 NV_OF_D3D12_API_FUNCTION_LIST api{};auto status=entry(NV_OF_API_VERSION,&api);std::cout<<"API: "<<int(status)<<"\n";if(status!=NV_OF_SUCCESS)return 6;
 NvOFHandle handle{};status=api.nvCreateOpticalFlowD3D12(device.Get(),&handle);std::cout<<"Create: "<<int(status)<<"\n";if(status!=NV_OF_SUCCESS)return 7;
 uint32_t count=0;status=api.nvOFGetCaps(handle,NV_OF_CAPS_SUPPORTED_OUTPUT_GRID_SIZES,nullptr,&count);std::cout<<"Grid capability status: "<<int(status)<<", count: "<<count<<"\n";
 NV_OF_INIT_PARAMS init{};init.width=640;init.height=360;init.outGridSize=NV_OF_OUTPUT_VECTOR_GRID_SIZE_4;init.hintGridSize=NV_OF_HINT_VECTOR_GRID_SIZE_UNDEFINED;init.mode=NV_OF_MODE_OPTICALFLOW;init.perfLevel=NV_OF_PERF_LEVEL_MEDIUM;init.inputBufferFormat=NV_OF_BUFFER_FORMAT_ABGR8;
 status=api.nvOFInit(handle,&init);std::cout<<"Initialize 640x360: "<<int(status)<<"\n";
 api.nvOFDestroy(handle);FreeLibrary(dll);return status==NV_OF_SUCCESS?0:8;
}
