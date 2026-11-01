#include "../ResolutionPolicy.h"
#include "../DepthFormatPolicy.h"
extern "C" HRESULT WINAPI B175SetNativeDepthFormat(DXGI_FORMAT);
extern "C" int __stdcall GhostingVerifyRuntime();
#include "../ProtectedFixes/CameraRuntime.h"
#include "WriteDiscardMirror.h"
#include "../ProtectedFixes/TemporalRuntime.h"
#include <windows.h>
#include <d3d11.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <algorithm>
#include <wrl/client.h>

#define GTAXESS_CORE_RENDER_FG_LOCKED 1
static_assert(GTAXESS_CORE_RENDER_FG_LOCKED == 1, "B339 core render/FG lock must remain enabled");
extern "C" BOOL WINAPI B172FreshFrameOwned();
extern "C" BOOL WINAPI B172WantsCapture(unsigned);
extern "C" BOOL WINAPI B172BeginFreshFrame(unsigned);
extern "C" void WINAPI B172AbortFreshFrame(unsigned,const char*,HRESULT);
extern "C" void WINAPI B175DropNoConstantSnapshot(unsigned);
extern "C" void WINAPI B175DropFastCapture(unsigned,const char*);
extern "C" HRESULT WINAPI B171PresentLiveFrame(ID3D11DeviceContext*,IDXGISwapChain*,ID3D11Texture2D*,const char*);
extern "C" BOOL WINAPI B396AllowFgSubmission();
extern "C" HRESULT WINAPI B175PrepareDiscoveredInternalColor(ID3D11Texture2D*,const char*);
extern "C" HRESULT WINAPI B164SubmitFrameInputs(const void*,UINT,const void*,UINT,const void*,UINT,const float*,const float*,const float*,const float*,UINT,UINT,const char*);
extern "C" HRESULT WINAPI B173SubmitGpuFrame(ID3D11DeviceContext*,ID3D11Texture2D*,const float*,const float*,const float*,const float*,UINT,UINT,const char*);





extern "C" HRESULT WINAPI B76LogSelectedDraws(unsigned firstSequence,unsigned secondSequence,const char* logPath);

extern "C" __declspec(dllexport) const char* WINAPI B63bGetBuildIdentity()
{
    if(!GhostingVerifyRuntime())return nullptr;
    return "B423 LOCKED FREEZING DLL EXTRACTION + B422 ACCEPTED FREEZE BEHAVIOR 2026-09-29";
}

extern "C" __declspec(dllexport) BOOL WINAPI B336CoreRenderFgLocked() { return GTAXESS_CORE_RENDER_FG_LOCKED ? TRUE : FALSE; }

extern "C" __declspec(dllexport) HRESULT WINAPI B27ReadbackColorDepthPair(
    ID3D11Device* device, ID3D11DeviceContext* context,
    ID3D11Texture2D* sourceColor, ID3D11Texture2D* sourceDepth,
    UINT* outColorSourceFormat, UINT* outDepthSourceFormat,
    UINT* outColorRowPitch, UINT* outDepthRowPitch)
{
    if (!device || !context || !sourceColor || !sourceDepth) return E_INVALIDARG;
    D3D11_TEXTURE2D_DESC c{}, d{}; sourceColor->GetDesc(&c); sourceDepth->GetDesc(&d);
    if (outColorSourceFormat) *outColorSourceFormat = (UINT)c.Format;
    if (outDepthSourceFormat) *outDepthSourceFormat = (UINT)d.Format;
    if (outColorRowPitch) *outColorRowPitch = 0;
    if (outDepthRowPitch) *outDepthRowPitch = 0;
    if (c.Format != DXGI_FORMAT_R16G16B16A16_FLOAT || d.Format != DXGI_FORMAT_R24G8_TYPELESS) return E_INVALIDARG;
    D3D11_TEXTURE2D_DESC cs=c, ds=d;
    cs.Usage=ds.Usage=D3D11_USAGE_STAGING; cs.BindFlags=ds.BindFlags=0;
    cs.CPUAccessFlags=ds.CPUAccessFlags=D3D11_CPU_ACCESS_READ; cs.MiscFlags=ds.MiscFlags=0;
    ComPtr<ID3D11Texture2D> ct,dt;
    HRESULT hr=device->CreateTexture2D(&cs,nullptr,&ct);
    if (SUCCEEDED(hr)) hr=device->CreateTexture2D(&ds,nullptr,&dt);
    if (FAILED(hr)||!ct||!dt) return FAILED(hr)?hr:E_FAIL;
    context->CopyResource(ct.Get(),sourceColor); context->CopyResource(dt.Get(),sourceDepth);
    D3D11_MAPPED_SUBRESOURCE m{};
    hr=context->Map(ct.Get(),0,D3D11_MAP_READ,0,&m);
    if(SUCCEEDED(hr)){if(outColorRowPitch)*outColorRowPitch=m.RowPitch;context->Unmap(ct.Get(),0);}
    if(SUCCEEDED(hr)){hr=context->Map(dt.Get(),0,D3D11_MAP_READ,0,&m);if(SUCCEEDED(hr)){if(outDepthRowPitch)*outDepthRowPitch=m.RowPitch;context->Unmap(dt.Get(),0);}}
    return hr;
}
static CbEvent g_cb_events[kCbEvents];
static MapEvent g_map_events[kMapEvents];
static Snapshot g_snapshots[kSnapshots];
static std::atomic<unsigned>* g_seq{}; static std::atomic<unsigned>* g_active{};
static SRWLOCK g_b175FastLock=SRWLOCK_INIT;
static ComPtr<ID3D11Texture2D> g_b175FastDepth;
static ComPtr<ID3D11Texture2D> g_b175FastColorPair[2];
static ComPtr<ID3D11Texture2D> g_b175FastCurrentColor;
static ComPtr<ID3D11Texture2D> g_b175FastBoundRtvColor;
static ComPtr<ID3D11Texture2D> g_b285FastBoundDepth;
static SRWLOCK g_b330NativeMotionLock=SRWLOCK_INIT;
static ComPtr<ID3D11Texture2D> g_b330NativeMotionSource;
static UINT g_b330NativeMotionWidth=0,g_b330NativeMotionHeight=0;
static unsigned g_b175FastDrawType{},g_b175FastDrawA{},g_b175FastDrawB{};
static int g_b175FastDrawC{};
static unsigned g_b175FastConstantBytes{};
static Snapshot g_b175FastSnapshot;
static std::atomic<bool> g_b175FastReady{false},g_b175FastHookActive{false},g_b175nPreCoreProbeReady{false};
static std::atomic<unsigned> g_b175FastHookInstallCount{0},g_b175FastCaptureCount{0};
static std::atomic<unsigned long long> g_b175FastSnapshotSerial{0};
static std::atomic<unsigned long long> g_b175FastColorSerial{0};
static std::atomic<unsigned long long> g_b175FastLastUpdateTick{0};
// B284: capture which 256-byte constant buffer is actually bound to the vertex shader at the exact validated GTA draw.
static SRWLOCK g_b284VsDrawLock=SRWLOCK_INIT;
static ComPtr<ID3D11Buffer> g_b284VsDrawCb;
static ComPtr<ID3D11VertexShader> g_b284VsDrawShader;
static unsigned g_b284VsDrawSlot=~0u;
static std::atomic<unsigned long long> g_b284VsDrawSerial{0};
static std::atomic<unsigned long long> g_b285GeometryDraws{0},g_b285RageDraws{0},g_b285RejectedScreenDraws{0};

extern "C" __declspec(dllexport) BOOL WINAPI B306DirectFastPathHealthy(){return g_b300DirectMode.load(std::memory_order_acquire)&&g_b301DirectCbResource.load(std::memory_order_acquire)!=nullptr&&g_b175FastHookActive.load(std::memory_order_acquire)?TRUE:FALSE;}

static unsigned long long g_b175FastLastCaptureSerial{};
static unsigned long long g_b175FastLastColorSerial{};
static unsigned g_b175FastStalePresents{};
static ULONGLONG g_b175FastHookStart{};
static thread_local PendingMap t_pending{};
static thread_local WriteDiscardMirror t_cameraMirror{};
static thread_local bool t_b539GameplayPass=false;
static ps_cb_fn o_vs_cb{},o_ps_cb{};static om_rt_fn o_fast_om_rt{};static draw_fn o_fast_draw{};static draw_indexed_fn o_fast_draw_indexed{};static draw_indexed_instanced_fn o_b321_draw_indexed_instanced{};static draw_instanced_fn o_b321_draw_instanced{};static dispatch_fn o_b321_dispatch{};static map_fn o_map{};static unmap_fn o_unmap{};
static std::atomic<unsigned long long> g_b321DrawCalls{0},g_b321DrawIndexedCalls{0},g_b321DrawIndexedInstancedCalls{0},g_b321DrawInstancedCalls{0},g_b321DispatchCalls{0};
static std::atomic<unsigned long long> g_b321DrawSampleOrdinal{0},g_b321DispatchSampleOrdinal{0},g_b321SampledDrawCalls{0},g_b321SampledDispatchCalls{0};
static std::atomic<long long> g_b321SampledDrawTicks{0},g_b321SampledDispatchTicks{0};
static constexpr unsigned long long kB321SampleStride=64ull;
// B322 diagnostic-only D3D11 state/resource CPU profiler. It wraps the already-active
// immediate-context methods after the B300 camera lock, preserving every existing target.
enum B322Group : unsigned { B322_CB=0, B322_SRV=1, B322_SHADER=2, B322_IA=3, B322_OMRS=4, B322_RESOURCE=5, B322_MAPUNMAP=6, B322_GROUP_COUNT=7 };
static std::atomic<unsigned long long> g_b322Calls[B322_GROUP_COUNT]{},g_b322SampleOrd[B322_GROUP_COUNT]{},g_b322Samples[B322_GROUP_COUNT]{};
static std::atomic<long long> g_b322Ticks[B322_GROUP_COUNT]{};
static constexpr unsigned long long kB322SampleStride=64ull;
// B323 diagnostic-only Map/Unmap ownership split. Count every direct-mode Map/Unmap,
// sample 1/64 for timing, and separate selected camera CB from unrelated GTA resources.
enum B323Cat : unsigned { B323_SEL_MAP=0, B323_SEL_UNMAP=1, B323_OTHER_MAP=2, B323_OTHER_UNMAP=3, B323_CAT_COUNT=4 };
static std::atomic<bool> g_b323ProfilerActive{false};
static std::atomic<unsigned long long> g_b323Calls[B323_CAT_COUNT]{},g_b323SampleOrd[B323_CAT_COUNT]{},g_b323Samples[B323_CAT_COUNT]{};
static std::atomic<long long> g_b323TotalTicks[B323_CAT_COUNT]{},g_b323OriginalTicks[B323_CAT_COUNT]{};
static constexpr unsigned long long kB323SampleStride=64ull;
static B323ResourceSample g_b323ResourceSamples[64]{}; static SRWLOCK g_b323ResourceLock=SRWLOCK_INIT;
static bool b323_should_sample(unsigned c){return ((g_b323SampleOrd[c].fetch_add(1,std::memory_order_relaxed)+1ull)%kB323SampleStride)==0ull;}
static void b323_record_sample(unsigned cat,ID3D11Resource*r,long long total,long long original){
 g_b323Samples[cat].fetch_add(1,std::memory_order_relaxed);g_b323TotalTicks[cat].fetch_add(total,std::memory_order_relaxed);g_b323OriginalTicks[cat].fetch_add(original,std::memory_order_relaxed);
 if(cat==B323_OTHER_MAP||cat==B323_OTHER_UNMAP){AcquireSRWLockExclusive(&g_b323ResourceLock);unsigned empty=64;for(unsigned i=0;i<64;++i){if(g_b323ResourceSamples[i].resource==r){g_b323ResourceSamples[i].samples++;g_b323ResourceSamples[i].totalTicks+=total;g_b323ResourceSamples[i].originalTicks+=original;ReleaseSRWLockExclusive(&g_b323ResourceLock);return;}if(!g_b323ResourceSamples[i].resource&&empty==64)empty=i;}if(empty<64){g_b323ResourceSamples[empty].resource=r;g_b323ResourceSamples[empty].samples=1;g_b323ResourceSamples[empty].totalTicks=total;g_b323ResourceSamples[empty].originalTicks=original;}ReleaseSRWLockExclusive(&g_b323ResourceLock);}
}
static ps_cb_fn o_b322_vs_cb{},o_b322_ps_cb{};static srv_fn o_b322_ps_srv{},o_b322_vs_srv{};static vs_shader_fn o_b322_vs_shader{};static ps_shader_fn o_b322_ps_shader{};
static ia_vb_fn o_b322_ia_vb{};static ia_ib_fn o_b322_ia_ib{};static ia_layout_fn o_b322_ia_layout{};static ia_topology_fn o_b322_ia_topology{};static om_rt_fn o_b322_om_rt{};static rs_viewports_fn o_b322_rs_viewports{};
static copy_sub_fn o_b322_copy_sub{};static copy_resource_fn o_b322_copy_resource{};static update_sub_fn o_b322_update_sub{};static map_fn o_b322_map{};static unmap_fn o_b322_unmap{};
// B308 diagnostic split: distinguish wrapper overhead from the original D3D11 Map/Unmap
// calls, and distinguish the selected camera CB from unrelated GTA resources.
static std::atomic<long long> g_b307MapUnmapQpcTicks{0},g_b307OmQpcTicks{0};
static std::atomic<unsigned long long> g_b307MapUnmapCalls{0},g_b307OmCalls{0};
static std::atomic<long long> g_b308MapTotalTicks{0},g_b308MapOriginalTicks{0},g_b308UnmapTotalTicks{0},g_b308UnmapOriginalTicks{0};
static std::atomic<long long> g_b308SelectedTotalTicks{0},g_b308SelectedOriginalTicks{0},g_b308UnrelatedTotalTicks{0},g_b308UnrelatedOriginalTicks{0};
static std::atomic<unsigned long long> g_b308MapCalls{0},g_b308UnmapCalls{0},g_b308SelectedCalls{0},g_b308UnrelatedCalls{0};
static std::atomic<unsigned> g_b308AbState{0};
static ID3D11DeviceContext* g_b308HookContext{};
static long long b307_qpc_now(){LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}
struct B307HookCostScope{std::atomic<long long>*ticks{};std::atomic<unsigned long long>*calls{};long long begin{};B307HookCostScope(std::atomic<long long>&t,std::atomic<unsigned long long>&c):ticks(&t),calls(&c),begin(b307_qpc_now()){}~B307HookCostScope(){const long long end=b307_qpc_now();ticks->fetch_add(end-begin,std::memory_order_relaxed);calls->fetch_add(1,std::memory_order_relaxed);}};
static void b308_record_map_timing(long long total,long long original,bool selected){g_b307MapUnmapQpcTicks.fetch_add(total,std::memory_order_relaxed);g_b307MapUnmapCalls.fetch_add(1,std::memory_order_relaxed);g_b308MapTotalTicks.fetch_add(total,std::memory_order_relaxed);g_b308MapOriginalTicks.fetch_add(original,std::memory_order_relaxed);g_b308MapCalls.fetch_add(1,std::memory_order_relaxed);if(selected){g_b308SelectedTotalTicks.fetch_add(total,std::memory_order_relaxed);g_b308SelectedOriginalTicks.fetch_add(original,std::memory_order_relaxed);g_b308SelectedCalls.fetch_add(1,std::memory_order_relaxed);}else{g_b308UnrelatedTotalTicks.fetch_add(total,std::memory_order_relaxed);g_b308UnrelatedOriginalTicks.fetch_add(original,std::memory_order_relaxed);g_b308UnrelatedCalls.fetch_add(1,std::memory_order_relaxed);}}
static void b308_record_unmap_timing(long long total,long long original,bool selected){g_b307MapUnmapQpcTicks.fetch_add(total,std::memory_order_relaxed);g_b307MapUnmapCalls.fetch_add(1,std::memory_order_relaxed);g_b308UnmapTotalTicks.fetch_add(total,std::memory_order_relaxed);g_b308UnmapOriginalTicks.fetch_add(original,std::memory_order_relaxed);g_b308UnmapCalls.fetch_add(1,std::memory_order_relaxed);if(selected){g_b308SelectedTotalTicks.fetch_add(total,std::memory_order_relaxed);g_b308SelectedOriginalTicks.fetch_add(original,std::memory_order_relaxed);g_b308SelectedCalls.fetch_add(1,std::memory_order_relaxed);}else{g_b308UnrelatedTotalTicks.fetch_add(total,std::memory_order_relaxed);g_b308UnrelatedOriginalTicks.fetch_add(original,std::memory_order_relaxed);g_b308UnrelatedCalls.fetch_add(1,std::memory_order_relaxed);}}
extern "C" __declspec(dllexport) BOOL WINAPI B307ConsumeHookTiming(double*mapUnmapMs,double*omMs,unsigned long long*mapUnmapCalls,unsigned long long*omCalls){LARGE_INTEGER f{};QueryPerformanceFrequency(&f);if(!f.QuadPart)return FALSE;const long long mt=g_b307MapUnmapQpcTicks.exchange(0,std::memory_order_acq_rel),ot=g_b307OmQpcTicks.exchange(0,std::memory_order_acq_rel);const unsigned long long mc=g_b307MapUnmapCalls.exchange(0,std::memory_order_acq_rel),oc=g_b307OmCalls.exchange(0,std::memory_order_acq_rel);if(mapUnmapMs)*mapUnmapMs=(double)mt*1000.0/(double)f.QuadPart;if(omMs)*omMs=(double)ot*1000.0/(double)f.QuadPart;if(mapUnmapCalls)*mapUnmapCalls=mc;if(omCalls)*omCalls=oc;return TRUE;}
extern "C" __declspec(dllexport) BOOL WINAPI B308ConsumeMapUnmapSplit(double*mapTotalMs,double*mapOriginalMs,double*unmapTotalMs,double*unmapOriginalMs,double*selectedTotalMs,double*selectedOriginalMs,double*unrelatedTotalMs,double*unrelatedOriginalMs,unsigned long long*mapCalls,unsigned long long*unmapCalls,unsigned long long*selectedCalls,unsigned long long*unrelatedCalls,unsigned*abState){LARGE_INTEGER f{};QueryPerformanceFrequency(&f);if(!f.QuadPart)return FALSE;auto cv=[&](std::atomic<long long>&v){return(double)v.exchange(0,std::memory_order_acq_rel)*1000.0/(double)f.QuadPart;};if(mapTotalMs)*mapTotalMs=cv(g_b308MapTotalTicks);if(mapOriginalMs)*mapOriginalMs=cv(g_b308MapOriginalTicks);if(unmapTotalMs)*unmapTotalMs=cv(g_b308UnmapTotalTicks);if(unmapOriginalMs)*unmapOriginalMs=cv(g_b308UnmapOriginalTicks);if(selectedTotalMs)*selectedTotalMs=cv(g_b308SelectedTotalTicks);if(selectedOriginalMs)*selectedOriginalMs=cv(g_b308SelectedOriginalTicks);if(unrelatedTotalMs)*unrelatedTotalMs=cv(g_b308UnrelatedTotalTicks);if(unrelatedOriginalMs)*unrelatedOriginalMs=cv(g_b308UnrelatedOriginalTicks);if(mapCalls)*mapCalls=g_b308MapCalls.exchange(0,std::memory_order_acq_rel);if(unmapCalls)*unmapCalls=g_b308UnmapCalls.exchange(0,std::memory_order_acq_rel);if(selectedCalls)*selectedCalls=g_b308SelectedCalls.exchange(0,std::memory_order_acq_rel);if(unrelatedCalls)*unrelatedCalls=g_b308UnrelatedCalls.exchange(0,std::memory_order_acq_rel);if(abState)*abState=g_b308AbState.load(std::memory_order_acquire);return TRUE;}
static void release_all();

