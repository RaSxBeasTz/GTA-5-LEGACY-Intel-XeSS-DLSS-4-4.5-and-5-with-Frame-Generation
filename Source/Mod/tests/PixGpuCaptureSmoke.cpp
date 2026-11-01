#include "../B25DepthHelper/PixCaptureOptIn.h"
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void hr(HRESULT value){if(FAILED(value))throw std::runtime_error(std::to_string((long)value));}
int main(){try{
 ComPtr<ID3D12Device>device;hr(gta::CreateProfiledD3D12Device(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"GTAXeSSPixSmoke";RegisterClassW(&wc);
 HWND window=CreateWindowW(wc.lpszClassName,L"PIX smoke",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,wc.hInstance,nullptr);if(!window)throw std::runtime_error("window");
 ComPtr<IDXGIFactory2>factory;hr(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
 D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue>queue;hr(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)));
 DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=sd.Height=32;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferCount=2;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 ComPtr<IDXGISwapChain1>swap;hr(factory->CreateSwapChainForHwnd(queue.Get(),window,&sd,nullptr,nullptr,&swap));ComPtr<IDXGISwapChain3>swap3;hr(swap.As(&swap3));
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=2;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;ComPtr<ID3D12DescriptorHeap>heap;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
 ComPtr<ID3D12Resource>buffers[2];auto handles=heap->GetCPUDescriptorHandleForHeapStart();const UINT stride=device->GetDescriptorHandleIncrementSize(hd.Type);for(unsigned i=0;i<2;++i){hr(swap3->GetBuffer(i,IID_PPV_ARGS(&buffers[i])));D3D12_CPU_DESCRIPTOR_HANDLE handle{handles.ptr+i*stride};device->CreateRenderTargetView(buffers[i].Get(),nullptr,handle);}
 ComPtr<ID3D12CommandAllocator>allocator;ComPtr<ID3D12GraphicsCommandList>list;hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));hr(list->Close());
 ComPtr<ID3D12Fence>fence;hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);std::wstring file(exe);file.resize(file.find_last_of(L"\\/")+1);file+=L"pix-programmatic-smoke.wpix";
 hr(gta::PixCaptureNextFrames(window,file.c_str(),2));
 for(unsigned n=0;n<8;++n){hr(allocator->Reset());hr(list->Reset(allocator.Get(),nullptr));const UINT index=swap3->GetCurrentBackBufferIndex();D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=buffers[index].Get();barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;list->ResourceBarrier(1,&barrier);D3D12_CPU_DESCRIPTOR_HANDLE handle{handles.ptr+index*stride};FLOAT colour[]{float(n)/8,0,1,1};list->ClearRenderTargetView(handle,colour,0,nullptr);barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;list->ResourceBarrier(1,&barrier);hr(list->Close());ID3D12CommandList*lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);hr(swap3->Present(0,0));hr(queue->Signal(fence.Get(),n+1));hr(fence->SetEventOnCompletion(n+1,event));WaitForSingleObject(event,10000);Sleep(100);}
 CloseHandle(event);DestroyWindow(window);std::wcout<<L"PASS: programmatic PIX frames written to "<<file<<L"\n";return 0;
}catch(const std::exception&e){std::cerr<<"PIX smoke failed: "<<e.what()<<"\n";return 1;}}
