#include "StreamlineRuntime.h"
#include <xess_fg/xefg_swapchain.h>
#include <xess_fg/xefg_swapchain_d3d12.h>
#include <xell/xell.h>
#include <xell/xell_d3d12.h>
#include <DirectXMath.h>
#include <mutex>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <d3dcompiler.h>
// The bridge ABI reuses existing frame/resource structures. All NVIDIA work is
// executed by Streamline, never by the Intel implementation behind these types.
namespace {
rtx::Streamline sdk;
rtx::ComPtr<ID3D12Device> nativeDevice,proxyDevice;
rtx::ComPtr<ID3D12CommandQueue> proxyQueue;
rtx::ComPtr<IDXGISwapChain3> swap;
sl::FrameToken* frame{};
uint32_t frameId{},generated=1,width{},height{},inputWidth{},inputHeight{};
uint32_t lastPresentedFrame{};
sl::DLSSMode upscaleMode=sl::DLSSMode::eDLAA;
bool enabled{},reset{},bound{};
ID3D12Resource* taggedDepth{};ID3D12Resource* taggedMotion{};
std::mutex initialization;
sl::ViewportHandle viewport(0);
std::wstring moduleDirectory(){HMODULE self{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&moduleDirectory),&self);wchar_t p[32768]{};GetModuleFileNameW(self,p,32768);return std::filesystem::path(p).parent_path().wstring();}
void log(const std::string& message){std::ofstream(moduleDirectory()+L"\\GTAXeSS-NVIDIA.log",std::ios::app)<<message<<"\n";}
void checkFg(sl::Result result,const char* stage){
 if(result==sl::Result::eWarnOutOfVRAM){static unsigned warnings=0;if(++warnings<=3||warnings%120==0)log(std::string("R629: FG_VRAM_WARNING stage=")+stage+", frame="+std::to_string(frameId)+", count="+std::to_string(warnings));return;}
 rtx::check(result,stage);
}
void begin(uint32_t id){if(!frame||frameId!=id){rtx::check(sdk.token(frame,&id),"frame token");frameId=id;}}
void setOptions(){sl::DLSSGOptions o{};o.mode=enabled?sl::DLSSGMode::eOn:sl::DLSSGMode::eOff;o.numFramesToGenerate=generated;o.numBackBuffers=3;o.mvecDepthWidth=inputWidth?inputWidth:width;o.mvecDepthHeight=inputHeight?inputHeight:height;o.colorWidth=width;o.colorHeight=height;o.colorBufferFormat=DXGI_FORMAT_B8G8R8A8_UNORM;checkFg(sdk.fgOptions(viewport,o),"DLSS FG options");}
template<class F> xefg_swapchain_result_t call(F f){try{f();return XEFG_SWAPCHAIN_RESULT_SUCCESS;}catch(const std::exception&e){log(e.what());return XEFG_SWAPCHAIN_RESULT_ERROR_DEVICE;}}
sl::float4x4 matrix(DirectX::FXMMATRIX m){sl::float4x4 out;DirectX::XMFLOAT4X4 values;DirectX::XMStoreFloat4x4(&values,m);std::memcpy(&out,&values,64);return out;}
DirectX::XMMATRIX load(const float*m){return DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(m));}
sl::float4x4 clipToPrevious{},previousToClip{};
}
#define API extern "C" __declspec(dllexport)
API HRESULT WINAPI NvidiaCreateQueue(ID3D12Device*device,const D3D12_COMMAND_QUEUE_DESC*desc,REFIID iid,void**out){
 try{std::lock_guard<std::mutex> lock(initialization);if(!device||!out)return E_INVALIDARG;
 if(!bound){sdk.start(moduleDirectory());sdk.bind(device);nativeDevice=device;proxyDevice=device;rtx::check(sdk.upgrade(reinterpret_cast<void**>(proxyDevice.GetAddressOf())),"upgrade device");bound=true;log("R623 NVIDIA Streamline 2.14.16 initialized; native interop device; proxy queue/swapchain");}
 rtx::check(proxyDevice->CreateCommandQueue(desc,IID_PPV_ARGS(&proxyQueue)),"create hooked queue");
 IUnknown* native{};rtx::check(sdk.native(proxyQueue.Get(),reinterpret_cast<void**>(&native)),"unwrap queue");auto hr=native->QueryInterface(iid,out);native->Release();return hr;
 }catch(const std::exception&e){log(e.what());return E_FAIL;}
}
API xell_result_t Nv_xellGetVersion(xell_version_t*v){if(!v)return (xell_result_t)-1;*v={2,14,16};return XELL_RESULT_SUCCESS;}
API xefg_swapchain_result_t Nv_xefgSwapChainGetVersion(xefg_swapchain_version_t*v){if(!v)return XEFG_SWAPCHAIN_RESULT_ERROR_INVALID_ARGUMENT;*v={2,14,16,0};return XEFG_SWAPCHAIN_RESULT_SUCCESS;}
API xell_result_t Nv_xellD3D12CreateContext(ID3D12Device*,xell_context_handle_t*out){if(!bound||!out)return (xell_result_t)-1;*out=reinterpret_cast<xell_context_handle_t>(&sdk);return XELL_RESULT_SUCCESS;}
API xell_result_t Nv_xellDestroyContext(xell_context_handle_t){return XELL_RESULT_SUCCESS;}
API xell_result_t Nv_xellSetSleepMode(xell_context_handle_t,const xell_sleep_params_t*){sl::ReflexOptions o{};o.mode=sl::ReflexMode::eLowLatency;auto r=sdk.reflexOptions(o);return r==sl::Result::eOk?XELL_RESULT_SUCCESS:(xell_result_t)-1;}
API xell_result_t Nv_xellSleep(xell_context_handle_t,uint32_t id){try{begin(id);rtx::check(sdk.sleep(*frame),"Reflex sleep");return XELL_RESULT_SUCCESS;}catch(const std::exception&e){log(e.what());return (xell_result_t)-1;}}
API xell_result_t Nv_xellAddMarkerData(xell_context_handle_t,uint32_t id,xell_latency_marker_type_t marker){try{begin(id);sl::PCLMarker m;switch(marker){case XELL_SIMULATION_START:m=sl::PCLMarker::eSimulationStart;break;case XELL_SIMULATION_END:m=sl::PCLMarker::eSimulationEnd;break;case XELL_RENDERSUBMIT_START:m=sl::PCLMarker::eRenderSubmitStart;break;case XELL_RENDERSUBMIT_END:m=sl::PCLMarker::eRenderSubmitEnd;break;case XELL_PRESENT_START:m=sl::PCLMarker::ePresentStart;break;case XELL_PRESENT_END:m=sl::PCLMarker::ePresentEnd;break;default:return (xell_result_t)-1;}rtx::check(sdk.marker(m,*frame),"Reflex marker");return XELL_RESULT_SUCCESS;}catch(const std::exception&e){log(e.what());return (xell_result_t)-1;}}
API xefg_swapchain_result_t Nv_xefgSwapChainD3D12CreateContext(ID3D12Device*,xefg_swapchain_handle_t*out){if(!bound||!out)return XEFG_SWAPCHAIN_RESULT_ERROR_UNINITIALIZED;*out=reinterpret_cast<xefg_swapchain_handle_t>(&sdk);return XEFG_SWAPCHAIN_RESULT_SUCCESS;}
API xefg_swapchain_result_t Nv_xefgSwapChainGetProperties(xefg_swapchain_handle_t,xefg_swapchain_properties_t*out){return call([&]{if(!out)throw std::runtime_error("Null properties");sl::DLSSGState state{};checkFg(sdk.fgState(viewport,state,nullptr),"FG capabilities");*out={};out->maxSupportedInterpolations=state.numFramesToGenerateMax;});}
API xefg_swapchain_result_t Nv_xefgSwapChainD3D12InitFromSwapChainDesc(xefg_swapchain_handle_t,HWND window,const DXGI_SWAP_CHAIN_DESC1*desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*fullscreen,ID3D12CommandQueue*,IDXGIFactory2*factory,const xefg_swapchain_d3d12_init_params_t*params){return call([&]{
 if(!desc||!params||!proxyQueue)throw std::runtime_error("Missing swapchain inputs");width=desc->Width;height=desc->Height;generated=params->maxInterpolatedFrames;
 auto ini=moduleDirectory()+L"\\GTAXeSS.ini";inputWidth=GetPrivateProfileIntW(L"Upscaling",L"InputWidth",width,ini.c_str());inputHeight=GetPrivateProfileIntW(L"Upscaling",L"InputHeight",height,ini.c_str());wchar_t quality[40]{};GetPrivateProfileStringW(L"Upscaling",L"Quality",L"AA",quality,40,ini.c_str());upscaleMode=_wcsicmp(quality,L"QUALITY")==0?sl::DLSSMode::eMaxQuality:_wcsicmp(quality,L"BALANCED")==0?sl::DLSSMode::eBalanced:_wcsicmp(quality,L"PERFORMANCE")==0?sl::DLSSMode::eMaxPerformance:_wcsicmp(quality,L"ULTRA_PERFORMANCE")==0?sl::DLSSMode::eUltraPerformance:sl::DLSSMode::eDLAA;
 sl::DLSSGState caps{};checkFg(sdk.fgState(viewport,caps,nullptr),"FG capabilities");if(generated>caps.numFramesToGenerateMax)throw std::runtime_error("Requested FG multiplier unsupported by driver/GPU");
 rtx::ComPtr<IDXGIFactory2> hooked=factory;rtx::check(sdk.upgrade(reinterpret_cast<void**>(hooked.GetAddressOf())),"upgrade factory");rtx::ComPtr<IDXGISwapChain1> created;rtx::check(hooked->CreateSwapChainForHwnd(proxyQueue.Get(),window,desc,fullscreen,nullptr,&created),"NVIDIA swapchain");rtx::check(created.As(&swap),"swapchain3");enabled=false;setOptions();log("NVIDIA FG swapchain "+std::to_string(width)+"x"+std::to_string(height)+" requested x"+std::to_string(generated+1));
 });}
