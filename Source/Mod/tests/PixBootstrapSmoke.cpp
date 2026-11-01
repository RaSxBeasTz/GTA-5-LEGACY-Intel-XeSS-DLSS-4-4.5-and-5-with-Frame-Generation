#include "../B25DepthHelper/PixCaptureOptIn.h"
#include <wrl/client.h>
#include <iostream>
int main(){Microsoft::WRL::ComPtr<ID3D12Device>device;HRESULT hr=gta::CreateProfiledD3D12Device(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));bool loaded=GetModuleHandleW(L"WinPixGpuCapturer.dll")!=nullptr;std::cout<<"PIX loaded="<<loaded<<" D3D12 hr="<<(long)hr<<"\n";return FAILED(hr)||!loaded?1:0;}
