#pragma once
#include <windows.h>
// Preserve the exact prior application setting, including when it was already 1.
// Mutations and verification run on the caller; the wrapper serializes access.
template<class Device> struct RenderAheadScope {
 bool active{};UINT original{},target{};
 HRESULT apply(Device*device,bool enable,UINT requested=1){
  if(!device)return E_NOINTERFACE;
  if(enable){if(requested<1||requested>16)return E_INVALIDARG;if(active&&target==requested)return S_FALSE;
   UINT before=original;HRESULT hr=S_OK;if(!active){hr=device->GetMaximumFrameLatency(&before);if(FAILED(hr))return hr;}
   hr=device->SetMaximumFrameLatency(requested);if(FAILED(hr))return hr;
   original=before;active=true;target=requested;
   UINT actual{};hr=device->GetMaximumFrameLatency(&actual);if(FAILED(hr)||actual!=requested){HRESULT restored=apply(device,false);return FAILED(restored)?restored:(FAILED(hr)?hr:E_FAIL);}return S_OK;
  }
  if(!active)return S_FALSE;
  HRESULT hr=device->SetMaximumFrameLatency(original);if(FAILED(hr))return hr;
  UINT actual{};hr=device->GetMaximumFrameLatency(&actual);if(FAILED(hr)||actual!=original)return FAILED(hr)?hr:E_FAIL;
  active=false;target=0;return S_OK;
 }
};
