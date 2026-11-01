#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <iostream>
using Microsoft::WRL::ComPtr;
int main(){ComPtr<IDXGIFactory1>f;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&f))))return 1;for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;if(f->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};if(FAILED(a->GetDesc1(&d))||(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))continue;std::wcout<<i<<L"|"<<d.DedicatedVideoMemory<<L"|"<<d.Description<<L"\n";}}
