#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include "main.h"

using b87_overlay_fn=HRESULT(WINAPI*)(ID3D11DeviceContext*,IDXGISwapChain*);

static std::string b88_log_path(){
    HMODULE m{};
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&b88_log_path),&m);
    char p[MAX_PATH]{};
    if(m)GetModuleFileNameA(m,p,MAX_PATH);
    std::string s(p);
    auto n=s.find_last_of("\\/");
    if(n!=std::string::npos)s.resize(n+1);
    return s+"GTAXeSS-B281-ScriptHook.log";
}

static void b88_log(const std::string& s){
    std::ofstream o(b88_log_path(),std::ios::app);
    if(o)o<<s<<"\n";
}

static std::string b88_utf8(const wchar_t* w){
    if(!w||!*w)return {};
    int n=WideCharToMultiByte(CP_UTF8,0,w,-1,nullptr,0,nullptr,nullptr);
    if(n<=1)return {};
    std::string s(static_cast<size_t>(n-1),'\0');
    WideCharToMultiByte(CP_UTF8,0,w,-1,s.data(),n,nullptr,nullptr);
    return s;
}

static void b88_log_adapter_once(IDXGISwapChain* swap){
    static bool done=false;
    if(done||!swap)return;
    ID3D11Device* device{};
    HRESULT deviceHr=swap->GetDevice(IID_PPV_ARGS(&device));
    if(FAILED(deviceHr)||!device){
        std::ostringstream o;o<<"B88: swapchain GetDevice failed hr="<<static_cast<long>(deviceHr);b88_log(o.str());done=true;return;
    }
    IDXGIDevice* dxgiDevice{};
    HRESULT dxgiHr=device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    IDXGIAdapter* adapter{};
    HRESULT adapterHr=dxgiDevice?dxgiDevice->GetAdapter(&adapter):E_NOINTERFACE;
    std::ostringstream o;
    o<<"B88: adapter verification begin; swapchain="<<swap<<", d3d11_device="<<device
     <<", dxgi_device_hr="<<static_cast<long>(dxgiHr)<<", get_adapter_hr="<<static_cast<long>(adapterHr);
    b88_log(o.str());
    if(adapter){
        DXGI_ADAPTER_DESC d{};HRESULT descHr=adapter->GetDesc(&d);
        std::ostringstream a;
        a<<"B88: active_d3d11_adapter description=\""<<b88_utf8(d.Description)<<"\""
         <<", vendor=0x"<<std::hex<<std::uppercase<<std::setw(4)<<std::setfill('0')<<d.VendorId
         <<", device=0x"<<std::setw(4)<<d.DeviceId<<", subsys=0x"<<std::setw(8)<<d.SubSysId
         <<", revision=0x"<<std::setw(2)<<d.Revision<<std::dec
         <<", dedicated_vram="<<static_cast<unsigned long long>(d.DedicatedVideoMemory)
         <<", dedicated_system="<<static_cast<unsigned long long>(d.DedicatedSystemMemory)
         <<", shared_system="<<static_cast<unsigned long long>(d.SharedSystemMemory)
         <<", luid_high="<<d.AdapterLuid.HighPart<<", luid_low="<<d.AdapterLuid.LowPart
         <<", get_desc_hr="<<static_cast<long>(descHr);
        b88_log(a.str());
        IDXGIFactory* factory{};
        if(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory)))&&factory){
            for(UINT i=0;;++i){
                IDXGIAdapter* e{};HRESULT ehr=factory->EnumAdapters(i,&e);
                if(ehr==DXGI_ERROR_NOT_FOUND)break;if(FAILED(ehr)||!e)break;
                DXGI_ADAPTER_DESC ed{};
                if(SUCCEEDED(e->GetDesc(&ed))){
                    bool match=ed.AdapterLuid.HighPart==d.AdapterLuid.HighPart&&ed.AdapterLuid.LowPart==d.AdapterLuid.LowPart;
                    std::ostringstream l;
                    l<<"B88: factory_adapter["<<i<<"] description=\""<<b88_utf8(ed.Description)<<"\""
                     <<", vendor=0x"<<std::hex<<std::uppercase<<std::setw(4)<<std::setfill('0')<<ed.VendorId
                     <<", device=0x"<<std::setw(4)<<ed.DeviceId<<std::dec
                     <<", dedicated_vram="<<static_cast<unsigned long long>(ed.DedicatedVideoMemory)
                     <<", matches_active="<<(match?1:0);
                    b88_log(l.str());
                }
                e->Release();
            }
            factory->Release();
        }
        adapter->Release();
    }
    if(dxgiDevice)dxgiDevice->Release();device->Release();
    b88_log("B88: adapter verification complete; this uses the actual D3D11 device owned by the GTA swapchain. No new D3D11 vtable hook is installed by B88 diagnostics.");
    done=true;
}

static void b87_on_present(void* p){
    auto* swap=reinterpret_cast<IDXGISwapChain*>(p);if(!swap)return;
    b88_log_adapter_once(swap);
    HMODULE h=GetModuleHandleA("B25DepthHelper.dll");if(!h)return;
    auto fn=reinterpret_cast<b87_overlay_fn>(GetProcAddress(h,"B87PresentOverlay"));if(!fn)return;
    ID3D11Device* d{};if(FAILED(swap->GetDevice(IID_PPV_ARGS(&d)))||!d)return;
    ID3D11DeviceContext* c{};d->GetImmediateContext(&c);
    if(c){fn(c,swap);c->Release();}d->Release();
}

struct B87OverlayRegistration{
    B87OverlayRegistration(){presentCallbackRegister(b87_on_present);}
    ~B87OverlayRegistration(){presentCallbackUnregister(b87_on_present);}
};
static B87OverlayRegistration g_b87_overlay_registration;
