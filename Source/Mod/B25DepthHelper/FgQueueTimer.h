#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
namespace gta {
struct FgQueueTimer {
 Microsoft::WRL::ComPtr<ID3D12QueryHeap> heap;Microsoft::WRL::ComPtr<ID3D12Resource> readback;Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> endList;
 UINT64 frequency{};bool pending{},armed{};
 void create(ID3D12Device*d,ID3D12CommandQueue*q){
  if(heap)return;D3D12_QUERY_HEAP_DESC hd{};hd.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;hd.Count=2;
  if(FAILED(q->GetTimestampFrequency(&frequency))||!frequency||FAILED(d->CreateQueryHeap(&hd,IID_PPV_ARGS(&heap))))return;
  D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=16;rd.Height=rd.DepthOrArraySize=rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
  HRESULT h=d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback));
  if(SUCCEEDED(h))h=d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator));
  if(SUCCEEDED(h))h=d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&endList));
  if(SUCCEEDED(h)){endList->EndQuery(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);endList->ResolveQueryData(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,readback.Get(),0);h=endList->Close();}
  if(FAILED(h)){heap.Reset();readback.Reset();allocator.Reset();endList.Reset();}
 }
 // Called only after this slot's final-use fence has retired.
 bool poll(UINT64&ns){if(!pending||!readback)return false;void*data{};D3D12_RANGE r{0,16};if(FAILED(readback->Map(0,&r,&data)))return false;auto*t=static_cast<UINT64*>(data);bool valid=t[1]>=t[0];if(valid)ns=UINT64(1.0e9*double(t[1]-t[0])/double(frequency));D3D12_RANGE none{};readback->Unmap(0,&none);pending=false;return valid;}
 void start(ID3D12GraphicsCommandList*list){if(heap&&endList){list->EndQuery(heap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);armed=true;}}
 void end(ID3D12CommandQueue*q){if(armed&&heap&&endList){armed=false;ID3D12CommandList*lists[]{endList.Get()};q->ExecuteCommandLists(1,lists);pending=true;}}
};
}