static unsigned next_sequence(){return g_seq?g_seq->fetch_add(1):0;}
static void remember_cb(ID3D11Buffer* b){if(!b)return;unsigned n=g_known_cb_count.load();if(n>kKnownCbs)n=kKnownCbs;for(unsigned i=0;i<n;++i)if(g_known_cbs[i].load()==b)return;D3D11_BUFFER_DESC d{};b->GetDesc(&d);unsigned i=g_known_cb_count.fetch_add(1);if(i<kKnownCbs){g_known_cb_bytes[i].store(d.ByteWidth);g_known_cbs[i].store(b);}}
static unsigned known_cb_bytes(ID3D11Resource* r){unsigned n=g_known_cb_count.load();if(n>kKnownCbs)n=kKnownCbs;for(unsigned i=0;i<n;++i)if((ID3D11Resource*)g_known_cbs[i].load()==r)return g_known_cb_bytes[i].load();return 0;}
static void record_cb_event(UINT stage,UINT start,UINT count,ID3D11Buffer* const* buffers){unsigned i=g_cb_next.fetch_add(1),seq=next_sequence();if(i<kCbEvents){auto&e=g_cb_events[i];e.ordinal=seq;e.stage=stage;e.start=start;e.count=count;for(auto&b:e.buffers)b=nullptr;unsigned n=count>kPsCbs?kPsCbs:count;for(unsigned j=0;j<n;++j){auto*b=buffers?buffers[j]:nullptr;if(b)b->AddRef();e.buffers[j]=b;remember_cb(b);if(b){const int bi=b280_known_cb_index(b);if(bi>=0){const unsigned bw=g_known_cb_bytes[bi].load(std::memory_order_acquire);if(bw==256){g_b282LastBindStage[bi].store(stage,std::memory_order_relaxed);g_b282LastBindSlot[bi].store(start+j,std::memory_order_relaxed);}}}}e.ready.store(1);}else{g_cb_overflow.fetch_add(1);if(buffers){unsigned n=count>kPsCbs?kPsCbs:count;for(unsigned j=0;j<n;++j)remember_cb(buffers[j]);}}}
static unsigned begin_map_event(unsigned type,ID3D11Resource*r,UINT sub,D3D11_MAP mt,UINT flags){unsigned i=g_map_next.fetch_add(1),seq=next_sequence();if(i<kMapEvents){if(r)r->AddRef();auto&e=g_map_events[i];e.ordinal=seq;e.type=type;e.subresource=sub;e.map_type=(unsigned)mt;e.map_flags=flags;e.hr=E_PENDING;e.resource=r;return i;}g_map_overflow.fetch_add(1);return~0u;}
static void finish_map_event(unsigned i,HRESULT hr){if(i<kMapEvents){g_map_events[i].hr=hr;g_map_events[i].ready.store(1);}}
static void record_snapshot(ID3D11Resource*r,const void*p,unsigned bytes){unsigned i=g_snapshot_next.fetch_add(1),seq=next_sequence();if(i<kSnapshots&&p&&bytes){auto&e=g_snapshots[i];e.sequence=seq;e.resource=r;e.byte_count=bytes>kSnapshotBytes?kSnapshotBytes:bytes;std::memcpy(e.bytes,p,e.byte_count);e.ready.store(1);}else if(i>=kSnapshots)g_snapshot_overflow.fetch_add(1);}
static void STDMETHODCALLTYPE h_vs_cb(ID3D11DeviceContext*c,UINT start,UINT count,ID3D11Buffer* const*buffers){if(g_active)g_active->fetch_add(1);record_cb_event(0,start,count,buffers);o_vs_cb(c,start,count,buffers);if(g_active)g_active->fetch_sub(1);}
static void STDMETHODCALLTYPE h_ps_cb(ID3D11DeviceContext*c,UINT start,UINT count,ID3D11Buffer* const*buffers){if(g_active)g_active->fetch_add(1);record_cb_event(1,start,count,buffers);o_ps_cb(c,start,count,buffers);if(g_active)g_active->fetch_sub(1);}
static HRESULT STDMETHODCALLTYPE h_map(ID3D11DeviceContext*c,ID3D11Resource*r,UINT sub,D3D11_MAP mt,UINT flags,D3D11_MAPPED_SUBRESOURCE*m){
 // B301 selected-resource fast bypass: after direct-camera lock, every unrelated Map goes
 // straight to D3D11 with no CB lookup, event recording, or snapshotting.
 if(g_b300DirectMode.load(std::memory_order_acquire)){
  ID3D11Resource*sel=g_b301DirectCbResource.load(std::memory_order_acquire);const bool selected=(r==sel);const unsigned cat=selected?B323_SEL_MAP:B323_OTHER_MAP;
  const bool b323Active=g_b323ProfilerActive.load(std::memory_order_relaxed);if(b323Active)g_b323Calls[cat].fetch_add(1,std::memory_order_relaxed);const bool timed=b323Active&&b323_should_sample(cat);long long tb=0,ob=0,oe=0;if(timed)tb=b307_qpc_now();
  if(!selected){const int b484Slot=-1; // B538: alternate CB watch is diagnostic only; retire GPU mapped-memory reads.
if(timed)ob=b307_qpc_now();HRESULT hr=o_map(c,r,sub,mt,flags,m);if(timed)oe=b307_qpc_now();if(b484Slot>=0){t_pending={};if(SUCCEEDED(hr)&&m&&m->pData&&mt==D3D11_MAP_WRITE_DISCARD){t_pending.resource=r;t_pending.data=m->pData;t_pending.byte_count=256;t_pending.target=true;t_pending.b484WatchSlot=b484Slot;}}if(timed)b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);return hr;}
  g_b456SelectedMaps.fetch_add(1,std::memory_order_relaxed);if(g_active)g_active->fetch_add(1);if(timed)ob=b307_qpc_now();HRESULT hr=o_map(c,r,sub,mt,flags,m);if(timed)oe=b307_qpc_now();t_pending={};
  if(SUCCEEDED(hr)&&m&&m->pData&&mt==D3D11_MAP_WRITE_DISCARD&&sub==0){
   void* mirror=t_cameraMirror.begin(m->pData,r,c);if(mirror)m->pData=mirror;
   t_pending.resource=r;t_pending.data=m->pData;t_pending.byte_count=256;t_pending.target=true;
  }
  if(g_active)g_active->fetch_sub(1);if(timed)b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);return hr;
 }
 const long long totalBegin=b307_qpc_now();ID3D11Resource*selectedResource=g_b301DirectCbResource.load(std::memory_order_acquire);const bool selected=r&&r==selectedResource;
 if(g_active)g_active->fetch_add(1);unsigned i=~0u;if(!g_b175FastHookActive.load(std::memory_order_acquire))i=begin_map_event(1,r,sub,mt,flags);const long long ob=b307_qpc_now();HRESULT hr=o_map(c,r,sub,mt,flags,m);const long long oe=b307_qpc_now();if(i!=~0u)finish_map_event(i,hr);t_pending={};if(SUCCEEDED(hr)&&m&&m->pData&&mt==D3D11_MAP_WRITE_DISCARD){unsigned bytes=g_b175FastHookActive.load(std::memory_order_acquire)&&r==(ID3D11Resource*)g_b175FastConstants.Get()?g_b175FastConstantBytes:known_cb_bytes(r);if(bytes){if(bytes==256&&sub==0){void* mirror=t_cameraMirror.begin(m->pData,r,c);if(mirror)m->pData=mirror;}t_pending.resource=r;t_pending.data=m->pData;t_pending.byte_count=bytes>kSnapshotBytes?kSnapshotBytes:bytes;t_pending.target=true;}}if(g_active)g_active->fetch_sub(1);const long long te=b307_qpc_now();b308_record_map_timing(te-totalBegin,oe-ob,selected);return hr;
}
static void STDMETHODCALLTYPE h_unmap(ID3D11DeviceContext*c,ID3D11Resource*r,UINT sub){
 // Flush even if direct-camera mode changed between Map and Unmap.
 t_cameraMirror.commit(r,c,sub);
 // B301 mirrors the Map fast bypass. Only the one selected camera resource enters the direct ring.
 if(g_b300DirectMode.load(std::memory_order_acquire)){
  ID3D11Resource*sel=g_b301DirectCbResource.load(std::memory_order_acquire);const bool selected=(r==sel);const unsigned cat=selected?B323_SEL_UNMAP:B323_OTHER_UNMAP;
  const bool b323Active=g_b323ProfilerActive.load(std::memory_order_relaxed);if(b323Active)g_b323Calls[cat].fetch_add(1,std::memory_order_relaxed);const bool timed=b323Active&&b323_should_sample(cat);long long tb=0,ob=0,oe=0;if(timed)tb=b307_qpc_now();
  if(!selected){if(t_pending.target&&t_pending.resource==r&&t_pending.data&&t_pending.b484WatchSlot>=0)b484_capture_watch(t_pending.b484WatchSlot,t_pending.data,t_pending.byte_count);if(timed)ob=b307_qpc_now();o_unmap(c,r,sub);if(timed){oe=b307_qpc_now();b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);}t_pending={};return;}
  if(!t_b539GameplayPass||!GhostingAdmitSelectedWrite(r,t_pending)){
   if(timed)ob=b307_qpc_now();o_unmap(c,r,sub);if(timed){oe=b307_qpc_now();b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);}t_pending={};return;
  }
  if(g_active)g_active->fetch_add(1);b309_capture_direct_camera_write_if_needed(r,t_pending.data,t_pending.byte_count);if(timed)ob=b307_qpc_now();o_unmap(c,r,sub);if(timed)oe=b307_qpc_now();t_pending={};if(g_active)g_active->fetch_sub(1);if(timed)b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);return;
 }
 const long long totalBegin=b307_qpc_now();ID3D11Resource*selectedResource=g_b301DirectCbResource.load(std::memory_order_acquire);const bool selected=r&&r==selectedResource;
 if(g_active)g_active->fetch_add(1);const bool fast=g_b175FastHookActive.load(std::memory_order_acquire);unsigned i=fast?~0u:begin_map_event(2,r,sub,(D3D11_MAP)0,0);if(t_pending.target&&t_pending.resource==r&&t_pending.data){if(fast)b280_observe_constant_write(r,t_pending.data,t_pending.byte_count);if(fast&&r==(ID3D11Resource*)g_b175FastConstants.Get()){AcquireSRWLockExclusive(&g_b175FastLock);g_b175FastSnapshot.byte_count=t_pending.byte_count;g_b175FastSnapshot.resource=r;std::memcpy(g_b175FastSnapshot.bytes,t_pending.data,t_pending.byte_count);g_b175FastSnapshot.ready.store(1,std::memory_order_release);g_b175FastSnapshotSerial.fetch_add(1,std::memory_order_acq_rel);g_b175FastLastUpdateTick.store(GetTickCount64(),std::memory_order_release);ReleaseSRWLockExclusive(&g_b175FastLock);}else record_snapshot(r,t_pending.data,t_pending.byte_count);}const long long ob=b307_qpc_now();o_unmap(c,r,sub);const long long oe=b307_qpc_now();t_pending={};if(i!=~0u)finish_map_event(i,S_OK);if(g_active)g_active->fetch_sub(1);const long long te=b307_qpc_now();b308_record_unmap_timing(te-totalBegin,oe-ob,selected);
}
static void STDMETHODCALLTYPE h_fast_om_rt(ID3D11DeviceContext*c,UINT count,ID3D11RenderTargetView*const*views,ID3D11DepthStencilView*dsv){
 // B394: retire per-call OM QPC profiling from the live GTA render hot path.
 if(g_active)g_active->fetch_add(1);
 o_fast_om_rt(c,count,views,dsv);
 ComPtr<ID3D11Resource>resource;ComPtr<ID3D11Texture2D>texture;if(count&&views&&views[0])views[0]->GetResource(&resource);if(resource)resource.As(&texture);
 ComPtr<ID3D11Resource>depthResource;ComPtr<ID3D11Texture2D>depthTexture;if(dsv)dsv->GetResource(&depthResource);if(depthResource)depthResource.As(&depthTexture);
 // B539: the same 256-byte CB is reused by shadow/reflection/UI passes.
 // Only retain camera writes while the discovered gameplay depth/color pair
 // is bound. All writes still upload through the CPU mirror.
 t_b539GameplayPass=depthTexture&&depthTexture.Get()==g_b175FastDepth.Get()&&texture&&
  (texture.Get()==g_b175FastColorPair[0].Get()||texture.Get()==g_b175FastColorPair[1].Get());

 // B394 freeze-only: GTA's OM hot path must never wait on our bookkeeping lock.
 // Missing one observation is safe: the next OM/draw refreshes the retained identity, while
 // blocking here stalls GTA before B171PresentLiveFrame and was invisible to the helper profiler.
 if(TryAcquireSRWLockExclusive(&g_b175FastLock)){
  g_b175FastBoundRtvColor=texture;g_b285FastBoundDepth=depthTexture;
  // B210: ESC/pause can change GTA's exact draw signature while retaining the already-
  // validated dynamically-sized XeSS color ping-pong pair. Treat binding either retained member as
  // fresh color evidence as well as the original exact-draw marker. This lets the live
  // path survive menu transitions without tearing down/reinstalling the context hook.
  if(g_b175FastHookActive.load(std::memory_order_acquire)&&texture&&(texture.Get()==g_b175FastColorPair[0].Get()||texture.Get()==g_b175FastColorPair[1].Get())){
   g_b175FastCurrentColor=texture;g_b175FastColorSerial.fetch_add(1,std::memory_order_acq_rel);g_b175FastLastUpdateTick.store(GetTickCount64(),std::memory_order_release);
  }
  ReleaseSRWLockExclusive(&g_b175FastLock);
 }
 if(g_active)g_active->fetch_sub(1);
}
static bool b175_fast_color_member(ID3D11Texture2D*t){return t&&(t==g_b175FastColorPair[0].Get()||t==g_b175FastColorPair[1].Get());}
static UINT g_b160w{},g_b160h{};
static bool b285_identity4(const float*m){double e=0;for(int r=0;r<4;++r)for(int c=0;c<4;++c){const double want=r==c?1.0:0.0;e=(std::max)(e,std::fabs((double)m[r*4+c]-want));}return e<1.0e-4;}
static bool b285_finite64(const float*m){for(int i=0;i<64;++i)if(!std::isfinite(m[i])||std::fabs(m[i])>10000000.0f)return false;return true;}
static void b175_fast_observe_draw(ID3D11DeviceContext*c,unsigned type,unsigned a,unsigned b,int base){
 const bool active=g_b175FastHookActive.load(std::memory_order_acquire);
 if(type==g_b175FastDrawType&&a==g_b175FastDrawA&&b==g_b175FastDrawB&&base==g_b175FastDrawC&&active){
  // B394: selected draw bookkeeping is opportunistic; never block GTA on our SRW lock.
  if(TryAcquireSRWLockExclusive(&g_b175FastLock)){
   if(g_b175FastBoundRtvColor&&b175_fast_color_member(g_b175FastBoundRtvColor.Get())){g_b175FastCurrentColor=g_b175FastBoundRtvColor;g_b175FastColorSerial.fetch_add(1,std::memory_order_acq_rel);}
   ReleaseSRWLockExclusive(&g_b175FastLock);
  }
 }
 // B286 deliberately does not classify matrices here. The indexed-draw hook itself is the test.
}
static void b286_observe_draw_indexed(ID3D11DeviceContext*c,UINT count,UINT start,INT base){
 if(!g_b175FastHookActive.load(std::memory_order_acquire))return;
 const unsigned long long serial=g_b286IndexedDraws.fetch_add(1,std::memory_order_relaxed)+1;
 // B300 ACQUISITION ONLY: DrawIndexed classification exists only until the direct camera CB is locked.
 // B298 still touched VS/CB state on every DrawIndexed, which the B298 log correlated with the
 // source cadence falling from ~64 FPS before F9 into the low 20s.  The retained OM identities
 // continue to update on every OMSetRenderTargets call, while matrix classification only performs
 // COM state queries on this bounded sample.  Diagnostic full-state probing remains lower-rate.
 constexpr unsigned kB300AcquireDrawSampleStride=8;
 // B431 is intentionally bounded to the reacquisition window. Normal gameplay remains stride 8.
 const unsigned b431AcquireStride=GhostingClassifierStride();
 const bool b299CameraSample=(serial%b431AcquireStride)==0;
 const bool b299DiagnosticSample=(serial%256ull)==0;
 if(!b299CameraSample&&!b299DiagnosticSample)return;
 bool fastColor=false,fastDepth=false;
 AcquireSRWLockShared(&g_b175FastLock);
 fastColor=g_b175FastBoundRtvColor&&b175_fast_color_member(g_b175FastBoundRtvColor.Get());
 fastDepth=g_b285FastBoundDepth&&g_b175FastDepth&&g_b285FastBoundDepth.Get()==g_b175FastDepth.Get();
 ReleaseSRWLockShared(&g_b175FastLock);

 ID3D11Buffer*cb1=nullptr;D3D11_BUFFER_DESC cb1Desc{};unsigned cb1Bytes=0;
 if(fastColor&&fastDepth){c->VSGetConstantBuffers(1,1,&cb1);if(cb1){cb1->GetDesc(&cb1Desc);cb1Bytes=cb1Desc.ByteWidth;remember_cb(cb1);}}
 ID3D11VertexShader*vs=nullptr;
 if(fastColor&&fastDepth&&cb1&&cb1Bytes==256){
  c->VSGetShader(&vs,nullptr,nullptr);
  const int bi=b280_known_cb_index(cb1);unsigned char raw[256]{};bool have=false;unsigned long long writeSerial=0;
  if(bi>=0){AcquireSRWLockShared(&g_b287LatestLock);have=g_b287LatestValid[bi];if(have){std::memcpy(raw,g_b287Latest256[bi],256);writeSerial=g_b287LatestWriteSerial[bi];}ReleaseSRWLockShared(&g_b287LatestLock);}
  have=GhostingClassifierSnapshotAllowed(cb1,have);
  AcquireSRWLockExclusive(&g_b287FrameLock);++g_b287MainPassDraws;ReleaseSRWLockExclusive(&g_b287FrameLock);
  if(have)GhostingClassifyDraw(raw,bi,vs,count,writeSerial);
 }
 // Low-rate B286 compatibility probe: keep the old full state visibility, but only 1/256 draws.
 if(serial<=16 || (serial&255ull)==0ull){
  ID3D11RenderTargetView*rtv=nullptr;ID3D11DepthStencilView*dsv=nullptr;c->OMGetRenderTargets(1,&rtv,&dsv);
  D3D11_TEXTURE2D_DESC rd{},dd{};ComPtr<ID3D11Resource>rr,dr;ComPtr<ID3D11Texture2D>rt,dt;
  if(rtv){rtv->GetResource(&rr);rr.As(&rt);if(rt)rt->GetDesc(&rd);}if(dsv){dsv->GetResource(&dr);dr.As(&dt);if(dt)dt->GetDesc(&dd);}
  if(!vs)c->VSGetShader(&vs,nullptr,nullptr);
  ID3D11Buffer*cbs[kPsCbs]{};c->VSGetConstantBuffers(0,kPsCbs,cbs);unsigned mask=0,bytes[kPsCbs]{};
  for(unsigned i=0;i<kPsCbs;++i){if(!cbs[i])continue;mask|=(1u<<i);D3D11_BUFFER_DESC bd{};cbs[i]->GetDesc(&bd);bytes[i]=bd.ByteWidth;remember_cb(cbs[i]);}
  AcquireSRWLockExclusive(&g_b286ProbeLock);g_b286LastVs=vs;g_b286LastIndexCount=count;g_b286LastStartIndex=start;g_b286LastBaseVertex=base;g_b286LastRtvW=rd.Width;g_b286LastRtvH=rd.Height;g_b286LastRtvFormat=(unsigned)rd.Format;g_b286LastDsvW=dd.Width;g_b286LastDsvH=dd.Height;g_b286LastDsvFormat=(unsigned)dd.Format;g_b286LastCbMask=mask;for(unsigned i=0;i<kPsCbs;++i)g_b286LastCbBytes[i]=bytes[i];g_b286LastFastColorMatch=fastColor;g_b286LastFastDepthMatch=fastDepth;ReleaseSRWLockExclusive(&g_b286ProbeLock);g_b286ProbeSamples.fetch_add(1,std::memory_order_relaxed);
  for(auto*cb:cbs)if(cb)cb->Release();if(rtv)rtv->Release();if(dsv)dsv->Release();
 }
 if(cb1)cb1->Release();if(vs)vs->Release();
}
static bool b322_should_sample(unsigned g){return ((g_b322SampleOrd[g].fetch_add(1,std::memory_order_relaxed)+1ull)%kB322SampleStride)==0ull;}
static void b322_done(unsigned g,const LARGE_INTEGER&a,const LARGE_INTEGER&b){g_b322Samples[g].fetch_add(1,std::memory_order_relaxed);g_b322Ticks[g].fetch_add(b.QuadPart-a.QuadPart,std::memory_order_relaxed);}
#define B322_VOID_PROFILE(G,CALL) do{g_b322Calls[G].fetch_add(1,std::memory_order_relaxed);if(b322_should_sample(G)){LARGE_INTEGER b322QpcBegin{},b322QpcEnd{};QueryPerformanceCounter(&b322QpcBegin);CALL;QueryPerformanceCounter(&b322QpcEnd);b322_done(G,b322QpcBegin,b322QpcEnd);}else{CALL;}}while(0)
static void STDMETHODCALLTYPE b322_vs_cb(ID3D11DeviceContext*c,UINT a,UINT b,ID3D11Buffer*const*v){B322_VOID_PROFILE(B322_CB,o_b322_vs_cb(c,a,b,v));}
static void STDMETHODCALLTYPE b322_ps_cb(ID3D11DeviceContext*c,UINT a,UINT b,ID3D11Buffer*const*v){B322_VOID_PROFILE(B322_CB,o_b322_ps_cb(c,a,b,v));}
static void STDMETHODCALLTYPE b322_ps_srv(ID3D11DeviceContext*c,UINT a,UINT b,ID3D11ShaderResourceView*const*v){B322_VOID_PROFILE(B322_SRV,o_b322_ps_srv(c,a,b,v));}
static void STDMETHODCALLTYPE b322_vs_srv(ID3D11DeviceContext*c,UINT a,UINT b,ID3D11ShaderResourceView*const*v){B322_VOID_PROFILE(B322_SRV,o_b322_vs_srv(c,a,b,v));}
static void STDMETHODCALLTYPE b322_vs_shader(ID3D11DeviceContext*c,ID3D11VertexShader*v,ID3D11ClassInstance*const*i,UINT n){B322_VOID_PROFILE(B322_SHADER,o_b322_vs_shader(c,v,i,n));}
static void STDMETHODCALLTYPE b322_ps_shader(ID3D11DeviceContext*c,ID3D11PixelShader*v,ID3D11ClassInstance*const*i,UINT n){B322_VOID_PROFILE(B322_SHADER,o_b322_ps_shader(c,v,i,n));}
static void STDMETHODCALLTYPE b322_ia_vb(ID3D11DeviceContext*c,UINT a,UINT n,ID3D11Buffer*const*b,const UINT*s,const UINT*o){B322_VOID_PROFILE(B322_IA,o_b322_ia_vb(c,a,n,b,s,o));}
static void STDMETHODCALLTYPE b322_ia_ib(ID3D11DeviceContext*c,ID3D11Buffer*b,DXGI_FORMAT f,UINT o){B322_VOID_PROFILE(B322_IA,o_b322_ia_ib(c,b,f,o));}
static void STDMETHODCALLTYPE b322_ia_layout(ID3D11DeviceContext*c,ID3D11InputLayout*l){B322_VOID_PROFILE(B322_IA,o_b322_ia_layout(c,l));}
static void STDMETHODCALLTYPE b322_ia_topology(ID3D11DeviceContext*c,D3D11_PRIMITIVE_TOPOLOGY t){B322_VOID_PROFILE(B322_IA,o_b322_ia_topology(c,t));}
static void STDMETHODCALLTYPE b322_om_rt(ID3D11DeviceContext*c,UINT n,ID3D11RenderTargetView*const*v,ID3D11DepthStencilView*d){B322_VOID_PROFILE(B322_OMRS,o_b322_om_rt(c,n,v,d));}
static void STDMETHODCALLTYPE b322_rs_viewports(ID3D11DeviceContext*c,UINT n,const D3D11_VIEWPORT*v){B322_VOID_PROFILE(B322_OMRS,o_b322_rs_viewports(c,n,v));}
static void STDMETHODCALLTYPE b322_copy_sub(ID3D11DeviceContext*c,ID3D11Resource*d,UINT ds,UINT x,UINT y,UINT z,ID3D11Resource*r,UINT rs,const D3D11_BOX*b){B322_VOID_PROFILE(B322_RESOURCE,o_b322_copy_sub(c,d,ds,x,y,z,r,rs,b));}
static void STDMETHODCALLTYPE b322_copy_resource(ID3D11DeviceContext*c,ID3D11Resource*d,ID3D11Resource*r){B322_VOID_PROFILE(B322_RESOURCE,o_b322_copy_resource(c,d,r));}
static void STDMETHODCALLTYPE b322_update_sub(ID3D11DeviceContext*c,ID3D11Resource*d,UINT ds,const D3D11_BOX*b,const void*p,UINT rp,UINT dp){B322_VOID_PROFILE(B322_RESOURCE,o_b322_update_sub(c,d,ds,b,p,rp,dp));}
static HRESULT STDMETHODCALLTYPE b322_map(ID3D11DeviceContext*c,ID3D11Resource*r,UINT sub,D3D11_MAP mt,UINT fl,D3D11_MAPPED_SUBRESOURCE*m){g_b322Calls[B322_MAPUNMAP].fetch_add(1,std::memory_order_relaxed);if(b322_should_sample(B322_MAPUNMAP)){LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);HRESULT hr=o_b322_map(c,r,sub,mt,fl,m);QueryPerformanceCounter(&b);b322_done(B322_MAPUNMAP,a,b);return hr;}return o_b322_map(c,r,sub,mt,fl,m);}
static void STDMETHODCALLTYPE b322_unmap(ID3D11DeviceContext*c,ID3D11Resource*r,UINT sub){B322_VOID_PROFILE(B322_MAPUNMAP,o_b322_unmap(c,r,sub));}
#undef B322_VOID_PROFILE
extern "C" __declspec(dllexport) BOOL WINAPI B322ConsumeD3D11StateResourceProfile(unsigned long long*counts,unsigned long long*samples,double*sampleMs,double*estimatedMs){
 LARGE_INTEGER f{};QueryPerformanceFrequency(&f);if(!f.QuadPart)return FALSE;for(unsigned g=0;g<B322_GROUP_COUNT;++g){const auto c=g_b322Calls[g].exchange(0,std::memory_order_acq_rel),n=g_b322Samples[g].exchange(0,std::memory_order_acq_rel);const long long t=g_b322Ticks[g].exchange(0,std::memory_order_acq_rel);const double ms=(double)t*1000.0/(double)f.QuadPart,est=(n&&c)?ms*((double)c/(double)n):0.0;if(counts)counts[g]=c;if(samples)samples[g]=n;if(sampleMs)sampleMs[g]=ms;if(estimatedMs)estimatedMs[g]=est;}return g_b322ProfilerActive.load(std::memory_order_acquire)?TRUE:FALSE;
}
extern "C" __declspec(dllexport) BOOL WINAPI B323ConsumeMapUnmapOwnershipProfile(unsigned long long*counts,unsigned long long*samples,double*estimatedTotalMs,double*estimatedOriginalMs,unsigned long long*selectedResource,unsigned long long*topResources,unsigned long long*topSampleCounts,double*topEstimatedTotalMs){
 LARGE_INTEGER f{};QueryPerformanceFrequency(&f);if(!f.QuadPart)return FALSE;for(unsigned c=0;c<B323_CAT_COUNT;++c){const auto cc=g_b323Calls[c].exchange(0,std::memory_order_acq_rel),nn=g_b323Samples[c].exchange(0,std::memory_order_acq_rel);const long long tt=g_b323TotalTicks[c].exchange(0,std::memory_order_acq_rel),oo=g_b323OriginalTicks[c].exchange(0,std::memory_order_acq_rel);if(counts)counts[c]=cc;if(samples)samples[c]=nn;if(estimatedTotalMs)estimatedTotalMs[c]=nn?((double)tt*1000.0/(double)f.QuadPart)*((double)cc/(double)nn):0.0;if(estimatedOriginalMs)estimatedOriginalMs[c]=nn?((double)oo*1000.0/(double)f.QuadPart)*((double)cc/(double)nn):0.0;}
 if(selectedResource)*selectedResource=(unsigned long long)(uintptr_t)g_b301DirectCbResource.load(std::memory_order_acquire);B323ResourceSample local[64]{};AcquireSRWLockExclusive(&g_b323ResourceLock);for(unsigned i=0;i<64;++i){local[i]=g_b323ResourceSamples[i];g_b323ResourceSamples[i]={};}ReleaseSRWLockExclusive(&g_b323ResourceLock);
 for(unsigned rank=0;rank<4;++rank){int best=-1;for(unsigned i=0;i<64;++i)if(local[i].resource&&(best<0||local[i].samples>local[best].samples))best=(int)i;if(best<0){if(topResources)topResources[rank]=0;if(topSampleCounts)topSampleCounts[rank]=0;if(topEstimatedTotalMs)topEstimatedTotalMs[rank]=0.0;continue;}if(topResources)topResources[rank]=(unsigned long long)(uintptr_t)local[best].resource;if(topSampleCounts)topSampleCounts[rank]=local[best].samples;if(topEstimatedTotalMs)topEstimatedTotalMs[rank]=(double)local[best].totalTicks*1000.0/(double)f.QuadPart*kB323SampleStride;local[best].resource=nullptr;}
 return g_b323ProfilerActive.load(std::memory_order_acquire)?TRUE:FALSE;
}
static bool b321_should_sample(std::atomic<unsigned long long>&ordinal){return ((ordinal.fetch_add(1,std::memory_order_relaxed)+1ull)%kB321SampleStride)==0ull;}
static void b321_add_sample_ticks(std::atomic<long long>&dst,const LARGE_INTEGER&a,const LARGE_INTEGER&b){dst.fetch_add(b.QuadPart-a.QuadPart,std::memory_order_relaxed);}
static void STDMETHODCALLTYPE b321_profile_draw(ID3D11DeviceContext*c,UINT count,UINT start){
 g_b321DrawCalls.fetch_add(1,std::memory_order_relaxed);const bool sample=b321_should_sample(g_b321DrawSampleOrdinal);if(sample){LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);o_fast_draw(c,count,start);QueryPerformanceCounter(&b);g_b321SampledDrawCalls.fetch_add(1,std::memory_order_relaxed);b321_add_sample_ticks(g_b321SampledDrawTicks,a,b);}else o_fast_draw(c,count,start);
}
static void STDMETHODCALLTYPE b321_profile_draw_indexed(ID3D11DeviceContext*c,UINT count,UINT start,INT base){
 g_b321DrawIndexedCalls.fetch_add(1,std::memory_order_relaxed);const bool sample=b321_should_sample(g_b321DrawSampleOrdinal);if(sample){LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);o_fast_draw_indexed(c,count,start,base);QueryPerformanceCounter(&b);g_b321SampledDrawCalls.fetch_add(1,std::memory_order_relaxed);b321_add_sample_ticks(g_b321SampledDrawTicks,a,b);}else o_fast_draw_indexed(c,count,start,base);
}
static void STDMETHODCALLTYPE b321_profile_draw_indexed_instanced(ID3D11DeviceContext*c,UINT indexCountPerInstance,UINT instanceCount,UINT startIndex,INT baseVertex,UINT startInstance){
 g_b321DrawIndexedInstancedCalls.fetch_add(1,std::memory_order_relaxed);const bool sample=b321_should_sample(g_b321DrawSampleOrdinal);if(sample){LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);o_b321_draw_indexed_instanced(c,indexCountPerInstance,instanceCount,startIndex,baseVertex,startInstance);QueryPerformanceCounter(&b);g_b321SampledDrawCalls.fetch_add(1,std::memory_order_relaxed);b321_add_sample_ticks(g_b321SampledDrawTicks,a,b);}else o_b321_draw_indexed_instanced(c,indexCountPerInstance,instanceCount,startIndex,baseVertex,startInstance);
}
static void STDMETHODCALLTYPE b321_profile_draw_instanced(ID3D11DeviceContext*c,UINT vertexCountPerInstance,UINT instanceCount,UINT startVertex,UINT startInstance){
 g_b321DrawInstancedCalls.fetch_add(1,std::memory_order_relaxed);const bool sample=b321_should_sample(g_b321DrawSampleOrdinal);if(sample){LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);o_b321_draw_instanced(c,vertexCountPerInstance,instanceCount,startVertex,startInstance);QueryPerformanceCounter(&b);g_b321SampledDrawCalls.fetch_add(1,std::memory_order_relaxed);b321_add_sample_ticks(g_b321SampledDrawTicks,a,b);}else o_b321_draw_instanced(c,vertexCountPerInstance,instanceCount,startVertex,startInstance);
}
static void STDMETHODCALLTYPE b321_profile_dispatch(ID3D11DeviceContext*c,UINT x,UINT y,UINT z){
 g_b321DispatchCalls.fetch_add(1,std::memory_order_relaxed);const bool sample=b321_should_sample(g_b321DispatchSampleOrdinal);if(sample){LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);o_b321_dispatch(c,x,y,z);QueryPerformanceCounter(&b);g_b321SampledDispatchCalls.fetch_add(1,std::memory_order_relaxed);b321_add_sample_ticks(g_b321SampledDispatchTicks,a,b);}else o_b321_dispatch(c,x,y,z);
}
extern "C" __declspec(dllexport) BOOL WINAPI B321ConsumeD3D11RenderProfile(unsigned long long*drawCalls,unsigned long long*drawIndexedCalls,unsigned long long*drawIndexedInstancedCalls,unsigned long long*drawInstancedCalls,unsigned long long*dispatchCalls,unsigned long long*sampledDrawCalls,unsigned long long*sampledDispatchCalls,double*sampledDrawMs,double*sampledDispatchMs,double*estimatedDrawMs,double*estimatedDispatchMs){
 LARGE_INTEGER f{};QueryPerformanceFrequency(&f);if(!f.QuadPart)return FALSE;const auto d=g_b321DrawCalls.exchange(0,std::memory_order_acq_rel),di=g_b321DrawIndexedCalls.exchange(0,std::memory_order_acq_rel),dii=g_b321DrawIndexedInstancedCalls.exchange(0,std::memory_order_acq_rel),inst=g_b321DrawInstancedCalls.exchange(0,std::memory_order_acq_rel),disp=g_b321DispatchCalls.exchange(0,std::memory_order_acq_rel);const auto sd=g_b321SampledDrawCalls.exchange(0,std::memory_order_acq_rel),sdisp=g_b321SampledDispatchCalls.exchange(0,std::memory_order_acq_rel);const long long dt=g_b321SampledDrawTicks.exchange(0,std::memory_order_acq_rel),dispt=g_b321SampledDispatchTicks.exchange(0,std::memory_order_acq_rel);const double dms=(double)dt*1000.0/(double)f.QuadPart,dispms=(double)dispt*1000.0/(double)f.QuadPart;const unsigned long long totalDraw=d+di+dii+inst;const double dest=(sd&&totalDraw)?dms*((double)totalDraw/(double)sd):0.0;const double dispest=(sdisp&&disp)?dispms*((double)disp/(double)sdisp):0.0;if(drawCalls)*drawCalls=d;if(drawIndexedCalls)*drawIndexedCalls=di;if(drawIndexedInstancedCalls)*drawIndexedInstancedCalls=dii;if(drawInstancedCalls)*drawInstancedCalls=inst;if(dispatchCalls)*dispatchCalls=disp;if(sampledDrawCalls)*sampledDrawCalls=sd;if(sampledDispatchCalls)*sampledDispatchCalls=sdisp;if(sampledDrawMs)*sampledDrawMs=dms;if(sampledDispatchMs)*sampledDispatchMs=dispms;if(estimatedDrawMs)*estimatedDrawMs=dest;if(estimatedDispatchMs)*estimatedDispatchMs=dispest;return g_b321ProfilerActive.load(std::memory_order_acquire)?TRUE:FALSE;
}
static void STDMETHODCALLTYPE h_fast_draw(ID3D11DeviceContext*c,UINT count,UINT start){
 if(g_active)g_active->fetch_add(1);b175_fast_observe_draw(c,2,count,start,0);
 o_fast_draw(c,count,start);if(g_active)g_active->fetch_sub(1);
}
static void STDMETHODCALLTYPE h_fast_draw_indexed(ID3D11DeviceContext*c,UINT count,UINT start,INT base){
 if(g_active)g_active->fetch_add(1);b286_observe_draw_indexed(c,count,start,base);b175_fast_observe_draw(c,1,count,start,base);
 o_fast_draw_indexed(c,count,start,base);if(g_active)g_active->fetch_sub(1);
}

