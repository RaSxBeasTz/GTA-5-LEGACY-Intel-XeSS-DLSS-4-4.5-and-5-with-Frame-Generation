#pragma once
#include "OpticalFlowSdk/nvOpticalFlowD3D12.h"
// Independent object-motion estimation. Resources stay owned for process lifetime,
// as do the existing SR/NR histories. Driver work is ordered by explicit GPU fences.
namespace opticalmotion {
struct Engine {
 HMODULE dll{};NV_OF_D3D12_API_FUNCTION_LIST api{};NvOFHandle handle{};
 rtx::ComPtr<ID3D12Device> device;rtx::ComPtr<ID3D12CommandQueue> queue;
 rtx::ComPtr<ID3D12Resource> images[2],forward,backward,resolved;
 NvOFGPUBufferHandle imageHandles[2]{},forwardHandle{},backwardHandle{};
 rtx::ComPtr<ID3D12Fence> copyFence,ofFence;UINT64 copyValue{},ofValue{},retire[3]{};
 rtx::ComPtr<ID3D12CommandAllocator> allocators[3];rtx::ComPtr<ID3D12GraphicsCommandList> copyList;
 rtx::ComPtr<ID3D12RootSignature> root;rtx::ComPtr<ID3D12PipelineState> convert,resolve;
 rtx::ComPtr<ID3D12DescriptorHeap> descriptors;unsigned w{},h{},index{},calls{};bool history{},hints{},failed{};
 void check(NV_OF_STATUS s,const char* stage){if(s!=NV_OF_SUCCESS)throw std::runtime_error(std::string(stage)+": OF "+std::to_string(int(s)));}
 rtx::ComPtr<ID3D12Resource> texture(DXGI_FORMAT format,D3D12_RESOURCE_FLAGS flags){
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;d.Flags=flags;rtx::ComPtr<ID3D12Resource> r;rtx::check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&r)),"object motion texture");return r;
 }
 void registration(ID3D12Resource* resource,NvOFGPUBufferHandle& out){NV_OF_REGISTER_RESOURCE_PARAMS_D3D12 p{};p.resource=resource;p.hOFGpuBuffer=&out;p.inputFencePoint={ofFence.Get(),ofValue};p.outputFencePoint={ofFence.Get(),++ofValue};check(api.nvOFRegisterResourceD3D12(handle,&p),"register object motion resource");}
 void init(ID3D12Device* d,ID3D12CommandQueue* q,unsigned width,unsigned height){
  device=d;queue=q;w=width;h=height;dll=LoadLibraryExW(L"nvofapi64.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll)throw std::runtime_error("Optical flow driver unavailable");
  using Entry=NV_OF_STATUS(NVOFAPI*)(uint32_t,NV_OF_D3D12_API_FUNCTION_LIST*);auto entry=reinterpret_cast<Entry>(GetProcAddress(dll,"NvOFAPICreateInstanceD3D12"));if(!entry)throw std::runtime_error("Optical flow entry unavailable");check(entry(NV_OF_API_VERSION,&api),"optical flow API");check(api.nvCreateOpticalFlowD3D12(d,&handle),"optical flow device");
  NV_OF_INIT_PARAMS p{};p.width=w;p.height=h;p.outGridSize=NV_OF_OUTPUT_VECTOR_GRID_SIZE_1;p.mode=NV_OF_MODE_OPTICALFLOW;p.perfLevel=NV_OF_PERF_LEVEL_MEDIUM;p.predDirection=NV_OF_PRED_DIRECTION_BOTH;p.inputBufferFormat=NV_OF_BUFFER_FORMAT_GRAYSCALE8;check(api.nvOFInit(handle,&p),"optical flow initialize");
  rtx::check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&copyFence)),"motion copy fence");rtx::check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&ofFence)),"optical flow fence");
  for(auto& a:allocators)rtx::check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a)),"motion allocator");rtx::check(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators[0].Get(),nullptr,IID_PPV_ARGS(&copyList)),"motion copy list");copyList->Close();
  for(unsigned i=0;i<2;++i){images[i]=texture(DXGI_FORMAT_R8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);registration(images[i].Get(),imageHandles[i]);}
  forward=texture(DXGI_FORMAT_R16G16_SINT,D3D12_RESOURCE_FLAG_NONE);registration(forward.Get(),forwardHandle);backward=texture(DXGI_FORMAT_R16G16_SINT,D3D12_RESOURCE_FLAG_NONE);registration(backward.Get(),backwardHandle);resolved=texture(DXGI_FORMAT_R16G16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,6,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,0}};D3D12_ROOT_PARAMETER parameters[2]{};for(unsigned i=0;i<2;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[i].DescriptorTable={1,&ranges[i]};}D3D12_ROOT_SIGNATURE_DESC rs{};rs.NumParameters=2;rs.pParameters=parameters;rtx::ComPtr<ID3DBlob> blob,error;rtx::check(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"motion root serialization");rtx::check(d->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"motion root");
  const char* conversion=R"(Texture2D<float4> source:register(t0);RWTexture2D<unorm float> output:register(u0);[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){uint w,h;output.GetDimensions(w,h);uint sw,sh;source.GetDimensions(sw,sh);uint2 samplePixel=min(uint2((float2(p.xy)+.5)*float2(sw,sh)/float2(w,h)),uint2(sw-1,sh-1));if(p.x<w&&p.y<h)output[p.xy]=dot(saturate(source.Load(int3(samplePixel,0)).rgb),float3(.2126,.7152,.0722));})";
  const char* resolving=R"(
