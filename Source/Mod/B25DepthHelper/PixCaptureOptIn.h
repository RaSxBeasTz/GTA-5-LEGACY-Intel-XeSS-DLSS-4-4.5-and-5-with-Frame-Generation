#pragma once
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <fstream>
#include <string>
namespace gta {
inline void PixBeforeD3D12(){
 static const bool once=[](){
  wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::wstring root(exe);root.resize(root.find_last_of(L"\\/")+1);const auto ini=root+L"GTAXeSS.ini";
  if(!GetPrivateProfileIntW(L"Diagnostics",L"PIXCapture",0,ini.c_str()))return false;
  // One-shot opt-in: future launches use the ordinary runtime even if capture crashes.
  const BOOL disarmed=WritePrivateProfileStringW(L"Diagnostics",L"PIXCapture",L"0",ini.c_str());
  HMODULE module=disarmed?LoadLibraryExW(L"C:\\Program Files\\Microsoft PIX\\2603.25\\WinPixGpuCapturer.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS):nullptr;
  const DWORD error=module?0:GetLastError();
  std::ofstream out(std::wstring(root+L"GTAXeSS-PIX.log"));out<<"R617: PIX_CAPTURE_PRELOAD process_id="<<GetCurrentProcessId()<<", module_loaded="<<(module?1:0)<<", one_shot_disarmed="<<(disarmed?1:0)<<", error="<<error<<", before_first_mod_d3d12_api=1\n";
  // Keep the capture layer loaded for the lifetime of its instrumented devices.
  return module!=nullptr;
 }();(void)once;
}
// Same exported functions/signatures used by Microsoft's pix3_win.h helpers.
inline HRESULT PixCaptureNextFrames(HWND target,const wchar_t*file,unsigned frames){
 HMODULE module=GetModuleHandleW(L"WinPixGpuCapturer.dll");if(!module)return S_FALSE;
 using set_fn=void(WINAPI*)(HWND);using capture_fn=HRESULT(WINAPI*)(PCWSTR,UINT32);
 auto set=(set_fn)GetProcAddress(module,"SetGlobalTargetWindow");auto capture=(capture_fn)GetProcAddress(module,"CaptureNextFrame");
 if(!set||!capture)return E_NOINTERFACE;set(target);return capture(file,frames);
}
inline std::wstring ValidationRoot(){wchar_t p[32768]{};GetModuleFileNameW(nullptr,p,32768);std::wstring root(p);return root.substr(0,root.find_last_of(L"\\/")+1);}
inline void CALLBACK ValidationMessage(D3D12_MESSAGE_CATEGORY,D3D12_MESSAGE_SEVERITY severity,D3D12_MESSAGE_ID id,LPCSTR message,void*){if(severity<=D3D12_MESSAGE_SEVERITY_WARNING)std::ofstream(ValidationRoot()+L"GTAXeSS-D3D12Validation.log",std::ios::app)<<"id="<<id<<" severity="<<severity<<" "<<message<<"\n";}
inline HRESULT CreateProfiledD3D12Device(IUnknown*adapter,D3D_FEATURE_LEVEL level,REFIID iid,void**device){PixBeforeD3D12();
 const bool validation=GetPrivateProfileIntW(L"Diagnostics",L"D3D12Validation",0,(ValidationRoot()+L"GTAXeSS.ini").c_str())!=0;
 if(validation){Microsoft::WRL::ComPtr<ID3D12Debug>debug;auto hr=D3D12GetDebugInterface(IID_PPV_ARGS(&debug));std::ofstream(ValidationRoot()+L"GTAXeSS-D3D12Validation.log",std::ios::app)<<"Enable debug hr="<<std::hex<<hr<<"\n";if(debug)debug->EnableDebugLayer();}
 auto hr=D3D12CreateDevice(adapter,level,iid,device);
 if(validation&&SUCCEEDED(hr)&&device&&*device){Microsoft::WRL::ComPtr<ID3D12InfoQueue1>queue;if(SUCCEEDED(static_cast<IUnknown*>(*device)->QueryInterface(IID_PPV_ARGS(&queue)))){DWORD cookie{};queue->RegisterMessageCallback(ValidationMessage,D3D12_MESSAGE_CALLBACK_FLAG_NONE,nullptr,&cookie);}}
 return hr;}
}