extern "C" __declspec(dllexport) HRESULT WINAPI B50AttachWriteTrace(ID3D11DeviceContext*context,std::atomic<unsigned>*sequence,std::atomic<unsigned>*active)
{
    if(!context||!sequence||!active)return E_INVALIDARG;
    auto***o=reinterpret_cast<void***>(context); if(!o||!*o)return E_FAIL; void**v=*o;
    g_seq=sequence;g_active=active;g_cb_next=0;g_cb_overflow=0;g_map_next=0;g_map_overflow=0;g_snapshot_next=0;g_snapshot_overflow=0;g_known_cb_count=0;
    for(unsigned i=0;i<kKnownCbs;++i){g_known_cbs[i]=nullptr;g_known_cb_bytes[i]=0;}
    for(auto&e:g_cb_events){e.ready=0;for(auto&b:e.buffers)b=nullptr;}
    for(auto&e:g_map_events){e.ready=0;e.resource=nullptr;}
    for(auto&e:g_snapshots){e.ready=0;e.byte_count=0;}
    ID3D11Buffer*initial[kPsCbs]{};context->VSGetConstantBuffers(0,kPsCbs,initial);record_cb_event(0,0,kPsCbs,initial);for(auto*b:initial)if(b)b->Release();
    for(auto&b:initial)b=nullptr;context->PSGetConstantBuffers(0,kPsCbs,initial);record_cb_event(1,0,kPsCbs,initial);for(auto*b:initial)if(b)b->Release();
    o_vs_cb=(ps_cb_fn)v[7];o_map=(map_fn)v[14];o_unmap=(unmap_fn)v[15];o_ps_cb=(ps_cb_fn)v[16];v[7]=(void*)&h_vs_cb;v[14]=(void*)&h_map;v[15]=(void*)&h_unmap;v[16]=(void*)&h_ps_cb;return S_OK;
}
extern "C" __declspec(dllexport) void WINAPI B175AbortDiscovery(){release_all();g_seq=nullptr;g_active=nullptr;}