Texture2D<int2> flow:register(t0);Texture2D<int2> reverseFlow:register(t1);
Texture2D<float2> camera:register(t2);Texture2D<float> depth:register(t3);
Texture2D<float> currentImage:register(t4);Texture2D<float> previousImage:register(t5);
RWTexture2D<float2> output:register(u0);
float errorAt(int2 p,float2 v,int2 size){float e=0;for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){int2 a=clamp(p+int2(x,y),0,size-1),b=clamp(int2(round(float2(a)+v)),0,size-1);e+=abs(currentImage.Load(int3(a,0))-previousImage.Load(int3(b,0)));}return e/9;}
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){
 uint w,h;output.GetDimensions(w,h);if(p.x>=w||p.y>=h)return;int2 size=int2(w,h),samplePixel=p.xy;
 float z=depth.Load(int3(p.xy,0));
 // Match the existing reversed-Z 3x3 foreground dilation, leaving depth intact.
 for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){int2 n=clamp(int2(p.xy)+int2(x,y),0,size-1);float d=depth.Load(int3(n,0));if(d>z){z=d;samplePixel=n;}}
 float2 base=camera.Load(int3(p.xy,0)),v=flow.Load(int3(samplePixel,0))/32.0;int2 previousPixel=int2(round(float2(samplePixel)+v));
 bool inside=all(previousPixel>=0)&&all(previousPixel<size);float2 back=reverseFlow.Load(int3(clamp(previousPixel,0,size-1),0))/32.0;
 bool valid=inside&&all(isfinite(v))&&length(v+back)<=1.5&&length(v-base)>.35&&z>0;
 if(valid){float oldError=errorAt(samplePixel,base,size),newError=errorAt(samplePixel,v,size);valid=oldError>.01&&newError<oldError*.75;}
 output[p.xy]=valid?v:base;
})";
  auto pipeline=[&](const char* source,rtx::ComPtr<ID3D12PipelineState>& target){rtx::ComPtr<ID3DBlob> code;rtx::check(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error),"motion shader");D3D12_COMPUTE_PIPELINE_STATE_DESC s{};s.pRootSignature=root.Get();s.CS={code->GetBufferPointer(),code->GetBufferSize()};rtx::check(d->CreateComputePipelineState(&s,IID_PPV_ARGS(&target)),"motion pipeline");};pipeline(conversion,convert);pipeline(resolving,resolve);
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=16*14;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;rtx::check(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&descriptors)),"motion descriptors");
 }
 void srv(unsigned offset,ID3D12Resource* r){auto cpu=descriptors->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=offset*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=r->GetDesc().Format;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;device->CreateShaderResourceView(r,&d,cpu);}
 void uav(unsigned offset,ID3D12Resource* r){auto cpu=descriptors->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=offset*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);D3D12_UNORDERED_ACCESS_VIEW_DESC d{};d.Format=r->GetDesc().Format;d.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;device->CreateUnorderedAccessView(r,nullptr,&d,cpu);}
 void dispatch(ID3D12GraphicsCommandList* list,ID3D12PipelineState* p,unsigned offset){ID3D12DescriptorHeap* heaps[]{descriptors.Get()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetPipelineState(p);auto gpu=descriptors->GetGPUDescriptorHandleForHeapStart();gpu.ptr+=offset*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);list->SetComputeRootDescriptorTable(0,gpu);gpu.ptr+=6*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);list->SetComputeRootDescriptorTable(1,gpu);list->Dispatch((w+7)/8,(h+7)/8,1);}
 ID3D12Resource* record(ID3D12GraphicsCommandList* list,ID3D12Resource* source,D3D12_RESOURCE_STATES state,ID3D12Resource* camera,ID3D12Resource* depth,bool reset,unsigned slot){
  if(failed||slot>=16)return nullptr;auto desc=source->GetDesc();if(!device)throw std::runtime_error("Optical motion engine must be initialized");if(desc.Width*h!=uint64_t(desc.Height)*w||camera->GetDesc().Width!=w||camera->GetDesc().Height!=h||depth->GetDesc().Width!=w||depth->GetDesc().Height!=h){history=false;hints=false;return nullptr;}
  unsigned allocator=++calls%3;if(copyFence->GetCompletedValue()<retire[allocator]){history=false;hints=false;return nullptr;}
  rtx::check(allocators[allocator]->Reset(),"motion allocator reset");rtx::check(copyList->Reset(allocators[allocator].Get(),convert.Get()),"motion list reset");rtx::check(queue->Wait(ofFence.Get(),ofValue),"motion previous work wait");
  unsigned current=index^1,offset=slot*14;srv(offset,source);uav(offset+6,images[current].Get());transition(copyList.Get(),source,state,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(copyList.Get(),images[current].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);dispatch(copyList.Get(),convert.Get(),offset);transition(copyList.Get(),images[current].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);transition(copyList.Get(),source,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,state);rtx::check(copyList->Close(),"motion copy close");ID3D12CommandList* lists[]{copyList.Get()};queue->ExecuteCommandLists(1,lists);rtx::check(queue->Signal(copyFence.Get(),++copyValue),"motion copy signal");retire[allocator]=copyValue;
  bool useHistory=history&&!reset;history=true;unsigned previous=index;index=current;if(!useHistory){hints=false;return nullptr;}
  NV_OF_FENCE_POINT input{copyFence.Get(),copyValue},output{ofFence.Get(),++ofValue};NV_OF_EXECUTE_INPUT_PARAMS_D3D12 in{};in.inputFrame=imageHandles[current];in.referenceFrame=imageHandles[previous];in.disableTemporalHints=hints?NV_OF_FALSE:NV_OF_TRUE;in.numFencePoints=1;in.fencePoint=&input;NV_OF_EXECUTE_OUTPUT_PARAMS_D3D12 out{};out.outputBuffer=forwardHandle;out.bwdOutputBuffer=backwardHandle;out.fencePoint=&output;check(api.nvOFExecuteD3D12(handle,&in,&out),"object motion execute");hints=true;rtx::check(queue->Wait(ofFence.Get(),ofValue),"object motion completion");
  offset+=7;transition(list,images[current].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(list,images[previous].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);srv(offset,forward.Get());srv(offset+1,backward.Get());srv(offset+2,camera);srv(offset+3,depth);srv(offset+4,images[current].Get());srv(offset+5,images[previous].Get());uav(offset+6,resolved.Get());transition(list,forward.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(list,backward.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(list,resolved.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);dispatch(list,resolve.Get(),offset);transition(list,resolved.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);transition(list,forward.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list,backward.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list,images[current].Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list,images[previous].Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);return resolved.Get();
 }
};
}
