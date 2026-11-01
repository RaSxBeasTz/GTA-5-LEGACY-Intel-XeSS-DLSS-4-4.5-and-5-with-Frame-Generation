#pragma once
#include <d3d11_4.h>
#include <d3d12.h>
#include <wrl/client.h>
namespace gta {
// One allocation, two API views. Native rendering owns it in COMMON until
// its capture fence is signalled; consumers return it to COMMON before reuse.
struct DirectSharedTexture {
 Microsoft::WRL::ComPtr<ID3D12Resource> gpu;
 Microsoft::WRL::ComPtr<ID3D11Texture2D> native;
 Microsoft::WRL::ComPtr<ID3D12Fence> returned;
 Microsoft::WRL::ComPtr<ID3D11Fence> nativeReturned;
 UINT64 returnValue{};
 HRESULT acquireForNative(ID3D11DeviceContext4* context){return !returnValue?S_OK:(context&&nativeReturned?context->Wait(nativeReturned.Get(),returnValue):E_NOINTERFACE);}
 HRESULT releaseToNative(ID3D12CommandQueue* queue){return queue&&returned?queue->Signal(returned.Get(),++returnValue):E_NOINTERFACE;}
 HRESULT create(ID3D11Device* nativeDevice,ID3D12Device* device,unsigned width,unsigned height,DXGI_FORMAT format,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET){
  gpu.Reset();native.Reset();returned.Reset();nativeReturned.Reset();returnValue=0;if(!nativeDevice||!device||!width||!height)return E_INVALIDARG;
  Microsoft::WRL::ComPtr<ID3D11Device1> native1;HRESULT hr=nativeDevice->QueryInterface(IID_PPV_ARGS(&native1));if(FAILED(hr))return hr;
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width;desc.Height=height;desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=format;desc.SampleDesc.Count=1;desc.Flags=flags;
  hr=device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_SHARED,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&gpu));if(FAILED(hr))return hr;
  HANDLE handle{};hr=device->CreateSharedHandle(gpu.Get(),nullptr,GENERIC_ALL,nullptr,&handle);if(FAILED(hr)){gpu.Reset();return hr;}
  hr=native1->OpenSharedResource1(handle,IID_PPV_ARGS(&native));CloseHandle(handle);if(SUCCEEDED(hr)){
   hr=device->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&returned));
   if(SUCCEEDED(hr))hr=device->CreateSharedHandle(returned.Get(),nullptr,GENERIC_ALL,nullptr,&handle);
   if(SUCCEEDED(hr)){Microsoft::WRL::ComPtr<ID3D11Device5> native5;hr=nativeDevice->QueryInterface(IID_PPV_ARGS(&native5));if(SUCCEEDED(hr))hr=native5->OpenSharedFence(handle,IID_PPV_ARGS(&nativeReturned));CloseHandle(handle);}
  }
  if(FAILED(hr)){gpu.Reset();native.Reset();returned.Reset();nativeReturned.Reset();}return hr;
 }
};
}