extern "C" __declspec(dllexport) BOOL WINAPI B175FastPathReady(){return g_b175FastReady.load(std::memory_order_acquire)?TRUE:FALSE;}
extern "C" __declspec(dllexport) BOOL WINAPI B175nPreCoreProbeReady(){return g_b175nPreCoreProbeReady.load(std::memory_order_acquire)?TRUE:FALSE;}
extern "C" __declspec(dllexport) void WINAPI B175ResetFastFreshness()
{
 AcquireSRWLockExclusive(&g_b175FastLock);g_b175FastLastCaptureSerial=g_b175FastSnapshotSerial.load(std::memory_order_acquire);g_b175FastLastColorSerial=g_b175FastColorSerial.load(std::memory_order_acquire);g_b175FastStalePresents=0;g_b175FastLastUpdateTick.store(GetTickCount64(),std::memory_order_release);ReleaseSRWLockExclusive(&g_b175FastLock);
}
extern "C" __declspec(dllexport) HRESULT WINAPI B175AttachFastTrace(ID3D11DeviceContext*context,std::atomic<unsigned>*active)
{
    if(!context||!active||!g_b175FastReady.load(std::memory_order_acquire))return E_FAIL;
    auto***o=reinterpret_cast<void***>(context);if(!o||!*o)return E_FAIL;void**v=*o;
    ID3D11RenderTargetView*initial{};context->OMGetRenderTargets(1,&initial,nullptr);ComPtr<ID3D11Resource>resource;ComPtr<ID3D11Texture2D>texture;if(initial){initial->GetResource(&resource);initial->Release();}if(resource)resource.As(&texture);AcquireSRWLockExclusive(&g_b175FastLock);g_b175FastBoundRtvColor=texture;g_b285FastBoundDepth.Reset();ReleaseSRWLockExclusive(&g_b175FastLock);
    if(g_b175FastDrawType!=1&&g_b175FastDrawType!=2)return E_FAIL;
    g_active=active;o_fast_draw_indexed=(draw_indexed_fn)v[12];o_fast_draw=(draw_fn)v[13];o_b321_draw_indexed_instanced=(draw_indexed_instanced_fn)v[20];o_b321_draw_instanced=(draw_instanced_fn)v[21];o_b321_dispatch=(dispatch_fn)v[41];o_map=(map_fn)v[14];o_unmap=(unmap_fn)v[15];o_fast_om_rt=(om_rt_fn)v[33];v[12]=(void*)&h_fast_draw_indexed;v[13]=(void*)&h_fast_draw;v[14]=(void*)&h_map;v[15]=(void*)&h_unmap;v[33]=(void*)&h_fast_om_rt;
    g_b308HookContext=context;g_b308AbState.store(2,std::memory_order_release);g_b308DirectLockTick.store(0,std::memory_order_release);g_b308AbStartTick.store(0,std::memory_order_release);
    g_b175FastSnapshotSerial.store(1,std::memory_order_release);g_b175FastColorSerial.store(0,std::memory_order_release);g_b175FastLastCaptureSerial=0;g_b175FastLastColorSerial=0;g_b175FastStalePresents=0;g_b175FastHookStart=GetTickCount64();g_b175FastLastUpdateTick.store(g_b175FastHookStart,std::memory_order_release);g_b175FastHookActive.store(true,std::memory_order_release);
    const unsigned installs=g_b175FastHookInstallCount.fetch_add(1,std::memory_order_acq_rel)+1;
    // B214: install count is diagnostic history, not a lifetime one-shot gate. A clean
    // teardown followed by rediscovery must be able to attach the fast trace again.
    return S_OK;
}
extern "C" __declspec(dllexport) BOOL WINAPI B390RetainedFastResourcesValid()
{
 AcquireSRWLockShared(&g_b175FastLock);
 const bool valid=g_b175FastReady.load(std::memory_order_acquire)&&g_b175FastDepth&&g_b175FastConstants&&g_b175FastColorPair[0]&&g_b175FastColorPair[1]&&g_b175FastDrawType>=1&&g_b175FastDrawType<=2;
 if(valid){D3D11_TEXTURE2D_DESC a{},b{},d{};D3D11_BUFFER_DESC cb{};g_b175FastColorPair[0]->GetDesc(&a);g_b175FastColorPair[1]->GetDesc(&b);g_b175FastDepth->GetDesc(&d);g_b175FastConstants->GetDesc(&cb);const bool descOk=a.Width&&a.Height&&a.Width==b.Width&&a.Height==b.Height&&d.Width==a.Width&&d.Height==a.Height&&cb.ByteWidth>0;ReleaseSRWLockShared(&g_b175FastLock);return descOk?TRUE:FALSE;}
 ReleaseSRWLockShared(&g_b175FastLock);return FALSE;
}
extern "C" __declspec(dllexport) void WINAPI B390FastTraceDetachPreserveResources(const char*path,const char*reason)
{
 const bool wasActive=g_b175FastHookActive.exchange(false,std::memory_order_acq_rel);g_b321ProfilerActive.store(false,std::memory_order_release);g_b322ProfilerActive.store(false,std::memory_order_release);g_active=nullptr;t_pending={};
 if(!path)return;std::ofstream out(path,std::ios::app);out<<"B390: FAST_HOOK_DETACH_PRESERVE reason="<<(reason?reason:"unspecified")<<", was_active="<<(wasActive?1:0)<<", retained_color_pair="<<(g_b175FastColorPair[0]&&g_b175FastColorPair[1]?1:0)<<", retained_depth="<<(g_b175FastDepth?1:0)<<", retained_constants="<<(g_b175FastConstants?1:0)<<", fast_ready="<<(g_b175FastReady.load()?1:0)<<", ghosting_unchanged=1\\n";out.flush();
}
extern "C" __declspec(dllexport) void WINAPI B175FastTraceTeardown(const char*path,const char*reason)
{
 const bool wasActive=g_b175FastHookActive.exchange(false,std::memory_order_acq_rel);g_b321ProfilerActive.store(false,std::memory_order_release);g_b322ProfilerActive.store(false,std::memory_order_release);
 if(g_b308AbState.load(std::memory_order_acquire)==1&&g_b308HookContext){auto***abObj=reinterpret_cast<void***>(g_b308HookContext);if(abObj&&*abObj){(*abObj)[14]=(void*)&h_map;(*abObj)[15]=(void*)&h_unmap;}g_b308AbState.store(2,std::memory_order_release);}
 AcquireSRWLockExclusive(&g_b175FastLock);g_b175FastSnapshot.ready.store(0,std::memory_order_release);g_b175FastSnapshot.byte_count=0;g_b175FastSnapshot.resource=nullptr;g_b175FastDepth.Reset();g_b175FastColorPair[0].Reset();g_b175FastColorPair[1].Reset();g_b175FastCurrentColor.Reset();g_b175FastBoundRtvColor.Reset();g_b285FastBoundDepth.Reset();g_b175FastConstants.Reset();g_b175FastConstantBytes=0;g_b175FastDrawType=0;g_b175FastDrawA=0;g_b175FastDrawB=0;g_b175FastDrawC=0;ReleaseSRWLockExclusive(&g_b175FastLock);
 g_b175FastReady.store(false,std::memory_order_release);g_active=nullptr;t_pending={};
 if(!path)return;std::ofstream out(path,std::ios::app);out<<"B174y: FAST_HOOK_TEARDOWN reason="<<(reason?reason:"unspecified")<<", was_active="<<(wasActive?1:0)<<", install_count="<<g_b175FastHookInstallCount.load()<<", active_duration_ms="<<(g_b175FastHookStart?GetTickCount64()-g_b175FastHookStart:0)<<", capture_count="<<g_b175FastCaptureCount.load()<<", persistent_hooks=Map_Unmap_OMSetRenderTargets_Draw_DrawIndexed, retained_color_pair_released=1, retained_depth_released=1, retained_constants_released=1, hooks_drained_before_release=1\n";out.flush();
}

