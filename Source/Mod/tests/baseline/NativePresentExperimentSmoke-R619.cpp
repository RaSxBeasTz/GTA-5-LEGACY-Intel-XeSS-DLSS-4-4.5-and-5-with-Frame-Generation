#include "../ScriptHookProbe/NativePresentExperiment.h"
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static gta::NativePresentExperiment experiment;
using PresentFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
static PresentFn original;static bool ready=true;static int mode=0;static unsigned callbacks{},calls{},lastFlags{};
static bool isReady(){return ready;}
static HRESULT STDMETHODCALLTYPE chain(IDXGISwapChain*s,UINT sync,UINT flags){
 ++calls;lastFlags=flags;
 if(mode==2&&(flags&DXGI_PRESENT_DO_NOT_WAIT))return S_OK;
 if(!gta::NativePresentExperiment::suppressRetryCapture()){++callbacks;experiment.noteCallback();}
 if(mode==1&&(flags&DXGI_PRESENT_DO_NOT_WAIT))return DXGI_ERROR_WAS_STILL_DRAWING;
 if(mode==3&&(flags&DXGI_PRESENT_DO_NOT_WAIT))return DXGI_ERROR_INVALID_CALL;
 if(mode==4)return DXGI_ERROR_DEVICE_REMOVED;
 return original(s,sync,flags);
}
namespace gta {struct NativePresentExperimentTestAccess{static void stall(NativePresentExperiment&e){e.pending=e.sequence+1;}};}
static void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
int main(){try{
 ComPtr<IDXGIFactory1>factory;check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))),"factory");ComPtr<IDXGIAdapter1>adapter;
 for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x8086&&wcsstr(d.Description,L"Arc")){adapter=a;break;}}
 check(adapter!=nullptr,"Arc adapter required");
 HWND window=CreateWindowExW(0,L"STATIC",L"GTAXeSS hidden native presentation smoke",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(window!=nullptr,"hidden window");
 DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=desc.BufferDesc.Height=32;desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.OutputWindow=window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 ComPtr<ID3D11Device>device;ComPtr<ID3D11DeviceContext>context;ComPtr<IDXGISwapChain>swap;D3D_FEATURE_LEVEL level;
 check(SUCCEEDED(D3D11CreateDeviceAndSwapChain(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,&level,&context)),"native device and swapchain");
 void**base=*reinterpret_cast<void***>(swap.Get());void*testTable[41]{};std::memcpy(testTable,base,sizeof(testTable));original=reinterpret_cast<PresentFn>(base[8]);testTable[8]=reinterpret_cast<void*>(&chain);*reinterpret_cast<void***>(swap.Get())=testTable;
 auto enable=[&](){check(experiment.enable(swap.Get(),device.Get(),&isReady),"enable experiment");calls=callbacks=lastFlags=0;};
 enable();check(SUCCEEDED(swap->Present(0,0))&&callbacks==1&&(lastFlags&DXGI_PRESENT_DO_NOT_WAIT),"real GPU present retains callback and adds nonblocking flag");
 mode=1;check(swap->Present(0,0)==S_OK&&experiment.busy==1&&callbacks==2,"busy present skips only native output");
 experiment.disable();calls=callbacks=0;mode=0;check(SUCCEEDED(swap->Present(0,0))&&!(lastFlags&DXGI_PRESENT_DO_NOT_WAIT)&&callbacks==1,"disabled exact pass through");
 mode=2;enable();check(SUCCEEDED(swap->Present(0,0))&&!experiment.enabled()&&callbacks==1&&calls==2,"missing callback falls back with exactly one capture");
 mode=3;enable();check(SUCCEEDED(swap->Present(0,0))&&!experiment.enabled()&&callbacks==1&&calls==2,"unsupported flags fall back without duplicate capture");
 mode=4;enable();check(swap->Present(0,0)==DXGI_ERROR_DEVICE_REMOVED&&!experiment.enabled()&&calls==1,"device failure preserved without retry");
 mode=0;enable();ready=false;check(SUCCEEDED(swap->Present(0,0))&&!(lastFlags&DXGI_PRESENT_DO_NOT_WAIT)&&experiment.attempts==0,"focus or stale output uses normal present");ready=true;
 enable();gta::NativePresentExperimentTestAccess::stall(experiment);const auto begin=GetTickCount64();check(SUCCEEDED(swap->Present(0,0))&&!experiment.enabled()&&experiment.fallbacks==1&&GetTickCount64()-begin<250,"GPU gate timeout restores native presentation");
 experiment.shutdown();*reinterpret_cast<void***>(swap.Get())=base;DestroyWindow(window);
 std::cout<<"PASS: Arc hardware present, GPU fence gate, callback preservation, busy output skip, unsupported flags, stale output, timeout, device-error propagation and normal restoration\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
