#include <windows.h>
#include <d3d12.h>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <algorithm>
#include "Dlss5NgxAbi.h"
namespace {
using Init=unsigned(__cdecl*)(unsigned long long,const wchar_t*,ID3D12Device*,unsigned,const void*);
using Populate=unsigned(__cdecl*)(FNGXParameter*);
using Create=unsigned(__cdecl*)(ID3D12GraphicsCommandList*,int,FNGXParameter*,FNGXHandle**);
using Evaluate=unsigned(__cdecl*)(ID3D12GraphicsCommandList*,const FNGXHandle*,const FNGXParameter*,void*);
using GetParams=unsigned(__cdecl*)(FNGXParameter**);
using Id=unsigned long long(__cdecl*)();using Version=unsigned(__cdecl*)();
HMODULE runtime{};FNGXParameter*params{};FNGXHandle*feature{};ID3D12Device*owner{};
Create create{};Evaluate evaluate{};unsigned width{},height{},depthWidth{},depthHeight{};bool failed{};unsigned long long frames{};std::mutex gate;
std::filesystem::path folder(){HMODULE self{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&folder),&self);wchar_t path[32768]{};GetModuleFileNameW(self,path,32768);return std::filesystem::path(path).parent_path();}
void log(const char*stage,unsigned result){std::ofstream(folder()/L"GTAXeSS-DLSS5.log",std::ios::app)<<stage<<" result=0x"<<std::hex<<result<<" frames="<<std::dec<<frames<<"\n";}
bool ok(unsigned code){return code==1;}
template<class T>T api(HMODULE dll,const char*name){return reinterpret_cast<T>(GetProcAddress(dll,name));}
bool initialize(ID3D12Device*device){
 if(owner)return owner==device;
 auto core=GetModuleHandleW(L"_nvngx.dll");if(!core)return false;
 auto get=api<GetParams>(core,"NVSDK_NGX_D3D12_GetCapabilityParameters");if(!get||!ok(get(&params))||!params)return false;
 auto ini=(folder()/L"GTAXeSS.ini").wstring();
 const bool rtx40=GetPrivateProfileIntW(L"NVIDIA",L"DLSS5RTX40Experimental",0,ini.c_str())!=0;
 const auto runtimeName=rtx40?L"nvngx_dlssnr_rtx40.dll":L"nvngx_dlssnr.dll";
 runtime=LoadLibraryExW((folder()/runtimeName).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
 std::ofstream(folder()/L"GTAXeSS-DLSS5.log",std::ios::app)<<"RUNTIME file="<<std::filesystem::path(runtimeName).string()<<" experimental_rtx40="<<rtx40<<" loaded="<<(runtime?1:0)<<"\n";
 if(!runtime)return false;
 auto init=api<Init>(runtime,"NVSDK_NGX_D3D12_Init_Ext");auto populate=api<Populate>(runtime,"NVSDK_NGX_D3D12_PopulateParameters_Impl");auto id=api<Id>(runtime,"NVSDK_NGX_GetApplicationId");auto version=api<Version>(runtime,"NVSDK_NGX_GetAPIVersion");create=api<Create>(runtime,"NVSDK_NGX_D3D12_CreateFeature");evaluate=api<Evaluate>(runtime,"NVSDK_NGX_D3D12_EvaluateFeature");if(!init||!populate||!id||!version||!create||!evaluate)return false;
 auto data=folder()/L"logs";CreateDirectoryW(data.c_str(),nullptr);auto result=init(id(),data.c_str(),device,version(),params);log("INIT",result);if(!ok(result))return false;result=populate(params);log("PARAMETERS",result);if(!ok(result))return false;owner=device;owner->AddRef();return true;
}
void rect(const char*prefix,unsigned w,unsigned h){std::string p(prefix);params->Set((p+"SubrectBaseX").c_str(),0u);params->Set((p+"SubrectBaseY").c_str(),0u);params->Set((p+"SubrectWidth").c_str(),w);params->Set((p+"SubrectHeight").c_str(),h);}
struct Controls{int intensity=100,tone=100,structure=100,skin=100,ui=1;bool operator==(const Controls&)const=default;};
Controls controls(){static Controls current;static ULONGLONG next{};auto now=GetTickCount64();if(now>=next){next=now+250;auto ini=(folder()/L"GTAXeSS.ini").wstring();auto read=[&](const wchar_t*key,int fallback){return std::clamp((int)GetPrivateProfileIntW(L"NVIDIA",key,fallback,ini.c_str()),0,100);};current={read(L"NRIntensity",100),read(L"NRTone",100),read(L"NRStructure",100),read(L"NRSkin",100),read(L"NRUICorrection",1)};}return current;}
}
// Resources/features remain owned until process exit: disabling/reinitializing the
// mod cannot free histories while NR/FG GPU work still references them.
extern "C" __declspec(dllexport) HRESULT WINAPI Dlss5Evaluate(ID3D12Device*device,ID3D12GraphicsCommandList*list,ID3D12Resource*colour,ID3D12Resource*output,ID3D12Resource*depth,ID3D12Resource*motion,BOOL reset){
 std::lock_guard<std::mutex>lock(gate);if(failed)return E_FAIL;
 if(!device||!list||!colour||!output||!depth||!motion)return E_INVALIDARG;
 auto c=colour->GetDesc(),o=output->GetDesc(),d=depth->GetDesc(),m=motion->GetDesc();
 if(c.Width!=o.Width||c.Height!=o.Height||d.Width!=m.Width||d.Height!=m.Height)return E_INVALIDARG;
 if(!initialize(device)){failed=true;log("INITIALIZATION_FAILED",GetLastError());return E_FAIL;}
 if(feature&&(width!=o.Width||height!=o.Height||depthWidth!=d.Width||depthHeight!=d.Height)){failed=true;log("CONTRACT_CHANGED_RESTART_REQUIRED",0);return E_FAIL;}
 // R643: complete the Feature-18 creation/evaluation contract used by the
 // working RenoDX community path. The earlier wrapper only populated private
 // DLSSNR keys, which can create/evaluate successfully without giving NGX the
 // standard render/output geometry or guide-layout flags.
 const unsigned inputWidth=(unsigned)c.Width,inputHeight=c.Height;
 params->Set("Width",inputWidth);params->Set("Height",inputHeight);
 params->Set("OutWidth",(unsigned)o.Width);params->Set("OutHeight",o.Height);
 params->Set("PerfQualityValue",0);
 // Public NGX flags: MVLowRes (bit 1) and DepthInverted (bit 3). GTA's
 // reconstructed guides are render-resolution and reverse-Z.
 params->Set("DLSS.Feature.Create.Flags",10);
 for(auto key:{"DLSSNR.Width","DLSSNR.InputWidth","DLSSNR.OutputWidth","DLSSNR.Output.Width"})params->Set(key,(unsigned)o.Width);
 for(auto key:{"DLSSNR.Height","DLSSNR.InputHeight","DLSSNR.OutputHeight","DLSSNR.Output.Height"})params->Set(key,o.Height);
 params->Set("DLSSNR.Scale",1.f);params->Set("DLSSNR.ScalingRatio",1.f);params->Set("DLSSNR.Hint.Render.Preset",1);params->Set("CreationNodeMask",1u);params->Set("VisibilityNodeMask",1u);
 params->Set("DLSSNR.Color",colour);params->Set("DLSSNR.Output",output);params->Set("DLSSNR.Depth",depth);params->Set("DLSSNR.MVec",motion);rect("DLSSNR.Color",inputWidth,inputHeight);rect("DLSSNR.Output",(unsigned)o.Width,o.Height);rect("DLSSNR.Depth",(unsigned)d.Width,d.Height);rect("DLSSNR.MVec",(unsigned)m.Width,m.Height);
 // Existing backend motion is in input-render pixels; NR color is output-sized.
 params->Set("DLSSNR.MVecScaleX",float(o.Width)/float(m.Width));params->Set("DLSSNR.MVecScaleY",float(o.Height)/float(m.Height));params->Set("DLSSNR.DepthInverted",1);params->Set("DLSSNR.Enabled",1);params->Set("DLSSNR.Reset",reset||!feature?1:0);params->Set("Jitter.Offset.X",0.f);params->Set("Jitter.Offset.Y",0.f);
 auto strength=controls();static Controls previous;static bool first=true;if(first||!(strength==previous)){params->Set("DLSSNR.Reset",1);std::ofstream(folder()/L"GTAXeSS-DLSS5.log",std::ios::app)<<"CONTROLS intensity="<<strength.intensity<<" tone="<<strength.tone<<" structure="<<strength.structure<<" skin="<<strength.skin<<" ui="<<strength.ui<<"\n";previous=strength;first=false;}
 params->Set("DLSSNR.Intensity",strength.intensity/100.f);params->Set("DLSSNR.LocalToneStrength",strength.tone/100.f);params->Set("DLSSNR.LocalStructureStrength",strength.structure/100.f);params->Set("DLSSNR.SkinStructureStrength",strength.skin/100.f);params->Set("DLSSNR.UseAutoMask",1);params->Set("DLSSNR.Style",0);params->Set("DLSSNR.UICorrection",strength.ui?1:0);
 if(!feature){std::ofstream(folder()/L"GTAXeSS-DLSS5.log",std::ios::app)<<"R643 CONTRACT feature=18 color="<<inputWidth<<"x"<<inputHeight<<" output="<<o.Width<<"x"<<o.Height<<" guides="<<d.Width<<"x"<<d.Height<<" create_flags=10 perf_quality=0\n";auto result=create(list,18,params,&feature);log("CREATE",result);if(!ok(result)||!feature){failed=true;return E_FAIL;}width=(unsigned)o.Width;height=o.Height;depthWidth=(unsigned)d.Width;depthHeight=d.Height;}
 auto result=evaluate(list,feature,params,nullptr);if(!ok(result)){failed=true;log("EVALUATE_FAILED",result);return E_FAIL;}++frames;if(frames<=3||frames%120==0)log("EVALUATED",result);return S_OK;
}
extern "C" __declspec(dllexport) unsigned long long WINAPI Dlss5GetFrames(){std::lock_guard<std::mutex>lock(gate);return frames;}