static IUnknown* id_resource(ID3D11Resource*r,UINT&w,UINT&h,UINT&fmt,UINT&bind,UINT&samples){if(!r)return nullptr;ComPtr<ID3D11Texture2D>t;if(FAILED(r->QueryInterface(IID_PPV_ARGS(&t)))||!t)return nullptr;D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);w=d.Width;h=d.Height;fmt=d.Format;bind=d.BindFlags;samples=d.SampleDesc.Count;ComPtr<IUnknown>u;if(FAILED(r->QueryInterface(IID_PPV_ARGS(&u)))||!u)return nullptr;return u.Get();}
static IUnknown* id_srv(ID3D11ShaderResourceView*v,UINT&w,UINT&h,UINT&f,UINT&b,UINT&s){if(!v)return nullptr;ComPtr<ID3D11Resource>r;v->GetResource(&r);return id_resource(r.Get(),w,h,f,b,s);}
static IUnknown* id_rtv(ID3D11RenderTargetView*v,UINT&w,UINT&h,UINT&f,UINT&b,UINT&s){if(!v)return nullptr;ComPtr<ID3D11Resource>r;v->GetResource(&r);return id_resource(r.Get(),w,h,f,b,s);}
static bool target_om(const B49OmEvent*e){if(!e||!e->r[0]||!e->dsv)return false;UINT w=0,h=0,f=0,b=0,s=0;if(!id_rtv(e->r[0],w,h,f,b,s)||!gta::SupportedCaptureExtent(w,h)||f!=87||s!=1)return false;ComPtr<ID3D11Resource>r;e->dsv->GetResource(&r);UINT dw=0,dh=0,df=0,db=0,ds=0;return id_resource(r.Get(),dw,dh,df,db,ds)&&dw==w&&dh==h&&gta::SupportedDepthFormat((DXGI_FORMAT)df)&&ds==1;}
static bool history_pair(ID3D11ShaderResourceView*a,ID3D11ShaderResourceView*b){UINT w=0,h=0,f=0,bd=0,s=0,w2=0,h2=0,f2=0,bd2=0,s2=0;auto*x=id_srv(a,w,h,f,bd,s);auto*y=id_srv(b,w2,h2,f2,bd2,s2);return x&&x==y&&w>=640&&h>=360&&w<=3840&&h<=2160&&f==10&&s==1&&w2==w&&h2==h&&f2==f&&s2==s;}
static bool find_consumer(const B49OmEvent*oms,UINT on,const B49PsEvent*ps,UINT pn,const B49DrawEvent*draws,UINT dn,unsigned lo,unsigned hi,ConsumerState&out){ID3D11ShaderResourceView*state[kSrvs]{};unsigned pi=0,oi=0;const B49OmEvent*curOm=nullptr;ConsumerState fallback{};bool haveFallback=false;for(unsigned di=0;di<dn;++di){const auto&d=draws[di];if(!d.ready.load())continue;while(pi<pn&&ps[pi].ready.load()&&ps[pi].ordinal<d.ordinal){const auto&e=ps[pi++];for(unsigned x=0;x<e.count;++x){unsigned slot=e.start+x;if(slot<kSrvs)state[slot]=e.r[x];}}while(oi<on&&oms[oi].ready.load()&&oms[oi].ordinal<d.ordinal)curOm=&oms[oi++];if(d.ordinal<lo||d.ordinal>=hi||!history_pair(state[9],state[14]))continue;ConsumerState q{};q.draw=d.ordinal;q.om=curOm;q.target=target_om(curOm);for(unsigned slot=0;slot<kSrvs;++slot)q.slots[slot]=state[slot];if(q.target){out=q;return true;}if(!haveFallback){fallback=q;haveFallback=true;}}if(haveFallback){out=fallback;return true;}return false;}
static void pipeline_at(unsigned drawOrdinal,PipelineState&out){unsigned cn=g_cb_next.load();if(cn>kCbEvents)cn=kCbEvents;for(unsigned i=0;i<cn;++i){auto&e=g_cb_events[i];if(!e.ready.load()||e.ordinal>=drawOrdinal)continue;unsigned n=e.count>kPsCbs?kPsCbs:e.count;for(unsigned j=0;j<n;++j){unsigned slot=e.start+j;if(slot<kPsCbs){out.cbs[slot]=e.buffers[j];out.cbLastOrdinal[slot]=e.ordinal;}}}}
static const MapEvent* last_write_discard_before(ID3D11Buffer*target,unsigned lo,unsigned hi){const MapEvent*best=nullptr;if(!target)return nullptr;unsigned n=g_map_next.load();if(n>kMapEvents)n=kMapEvents;for(unsigned i=0;i<n;++i){auto&e=g_map_events[i];if(!e.ready.load()||e.type!=1||e.map_type!=(unsigned)D3D11_MAP_WRITE_DISCARD||FAILED(e.hr)||e.resource!=(ID3D11Resource*)target||e.ordinal<lo||e.ordinal>=hi)continue;if(!best||e.ordinal>best->ordinal)best=&e;}return best;}
static const Snapshot* last_snapshot_before(ID3D11Buffer*target,unsigned lo,unsigned hi){const Snapshot*best=nullptr;if(!target)return nullptr;unsigned n=g_snapshot_next.load();if(n>kSnapshots)n=kSnapshots;for(unsigned i=0;i<n;++i){auto&e=g_snapshots[i];if(!e.ready.load()||e.resource!=(ID3D11Resource*)target||e.sequence<lo||e.sequence>=hi)continue;if(!best||e.sequence>best->sequence)best=&e;}return best;}
static void release_all(){unsigned cn=g_cb_next.load();if(cn>kCbEvents)cn=kCbEvents;for(unsigned i=0;i<cn;++i)for(auto&b:g_cb_events[i].buffers)if(b){b->Release();b=nullptr;}unsigned mn=g_map_next.load();if(mn>kMapEvents)mn=kMapEvents;for(unsigned i=0;i<mn;++i)if(g_map_events[i].resource){g_map_events[i].resource->Release();g_map_events[i].resource=nullptr;}}
static float len3(float x,float y,float z){return std::sqrt(x*x+y*y+z*z);}
static float dot3(float ax,float ay,float az,float bx,float by,float bz){return ax*bx+ay*by+az*bz;}
static void log_basis(std::ofstream&out,const char*label,const Snapshot*s,unsigned base,bool columns)
{
    float v[3][3]{};
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){unsigned idx=columns?base+b*4+a:base+a*4+b;v[a][b]=f32(s,idx);}
    float l0=len3(v[0][0],v[0][1],v[0][2]),l1=len3(v[1][0],v[1][1],v[1][2]),l2=len3(v[2][0],v[2][1],v[2][2]);
    float d01=dot3(v[0][0],v[0][1],v[0][2],v[1][0],v[1][1],v[1][2]);
    float d02=dot3(v[0][0],v[0][1],v[0][2],v[2][0],v[2][1],v[2][2]);
    float d12=dot3(v[1][0],v[1][1],v[1][2],v[2][0],v[2][1],v[2][2]);
    float score=std::fabs(l0-1)+std::fabs(l1-1)+std::fabs(l2-1)+std::fabs(d01)+std::fabs(d02)+std::fabs(d12);
    out<<"B75: "<<label<<" base_float="<<base<<", layout="<<(columns?"columns":"rows")<<", v0={"<<std::setprecision(9)<<v[0][0]<<","<<v[0][1]<<","<<v[0][2]<<"}, v1={"<<v[1][0]<<","<<v[1][1]<<","<<v[1][2]<<"}, v2={"<<v[2][0]<<","<<v[2][1]<<","<<v[2][2]<<"}, lengths={"<<l0<<","<<l1<<","<<l2<<"}, dots={"<<d01<<","<<d02<<","<<d12<<"}, orthonormal_score="<<score<<"\n";
}





static IUnknown* b160view(ID3D11View*v,UINT&w,UINT&h,UINT&f,UINT&s){if(!v)return nullptr;ComPtr<ID3D11Resource>r;v->GetResource(&r);UINT b=0;return id_resource(r.Get(),w,h,f,b,s);}
static bool b160better(const B160Candidate&a,const B160Candidate&b){const uint64_t aa=(uint64_t)a.width*a.height,ba=(uint64_t)b.width*b.height;return aa!=ba?aa>ba:a.width!=b.width?a.width>b.width:a.height!=b.height?a.height>b.height:a.draw<b.draw;}
static unsigned b160marker_error(UINT marker,UINT full){const uint64_t doubled=(uint64_t)marker*2;return (unsigned)(doubled>full?doubled-full:full-doubled);}
static void b160remember_top(B160Reject&r,const B160Candidate&candidate){for(unsigned i=0;i<r.top_count;++i)if(r.top_width[i]==candidate.width&&r.top_height[i]==candidate.height)return;const uint64_t area=(uint64_t)candidate.width*candidate.height;if(r.top_count==3&&area<=(uint64_t)r.top_width[2]*r.top_height[2])return;unsigned at=r.top_count<3?r.top_count++:2;r.top_width[at]=candidate.width;r.top_height[at]=candidate.height;for(unsigned i=at;i>0;--i){uint64_t here=(uint64_t)r.top_width[i]*r.top_height[i],prior=(uint64_t)r.top_width[i-1]*r.top_height[i-1];if(here<=prior)break;std::swap(r.top_width[i],r.top_width[i-1]);std::swap(r.top_height[i],r.top_height[i-1]);}}
static unsigned b160draw(const B49OmEvent*om,UINT on,const B49PsEvent*ps,UINT pn,const B49DrawEvent*d,UINT dn,unsigned lo,unsigned hi,const B160Candidate*expected,B160Candidate&selected,B160Reject&reject,std::ostream&diagnostics){
 ID3D11ShaderResourceView*sr[kSrvs]{};ID3D11Buffer*cbs[2][kPsCbs]{};const B49OmEvent*o=nullptr;UINT pi=0,oi=0,ci=0,cn=(std::min)(g_cb_next.load(),kCbEvents);for(UINT di=0;di<dn;++di){if(!d[di].ready.load())continue;
 while(pi<pn&&ps[pi].ready.load()&&ps[pi].ordinal<d[di].ordinal){auto&e=ps[pi++];for(UINT j=0;j<e.count&&j<kSrvs;++j){UINT s=e.start+j;if(s<kSrvs)sr[s]=e.r[j];}}
 while(ci<cn&&g_cb_events[ci].ready.load()&&g_cb_events[ci].ordinal<d[di].ordinal){auto&e=g_cb_events[ci++];if(e.stage<=1)for(UINT j=0;j<e.count&&j<kPsCbs;++j){UINT slot=e.start+j;if(slot<kPsCbs)cbs[e.stage][slot]=e.buffers[j];}}
 while(oi<on&&om[oi].ready.load()&&om[oi].ordinal<d[di].ordinal)o=&om[oi++];if(d[di].ordinal<lo||d[di].ordinal>=hi||!o)continue;++reject.inspected;
 UINT rw=0,rh=0,rf=0,rs=0,dw=0,dh=0,df=0,ds=0;IUnknown*rtv=b160view(o->r[0],rw,rh,rf,rs),*depth=b160view(o->dsv,dw,dh,df,ds);
 reject.observed_width=rw;reject.observed_height=rh;reject.observed_rtv_format=rf;reject.observed_dsv_width=dw;reject.observed_dsv_height=dh;reject.observed_dsv_format=df;
 if(!rtv||rf!=10){++reject.rtv_format;continue;}if(!gta::SupportedCaptureExtent(rw,rh)){++reject.bounds;continue;}if(rs!=1||ds!=1){++reject.samples;continue;}if(!depth||!gta::SupportedDepthFormat((DXGI_FORMAT)df)){if(reject.dsv_format<8)diagnostics<<"R627: DISCOVERY_DEPTH_REJECT draw="<<d[di].ordinal<<", vertices="<<d[di].a<<", color="<<rw<<"x"<<rh<<", depth="<<dw<<"x"<<dh<<", depth_format="<<df<<", depth_present="<<(depth?1:0)<<"\n";++reject.dsv_format;continue;}if(dw!=rw||dh!=rh){++reject.dsv_size;continue;}
 IUnknown*markerIdentity=nullptr;UINT mw=0,mh=0,mf=0;for(auto*v:sr){UINT vw=0,vh=0,vf=0,vs=0;IUnknown*id=b160view(v,vw,vh,vf,vs);if(id&&vf==34&&vs==1&&b160marker_error(vw,rw)<=2&&b160marker_error(vh,rh)<=2){markerIdentity=id;mw=vw;mh=vh;mf=vf;break;}if(id&&vf==34){reject.observed_marker_width=vw;reject.observed_marker_height=vh;reject.observed_marker_format=vf;}}
 if(!markerIdentity){++reject.marker;continue;}ID3D11Buffer*constant=cbs[1][1]?cbs[1][1]:cbs[0][1];if(expected&&(rw!=expected->width||rh!=expected->height||depth!=expected->depth_identity||markerIdentity!=expected->marker_identity||constant!=expected->constant_buffer)){++reject.identity;continue;}
 B160Candidate candidate{d[di].ordinal,rw,rh,rf,df,mw,mh,mf,rtv,depth,markerIdentity,constant};++reject.valid;b160remember_top(reject,candidate);if(!selected.draw||b160better(candidate,selected))selected=candidate;
 }return selected.draw;}
