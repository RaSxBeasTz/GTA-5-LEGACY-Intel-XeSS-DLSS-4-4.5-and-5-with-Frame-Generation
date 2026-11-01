#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cwctype>
#include <string>
extern "C" __declspec(dllexport) BOOL WINAPI IntelArcMatchesAdapter(UINT vendor,const wchar_t* description){
 if(vendor!=0x8086||!description)return FALSE;
 std::wstring name(description);for(auto& c:name)c=(wchar_t)towlower(c);
 size_t p=0;while((p=name.find(L"arc",p))!=std::wstring::npos){
  const bool left=p==0||!iswalnum(name[p-1]);const size_t end=p+3;
  const bool right=end==name.size()||!iswalnum(name[end]);
  if(left&&right)return TRUE;++p;
 }return FALSE;
}
extern "C" __declspec(dllexport) unsigned WINAPI IntelArcApiVersion(){return 1;}
extern "C" __declspec(dllexport) BOOL WINAPI IntelArcAllowsDevice(ID3D11Device* device){
 if(!device)return FALSE;
 Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
 if(FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetDesc(&desc)))return FALSE;
 return IntelArcMatchesAdapter(desc.VendorId,desc.Description);
}