API xefg_swapchain_result_t Nv_xefgSwapChainD3D12GetSwapChainPtr(xefg_swapchain_handle_t,REFIID iid,void**out){return call([&]{if(!swap)throw std::runtime_error("No NVIDIA swapchain");rtx::check(swap->QueryInterface(iid,out),"swapchain interface");});}
API xefg_swapchain_result_t Nv_xefgSwapChainSetEnabled(xefg_swapchain_handle_t,uint32_t value){return call([&]{enabled=value!=0;setOptions();});}
API xefg_swapchain_result_t Nv_xefgSwapChainSetNumInterpolatedFrames(xefg_swapchain_handle_t,uint32_t value){return call([&]{sl::DLSSGState caps{};checkFg(sdk.fgState(viewport,caps,nullptr),"FG capabilities");if(value<1||value>caps.numFramesToGenerateMax)throw std::runtime_error("Unsupported generated-frame count");generated=value;setOptions();});}
API xefg_swapchain_result_t Nv_xefgSwapChainSetPresentId(xefg_swapchain_handle_t,uint32_t id){return call([&]{begin(id);});}
API void WINAPI NvidiaSetMotionTransform(const float* currentToPrevious,const float* previousToCurrent){std::memcpy(&clipToPrevious,currentToPrevious,64);std::memcpy(&previousToClip,previousToCurrent,64);}
API xefg_swapchain_result_t Nv_xefgSwapChainTagFrameConstants(xefg_swapchain_handle_t,uint32_t id,const xefg_swapchain_frame_constant_data_t*in){return call([&]{
 if(!in||!width||!height)throw std::runtime_error("Invalid frame constants");begin(id);sl::Constants c{};auto projection=load(in->projectionMatrix),view=load(in->viewMatrix);auto inverseView=DirectX::XMMatrixInverse(nullptr,view);DirectX::XMFLOAT4X4 camera;DirectX::XMStoreFloat4x4(&camera,inverseView);
 c.cameraViewToClip=matrix(projection);c.clipToCameraView=matrix(DirectX::XMMatrixInverse(nullptr,projection));c.clipToLensClip=matrix(DirectX::XMMatrixIdentity());c.clipToPrevClip=clipToPrevious;c.prevClipToClip=previousToClip;
 c.jitterOffset={in->jitterOffsetX,in->jitterOffsetY};c.mvecScale={1.0f/(inputWidth?inputWidth:width),1.0f/(inputHeight?inputHeight:height)};c.cameraPinholeOffset={0,0};c.cameraPos={camera._41,camera._42,camera._43};c.cameraRight={camera._11,camera._12,camera._13};c.cameraUp={camera._21,camera._22,camera._23};c.cameraFwd={camera._31,camera._32,camera._33};
 const float a=in->projectionMatrix[10],b=in->projectionMatrix[14];c.cameraNear=std::fabs(b/(1-a));c.cameraFar=std::fabs(b/a);if(!std::isfinite(c.cameraFar)||c.cameraFar<c.cameraNear)c.cameraFar=10000.f;c.cameraFOV=2*std::atan(1/std::fabs(in->projectionMatrix[5]));c.cameraAspectRatio=(float)width/height;c.depthInverted=sl::Boolean::eTrue;c.cameraMotionIncluded=sl::Boolean::eTrue;c.motionVectors3D=sl::Boolean::eFalse;c.motionVectorsDilated=sl::Boolean::eTrue;c.reset=in->resetHistory?sl::Boolean::eTrue:sl::Boolean::eFalse;reset=in->resetHistory!=0;static unsigned trace=0;if(++trace<=3)log("R627: CONSTANTS near="+std::to_string(c.cameraNear)+", far="+std::to_string(c.cameraFar)+", fov="+std::to_string(c.cameraFOV)+", projection5="+std::to_string(in->projectionMatrix[5])+", projection10="+std::to_string(a)+", projection14="+std::to_string(b));c.minRelativeLinearDepthObjectSeparation=4.f;rtx::check(sdk.constants(c,*frame,viewport),"NVIDIA frame constants");if(frameId<12||frameId%120==0)log("R639: FG_DEPTH_SEPARATION frame="+std::to_string(frameId)+", threshold=4, constants_updates=1");
 });}
