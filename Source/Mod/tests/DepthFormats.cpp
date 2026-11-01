#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <iostream>
#include <cmath>
#include "../DepthFormatPolicy.h"
using Microsoft::WRL::ComPtr;
void check(HRESULT h){if(FAILED(h))throw h;}
int main(){try{
 ComPtr<ID3D11Device>device;ComPtr<ID3D11DeviceContext>context;check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
 const char*code="Texture2D<float> depth:register(t0);RWTexture2D<float> result:register(u0);[numthreads(1,1,1)]void main(uint3 id:SV_DispatchThreadID){result[id.xy]=depth.Load(int3(id.xy,0));}";
 ComPtr<ID3DBlob>blob,error;check(D3DCompile(code,strlen(code),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&blob,&error));ComPtr<ID3D11ComputeShader>shader;check(device->CreateComputeShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&shader));
 for(auto format:{DXGI_FORMAT_R24G8_TYPELESS,DXGI_FORMAT_R32G8X24_TYPELESS}){
  D3D11_TEXTURE2D_DESC d{};d.Width=64;d.Height=36;d.MipLevels=1;d.ArraySize=1;d.Format=format;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;ComPtr<ID3D11Texture2D>source,copy,output,readback;check(device->CreateTexture2D(&d,nullptr,&source));
  D3D11_DEPTH_STENCIL_VIEW_DESC dd{};dd.Format=format==DXGI_FORMAT_R24G8_TYPELESS?DXGI_FORMAT_D24_UNORM_S8_UINT:DXGI_FORMAT_D32_FLOAT_S8X24_UINT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;ComPtr<ID3D11DepthStencilView>dsv;check(device->CreateDepthStencilView(source.Get(),&dd,&dsv));context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,0.25f,0);
  d.BindFlags=D3D11_BIND_SHADER_RESOURCE;check(device->CreateTexture2D(&d,nullptr,&copy));context->CopyResource(copy.Get(),source.Get());D3D11_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=gta::DepthSrvFormat(format);sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;ComPtr<ID3D11ShaderResourceView>srv;check(device->CreateShaderResourceView(copy.Get(),&sd,&srv));
  d.Format=DXGI_FORMAT_R32_FLOAT;d.BindFlags=D3D11_BIND_UNORDERED_ACCESS;check(device->CreateTexture2D(&d,nullptr,&output));ComPtr<ID3D11UnorderedAccessView>uav;check(device->CreateUnorderedAccessView(output.Get(),nullptr,&uav));ID3D11ShaderResourceView*srvs[]{srv.Get()};ID3D11UnorderedAccessView*uavs[]{uav.Get()};context->CSSetShader(shader.Get(),nullptr,0);context->CSSetShaderResources(0,1,srvs);context->CSSetUnorderedAccessViews(0,1,uavs,nullptr);context->Dispatch(64,36,1);ID3D11ShaderResourceView*nullSrv{};ID3D11UnorderedAccessView*nullUav{};context->CSSetShaderResources(0,1,&nullSrv);context->CSSetUnorderedAccessViews(0,1,&nullUav,nullptr);
  d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check(device->CreateTexture2D(&d,nullptr,&readback));context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));bool good=true;for(unsigned y=0;y<36;y++)for(unsigned x=0;x<64;x++){auto value=reinterpret_cast<float*>(static_cast<char*>(mapped.pData)+y*mapped.RowPitch)[x];good&=std::isfinite(value)&&std::fabs(value-0.25f)<1e-6f;}context->Unmap(readback.Get(),0);if(!good)throw E_FAIL;std::cout<<"PASS depth format="<<format<<" SRV="<<sd.Format<<" copy/sample 2304 pixels=0.25\n";
 }
 return 0;
 }catch(HRESULT h){std::cerr<<"FAIL hr="<<std::hex<<h<<"\n";return 1;}}
