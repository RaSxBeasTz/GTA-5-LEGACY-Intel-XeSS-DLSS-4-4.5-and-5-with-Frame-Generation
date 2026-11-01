#pragma once
#include <windows.h>
#include <string>
#include <type_traits>
namespace rtxbridge {
inline std::wstring directory(){wchar_t p[32768]{};GetModuleFileNameW(nullptr,p,32768);std::wstring s(p);return s.substr(0,s.find_last_of(L"\\/")+1);}
inline bool selected(){static bool value=[](){wchar_t name[32]{};GetPrivateProfileStringW(L"Graphics",L"Provider",L"Intel",name,32,(directory()+L"GTAXeSS.ini").c_str());return _wcsicmp(name,L"NVIDIA")==0;}();return value;}
inline bool dlaa(){static bool value=GetPrivateProfileIntW(L"NVIDIA",L"DLAA",0,(directory()+L"GTAXeSS.ini").c_str())!=0;return selected()&&(value||gta::UpscalingEnabled());}
inline unsigned interpolations(){if(!selected())return 3;static unsigned m=GetPrivateProfileIntW(L"NVIDIA",L"Multiplier",2,(directory()+L"GTAXeSS.ini").c_str());return m==2||m==4||m==6?m-1:0;}
inline HMODULE module(){static HMODULE m=LoadLibraryExW((directory()+L"NVIDIA RTX.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);return m;}
template<class Fn,class...Args>auto invoke(Fn intel,const char* name,Args&&...args)->decltype(intel(static_cast<Args&&>(args)...)) {using Result=decltype(intel(static_cast<Args&&>(args)...));if(!selected())return intel(static_cast<Args&&>(args)...);auto fn=module()?reinterpret_cast<Fn>(GetProcAddress(module(),name)):nullptr;return fn?fn(static_cast<Args&&>(args)...):static_cast<Result>(-11);}
inline HRESULT queue(ID3D12Device*d,const D3D12_COMMAND_QUEUE_DESC*q,REFIID iid,void**out){if(!selected())return d->CreateCommandQueue(q,iid,out);using F=HRESULT(WINAPI*)(ID3D12Device*,const D3D12_COMMAND_QUEUE_DESC*,REFIID,void**);auto fn=module()?reinterpret_cast<F>(GetProcAddress(module(),"NvidiaCreateQueue")):nullptr;return fn?fn(d,q,iid,out):HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);}
inline HRESULT transform(const float*forward,const float*inverse){using F=void(WINAPI*)(const float*,const float*);auto fn=module()?reinterpret_cast<F>(GetProcAddress(module(),"NvidiaSetMotionTransform")):nullptr;if(!fn)return E_NOINTERFACE;fn(forward,inverse);return S_OK;}
inline HRESULT beforePresent(){using F=HRESULT(WINAPI*)();auto fn=module()?reinterpret_cast<F>(GetProcAddress(module(),"NvidiaBeforePresent")):nullptr;return fn?fn():E_NOINTERFACE;}
inline HRESULT afterPresent(){using F=HRESULT(WINAPI*)();auto fn=module()?reinterpret_cast<F>(GetProcAddress(module(),"NvidiaAfterPresent")):nullptr;return fn?fn():E_NOINTERFACE;}
inline HRESULT draw(ID3D12GraphicsCommandList*l,ID3D12Resource*s,D3D12_RESOURCE_STATES state,ID3D12Resource*b,unsigned slot){using F=HRESULT(WINAPI*)(ID3D12GraphicsCommandList*,ID3D12Resource*,D3D12_RESOURCE_STATES,ID3D12Resource*,unsigned);auto fn=module()?reinterpret_cast<F>(GetProcAddress(module(),"NvidiaDrawDlaa")):nullptr;return fn?fn(l,s,state,b,slot):E_NOINTERFACE;}
}
#define xefgSwapChainD3D12CreateContext(...) rtxbridge::invoke(&::xefgSwapChainD3D12CreateContext,"Nv_xefgSwapChainD3D12CreateContext",__VA_ARGS__)
#define xefgSwapChainD3D12GetSwapChainPtr(...) rtxbridge::invoke(&::xefgSwapChainD3D12GetSwapChainPtr,"Nv_xefgSwapChainD3D12GetSwapChainPtr",__VA_ARGS__)
#define xefgSwapChainD3D12InitFromSwapChainDesc(...) rtxbridge::invoke(&::xefgSwapChainD3D12InitFromSwapChainDesc,"Nv_xefgSwapChainD3D12InitFromSwapChainDesc",__VA_ARGS__)
#define xefgSwapChainD3D12TagFrameResource(...) rtxbridge::invoke(&::xefgSwapChainD3D12TagFrameResource,"Nv_xefgSwapChainD3D12TagFrameResource",__VA_ARGS__)
#define xefgSwapChainDestroy(...) rtxbridge::invoke(&::xefgSwapChainDestroy,"Nv_xefgSwapChainDestroy",__VA_ARGS__)
#define xefgSwapChainGetLastPresentStatus(...) rtxbridge::invoke(&::xefgSwapChainGetLastPresentStatus,"Nv_xefgSwapChainGetLastPresentStatus",__VA_ARGS__)
#define xefgSwapChainGetProperties(...) rtxbridge::invoke(&::xefgSwapChainGetProperties,"Nv_xefgSwapChainGetProperties",__VA_ARGS__)
#define xefgSwapChainGetVersion(...) rtxbridge::invoke(&::xefgSwapChainGetVersion,"Nv_xefgSwapChainGetVersion",__VA_ARGS__)
#define xefgSwapChainSetEnabled(...) rtxbridge::invoke(&::xefgSwapChainSetEnabled,"Nv_xefgSwapChainSetEnabled",__VA_ARGS__)
#define xefgSwapChainSetLatencyReduction(...) rtxbridge::invoke(&::xefgSwapChainSetLatencyReduction,"Nv_xefgSwapChainSetLatencyReduction",__VA_ARGS__)
#define xefgSwapChainSetLoggingCallback(...) rtxbridge::invoke(&::xefgSwapChainSetLoggingCallback,"Nv_xefgSwapChainSetLoggingCallback",__VA_ARGS__)
#define xefgSwapChainSetNumInterpolatedFrames(...) rtxbridge::invoke(&::xefgSwapChainSetNumInterpolatedFrames,"Nv_xefgSwapChainSetNumInterpolatedFrames",__VA_ARGS__)
#define xefgSwapChainSetPresentId(...) rtxbridge::invoke(&::xefgSwapChainSetPresentId,"Nv_xefgSwapChainSetPresentId",__VA_ARGS__)
#define xefgSwapChainSetUiCompositionState(...) rtxbridge::invoke(&::xefgSwapChainSetUiCompositionState,"Nv_xefgSwapChainSetUiCompositionState",__VA_ARGS__)
#define xefgSwapChainTagFrameConstants(...) rtxbridge::invoke(&::xefgSwapChainTagFrameConstants,"Nv_xefgSwapChainTagFrameConstants",__VA_ARGS__)
#define xellAddMarkerData(...) rtxbridge::invoke(&::xellAddMarkerData,"Nv_xellAddMarkerData",__VA_ARGS__)
#define xellD3D12CreateContext(...) rtxbridge::invoke(&::xellD3D12CreateContext,"Nv_xellD3D12CreateContext",__VA_ARGS__)
#define xellDestroyContext(...) rtxbridge::invoke(&::xellDestroyContext,"Nv_xellDestroyContext",__VA_ARGS__)
#define xellGetVersion(...) rtxbridge::invoke(&::xellGetVersion,"Nv_xellGetVersion",__VA_ARGS__)
#define xellSetSleepMode(...) rtxbridge::invoke(&::xellSetSleepMode,"Nv_xellSetSleepMode",__VA_ARGS__)
#define xellSleep(...) rtxbridge::invoke(&::xellSleep,"Nv_xellSleep",__VA_ARGS__)
