#include "../ScriptHookProbe/NativePresentExperiment.h"
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static gta::NativePresentExperiment experiment;
using PresentFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
static PresentFn original;static bool ready=true;static int mode=0;static unsigned callbacks{},calls{},lastFlags{};
static bool isReady(){return ready;}
static ID3D11DeviceContext* renderContext{};static ID3D11RenderTargetView* renderTarget{};
static void capture(IDXGISwapChain*){if(mode==6)return;++callbacks;experiment.noteCallback();const float color[]{.25f,.5f,.75f,1};renderContext->ClearRenderTargetView(renderTarget,color);if(mode==5)ready=false;if(mode==7)experiment.disable();}
static HRESULT STDMETHODCALLTYPE chain(IDXGISwapChain*s,UINT sync,UINT flags){
 ++calls;lastFlags=flags;
 if(mode==2&&(flags&DXGI_PRESENT_TEST))return original(s,sync,flags);
 if(!gta::NativePresentExperiment::suppressRetryCapture()){++callbacks;experiment.noteCallback();}
 if(mode==1&&(flags&DXGI_PRESENT_TEST))return DXGI_STATUS_OCCLUDED;
 if(mode==3&&(flags&DXGI_PRESENT_TEST))return DXGI_ERROR_INVALID_CALL;
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
 ComPtr<ID3D11Texture2D>buffer;ComPtr<ID3D11RenderTargetView>rtv;check(SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))&&SUCCEEDED(device->CreateRenderTargetView(buffer.Get(),nullptr,&rtv)),"render target");renderContext=context.Get();renderTarget=rtv.Get();
 void**base=*reinterpret_cast<void***>(swap.Get());void*testTable[41]{};std::memcpy(testTable,base,sizeof(testTable));original=reinterpret_cast<PresentFn>(base[8]);testTable[8]=reinterpret_cast<void*>(&chain);*reinterpret_cast<void***>(swap.Get())=testTable;
 auto enable=[&](){check(experiment.enable(swap.Get(),device.Get(),&isReady,&capture),"enable experiment");calls=callbacks=lastFlags=0;};
 UINT countBefore{},countAfter{};check(SUCCEEDED(swap->GetLastPresentCount(&countBefore)),"baseline present count");
 enable();for(unsigned i=0;i<64;++i)check(swap->Present(0,0)==S_OK,"visibility-only GPU cycle");check(callbacks==64&&calls==64&&lastFlags==DXGI_PRESENT_TEST&&experiment.busy==64,"one capture per suppressed native presentation");
 check(SUCCEEDED(swap->GetLastPresentCount(&countAfter))&&countAfter==countBefore,"native image present count unchanged");
 D3D11_TEXTURE2D_DESC stagingDesc{};buffer->GetDesc(&stagingDesc);stagingDesc.Usage=D3D11_USAGE_STAGING;stagingDesc.BindFlags=0;stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;stagingDesc.MiscFlags=0;ComPtr<ID3D11Texture2D>staging;check(SUCCEEDED(device->CreateTexture2D(&stagingDesc,nullptr,&staging)),"staging");context->CopyResource(staging.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"readback");auto*pixel=(unsigned char*)mapped.pData;check(pixel[0]>=190&&pixel[0]<=192&&pixel[1]>=127&&pixel[1]<=129&&pixel[2]>=63&&pixel[2]<=65,"BGRA render output preserved without native presentation");context->Unmap(staging.Get(),0);
 mode=1;enable();check(swap->Present(0,0)==S_OK&&experiment.busy==1&&callbacks==1,"expected native occlusion masked only behind fresh FG");
 experiment.disable();calls=callbacks=0;mode=0;check(SUCCEEDED(swap->Present(0,0))&&!(lastFlags&DXGI_PRESENT_DO_NOT_WAIT)&&callbacks==1,"disabled exact pass through");
 mode=2;enable();check(SUCCEEDED(swap->Present(0,0))&&experiment.enabled()&&callbacks==1&&calls==1,"explicit callback survives omitted ScriptHook test callback");
 mode=3;enable();check(SUCCEEDED(swap->Present(0,0))&&!experiment.enabled()&&callbacks==1&&calls==2,"unsupported flags fall back without duplicate capture");
 mode=4;enable();check(swap->Present(0,0)==DXGI_ERROR_DEVICE_REMOVED&&!experiment.enabled()&&calls==1,"device failure preserved without retry");
 mode=5;enable();check(SUCCEEDED(swap->Present(0,0))&&!experiment.enabled()&&lastFlags==0&&callbacks==1,"focus/output loss during capture restores native once");ready=true;
 mode=6;enable();check(SUCCEEDED(swap->Present(0,0))&&!experiment.enabled()&&callbacks==1&&experiment.fallbackReason==2,"missing explicit capture restores normal");
 mode=7;enable();check(SUCCEEDED(swap->Present(0,0))&&!experiment.enabled()&&lastFlags==0&&callbacks==1,"F8/F9 transition inside capture restores native once");
 mode=0;enable();ready=false;check(SUCCEEDED(swap->Present(0,0))&&!(lastFlags&DXGI_PRESENT_DO_NOT_WAIT)&&experiment.attempts==0,"focus or stale output uses normal present");ready=true;
 enable();gta::NativePresentExperimentTestAccess::stall(experiment);const auto begin=GetTickCount64();check(SUCCEEDED(swap->Present(0,0))&&!experiment.enabled()&&experiment.fallbacks==1&&experiment.gateExit==1&&experiment.gateCompleted<experiment.gatePending&&experiment.gateHr==S_OK&&GetTickCount64()-begin<250,"GPU gate timeout restores native presentation");
 experiment.shutdown();*reinterpret_cast<void***>(swap.Get())=base;DestroyWindow(window);
 std::cout<<"PASS: 64 Arc render/fence cycles, no native image presents, BGRA readback, exact-once capture, skipped ScriptHook test callback, occlusion/flags/device/focus/F9/timeout fallback\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
