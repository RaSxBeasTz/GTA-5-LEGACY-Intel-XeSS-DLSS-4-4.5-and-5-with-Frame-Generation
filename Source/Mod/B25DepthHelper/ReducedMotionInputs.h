#pragma once
#include <d3d11on12.h>
#include <d3d12.h>
#include <wrl/client.h>
namespace gta {
inline constexpr const char* ReducedMotionShader=R"(
cbuffer FrameData : register(b0) {
 row_major float4x4 previousViewProjection;
 row_major float4x4 currentInverseViewProjection;
 uint2 frameSize;float2 padding;
 row_major float4x4 clipToPrevious;
};
Texture2D<float4> depthInput : register(t0);
RWTexture2D<float2> motionOutput : register(u0);
RWTexture2D<float> depthOutput : register(u1);
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID){
 if(any(id.xy>=frameSize))return;
 uint sw,sh;depthInput.GetDimensions(sw,sh);
 float2 uv=(float2(id.xy)+0.5)/float2(frameSize);
 uint2 samplePixel=min(uint2(uv*float2(sw,sh)),uint2(sw-1,sh-1));
 uint3 bytes=(uint3)round(saturate(depthInput.Load(int3(samplePixel,0)).rgb)*255.0);
 float z=(float(bytes.x)+float(bytes.y)*256.0+float(bytes.z)*65536.0)/16777215.0;
 depthOutput[id.xy]=z;
 // Low-resolution input is undilated; XeFG upsamples and dilates it using depth.
 float2 motion=0;
 if(z>0&&z<1){float4 clip=float4(2*uv.x-1,1-2*uv.y,z,1);
  float4 prior=mul(clip,clipToPrevious);
  if(abs(prior.w)>1e-6){float2 ndc=prior.xy/prior.w;
   motion=clamp(float2((ndc.x-clip.x)*frameSize.x*0.5,(clip.y-ndc.y)*frameSize.y*0.5),-65504.0,65504.0);
  }
 }
 motionOutput[id.xy]=motion;
}
)";
struct ReducedMotionInputs {
 Microsoft::WRL::ComPtr<ID3D12Resource> motion,depth;
 Microsoft::WRL::ComPtr<ID3D11Resource> wrappedMotion,wrappedDepth;
 Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> motionUav,depthUav;
 unsigned width{},height{};
 HRESULT create(ID3D12Device*d,ID3D11On12Device*on12,ID3D11Device*d11,unsigned fullWidth,unsigned fullHeight){
  width=(fullWidth+1)/2;height=(fullHeight+1)/2;
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width;desc.Height=height;desc.DepthOrArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  desc.Format=DXGI_FORMAT_R16G16_FLOAT;HRESULT hr=d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&motion));
  desc.Format=DXGI_FORMAT_R32_FLOAT;if(SUCCEEDED(hr))hr=d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&depth));
  D3D11_RESOURCE_FLAGS flags{};flags.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
  if(SUCCEEDED(hr))hr=on12->CreateWrappedResource(motion.Get(),&flags,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&wrappedMotion));
  if(SUCCEEDED(hr))hr=on12->CreateWrappedResource(depth.Get(),&flags,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&wrappedDepth));
  if(SUCCEEDED(hr))hr=d11->CreateUnorderedAccessView(wrappedMotion.Get(),nullptr,&motionUav);
  if(SUCCEEDED(hr))hr=d11->CreateUnorderedAccessView(wrappedDepth.Get(),nullptr,&depthUav);
  return hr;
 }
};
}