API xefg_swapchain_result_t Nv_xefgSwapChainD3D12TagFrameResource(xefg_swapchain_handle_t,ID3D12CommandList*,uint32_t id,const xefg_swapchain_d3d12_resource_data_t*in){return call([&]{if(!in||!in->pResource)throw std::runtime_error("Missing frame resource");begin(id);if(in->type==XEFG_SWAPCHAIN_RES_DEPTH)taggedDepth=in->pResource;if(in->type==XEFG_SWAPCHAIN_RES_MOTION_VECTOR)taggedMotion=in->pResource;sl::Resource resource(sl::ResourceType::eTex2d,in->pResource,in->incomingState);sl::Extent extent{in->resourceBase.x,in->resourceBase.y,in->resourceSize.x,in->resourceSize.y};sl::BufferType type=in->type==XEFG_SWAPCHAIN_RES_MOTION_VECTOR?sl::kBufferTypeMotionVectors:in->type==XEFG_SWAPCHAIN_RES_DEPTH?sl::kBufferTypeDepth:sl::kBufferTypeHUDLessColor;sl::ResourceTag tag(&resource,type,sl::ResourceLifecycle::eValidUntilPresent,&extent);rtx::check(sdk.tags(*frame,viewport,&tag,1,nullptr),"NVIDIA resource tag");});}
static xefg_swapchain_result_t readPresentStatus(xefg_swapchain_handle_t,xefg_swapchain_present_status_t*out){return call([&]{if(!out)throw std::runtime_error("Null present status");sl::DLSSGState state{};checkFg(sdk.fgState(viewport,state,nullptr),"NVIDIA actual present state");out->framesPresented=state.numFramesActuallyPresented;out->isFrameGenEnabled=enabled;out->frameGenResult=state.status!=sl::DLSSGStatus::eOk?XEFG_SWAPCHAIN_RESULT_ERROR_DEVICE:(out->framesPresented==(enabled?generated+1:1)?XEFG_SWAPCHAIN_RESULT_SUCCESS:XEFG_SWAPCHAIN_RESULT_WARNING_TOO_FEW_FRAMES);if(frameId<12||frameId%120==0)log("present="+std::to_string(frameId)+" actual="+std::to_string(out->framesPresented)+" requested="+std::to_string(generated+1)+" status="+std::to_string((unsigned)state.status));
 // Presenting queue is already synchronized by DLSS-G's default block-client
 // policy. Explicitly order retirement behind asynchronous input processing too.
 if(enabled&&state.inputsProcessingCompletionFence&&state.lastPresentInputsProcessingCompletionFenceValue)rtx::check(proxyQueue->Wait(static_cast<ID3D12Fence*>(state.inputsProcessingCompletionFence),state.lastPresentInputsProcessingCompletionFenceValue),"NVIDIA input retirement fence");
 });}
