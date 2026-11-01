#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <iostream>
#include <cwchar>
#include "../B25DepthHelper/WriteDiscardMirror.h"
using Microsoft::WRL::ComPtr;
volatile unsigned long long sink=0;
__declspec(noinline) void inspect(const void* p){auto*b=(const volatile unsigned char*)p;unsigned long long h=1469598103934665603ULL;for(unsigned i=0;i<256;++i){h^=b[i];h*=1099511628211ULL;}sink=h;}
int main(){ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return 1;
 ComPtr<IDXGIAdapter1> chosen;for(unsigned i=0;;++i){ComPtr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x8086&&std::wcsstr(d.Description,L"Arc")){chosen=a;std::wcout<<L"Adapter: "<<d.Description<<L"\n";break;}}
 if(!chosen){std::cout<<"SKIP: Arc adapter unavailable\n";return 0;}
 ComPtr<ID3D11Device> dev;ComPtr<ID3D11DeviceContext> ctx;if(FAILED(D3D11CreateDevice(chosen.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,nullptr,&ctx)))return 2;
 D3D11_BUFFER_DESC desc{};desc.ByteWidth=256;desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;ComPtr<ID3D11Buffer> buffer;if(FAILED(dev->CreateBuffer(&desc,nullptr,&buffer)))return 3;
 unsigned char data[256]{};LARGE_INTEGER freq{},begin{},end{};QueryPerformanceFrequency(&freq);double ms[2]{};
 for(unsigned pass=0;pass<2;++pass){WriteDiscardMirror mirror;QueryPerformanceCounter(&begin);for(unsigned i=0;i<4000;++i){D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(ctx->Map(buffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m)))return 4;data[0]=(unsigned char)i;
 if(pass==0){std::memcpy(m.pData,data,256);inspect(m.pData);}else{void* cpu=mirror.begin(m.pData,buffer.Get(),ctx.Get());std::memcpy(cpu,data,256);inspect(cpu);mirror.commit(buffer.Get(),ctx.Get(),0);}ctx->Unmap(buffer.Get(),0);}QueryPerformanceCounter(&end);ms[pass]=1000.0*(end.QuadPart-begin.QuadPart)/freq.QuadPart;}
 std::cout<<"4000 updates: mapped read="<<ms[0]<<" ms, CPU mirror="<<ms[1]<<" ms, ratio="<<ms[0]/ms[1]<<"\n";return 0;}
