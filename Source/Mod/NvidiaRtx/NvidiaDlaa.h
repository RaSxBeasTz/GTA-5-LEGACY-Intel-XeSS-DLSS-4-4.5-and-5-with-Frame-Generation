#pragma once
#include "DlssVersionPolicy.h"
// Included by the NVIDIA backend; no dependencies enter the Intel DLLs.
namespace {
rtx::ComPtr<ID3D12RootSignature> colourRoot;
rtx::ComPtr<ID3D12PipelineState> colourPipeline,colourEnhancedPipeline;
rtx::ComPtr<ID3D12DescriptorHeap> colourSrvs,colourRtvs;
// R629: scratch SR/NR images are consumed entirely on the same ordered queue.
// FG reads separate backbuffers; the transport slots still own descriptors and input guides.
rtx::ComPtr<ID3D12Resource> dlaaOutput[1],nrOutput[1];
rtx::ComPtr<ID3D12Resource> fgSceneColour[16];

bool nrTestOverride=false;
bool nrEnabled(){static bool enabled=GetPrivateProfileIntW(L"NVIDIA",L"DLSS5",0,(moduleDirectory()+L"\\GTAXeSS.ini").c_str())!=0;return enabled||nrTestOverride;}
bool visualEnhancementEnabled(){static ULONGLONG next=0;static bool enabled=false;const ULONGLONG now=GetTickCount64();if(now>=next){enabled=GetPrivateProfileIntW(L"VisualEnhancement",L"Enabled",0,(moduleDirectory()+L"\\GTAXeSS.ini").c_str())!=0;next=now+250;}return enabled;}
using NrEvaluate=HRESULT(WINAPI*)(ID3D12Device*,ID3D12GraphicsCommandList*,ID3D12Resource*,ID3D12Resource*,ID3D12Resource*,ID3D12Resource*,BOOL);
NrEvaluate nrEvaluate(){static HMODULE dll=LoadLibraryExW((moduleDirectory()+L"\\GTAXeSS.DLSS5_nvngx.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);return dll?reinterpret_cast<NrEvaluate>(GetProcAddress(dll,"Dlss5Evaluate")):nullptr;}
void transition(ID3D12GraphicsCommandList*l,ID3D12Resource*r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){if(from==to)return;D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};l->ResourceBarrier(1,&b);}
void prepareColour(){if(colourRoot)return;
 D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};D3D12_ROOT_PARAMETER root{};root.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;root.DescriptorTable={1,&range};root.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;D3D12_ROOT_SIGNATURE_DESC rs{};rs.NumParameters=1;rs.pParameters=&root;rs.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;rtx::ComPtr<ID3DBlob> blob,error;rtx::check(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"colour root serialization");rtx::check(nativeDevice->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&colourRoot)),"colour root");
 const char*shader=R"(Texture2D<float4> image:register(t0);float4 vs(uint id:SV_VertexID):SV_Position{return float4(id==2?3:-1,id==1?3:-1,0,1);}float4 ps(float4 p:SV_Position):SV_Target{return float4(image.Load(int3(p.xy,0)).rgb,1);}float4 psEnhanced(float4 p:SV_Position):SV_Target{uint w,h;image.GetDimensions(w,h);int2 q=int2(p.xy);float3 c=saturate(image.Load(int3(q,0)).rgb);float3 n=image.Load(int3(clamp(q+int2(0,-1),int2(0,0),int2(w-1,h-1)),0)).rgb;float3 s=image.Load(int3(clamp(q+int2(0,1),int2(0,0),int2(w-1,h-1)),0)).rgb;float3 e=image.Load(int3(clamp(q+int2(1,0),int2(0,0),int2(w-1,h-1)),0)).rgb;float3 west=image.Load(int3(clamp(q+int2(-1,0),int2(0,0),int2(w-1,h-1)),0)).rgb;float3 blur=(n+s+e+west)*0.25;c=saturate(c+(c-blur)*0.14);float l=dot(c,float3(0.2126,0.7152,0.0722));c=lerp(l.xxx,c,1.065);c=pow(max(c,0.0),0.975);c=(c-0.5)*1.045+0.5;c*=float3(1.018,1.006,0.984);c=c/(0.975+0.025*c);float2 uv=(p.xy+0.5)/float2(w,h);float vig=saturate(1.0-dot(uv-0.5,uv-0.5)*0.18);c=c*vig+0.004;return float4(saturate(c),1);})";
 rtx::ComPtr<ID3DBlob> vs,ps,psEnhanced;rtx::check(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&error),"colour VS");rtx::check(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,&error),"colour PS");rtx::check(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"psEnhanced","ps_5_0",0,0,&psEnhanced,&error),"enhanced colour PS");D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=colourRoot.Get();p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={ps->GetBufferPointer(),ps->GetBufferSize()};p.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;p.SampleMask=UINT_MAX;p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=DXGI_FORMAT_B8G8R8A8_UNORM;p.SampleDesc.Count=1;rtx::check(nativeDevice->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&colourPipeline)),"colour pipeline");p.PS={psEnhanced->GetBufferPointer(),psEnhanced->GetBufferSize()};rtx::check(nativeDevice->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&colourEnhancedPipeline)),"enhanced colour pipeline");D3D12_DESCRIPTOR_HEAP_DESC h{};h.NumDescriptors=16;h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;h.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;rtx::check(nativeDevice->CreateDescriptorHeap(&h,IID_PPV_ARGS(&colourSrvs)),"DLAA SRV heap");h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;h.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;rtx::check(nativeDevice->CreateDescriptorHeap(&h,IID_PPV_ARGS(&colourRtvs)),"DLAA RTV heap");
}
}
#include "NrComparison.h"
#include "FgObjectMotion.h"
API HRESULT WINAPI NvidiaDrawDlaa(ID3D12GraphicsCommandList*list,ID3D12Resource*source,D3D12_RESOURCE_STATES sourceState,ID3D12Resource*backbuffer,unsigned slot){try{
 if(!list||!source||!backbuffer||slot>=16||!frame||!taggedDepth||!taggedMotion||!swap)return E_INVALIDARG;prepareColour();auto desc=backbuffer->GetDesc();
 const unsigned backIndex=swap->GetCurrentBackBufferIndex();if(backIndex>=16)return E_INVALIDARG;
 if(!fgSceneColour[backIndex]){D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;auto d=desc;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;rtx::check(nativeDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&fgSceneColour[backIndex])),"FG postprocessed colour");}
 auto sceneColour=fgSceneColour[backIndex].Get();
 if(!dlaaOutput[0]){D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;auto d=desc;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;rtx::check(nativeDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&dlaaOutput[0])),"DLAA output");}
 auto output=dlaaOutput[0].Get();sl::DLSSOptions options{};options.mode=upscaleMode;options.outputWidth=width;options.outputHeight=height;options.colorBuffersHDR=sl::Boolean::eFalse;options.useAutoExposure=sl::Boolean::eTrue;static const std::wstring version=[](){wchar_t value[16]{};GetPrivateProfileStringW(L"NVIDIA",L"DLSSVersion",L"4.5",value,16,(moduleDirectory()+L"\\GTAXeSS.ini").c_str());return std::wstring(value);}();rtx::setVersionPresets(options,version.c_str());static bool modelLogged=false;if(!modelLogged){modelLogged=true;log("R626 DLSS SR preset="+std::to_string((unsigned)rtx::versionPreset(version.c_str(),upscaleMode))+", neural_rendering="+std::to_string(nrEnabled()));}rtx::check(sdk.dlssOptions(viewport,options),"DLAA options");
 transition(list,source,sourceState,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 sl::Resource input(sl::ResourceType::eTex2d,source,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),result(sl::ResourceType::eTex2d,output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);auto sourceDesc=source->GetDesc();sl::Extent inputExtent{0,0,(uint32_t)sourceDesc.Width,sourceDesc.Height},extent{0,0,width,height};sl::ResourceTag tags[]{sl::ResourceTag(&input,sl::kBufferTypeScalingInputColor,sl::ResourceLifecycle::eValidUntilEvaluate,&inputExtent),sl::ResourceTag(&result,sl::kBufferTypeScalingOutputColor,sl::ResourceLifecycle::eValidUntilEvaluate,&extent)};rtx::check(sdk.tags(*frame,viewport,tags,2,list),"DLAA colour tags");const sl::BaseStructure* inputs[]{&viewport};static unsigned trace=0;const bool tracing=++trace<=3;if(tracing)log("R627: DLSS_EVALUATE_BEGIN");// R658: SL 39 is NVIDIA's nonfatal eWarnOutOfVRAM warning, not an
 // evaluation failure. Preserve the SR/NR/FG pipeline and report the pressure
 // just as R629 already does for DLSS-G state/options. Other errors still fail.
 const auto evaluationResult=sdk.evaluate(sl::kFeatureDLSS,*frame,inputs,1,list);
 if(evaluationResult==sl::Result::eWarnOutOfVRAM){
  static unsigned dlssVramWarnings=0;
  const unsigned count=++dlssVramWarnings;
  if(count<=3||count%120==0)
   log("R658: DLSS_EVALUATE_VRAM_WARNING result=39, frame="+std::to_string(frameId)+", count="+std::to_string(count)+", action=nonfatal_warning, pipeline=retained");
 }else rtx::check(evaluationResult,"DLSS DLAA evaluation");if(tracing)log("R627: DLSS_EVALUATE_END");transition(list,output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(list,source,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,sourceState);
 if(nrEnabled()){
  static bool nrFailed=false;
  if(!nrFailed){
   if(!nrOutput[0]){D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;auto d=output->GetDesc();rtx::check(nativeDevice->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&nrOutput[0])),"NR output");}
   transition(list,output,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   auto apply=nrEvaluate();auto hr=apply?apply(nativeDevice.Get(),list,output,nrOutput[0].Get(),taggedDepth,taggedMotion,reset):E_NOINTERFACE;
   if(SUCCEEDED(hr)){nrcomparison::record(list,output,nrOutput[0].Get(),source,sourceState);transition(list,nrOutput[0].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(list,output,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);output=nrOutput[0].Get();}
   else{transition(list,output,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);nrFailed=true;log("R625 DLSS5 disabled after evaluation failure; DLSS/FG continues, hr="+std::to_string((long)hr));}
  }
 }
auto srv=colourSrvs->GetCPUDescriptorHandleForHeapStart();srv.ptr+=slot*nativeDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=1;nativeDevice->CreateShaderResourceView(output,&view,srv);auto rtv=colourRtvs->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=slot*nativeDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);nativeDevice->CreateRenderTargetView(sceneColour,nullptr,rtv);
 const bool visualEnhanced=visualEnhancementEnabled();transition(list,sceneColour,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);list->SetGraphicsRootSignature(colourRoot.Get());list->SetPipelineState(visualEnhanced?colourEnhancedPipeline.Get():colourPipeline.Get());ID3D12DescriptorHeap* heaps[]{colourSrvs.Get()};list->SetDescriptorHeaps(1,heaps);auto gpu=colourSrvs->GetGPUDescriptorHandleForHeapStart();gpu.ptr+=slot*nativeDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);list->SetGraphicsRootDescriptorTable(0,gpu);D3D12_VIEWPORT vp{0,0,(float)width,(float)height,0,1};D3D12_RECT scissor{0,0,(LONG)width,(LONG)height};list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);if(tracing)log("R645: OUTPUT_DRAW_BEGIN visual_enhancement="+std::to_string(visualEnhanced?1:0));list->DrawInstanced(3,1,0,0);if(tracing)log("R645: OUTPUT_DRAW_END");transition(list,output,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 // FG's scene input must match the final postprocessed image, before our HUD.
 // Retain one colour per swapchain buffer until the existing FG retirement fence.
 transition(list,sceneColour,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
 transition(list,backbuffer,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);
 list->CopyResource(backbuffer,sceneColour);
 transition(list,backbuffer,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_RENDER_TARGET);
 transition(list,sceneColour,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 sl::Resource fgColour(sl::ResourceType::eTex2d,sceneColour,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 sl::ResourceTag fgTag(&fgColour,sl::kBufferTypeHUDLessColor,sl::ResourceLifecycle::eValidUntilPresent,&extent);
 rtx::check(sdk.tags(*frame,viewport,&fgTag,1,list),"FG final scene colour");
 fgmotion::scene=sceneColour;fgmotion::slot=slot;
 if(tracing)log("R630: FG_COLOUR_MATCH source=post_SR_NR_before_mod_HUD, width="+std::to_string(width)+", height="+std::to_string(height)+", backbuffer="+std::to_string(backIndex));return S_OK;
 }catch(const std::exception&e){log(e.what());return E_FAIL;}}







