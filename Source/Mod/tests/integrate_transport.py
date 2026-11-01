from pathlib import Path
p=Path('redesign/B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();s=s.replace('#include "FrameTransportPolicy.h"','#include "FrameTransportPolicy.h"\n#include "DirectSharedTexture.h"\n#include "CompletedFramePolicy.h"')
needle='struct B173iColorSlot\n';assert needle in s
config='''static int redesign_option(const wchar_t* name,int fallback=1){
 wchar_t file[32768]{};GetModuleFileNameW(GetModuleHandleW(nullptr),file,32768);std::wstring path(file);path.resize(path.find_last_of(L"\\\\/")+1);
 return (int)GetPrivateProfileIntW(L"Redesign",name,fallback,(path+L"GTAXeSS.ini").c_str());
}
static bool direct_colour_enabled(){static const bool enabled=redesign_option(L"DirectSharedColor")!=0;return enabled;}
static bool completed_scheduler_enabled(){static const bool enabled=redesign_option(L"LatestCompleted")!=0;return enabled;}
static bool single_queue_enabled(){static const bool enabled=redesign_option(L"SingleQueue")!=0;return enabled;}
'''
s=s.replace(needle,config+needle).replace('struct B173iColorSlot\n{','struct B173iColorSlot\n{\n gta::DirectSharedTexture direct;\n bool directNative{};')
needle='static HRESULT b174c_seed_all_proxy_backbuffers(';i=s.index(needle);s=s[:i]+'''static D3D12_RESOURCE_STATES colour_input_state(ID3D12Resource* resource){
 for(const auto&slot:g_b173iColorSlots)if(slot.directNative&&slot.output.Get()==resource)return D3D12_RESOURCE_STATE_COMMON;
 return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}
'''+s[i:]
s=s.replace('sourceBefore.Transition.StateBefore=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE','sourceBefore.Transition.StateBefore=colour_input_state(source)').replace('before[0].Transition.StateBefore=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE','before[0].Transition.StateBefore=colour_input_state(source)').replace('after[0].Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE','after[0].Transition.StateAfter=colour_input_state(source)')
# Optional single actor/queue: commands remain serial, with a fallback switch.
old='HRESULT hr=g_device12?g_device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&g_b174iPreparationQueue)):E_NOINTERFACE;';new='HRESULT hr=E_NOINTERFACE;if(single_queue_enabled()&&g_queue12){g_b174iPreparationQueue=g_queue12;hr=S_OK;}else if(g_device12)hr=g_device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&g_b174iPreparationQueue));';assert old in s;s=s.replace(old,new)
needle='auto&slot=g_b173iColorSlots[i];ComPtr<IDXGIResource>shared;';assert needle in s;s=s.replace(needle,'auto&slot=g_b173iColorSlots[i];if(slot.directNative){++colorPrepared;continue;}ComPtr<IDXGIResource>shared;')
# Skip worker copy for the resource the native producer already filled.
start=s.index(' ID3D11Resource*colorResources[]{colorSlot.wrapped.Get()}');end=s.index('QueryPerformanceCounter(&afterColor);',start);s=s[:start]+' if(!colorSlot.directNative){'+s[start:end]+'}\n '+s[end:]
# Native/full-size pools have one shared allocation, not two transports plus XeSS.
needle=' slot=B173iColorSlot{};';assert s.count(needle)==1
s=s.replace(needle,needle+'''
 if(direct_colour_enabled()&&source.Width==g_b232OutputWidth&&source.Height==g_b232OutputHeight&&source.Format==DXGI_FORMAT_B8G8R8A8_UNORM&&source.MipLevels==1&&source.ArraySize==1&&source.SampleDesc.Count==1&&source.SampleDesc.Quality==0){
  HRESULT directHr=slot.direct.create(nativeDevice,g_device12.Get(),source.Width,source.Height,source.Format);
  if(SUCCEEDED(directHr)){slot.shared=slot.direct.native;slot.output=slot.direct.gpu;slot.sourceDesc=source;slot.directNative=true;slot.ready=true;return S_OK;}
  slot=B173iColorSlot{}; // Capability failure retains the established transport.
 }
''')
# Native shared colour needs no further On12 copy; all uses return to COMMON.
old='g_queue12&&g_b174iPreparationFence?g_queue12->Wait(g_b174iPreparationFence.Get(),job.on12Marker):E_NOINTERFACE;';new='g_queue12&&g_b174iPreparationFence?(g_queue12.Get()==g_b174iPreparationQueue.Get()?S_OK:g_queue12->Wait(g_b174iPreparationFence.Get(),job.on12Marker)):E_NOINTERFACE;';assert old in s;s=s.replace(old,new)
# Gate legacy source-gap reset when completed-frame scheduling owns queue collapse.
s=s.replace('if(fresh&&b385_load_protected_modules()&&g_b423FreezeCollapseSourceGap','if(!completed_scheduler_enabled()&&fresh&&b385_load_protected_modules()&&g_b423FreezeCollapseSourceGap')
# The recorder F10 used to accidentally trigger expensive render-thread readbacks.
s=s.replace('static void b333_poll_forensic_capture(ID3D11DeviceContext*ctx,ID3D11Texture2D*color,unsigned long long frame,std::ofstream&out){','static void b333_poll_forensic_capture(ID3D11DeviceContext*ctx,ID3D11Texture2D*color,unsigned long long frame,std::ofstream&out){static const bool enabled=[](){wchar_t file[32768]{};GetModuleFileNameW(GetModuleHandleW(nullptr),file,32768);std::wstring p(file);p.resize(p.find_last_of(L"\\\\/")+1);return GetPrivateProfileIntW(L"NativeMotion",L"Capture",0,(p+L"GTAXeSS.ini").c_str())!=0;}();if(!enabled)return;')
# Dump actual half vectors, not just false-colour evidence. Capture only on opt-in F10.
needle='std::vector<unsigned char>vis((size_t)d.Width*d.Height*4);';assert s.count(needle)==1;s=s.replace(needle,'std::ofstream rawVectors(dir+"\\\\motion_"+tag+"_raw.rg16f",std::ios::binary);for(UINT y=0;y<d.Height;++y)rawVectors.write((char*)m.pData+(size_t)y*m.RowPitch,(std::streamsize)d.Width*4);'+needle)
p.write_text(s)
print('Direct shared colour, common-state ownership, single-queue option and raw native-vector capture integrated')