static const B49OmEvent* b160om_at(const B49OmEvent*om,UINT on,unsigned draw){const B49OmEvent*out=nullptr;for(UINT i=0;i<on;++i)if(om[i].ready.load()&&om[i].ordinal<draw)out=&om[i];return out;}
extern "C" __declspec(dllexport) HRESULT WINAPI B175nCheck170AnalyzeRtvProbe(const B49OmEvent*om,UINT on,const B49DrawEvent*draws,UINT dn,UINT split,const char*path){
 if(!om||!draws||!path)return E_INVALIDARG;std::ofstream out(path,std::ios::app);if(!out)return E_FAIL;
 g_b175nPreCoreProbeReady.store(false,std::memory_order_release);
 struct Probe{unsigned draw{};IUnknown*color{};IUnknown*depth{};UINT width{},height{},format{},bind{},samples{};ComPtr<ID3D11Texture2D>colorTexture;ComPtr<ID3D11Texture2D>depthTexture;};
 auto select=[&](unsigned lo,unsigned hi,Probe&selected,unsigned&matches){const B49OmEvent*state=nullptr;UINT oi=0;for(UINT di=0;di<dn;++di){const auto&draw=draws[di];if(!draw.ready.load(std::memory_order_acquire))continue;while(oi<on&&om[oi].ready.load(std::memory_order_acquire)&&om[oi].ordinal<draw.ordinal)state=&om[oi++];if(draw.ordinal<lo||draw.ordinal>=hi||draw.type!=2||draw.a!=100||draw.b!=0||!state||!state->r[0]||!state->dsv)continue;UINT w=0,h=0,f=0,b=0,s=0,dw=0,dh=0,df=0,db=0,ds=0;IUnknown*color=id_rtv(state->r[0],w,h,f,b,s);ComPtr<ID3D11Resource>colorResource,depthResource;state->r[0]->GetResource(&colorResource);state->dsv->GetResource(&depthResource);IUnknown*depth=id_resource(depthResource.Get(),dw,dh,df,db,ds);if(!color||!depth||!gta::SupportedCaptureExtent(w,h)||f!=DXGI_FORMAT_R16G16B16A16_FLOAT||s!=1||!(b&D3D11_BIND_RENDER_TARGET)||dw!=w||dh!=h||!gta::SupportedDepthFormat((DXGI_FORMAT)df)||ds!=1)continue;++matches;if(!selected.draw){selected.draw=draw.ordinal;selected.color=color;selected.depth=depth;selected.width=w;selected.height=h;selected.format=f;selected.bind=b;selected.samples=s;colorResource.As(&selected.colorTexture);depthResource.As(&selected.depthTexture);}}};
 Probe first{},second{};unsigned firstMatches=0,secondMatches=0;select(0,split,first,firstMatches);select(split,~0u,second,secondMatches);
 const bool depthStable=first.depth&&first.depth==second.depth,colorDistinct=first.color&&second.color&&first.color!=second.color,valid=first.draw&&second.draw&&depthStable&&colorDistinct;
 if(valid&&first.colorTexture&&second.colorTexture&&second.depthTexture){AcquireSRWLockExclusive(&g_b175FastLock);g_b175FastColorPair[0]=first.colorTexture;g_b175FastColorPair[1]=second.colorTexture;g_b175FastCurrentColor=second.colorTexture;g_b175FastDepth=second.depthTexture;g_b175FastDrawType=2;g_b175FastDrawA=100;g_b175FastDrawB=0;g_b175FastDrawC=0;g_b160w=second.width;g_b160h=second.height;ReleaseSRWLockExclusive(&g_b175FastLock);g_b175nPreCoreProbeReady.store(true,std::memory_order_release);}
 out<<"B175n: ISOLATED_RTV_PROBE_RESULT first_draw="<<first.draw<<", second_draw="<<second.draw<<", first_matches="<<firstMatches<<", second_matches="<<secondMatches<<", signature={2,100,0,0}, first_desc={"<<first.width<<"x"<<first.height<<","<<first.format<<","<<first.bind<<","<<first.samples<<"}, second_desc={"<<second.width<<"x"<<second.height<<","<<second.format<<","<<second.bind<<","<<second.samples<<"}, depth_identity_stable="<<(depthStable?1:0)<<", color_identity_distinct="<<(colorDistinct?1:0)<<", valid="<<(valid?1:0)<<", selected_color_pair_retained="<<(g_b175FastColorPair[0]&&g_b175FastColorPair[1]?1:0)<<", selected_depth_retained="<<(g_b175FastDepth?1:0)<<", xess_context_created=0, d3d12_device_created=0, d3d11on12_created=0, persistent_hooks_installed=0, dependent_prewarm_started=0\n";
 out.flush();return g_b175nPreCoreProbeReady.load(std::memory_order_acquire)?S_OK:E_FAIL;
}
static const B49DrawEvent* b175_draw_at(const B49DrawEvent*draws,UINT dn,unsigned ordinal){for(UINT i=0;i<dn;++i)if(draws[i].ready.load(std::memory_order_acquire)&&draws[i].ordinal==ordinal)return&draws[i];return nullptr;}static void b160pipe(unsigned draw,B160Pipe&p){unsigned n=g_cb_next.load();if(n>kCbEvents)n=kCbEvents;for(unsigned i=0;i<n;++i){auto&e=g_cb_events[i];if(!e.ready.load()||e.ordinal>=draw||e.stage>1)continue;for(UINT j=0;j<e.count&&j<kPsCbs;++j){UINT s=e.start+j;if(s<kPsCbs)p.cb[e.stage][s]=e.buffers[j];}}}
static Snapshot g_b172Previous;static bool g_b172HavePrevious=false;
static void b172remember(const Snapshot*s){if(!s)return;g_b172Previous.byte_count=s->byte_count;std::memcpy(g_b172Previous.bytes,s->bytes,s->byte_count);g_b172HavePrevious=true;}
static void b160mul(const B160M4&m,const double*x,double*y){for(int r=0;r<4;++r){y[r]=0;for(int c=0;c<4;++c)y[r]+=m.a[r][c]*x[c];}}
static B160Stats b160reproject(const D3D11_MAPPED_SUBRESOURCE&map,const Snapshot*previous,const Snapshot*current,unsigned base,bool transpose,bool reversed){
 B160Stats s{};B160M4 pm=b160matrix(previous,base,transpose),cm=b160matrix(current,base,transpose),inv{};if(!b160inverse(cm,inv))return s;double priorMag=0;bool havePrior=false;
 for(UINT y=16;y<g_b160h;y+=32){havePrior=false;for(UINT x=16;x<g_b160w;x+=32){s.sampled++;const UINT raw=(*(const UINT*)((const BYTE*)map.pData+(size_t)y*map.RowPitch+x*4))&0x00ffffffu;if(raw==0||raw==0x00ffffffu)continue;s.depth_valid++;double physical=(double)raw/16777215.0,z=reversed?1.0-physical:physical;double clip[4]={2.0*(x+.5)/g_b160w-1.0,1.0-2.0*(y+.5)/g_b160h,z,1.0},world[4],prior[4];b160mul(inv,clip,world);if(!std::isfinite(world[3])||fabs(world[3])<1e-10)continue;for(double&v:world)v/=world[3];b160mul(pm,world,prior);if(!std::isfinite(prior[3])||fabs(prior[3])<1e-10)continue;double px=prior[0]/prior[3],py=prior[1]/prior[3];if(!std::isfinite(px)||!std::isfinite(py))continue;s.finite++;if(fabs(px)<=1.05&&fabs(py)<=1.05)s.onscreen++;double dx=(px-clip[0])*(g_b160w*.5),dy=(clip[1]-py)*(g_b160h*.5),m=sqrt(dx*dx+dy*dy);if(std::isfinite(m)){s.sum+=m;s.sumdx+=dx;s.sumdy+=dy;if(m>s.maximum)s.maximum=m;s.mag.push_back(m);s.depth.push_back(physical);if(m<500.0)s.reasonable++;if(havePrior){s.neighbor_sum+=fabs(m-priorMag);s.neighbors++;}priorMag=m;havePrior=true;}}
 }return s;}
static double b160median(B160Stats&s){if(s.mag.empty())return 0;auto v=s.mag;std::sort(v.begin(),v.end());return v[v.size()/2];}
static void b160logstats(std::ofstream&o,unsigned base,unsigned changed,const char*layout,bool reversed,B160Stats&s){
 double median=b160median(s),p95=0,corr=0,lowMean=0,highMean=0;if(!s.mag.empty()){auto v=s.mag;std::sort(v.begin(),v.end());p95=v[(v.size()*95)/100];double mz=0,mm=0;for(size_t i=0;i<s.mag.size();++i){mz+=s.depth[i];mm+=s.mag[i];}mz/=s.mag.size();mm/=s.mag.size();double num=0,dz=0,dm=0;std::vector<std::pair<double,double>>p;for(size_t i=0;i<s.mag.size();++i){double a=s.depth[i]-mz,b=s.mag[i]-mm;num+=a*b;dz+=a*a;dm+=b*b;p.push_back({s.depth[i],s.mag[i]});}if(dz>0&&dm>0)corr=num/sqrt(dz*dm);std::sort(p.begin(),p.end());size_t q=p.size()/4;for(size_t i=0;i<q;++i){lowMean+=p[i].second;highMean+=p[p.size()-1-i].second;}if(q){lowMean/=q;highMean/=q;}}
 o<<"B171: COHERENCE base_float="<<base<<", changed_values="<<changed<<", layout="<<layout<<", depth="<<(reversed?"reversed":"normal")<<", sampled="<<s.sampled<<", finite="<<s.finite<<", onscreen="<<s.onscreen<<", mean_px="<<(s.mag.empty()?0:s.sum/s.mag.size())<<", median_px="<<median<<", p95_px="<<p95<<", max_px="<<s.maximum<<", mean_vector_px={"<<(s.mag.empty()?0:s.sumdx/s.mag.size())<<","<<(s.mag.empty()?0:s.sumdy/s.mag.size())<<"}, depth_motion_correlation="<<corr<<", low_raw_depth_quartile_mean_px="<<lowMean<<", high_raw_depth_quartile_mean_px="<<highMean<<", horizontal_neighbor_delta_mean_px="<<(s.neighbors?s.neighbor_sum/s.neighbors:0)<<"\n";}
