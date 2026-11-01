#pragma once
#include <d3d11_4.h>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>
#include "NativeGpuTimer.h"
namespace gta {
// One allocation, two API views. Native rendering owns it in COMMON until
// its capture fence is signalled; consumers return it to COMMON before reuse.
struct DirectSharedTexture {
 Microsoft::WRL::ComPtr<ID3D12Resource> gpu;
 Microsoft::WRL::ComPtr<ID3D11Texture2D> native;
 Microsoft::WRL::ComPtr<ID3D12Fence> returned;
 Microsoft::WRL::ComPtr<ID3D11Fence> nativeReturned;
 NativeGpuTimer depthTimer,colourTimer;
 UINT64 returnValue{};
 UINT64 waitsQueued{},waitsAlreadyComplete{};
 HRESULT acquireForNative(ID3D11DeviceContext4* context){
  if(!returnValue)return S_OK;if(!context||!nativeReturned)return E_NOINTERFACE;
  const UINT64 complete=nativeReturned->GetCompletedValue();if(complete==UINT64_MAX)return DXGI_ERROR_DEVICE_REMOVED;
  if(complete>=returnValue){++waitsAlreadyComplete;return S_OK;}
  ++waitsQueued;return context->Wait(nativeReturned.Get(),returnValue);
 }
 HRESULT releaseToNative(ID3D12CommandQueue* queue){return queue&&returned?queue->Signal(returned.Get(),++returnValue):E_NOINTERFACE;}
 HRESULT create(ID3D11Device* nativeDevice,ID3D12Device* device,unsigned width,unsigned height,DXGI_FORMAT format,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET){
  gpu.Reset();native.Reset();returned.Reset();nativeReturned.Reset();depthTimer=NativeGpuTimer{};colourTimer=NativeGpuTimer{};returnValue=0;waitsQueued=waitsAlreadyComplete=0;if(!nativeDevice||!device||!width||!height)return E_INVALIDARG;
  // R604: native D3D11 owns allocation metadata; D3D12 opens the NT resource.
  D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;desc.Format=format;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
  desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|((flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)?D3D11_BIND_DEPTH_STENCIL:D3D11_BIND_RENDER_TARGET);
  desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
  HRESULT hr=nativeDevice->CreateTexture2D(&desc,nullptr,&native);if(FAILED(hr))return hr;
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;hr=native.As(&resource);HANDLE handle{};
  if(SUCCEEDED(hr))hr=resource->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&handle);
  if(SUCCEEDED(hr)){hr=device->OpenSharedHandle(handle,IID_PPV_ARGS(&gpu));CloseHandle(handle);}
  if(SUCCEEDED(hr)){
   hr=device->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&returned));
   if(SUCCEEDED(hr))hr=device->CreateSharedHandle(returned.Get(),nullptr,GENERIC_ALL,nullptr,&handle);
   if(SUCCEEDED(hr)){Microsoft::WRL::ComPtr<ID3D11Device5> native5;hr=nativeDevice->QueryInterface(IID_PPV_ARGS(&native5));if(SUCCEEDED(hr))hr=native5->OpenSharedFence(handle,IID_PPV_ARGS(&nativeReturned));CloseHandle(handle);}
  }
  if(FAILED(hr)){gpu.Reset();native.Reset();returned.Reset();nativeReturned.Reset();}else{depthTimer.create(nativeDevice);colourTimer.create(nativeDevice);}return hr;
 }
};
}