API xefg_swapchain_result_t Nv_xefgSwapChainSetLoggingCallback(xefg_swapchain_handle_t,xefg_swapchain_logging_level_t,xefg_swapchain_app_log_callback_t,void*){return XEFG_SWAPCHAIN_RESULT_SUCCESS;}
API xefg_swapchain_result_t Nv_xefgSwapChainSetLatencyReduction(xefg_swapchain_handle_t,void*){return XEFG_SWAPCHAIN_RESULT_SUCCESS;}
API xefg_swapchain_result_t Nv_xefgSwapChainSetUiCompositionState(xefg_swapchain_handle_t,xefg_swapchain_ui_composition_state_t state){return state==XEFG_SWAPCHAIN_UI_COMPOSITION_STATE_DISABLED?XEFG_SWAPCHAIN_RESULT_SUCCESS:XEFG_SWAPCHAIN_RESULT_ERROR_UNSUPPORTED;}
API xefg_swapchain_result_t Nv_xefgSwapChainDestroy(xefg_swapchain_handle_t){return call([&]{enabled=false;if(bound)setOptions();swap.Reset();});}
#include "NvidiaDlaa.h"

namespace {xefg_swapchain_present_status_t cachedStatus{};uint32_t cachedFrame=~0u;xefg_swapchain_result_t cachedResult{};}
API HRESULT WINAPI NvidiaAfterPresent(){try{nrcomparison::poll();}catch(const std::exception&e){log(e.what());}try{motionpaircapture::poll();}catch(const std::exception&e){log(e.what());}cachedResult=readPresentStatus(nullptr,&cachedStatus);cachedFrame=frameId;if(cachedResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&cachedStatus.framesPresented)lastPresentedFrame=frameId;return cachedResult==XEFG_SWAPCHAIN_RESULT_SUCCESS?S_OK:E_FAIL;}
API xefg_swapchain_result_t Nv_xefgSwapChainGetLastPresentStatus(xefg_swapchain_handle_t,xefg_swapchain_present_status_t*out){if(!out)return XEFG_SWAPCHAIN_RESULT_ERROR_INVALID_ARGUMENT;if(cachedFrame!=frameId)NvidiaAfterPresent();*out=cachedStatus;return cachedResult;}