static void b160dump(std::ofstream&o,const char*frame,const Snapshot*s){unsigned n=s?s->byte_count/4:0;for(unsigned base=0;base<n;base+=8){o<<"B171: SLOT1_FLOATS frame="<<frame<<", base="<<base<<", values={";for(unsigned j=0;j<8&&base+j<n;++j){if(j)o<<",";o<<std::setprecision(9)<<f32(s,base+j);}o<<"}\n";}}
static uint16_t b160half(float value){uint32_t x=0;std::memcpy(&x,&value,4);uint32_t sign=(x>>16)&0x8000u,exp=(x>>23)&255u,mant=x&0x7fffffu;if(exp==255)return(uint16_t)(sign|0x7c00u|(mant?0x200u:0));int e=(int)exp-127+15;if(e>=31)return(uint16_t)(sign|0x7bffu);if(e<=0){if(e<-10)return(uint16_t)sign;mant|=0x800000u;uint32_t shift=(uint32_t)(14-e),v=mant>>shift;if((mant>>(shift-1))&1u)v++;return(uint16_t)(sign|v);}uint32_t v=sign|((uint32_t)e<<10)|(mant>>13);if(mant&0x1000u)v++;return(uint16_t)v;}
static uint64_t b160hash(const BYTE*p,UINT pitch,UINT w,UINT h){uint64_t q=1469598103934665603ull;for(UINT y=0;y<h;++y)for(UINT x=0;x<w*4;++x){q^=p[(size_t)y*pitch+x];q*=1099511628211ull;}return q;}
static HRESULT b160motion_texture(ID3D11DeviceContext*c,const D3D11_MAPPED_SUBRESOURCE&previousDepth,const D3D11_MAPPED_SUBRESOURCE&depth,const Snapshot*previous,const Snapshot*current,const char*path,std::ofstream&o){
 if(!c||!previous||!current||!g_b160w||!g_b160h)return E_INVALIDARG;const UINT w=g_b160w,h=g_b160h;B160M4 pm=b160matrix(previous,32,true),cm=b160matrix(current,32,true),inv{};if(!b160inverse(cm,inv))return E_FAIL;
 std::vector<uint16_t>pixels((size_t)w*h*2);std::vector<float>firstDepthPixels((size_t)w*h),depthPixels((size_t)w*h);B160MotionStats st{};
 for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){size_t i=((size_t)y*w+x)*2;UINT firstRaw=(*(const UINT*)((const BYTE*)previousDepth.pData+(size_t)y*previousDepth.RowPitch+x*4))&0x00ffffffu;if(firstRaw&&firstRaw!=0x00ffffffu)firstDepthPixels[(size_t)y*w+x]=(float)((double)firstRaw/16777215.0);UINT raw=(*(const UINT*)((const BYTE*)depth.pData+(size_t)y*depth.RowPitch+x*4))&0x00ffffffu;if(raw==0||raw==0x00ffffffu)continue;st.valid++;double z=(double)raw/16777215.0;depthPixels[(size_t)y*w+x]=(float)z;double clip[4]={2.0*(x+.5)/w-1.0,1.0-2.0*(y+.5)/h,z,1.0},world[4],prior[4];b160mul(inv,clip,world);if(!std::isfinite(world[3])||fabs(world[3])<1e-10)continue;for(double&v:world)v/=world[3];b160mul(pm,world,prior);if(!std::isfinite(prior[3])||fabs(prior[3])<1e-10)continue;double px=prior[0]/prior[3],py=prior[1]/prior[3],dx=(px-clip[0])*(w*.5),dy=(clip[1]-py)*(h*.5);if(!std::isfinite(dx)||!std::isfinite(dy))continue;st.finite++;if(fabs(px)<=1.05&&fabs(py)<=1.05)st.onscreen++;double mag=sqrt(dx*dx+dy*dy);st.sum+=mag;if(mag>st.maximum)st.maximum=mag;if(mag>.0001)st.nonzero++;float fx=(float)(dx>65504?65504:dx<-65504?-65504:dx),fy=(float)(dy>65504?65504:dy<-65504?-65504:dy);pixels[i]=b160half(fx);pixels[i+1]=b160half(fy);}
 ComPtr<ID3D11Device>dev;c->GetDevice(&dev);D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_R16G16_FLOAT;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA init{pixels.data(),w*4,0};ComPtr<ID3D11Texture2D>motion;HRESULT createHr=dev->CreateTexture2D(&d,&init,&motion);HRESULT srvHr=E_FAIL;ComPtr<ID3D11ShaderResourceView>srv;if(SUCCEEDED(createHr))srvHr=dev->CreateShaderResourceView(motion.Get(),nullptr,&srv);
 uint64_t cpuHash=b160hash((const BYTE*)pixels.data(),w*4,w,h),gpuHash=0;HRESULT mapHr=E_FAIL;unsigned gpuNonzero=0;if(SUCCEEDED(createHr)){d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D>read;if(SUCCEEDED(dev->CreateTexture2D(&d,nullptr,&read))){c->CopyResource(read.Get(),motion.Get());D3D11_MAPPED_SUBRESOURCE m{};mapHr=c->Map(read.Get(),0,D3D11_MAP_READ,0,&m);if(SUCCEEDED(mapHr)){gpuHash=b160hash((const BYTE*)m.pData,m.RowPitch,w,h);for(UINT y=0;y<h;++y){auto*p=(const uint16_t*)((const BYTE*)m.pData+(size_t)y*m.RowPitch);for(UINT x=0;x<w;++x)if(p[x*2]||p[x*2+1])gpuNonzero++;}c->Unmap(read.Get(),0);}}}
 o<<"B171: MOTION_TEXTURE width="<<w<<", height="<<h<<", format="<<(unsigned)DXGI_FORMAT_R16G16_FLOAT<<", matrix_base=32, layout=directx_row_vector, depth=normal, valid_depth_pixels="<<st.valid<<", finite_motion_pixels="<<st.finite<<", onscreen_pixels="<<st.onscreen<<", nonzero_cpu_pixels="<<st.nonzero<<", mean_magnitude_px="<<(st.finite?st.sum/st.finite:0)<<", max_magnitude_px="<<st.maximum<<"\n";
 o<<"B171: PRIVATE_TEXTURE_VALIDATION create_hr=0x"<<std::hex<<(unsigned)createHr<<", srv_hr=0x"<<(unsigned)srvHr<<", readback_map_hr=0x"<<(unsigned)mapHr<<std::dec<<", cpu_hash="<<cpuHash<<", gpu_hash="<<gpuHash<<", hash_match="<<(cpuHash==gpuHash)<<", nonzero_gpu_pixels="<<gpuNonzero<<", gta_resource_modified=0\n";
 float firstView[16]{},firstProjection[16]{},secondView[16]{},secondProjection[16]{};
 auto makeConstants=&GhostingPrewarmConstants1;
 bool firstConstantsValid=makeConstants(previous,firstView,firstProjection),secondConstantsValid=makeConstants(current,secondView,secondProjection),constantsValid=firstConstantsValid&&secondConstantsValid;
 o<<"B171: FRAME_CONSTANTS_SOURCE sets=2, first_finite="<<(firstConstantsValid?1:0)<<", second_finite="<<(secondConstantsValid?1:0)<<", view_base=16, view_projection_base=32, row_major=1, jitter={0,0}, motion_scale={1,1}\n";
 HRESULT submitHr=(SUCCEEDED(createHr)&&SUCCEEDED(srvHr)&&SUCCEEDED(mapHr)&&cpuHash==gpuHash&&constantsValid)?B164SubmitFrameInputs(pixels.data(),w*4,firstDepthPixels.data(),w*4,depthPixels.data(),w*4,firstView,firstProjection,secondView,secondProjection,w,h,path):E_FAIL;o<<"B171: MOTION_BRIDGE_SUBMIT background=1, hr=0x"<<std::hex<<(unsigned)submitHr<<std::dec<<", present_thread_blocked=0\n";return FAILED(createHr)?createHr:FAILED(srvHr)?srvHr:FAILED(mapHr)?mapHr:submitHr;}
extern "C" __declspec(dllexport) HRESULT WINAPI B175nCheck170AnalyzeDiscovery(const B49OmEvent*om,UINT on,const B49PsEvent*ps,UINT pn,const B49DrawEvent*d,UINT dn,UINT split,const char*path){
 if(!om||!ps||!d||!path)return E_INVALIDARG;std::ofstream o(path,std::ios::app);if(!o)return E_FAIL;
 g_b175FastReady.store(false,std::memory_order_release);g_b175FastDepth.Reset();g_b175FastConstants.Reset();
 B160Candidate firstCandidate{},secondCandidate{};B160Reject firstReject{},secondReject{};
 unsigned da=b160draw(om,on,ps,pn,d,dn,0,split,nullptr,firstCandidate,firstReject,o);
 unsigned db=b160draw(om,on,ps,pn,d,dn,split,~0u,da?&firstCandidate:nullptr,secondCandidate,secondReject,o);
 B160Pipe a{},b{};b160pipe(da,a);b160pipe(db,b);
 ID3D11Buffer*ca=a.cb[1][1]?a.cb[1][1]:a.cb[0][1],*cb=b.cb[1][1]?b.cb[1][1]:b.cb[0][1];const Snapshot*sa=last_snapshot_before(ca,0,da),*sb=last_snapshot_before(cb,split,db);D3D11_BUFFER_DESC desc{};if(ca)ca->GetDesc(&desc);
 ComPtr<ID3D11Resource>fastDepthResource;const B49OmEvent*fastState=b160om_at(om,on,db);if(fastState&&fastState->dsv)fastState->dsv->GetResource(&fastDepthResource);ComPtr<ID3D11Texture2D>fastDepth;if(fastDepthResource)fastDepthResource.As(&fastDepth);
 D3D11_TEXTURE2D_DESC depthDesc{};if(fastDepth)fastDepth->GetDesc(&depthDesc);
 const bool sizeObserved=da&&db&&firstCandidate.width&&firstCandidate.height;
 const bool depthIdentityObserved=sizeObserved&&firstCandidate.depth_identity==secondCandidate.depth_identity;
 const bool markerIdentityObserved=sizeObserved&&firstCandidate.marker_identity==secondCandidate.marker_identity;
 const bool constantIdentityObserved=ca&&ca==cb;
 const int markerErrorX=sizeObserved?(int)b160marker_error(firstCandidate.marker_width,firstCandidate.width):-1;
 const int markerErrorY=sizeObserved?(int)b160marker_error(firstCandidate.marker_height,firstCandidate.height):-1;
 const bool snapshotsObserved=sa&&sb;
 const bool depthDescriptorObserved=fastDepth&&gta::SupportedDepthFormat(depthDesc.Format)&&depthDesc.SampleDesc.Count==1&&depthDesc.Width==firstCandidate.width&&depthDesc.Height==firstCandidate.height;
 const B49DrawEvent*firstDraw=b175_draw_at(d,dn,da),*secondDraw=b175_draw_at(d,dn,db);
 const bool drawSignatureStable=firstDraw&&secondDraw&&firstDraw->type==secondDraw->type&&firstDraw->a==secondDraw->a&&firstDraw->b==secondDraw->b&&firstDraw->c==secondDraw->c;
 const B49OmEvent*firstOm=b160om_at(om,on,da),*secondOm=b160om_at(om,on,db);ComPtr<ID3D11Resource>firstColorResource,secondColorResource;ComPtr<ID3D11Texture2D>firstColor,secondColor;
 if(firstOm&&firstOm->r[0])firstOm->r[0]->GetResource(&firstColorResource);if(firstColorResource)firstColorResource.As(&firstColor);
 if(secondOm&&secondOm->r[0])secondOm->r[0]->GetResource(&secondColorResource);if(secondColorResource)secondColorResource.As(&secondColor);
 D3D11_TEXTURE2D_DESC firstColorDesc{},secondColorDesc{};if(firstColor)firstColor->GetDesc(&firstColorDesc);if(secondColor)secondColor->GetDesc(&secondColorDesc);
 const bool firstColorValid=firstColor&&firstColorDesc.Width==firstCandidate.width&&firstColorDesc.Height==firstCandidate.height&&firstColorDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT&&firstColorDesc.SampleDesc.Count==1&&(firstColorDesc.BindFlags&D3D11_BIND_RENDER_TARGET);
 const bool secondColorValid=secondColor&&secondColorDesc.Width==secondCandidate.width&&secondColorDesc.Height==secondCandidate.height&&secondColorDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT&&secondColorDesc.SampleDesc.Count==1&&(secondColorDesc.BindFlags&D3D11_BIND_RENDER_TARGET);
 const bool colorPingPongStable=firstColorValid&&secondColorValid&&firstColor.Get()!=secondColor.Get();
 const bool exactInternalSize=sizeObserved&&firstCandidate.width>=640&&firstCandidate.height>=360&&firstCandidate.width<=3840&&firstCandidate.height<=2160;
 const bool selectionValid=exactInternalSize&&depthIdentityObserved&&markerIdentityObserved&&constantIdentityObserved&&snapshotsObserved&&depthDescriptorObserved&&colorPingPongStable&&drawSignatureStable&&markerErrorX<=2&&markerErrorY<=2&&g_cb_overflow.load()==0&&g_snapshot_overflow.load()==0;
 auto logTop=[&](const B160Reject&r){o<<"[";for(unsigned i=0;i<r.top_count;++i){if(i)o<<",";o<<r.top_width[i]<<"x"<<r.top_height[i];}o<<"]";};
 o<<"B174y: DYNAMIC_DISCOVERY_SELECTION first_draw="<<da<<", second_draw="<<db<<", selected_size="<<firstCandidate.width<<"x"<<firstCandidate.height<<", second_size="<<secondCandidate.width<<"x"<<secondCandidate.height<<", marker_size="<<firstCandidate.marker_width<<"x"<<firstCandidate.marker_height<<", marker_error_px={"<<markerErrorX<<","<<markerErrorY<<"}, depth_identity_observed="<<(depthIdentityObserved?1:0)<<", marker_identity_observed="<<(markerIdentityObserved?1:0)<<", constant_identity_observed="<<(constantIdentityObserved?1:0)<<", snapshots_observed="<<(snapshotsObserved?2:0)<<", selection_valid="<<(selectionValid?1:0)<<"\n";
 o<<"B174y: DYNAMIC_DISCOVERY_COUNTS first={inspected="<<firstReject.inspected<<",valid="<<firstReject.valid<<",rtv_format="<<firstReject.rtv_format<<",bounds="<<firstReject.bounds<<",samples="<<firstReject.samples<<",dsv_format="<<firstReject.dsv_format<<",dsv_size="<<firstReject.dsv_size<<",marker="<<firstReject.marker<<",identity="<<firstReject.identity<<",top=";logTop(firstReject);o<<"}, second={inspected="<<secondReject.inspected<<",valid="<<secondReject.valid<<",rtv_format="<<secondReject.rtv_format<<",bounds="<<secondReject.bounds<<",samples="<<secondReject.samples<<",dsv_format="<<secondReject.dsv_format<<",dsv_size="<<secondReject.dsv_size<<",marker="<<secondReject.marker<<",identity="<<secondReject.identity<<",top=";logTop(secondReject);o<<"}\n";
 o<<"B175n: INTERNAL_COLOR_RTV_DISCOVERY source=OM_RTV0, first_anchor_draw="<<da<<", second_anchor_draw="<<db<<", first_signature={"<<(firstDraw?firstDraw->type:0)<<","<<(firstDraw?firstDraw->a:0)<<","<<(firstDraw?firstDraw->b:0)<<","<<(firstDraw?firstDraw->c:0)<<"}, second_signature={"<<(secondDraw?secondDraw->type:0)<<","<<(secondDraw?secondDraw->a:0)<<","<<(secondDraw?secondDraw->b:0)<<","<<(secondDraw?secondDraw->c:0)<<"}, signature_stable="<<(drawSignatureStable?1:0)<<", first_desc={"<<firstColorDesc.Width<<"x"<<firstColorDesc.Height<<","<<(unsigned)firstColorDesc.Format<<","<<firstColorDesc.BindFlags<<"}, second_desc={"<<secondColorDesc.Width<<"x"<<secondColorDesc.Height<<","<<(unsigned)secondColorDesc.Format<<","<<secondColorDesc.BindFlags<<"}, distinct_cross_frame_ping_pong="<<(colorPingPongStable?1:0)<<", exact_anchor=selected_depth_constants_draw, valid="<<(colorPingPongStable&&drawSignatureStable?1:0)<<"\n";
 if(selectionValid){g_b160w=firstCandidate.width;g_b160h=firstCandidate.height;}
 HRESULT colorPrepareHr=E_FAIL;
 if(selectionValid){D3D11_BUFFER_DESC fastDesc{};cb->GetDesc(&fastDesc);g_b175FastConstantBytes=fastDesc.ByteWidth;g_b175FastConstants=cb;g_b175FastDepth=fastDepth;AcquireSRWLockExclusive(&g_b175FastLock);g_b175FastDrawType=secondDraw->type;g_b175FastDrawA=secondDraw->a;g_b175FastDrawB=secondDraw->b;g_b175FastDrawC=secondDraw->c;g_b175FastColorPair[0]=firstColor;g_b175FastColorPair[1]=secondColor;g_b175FastCurrentColor=g_b175FastColorPair[1];g_b175FastSnapshot.byte_count=(std::min)(sb->byte_count,kSnapshotBytes);g_b175FastSnapshot.resource=cb;std::memcpy(g_b175FastSnapshot.bytes,sb->bytes,g_b175FastSnapshot.byte_count);g_b175FastSnapshot.ready.store(1,std::memory_order_release);ReleaseSRWLockExclusive(&g_b175FastLock);colorPrepareHr=B175SetNativeDepthFormat(depthDesc.Format);if(SUCCEEDED(colorPrepareHr))colorPrepareHr=B175PrepareDiscoveredInternalColor(g_b175FastColorPair[0].Get(),path);g_b175FastReady.store(SUCCEEDED(colorPrepareHr),std::memory_order_release);}
 float firstView[16]{},firstProjection[16]{},secondView[16]{},secondProjection[16]{};
 auto makeConstants=&GhostingPrewarmConstants0;
 const bool constantsValid=sa&&sb&&makeConstants(sa,firstView,firstProjection)&&makeConstants(sb,secondView,secondProjection);
 std::vector<unsigned char> zeroMotion((size_t)g_b160w*g_b160h*4,0),zeroDepth((size_t)g_b160w*g_b160h*4,0);
 HRESULT prewarmHr=g_b175FastReady.load(std::memory_order_acquire)&&constantsValid?B164SubmitFrameInputs(zeroMotion.data(),g_b160w*4,zeroDepth.data(),g_b160w*4,zeroDepth.data(),g_b160w*4,firstView,firstProjection,secondView,secondProjection,g_b160w,g_b160h,path):E_FAIL;
 HRESULT mapHr=S_FALSE,firstMapHr=S_FALSE,motionHr=prewarmHr;
 // B330: retain the strongest exact GTA native motion candidate discovered during the same cross-frame probe.
 { struct C{ComPtr<ID3D11Texture2D> tex;unsigned binds=0;}; C best{};
   for(UINT i=0;i<pn;++i){if(!ps[i].ready.load(std::memory_order_acquire))continue;for(UINT x=0;x<ps[i].count;++x){auto*srv=ps[i].r[x];if(!srv)continue;ComPtr<ID3D11Resource>r;srv->GetResource(&r);ComPtr<ID3D11Texture2D>t;if(!r||FAILED(r.As(&t))||!t)continue;D3D11_TEXTURE2D_DESC md{};t->GetDesc(&md);if(md.Width!=960||md.Height!=540||md.Format!=DXGI_FORMAT_R16G16_FLOAT||md.SampleDesc.Count!=1)continue;unsigned binds=0;for(UINT j=0;j<pn;++j)if(ps[j].ready.load(std::memory_order_acquire))for(UINT y=0;y<ps[j].count;++y){auto*s2=ps[j].r[y];if(!s2)continue;ComPtr<ID3D11Resource>r2;s2->GetResource(&r2);if(r2.Get()==r.Get())++binds;}if(binds>best.binds){best.tex=t;best.binds=binds;}}}
   AcquireSRWLockExclusive(&g_b330NativeMotionLock);g_b330NativeMotionSource=best.tex;g_b330NativeMotionWidth=best.tex?960:0;g_b330NativeMotionHeight=best.tex?540:0;ReleaseSRWLockExclusive(&g_b330NativeMotionLock);
   o<<"B335: NATIVE_MOTION_REJECTED_CANDIDATE retained_for_diagnostics="<<(best.tex?1:0)<<", size="<<(best.tex?"960x540":"0x0")<<", format=R16G16_FLOAT, bind_events="<<best.binds<<", runtime_switch=0, rejection_reason=B334_scene_or_depth_like_values, fallback=reconstructed_motion\n"; }
 if(motionHr==S_OK)b172remember(sb);
 o<<"B174y: FAST_PATH_DISCOVERY_COMPLETE ready="<<(g_b175FastReady.load()?1:0)<<", selected_color_pair_retained="<<(g_b175FastColorPair[0]&&g_b175FastColorPair[1]?1:0)<<", selected_depth_retained="<<(g_b175FastDepth?1:0)<<", selected_constants_retained="<<(g_b175FastConstants?1:0)<<", selected_size="<<g_b160w<<"x"<<g_b160h<<", color_prepare_hr="<<(long)colorPrepareHr<<", dependent_prewarm_submit_hr="<<(long)prewarmHr<<"\n";
 const bool b297TransientSelectionMiss=!selectionValid&&exactInternalSize&&depthIdentityObserved&&markerIdentityObserved&&constantIdentityObserved&&snapshotsObserved&&depthDescriptorObserved&&drawSignatureStable;
 if(b297TransientSelectionMiss)o<<"B297: TRANSIENT_DISCOVERY_SAMPLE_REJECTED reason=selection_valid_0_with_required_depth_marker_constants_and_draw_signature_present, action=request_clean_cross_frame_retry, retryable=1, retained_resources=0\n";
 release_all();if(g_b175FastReady.load()&&SUCCEEDED(prewarmHr))return S_OK;if(b297TransientSelectionMiss)return E_PENDING;return E_FAIL;}

