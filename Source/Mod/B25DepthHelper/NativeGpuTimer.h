#pragma once
#include <d3d11.h>
#include <wrl/client.h>
namespace gta {
// Queries are polled only when a slot is reused. Never flush or wait for timing.
struct NativeGpuTimer {
 Microsoft::WRL::ComPtr<ID3D11Query> disjoint,start,finish;
 bool pending{},active{},valid{};double milliseconds{};UINT64 samples{};
 void create(ID3D11Device* device){D3D11_QUERY_DESC d{D3D11_QUERY_TIMESTAMP_DISJOINT,0};if(FAILED(device->CreateQuery(&d,&disjoint)))return;d.Query=D3D11_QUERY_TIMESTAMP;if(FAILED(device->CreateQuery(&d,&start))||FAILED(device->CreateQuery(&d,&finish))){disjoint.Reset();start.Reset();finish.Reset();}}
 void begin(ID3D11DeviceContext* context){active=false;if(!disjoint||!start||!finish)return;
  if(pending){D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{};UINT64 a{},b{};
   if(context->GetData(disjoint.Get(),&d,sizeof(d),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK||context->GetData(start.Get(),&a,sizeof(a),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK||context->GetData(finish.Get(),&b,sizeof(b),D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)return;
   valid=!d.Disjoint&&d.Frequency&&b>=a;if(valid){milliseconds=1000.0*double(b-a)/double(d.Frequency);++samples;}pending=false;
  }
  context->Begin(disjoint.Get());context->End(start.Get());active=true;
 }
 void end(ID3D11DeviceContext* context){if(!active)return;context->End(finish.Get());context->End(disjoint.Get());active=false;pending=true;}
};
}