extern "C" __declspec(dllexport) ID3D11Texture2D* WINAPI B330AcquireNativeMotionSource(){AcquireSRWLockShared(&g_b330NativeMotionLock);auto*p=g_b330NativeMotionSource.Get();if(p)p->AddRef();ReleaseSRWLockShared(&g_b330NativeMotionLock);return p;}

static void drawTransport0(ID3D11DeviceContext*context,ID3D11Resource*selectedResource){auto***obj=reinterpret_cast<void***>(context);if(obj&&*obj){void**vt=*obj;vt[12]=(void*)&h_fast_draw_indexed;vt[13]=(void*)&h_fast_draw;g_b300DrawHooksReleased.store(false,std::memory_order_release);}}
static void drawTransport1(ID3D11DeviceContext*context,ID3D11Resource*selectedResource){auto***obj=reinterpret_cast<void***>(context);if(obj&&*obj){void**vt=*obj;vt[12]=(void*)o_fast_draw_indexed;vt[13]=(void*)o_fast_draw;g_b300DrawHooksReleased.store(true,std::memory_order_release);}}
static void drawTransport2(ID3D11DeviceContext*context,ID3D11Resource*selectedResource){auto***obj=reinterpret_cast<void***>(context);if(obj&&*obj&&selectedResource){void**vt=*obj;
   // B389 freeze-only: retire the B321/B322/B323 diagnostic profilers from live gameplay.
   // B391 freeze-only: retire synchronous per-reset/per-15-frame camera logging from the GTA Present hot path; functional camera/motion/reset behavior is unchanged.
   // Direct camera mode no longer needs Draw classification; restore those calls directly to GTA's original D3D11 methods.
   // Keep the existing functional Map/Unmap and OM hooks in place because they provide the selected camera write and live input identities.
   vt[12]=(void*)o_fast_draw_indexed;vt[13]=(void*)o_fast_draw;vt[20]=(void*)o_b321_draw_indexed_instanced;vt[21]=(void*)o_b321_draw_instanced;vt[41]=(void*)o_b321_dispatch;
   g_b321ProfilerActive.store(false,std::memory_order_release);g_b322ProfilerActive.store(false,std::memory_order_release);g_b323ProfilerActive.store(false,std::memory_order_release);g_b300DrawHooksReleased.store(true,std::memory_order_release);
   }}
static void drawTransport3(ID3D11DeviceContext*context,ID3D11Resource*selectedResource){auto***b497Obj=reinterpret_cast<void***>(context);if(b497Obj&&*b497Obj){void**vt=*b497Obj;vt[12]=(void*)&h_fast_draw_indexed;vt[13]=(void*)&h_fast_draw;g_b300DrawHooksReleased.store(false,std::memory_order_release);}}
extern "C" __declspec(dllexport) HRESULT WINAPI B175FastCaptureAtPresent(ID3D11DeviceContext*context,IDXGISwapChain*swap,unsigned sourceId,const char*path)
{
 LARGE_INTEGER b306Begin{},b306AfterPreamble{},b306AfterCamera{},b306AfterDepth{},b306AfterQueue{};QueryPerformanceCounter(&b306Begin);
 auto b306ms=[](const LARGE_INTEGER&a,const LARGE_INTEGER&b){static LARGE_INTEGER f=[](){LARGE_INTEGER x{};QueryPerformanceFrequency(&x);return x;}();return f.QuadPart?((double)(b.QuadPart-a.QuadPart)*1000.0/(double)f.QuadPart):0.0;};
 if(!GhostingVerifyRuntime())return E_NOINTERFACE;
 if(!context||!swap||!path)return E_INVALIDARG;
 // B324 frame boundary: all selected Unmaps until the next real GTA Present share this epoch.
 g_b324SourceFrameEpoch.fetch_add(1,std::memory_order_acq_rel);
 if(!g_b175FastHookActive.load(std::memory_order_acquire)||!g_b175FastDepth||!g_b175FastConstants||!g_b175FastColorPair[0]||!g_b175FastColorPair[1]){B175DropFastCapture(sourceId,"fast_resources_temporarily_unavailable");return E_FAIL;}
 if(!B172WantsCapture(sourceId))return S_FALSE;
	// B399: hard native bypass after stale FG. Admission only reopens for one bounded three-frame re-arm burst after a native-only cooldown.
	if(!B396AllowFgSubmission())return S_FALSE;
 D3D11_TEXTURE2D_DESC depthDesc{};g_b175FastDepth->GetDesc(&depthDesc);D3D11_BUFFER_DESC constantDesc{};g_b175FastConstants->GetDesc(&constantDesc);
 const unsigned long long serial=g_b175FastSnapshotSerial.load(std::memory_order_acquire),colorSerial=g_b175FastColorSerial.load(std::memory_order_acquire);if(!colorSerial||colorSerial==g_b175FastLastColorSerial){B175DropFastCapture(sourceId,"no_current_internal_color_at_exact_draw");return E_PENDING;}g_b175FastLastColorSerial=colorSerial;if(serial==g_b175FastLastCaptureSerial)++g_b175FastStalePresents;else{g_b175FastLastCaptureSerial=serial;g_b175FastStalePresents=0;}
 const ULONGLONG updateAgeMs=GetTickCount64()-g_b175FastLastUpdateTick.load(std::memory_order_acquire);
 const bool descriptorsMatch=depthDesc.Width==g_b160w&&depthDesc.Height==g_b160h&&gta::SupportedDepthFormat(depthDesc.Format)&&constantDesc.ByteWidth==g_b175FastConstantBytes;
 if(!descriptorsMatch){B175DropFastCapture(sourceId,"resource_descriptor_validation_failed");std::ofstream out(path,std::ios::app);out<<"B174y: FAST_CAPTURE_RESOURCE_VALIDATION_FAILURE depth_match="<<(depthDesc.Width==g_b160w&&depthDesc.Height==g_b160h&&gta::SupportedDepthFormat(depthDesc.Format)?1:0)<<", constants_match="<<(constantDesc.ByteWidth==g_b175FastConstantBytes?1:0)<<", transient=1, safe_stop=0\n";out.flush();return E_FAIL;}
 const bool b303DirectCameraEstablished=g_b300DirectMode.load(std::memory_order_acquire)&&g_b301DirectCbResource.load(std::memory_order_acquire)!=nullptr;
 const bool b303LegacyConstantStale=g_b175FastStalePresents>=120||updateAgeMs>=5000;
 if(b303LegacyConstantStale&&!b303DirectCameraEstablished){B175DropFastCapture(sourceId,"identified_resource_identity_stale");std::ofstream out(path,std::ios::app);out<<"B174y: IDENTIFIED_RESOURCE_IDENTITY_STALE descriptor_match=1, stale_presents="<<g_b175FastStalePresents<<", update_age_ms="<<updateAgeMs<<", hr=DXGI_ERROR_INVALID_CALL, safe_stop=1, rediscovery=0\n";out.flush();return DXGI_ERROR_INVALID_CALL;}
 if(b303LegacyConstantStale&&b303DirectCameraEstablished&&!g_b303StaleDiscoveryConstantBypassLogged.exchange(true,std::memory_order_acq_rel)){std::ofstream out(path,std::ios::app);out<<"B303: STALE_DISCOVERY_CONSTANT_IGNORED source_frame_id="<<sourceId<<", stale_presents="<<g_b175FastStalePresents<<", update_age_ms="<<updateAgeMs<<", direct_camera_cb_locked=1, selected_camera_resource="<<g_b301DirectCbResource.load(std::memory_order_acquire)<<", action=continue_live_xess_xefg_without_safe_stop, rediscovery_required=0, b301_fast_bypass_unchanged=1, b276_x4_unchanged=1\n";out.flush();}
 Snapshot current{};ComPtr<ID3D11Texture2D>currentColor;AcquireSRWLockShared(&g_b175FastLock);current.byte_count=g_b175FastSnapshot.byte_count;current.resource=g_b175FastSnapshot.resource;std::memcpy(current.bytes,g_b175FastSnapshot.bytes,current.byte_count);current.ready.store(g_b175FastSnapshot.ready.load(std::memory_order_acquire),std::memory_order_release);currentColor=g_b175FastCurrentColor;ReleaseSRWLockShared(&g_b175FastLock);
 D3D11_TEXTURE2D_DESC colorDesc{};if(currentColor)currentColor->GetDesc(&colorDesc);const bool colorValid=currentColor&&b175_fast_color_member(currentColor.Get())&&colorDesc.Width==g_b160w&&colorDesc.Height==g_b160h&&colorDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT&&colorDesc.SampleDesc.Count==1;if(!colorValid){B175DropFastCapture(sourceId,"internal_color_identity_or_descriptor_mismatch");return DXGI_ERROR_INVALID_CALL;}
 if(!current.ready.load(std::memory_order_acquire)||current.byte_count<192){B175DropNoConstantSnapshot(sourceId);std::ofstream out(path,std::ios::app);out<<"B175: FAST_CAPTURE_PENDING source_frame_id="<<sourceId<<", snapshot_ready="<<(current.ready.load()?1:0)<<", snapshot_bytes="<<current.byte_count<<", disposition_logged=1, hr="<<(long)E_PENDING<<"\n";out.flush();return E_PENDING;}
 if(!B172BeginFreshFrame(sourceId))return E_FAIL;
 QueryPerformanceCounter(&b306AfterPreamble);
 static const bool transportBound=[](){CameraTransport t{&drawTransport0,&drawTransport1,&drawTransport2,&drawTransport3};GhostingBindCameraTransport(t);return true;}();
 CameraResult camera{};GhostingSelectCamera(context,sourceId,path,current,serial,camera);
 const bool valid=camera.valid;const auto&currentVpF=camera.vp;const auto&currentInverseVpF=camera.inverseVp;const auto&currentViewF=camera.view;const auto&currentProjectionF=camera.projection;b306AfterCamera=camera.afterCamera;
 HRESULT hr=valid?B173SubmitGpuFrame(context,g_b175FastDepth.Get(),currentVpF,currentInverseVpF,currentViewF,currentProjectionF,g_b160w,g_b160h,path):E_FAIL;
 QueryPerformanceCounter(&b306AfterDepth);
 if(FAILED(hr)){B172AbortFreshFrame(sourceId,valid?"depth_submission_failed":"invalid_frame_constants",hr);return hr;}
 hr=B171PresentLiveFrame(context,swap,currentColor.Get(),path);
 QueryPerformanceCounter(&b306AfterQueue);
 const unsigned captures=g_b175FastCaptureCount.fetch_add(1,std::memory_order_acq_rel)+1;
 const double b306TotalMs=b306ms(b306Begin,b306AfterQueue);if(captures<=3||(captures%600)==0){std::ofstream t(path,std::ios::app);t<<"B306: GTA_PRESENT_HOOK_TIMING source_frame_id="<<sourceId<<", capture_count="<<captures<<", preamble_ms="<<b306ms(b306Begin,b306AfterPreamble)<<", camera_ms="<<b306ms(b306AfterPreamble,b306AfterCamera)<<", depth_motion_submit_ms="<<b306ms(b306AfterCamera,b306AfterDepth)<<", color_xess_queue_ms="<<b306ms(b306AfterDepth,b306AfterQueue)<<", hud_perf_sampling_ms=0, hud_perf_sampling_thread=worker, total_helper_ms="<<b306TotalMs<<", target_below_2ms="<<(b306TotalMs<2.0?1:0)<<"\n";t.flush();}
 if(captures==1||captures%600==0){std::ofstream out(path,std::ios::app);out<<"B175n: FAST_PATH_CAPTURE source_frame_id="<<sourceId<<", capture_count="<<captures<<", hook_install_count="<<g_b175FastHookInstallCount.load()<<", hook_active_ms="<<(GetTickCount64()-g_b175FastHookStart)<<", persistent_om_rtv0=1, persistent_draw_signature={"<<g_b175FastDrawType<<","<<g_b175FastDrawA<<","<<g_b175FastDrawB<<","<<g_b175FastDrawC<<"}, current_color="<<colorDesc.Width<<"x"<<colorDesc.Height<<"_RGBA16F, candidate_scans=0, snapshot_arrays=0, hr="<<(long)hr<<"\n";}
 return hr;
}


