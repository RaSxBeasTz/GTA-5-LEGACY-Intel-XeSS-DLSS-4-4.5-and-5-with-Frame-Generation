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
using Microsoft::WRL::ComPtr;
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
extern "C" void WINAPI B297ResetCameraHistory();
extern "C" void WINAPI B407MarkUnsafeCameraFrame();
extern "C" void WINAPI B408MarkSafeCameraFrame();
extern "C" void WINAPI B345ApplyCameraZeroMotionSafety(BOOL);

extern "C" HRESULT WINAPI B76LogSelectedDraws(unsigned firstSequence,unsigned secondSequence,const char* logPath);

extern "C" __declspec(dllexport) const char* WINAPI B63bGetBuildIdentity()
{
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

constexpr unsigned kRtvs=4,kSrvs=16,kPsCbs=14,kCbEvents=4096,kMapEvents=16384,kKnownCbs=128,kSnapshots=8192,kSnapshotBytes=352;
struct B49OmEvent{std::atomic<unsigned> ready;ID3D11RenderTargetView* r[kRtvs];ID3D11DepthStencilView* dsv;unsigned ordinal,numviews;};
struct B49PsEvent{std::atomic<unsigned> ready;ID3D11ShaderResourceView* r[kSrvs];unsigned ordinal,start,count;};
struct B49DrawEvent{std::atomic<unsigned> ready;unsigned ordinal,type,a,b;int c;};
struct CbEvent{std::atomic<unsigned> ready{0};unsigned ordinal{},stage{},start{},count{};ID3D11Buffer* buffers[kPsCbs]{};};
struct MapEvent{std::atomic<unsigned> ready{0};unsigned ordinal{},type{},subresource{},map_type{},map_flags{};HRESULT hr{};ID3D11Resource* resource{};};
struct Snapshot{std::atomic<unsigned> ready{0};unsigned sequence{},byte_count{};ID3D11Resource* resource{};unsigned char bytes[kSnapshotBytes]{};};
static CbEvent g_cb_events[kCbEvents];
static MapEvent g_map_events[kMapEvents];
static Snapshot g_snapshots[kSnapshots];
static std::atomic<ID3D11Buffer*> g_known_cbs[kKnownCbs];
static std::atomic<unsigned> g_known_cb_bytes[kKnownCbs];
static std::atomic<unsigned> g_known_cb_count{0},g_cb_next{0},g_cb_overflow{0},g_map_next{0},g_map_overflow{0},g_snapshot_next{0},g_snapshot_overflow{0};
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
static ComPtr<ID3D11Buffer> g_b175FastConstants;
static unsigned g_b175FastConstantBytes{};
static Snapshot g_b175FastSnapshot;
static std::atomic<bool> g_b175FastReady{false},g_b175FastHookActive{false},g_b175nPreCoreProbeReady{false};
static std::atomic<unsigned> g_b175FastHookInstallCount{0},g_b175FastCaptureCount{0};
static std::atomic<unsigned long long> g_b175FastSnapshotSerial{0};
static std::atomic<unsigned long long> g_b175FastColorSerial{0};
static std::atomic<unsigned long long> g_b175FastLastUpdateTick{0};
// B287 keeps the proven B286 DrawIndexed interception and adds diagnostic-only main-3D rage_matrices frame capture.
// The B276 XeSS/XeFG runtime remains frozen and does not consume these matrices.
static SRWLOCK g_b280CandidateLock=SRWLOCK_INIT;
static unsigned char g_b280PreviousBytes[kKnownCbs][kSnapshotBytes]{};
static unsigned g_b280PreviousByteCount[kKnownCbs]{};
static bool g_b280PreviousValid[kKnownCbs]{};
struct B281CandidateWindow{unsigned cbIndex{},byteOffset{},byteWidth{},updates{};double maxDelta{},rmsDelta{};bool selected{};float values[16]{};};
static B281CandidateWindow g_b280CandidateWindows[kKnownCbs]{};
static unsigned g_b280CandidateUpdateSerial{};
static int b280_known_cb_index(ID3D11Resource*r){unsigned n=g_known_cb_count.load(std::memory_order_acquire);if(n>kKnownCbs)n=kKnownCbs;for(unsigned i=0;i<n;++i)if((ID3D11Resource*)g_known_cbs[i].load(std::memory_order_acquire)==r)return (int)i;return -1;}
static std::atomic<unsigned> g_b282TargetWriteCount[kKnownCbs]{};
static std::atomic<unsigned> g_b282LastBindStage[kKnownCbs]{};
static std::atomic<unsigned> g_b282LastBindSlot[kKnownCbs]{};
// B284: capture which 256-byte constant buffer is actually bound to the vertex shader at the exact validated GTA draw.
static SRWLOCK g_b284VsDrawLock=SRWLOCK_INIT;
static ComPtr<ID3D11Buffer> g_b284VsDrawCb;
static ComPtr<ID3D11VertexShader> g_b284VsDrawShader;
static unsigned g_b284VsDrawSlot=~0u;
static std::atomic<unsigned long long> g_b284VsDrawSerial{0};
static std::atomic<unsigned long long> g_b285GeometryDraws{0},g_b285RageDraws{0},g_b285RejectedScreenDraws{0};
// B286 compatibility probe remains available. B287 adds exact main-pass matrix accumulation without touching runtime inputs.
static SRWLOCK g_b286ProbeLock=SRWLOCK_INIT;
static std::atomic<unsigned long long> g_b286IndexedDraws{0},g_b286ProbeSamples{0};
static ComPtr<ID3D11VertexShader> g_b286LastVs;
static unsigned g_b286LastIndexCount{},g_b286LastStartIndex{};
static int g_b286LastBaseVertex{};
static unsigned g_b286LastRtvW{},g_b286LastRtvH{},g_b286LastRtvFormat{};
static unsigned g_b286LastDsvW{},g_b286LastDsvH{},g_b286LastDsvFormat{};
static unsigned g_b286LastCbMask{};
static unsigned g_b286LastCbBytes[kPsCbs]{};
static bool g_b286LastFastColorMatch{},g_b286LastFastDepthMatch{};
// B287: latest exact 256-byte rage_matrices write plus per-frame main-3D accumulation.
static SRWLOCK g_b287LatestLock=SRWLOCK_INIT;
static unsigned char g_b287Latest256[kKnownCbs][256]{};
static bool g_b287LatestValid[kKnownCbs]{};
static unsigned long long g_b287LatestWriteSerial[kKnownCbs]{};
static SRWLOCK g_b287FrameLock=SRWLOCK_INIT;
static unsigned long long g_b287MainPassDraws{},g_b287QualifyingDraws{},g_b287RejectedScreenDraws{};
static unsigned g_b287LastCbIndex=~0u,g_b287LastIndexCount{};
static ComPtr<ID3D11VertexShader> g_b287LastVs;
static unsigned long long g_b287LastWriteSerial{};
static double g_b287SumWorld[16]{},g_b287SumWorldView[16]{},g_b287SumWorldViewProj[16]{},g_b287SumViewInverse[16]{};
static double g_b291SumDirectView[16]{},g_b291SumDirectProjection[16]{};
static unsigned long long g_b291DirectDeriveDraws=0,g_b291DirectRejectDraws=0;
// B300: after a stable gameplay-camera CB is acquired, stop classifying DrawIndexed calls.
// Map/Unmap keeps a tiny ring of raw 256-byte writes for the selected CB; Present chooses the
// candidate continuous with the last accepted gameplay camera. This removes the hot DrawIndexed
// COM-query path while retaining B299/B298 safety gates.
constexpr unsigned kB300DirectWriteRing=64;
struct B300RawCameraWrite{unsigned char bytes[256]{};unsigned long long serial{};};
static SRWLOCK g_b300DirectWriteLock=SRWLOCK_INIT;
static B300RawCameraWrite g_b300DirectWrites[kB300DirectWriteRing]{};
static unsigned g_b300DirectWriteHead=0,g_b300DirectWriteCount=0;
// B445: preserve the exact bounded selected-camera write across the capture->Present handoff.
// h_unmap is already limited by B324/B339 to at most two eligible selected-resource captures per source frame,
// so this adds no Draw scanning and no unbounded render-thread work.
static B300RawCameraWrite g_b445LatestSelectedSnapshot{};
static ID3D11Resource* g_b445LatestSelectedResource=nullptr;
static unsigned long long g_b445LatestSelectedSerial=0;
static std::atomic<bool> g_b300DirectMode{false},g_b300DrawHooksReleased{false};
static std::atomic<int> g_b300DirectCbIndex{-1};
static std::atomic<ID3D11Resource*> g_b301DirectCbResource{nullptr};
// B430: quarantine the exact fresh-but-static direct resource during the one-shot reacquisition.
static std::atomic<ID3D11Resource*> g_b430RejectedStaticCameraResource{nullptr};
static std::atomic<bool> g_b430ResponsiveReacquireActive{false};
// B431: while B430 quarantine is active, widen DrawIndexed camera classification briefly and
// skip the quarantined resource so alternate 256-byte camera CBs can win the selector.
static std::atomic<unsigned long long> g_b431RejectedClassifierDraws{0};
// B432: one-shot authorization for the final submit guard after a different, responsive camera
// resource survives the B430 quarantine and three-frame B300 seed confirmation.
static std::atomic<bool> g_b432ValidatedReacquireBaselinePending{false};
// B434: one-shot handoff from the validated B433 final-camera commit to the motion/history consumer.
// This aligns only the deliberate reacquire transition; ordinary B329 discontinuity protection remains unchanged.
static std::atomic<bool> g_b434ValidatedBaselineCommitPending{false};
extern "C" __declspec(dllexport) BOOL WINAPI B434ConsumeValidatedBaselineCommit(){return g_b434ValidatedBaselineCommitPending.exchange(false,std::memory_order_acq_rel)?TRUE:FALSE;}
extern "C" unsigned WINAPI B440InvalidMotionCameraStreak();
extern "C" __declspec(dllexport) BOOL WINAPI B306DirectFastPathHealthy(){return g_b300DirectMode.load(std::memory_order_acquire)&&g_b301DirectCbResource.load(std::memory_order_acquire)!=nullptr&&g_b175FastHookActive.load(std::memory_order_acquire)?TRUE:FALSE;}
static std::atomic<unsigned long long> g_b301DirectWriteSerial{0};
// B309: selected-camera writes are deduplicated by the 64-byte gViewInverse block at +192.
// Only unique/periodic candidates enter the bounded ring; matrix validation stays in Present.
static std::atomic<unsigned long long> g_b309DirectGeneration{1};
static std::atomic<unsigned long long> g_b309SelectedUnmaps{0},g_b309DuplicateSkips{0},g_b309CandidatesCaptured{0};
// B456 diagnostic-only: prove whether the locked 256-byte selected resource continues to receive
// advancing Map/Unmap writes after B437 loses camera validity. Hot-path work is atomics/byte compares only;
// all file logging remains on the existing sparse source-frame path.
static std::atomic<unsigned long long> g_b456SelectedMaps{0},g_b456SelectedUnmaps{0},g_b456FullTupleChanges{0},g_b456ViewInverseChanges{0},g_b456PublishedWrites{0};
static unsigned long long b475_fingerprint256(const unsigned char* b);
// B489 diagnostic-only: correlate the exact B445 authoritative handoff object at publish and consume.
// No per-Unmap hashing/timing is performed. The exact 256-byte snapshots are copied only on the existing
// bounded B445 publish/consume path; fingerprints are calculated later at the source-frame capture boundary.
struct B489AuthoritativeHandoffSnapshot{unsigned long long publishedSerial{};unsigned publishedSourceFrame{};unsigned long long consumedSerial{};unsigned consumedSourceFrame{};unsigned char publishedBytes[256]{};unsigned char consumedBytes[256]{};BOOL publishedValid{};BOOL consumedValid{};};
static SRWLOCK g_b489AuthoritativeLock=SRWLOCK_INIT;
static B489AuthoritativeHandoffSnapshot g_b489Authoritative{};
static void b489_record_authoritative_publish(unsigned long long serial,unsigned sourceFrame,const unsigned char*bytes){if(!serial||!bytes)return;AcquireSRWLockExclusive(&g_b489AuthoritativeLock);g_b489Authoritative.publishedSerial=serial;g_b489Authoritative.publishedSourceFrame=sourceFrame;std::memcpy(g_b489Authoritative.publishedBytes,bytes,256);g_b489Authoritative.publishedValid=TRUE;ReleaseSRWLockExclusive(&g_b489AuthoritativeLock);}
static void b489_record_authoritative_consume(unsigned long long serial,unsigned sourceFrame,const unsigned char*bytes){if(!serial||!bytes)return;AcquireSRWLockExclusive(&g_b489AuthoritativeLock);g_b489Authoritative.consumedSerial=serial;g_b489Authoritative.consumedSourceFrame=sourceFrame;std::memcpy(g_b489Authoritative.consumedBytes,bytes,256);g_b489Authoritative.consumedValid=TRUE;ReleaseSRWLockExclusive(&g_b489AuthoritativeLock);}
extern "C" __declspec(dllexport) BOOL WINAPI B489GetAuthoritativeHandoff(B489AuthoritativeHandoffSnapshot*out){if(!out)return FALSE;AcquireSRWLockShared(&g_b489AuthoritativeLock);*out=g_b489Authoritative;ReleaseSRWLockShared(&g_b489AuthoritativeLock);return (out->publishedValid||out->consumedValid)?TRUE:FALSE;}
static unsigned long long b475_fingerprint256(const unsigned char* b);
// B484 diagnostic-only: bounded live Map/Unmap watchlist for already-known medium constant buffers.
// B481 proved there is only one known 256-byte CB, so B484 watches at most four pre-existing
// 256..512-byte non-selected CBs. No Draw hooks are reopened and camera authority is never changed.
constexpr unsigned kB484WatchSlots=4;
static std::atomic<ID3D11Resource*> g_b484WatchResource[kB484WatchSlots]{};
static std::atomic<unsigned> g_b484WatchBytes[kB484WatchSlots]{};
static std::atomic<unsigned long long> g_b484WatchUnmaps[kB484WatchSlots]{},g_b484WatchChanges[kB484WatchSlots]{},g_b484WatchLatestFp[kB484WatchSlots]{};
static SRWLOCK g_b484WatchLock=SRWLOCK_INIT;
static unsigned char g_b484WatchLatest[kB484WatchSlots][256]{};
static bool g_b484WatchValid[kB484WatchSlots]{};
static int b484_watch_slot(ID3D11Resource*r){if(!r)return -1;for(unsigned i=0;i<kB484WatchSlots;++i)if(g_b484WatchResource[i].load(std::memory_order_acquire)==r)return (int)i;return -1;}
static void b484_configure_watchlist(ID3D11Resource*selected){
 for(unsigned i=0;i<kB484WatchSlots;++i){g_b484WatchResource[i].store(nullptr,std::memory_order_release);g_b484WatchBytes[i].store(0,std::memory_order_release);g_b484WatchUnmaps[i].store(0,std::memory_order_relaxed);g_b484WatchChanges[i].store(0,std::memory_order_relaxed);g_b484WatchLatestFp[i].store(0,std::memory_order_relaxed);g_b484WatchValid[i]=false;}
 unsigned out=0,n=g_known_cb_count.load(std::memory_order_acquire);if(n>kKnownCbs)n=kKnownCbs;
 for(unsigned i=0;i<n&&out<kB484WatchSlots;++i){ID3D11Resource*r=(ID3D11Resource*)g_known_cbs[i].load(std::memory_order_acquire);const unsigned bytes=g_known_cb_bytes[i].load(std::memory_order_acquire);if(!r||r==selected||bytes<256||bytes>512)continue;g_b484WatchBytes[out].store(bytes,std::memory_order_release);g_b484WatchResource[out].store(r,std::memory_order_release);++out;}
}
struct B485MatrixProbe{unsigned finiteMatrices{};unsigned affineLike{};unsigned invertible{};double bestInverseResidual{1.0e30};int bestInverseA{-1};int bestInverseB{-1};double bestDetAbs{};};
static bool b485_finite16(const float*m){for(unsigned i=0;i<16;++i)if(!std::isfinite((double)m[i])||std::fabs((double)m[i])>10000000.0)return false;return true;}
static double b485_det4(const float*m){
 const double a=m[0],b=m[1],c=m[2],d=m[3],e=m[4],f=m[5],g=m[6],h=m[7],i=m[8],j=m[9],k=m[10],l=m[11],q=m[12],r=m[13],t=m[14],u=m[15];
 return a*(f*(k*u-l*t)-g*(j*u-l*r)+h*(j*t-k*r))-b*(e*(k*u-l*t)-g*(i*u-l*q)+h*(i*t-k*q))+c*(e*(j*u-l*r)-f*(i*u-l*q)+h*(i*r-j*q))-d*(e*(j*t-k*r)-f*(i*t-k*q)+g*(i*r-j*q));
}
static double b485_inverse_pair_residual(const float*a,const float*b){
 double worst=0.0;for(int row=0;row<4;++row)for(int col=0;col<4;++col){double v=0.0;for(int k=0;k<4;++k)v+=(double)a[row*4+k]*(double)b[k*4+col];const double want=(row==col)?1.0:0.0;worst=(std::max)(worst,std::fabs(v-want));}return worst;
}
static B485MatrixProbe b485_probe_first256(const unsigned char*raw){
 B485MatrixProbe o{};const float*f=(const float*)raw;
 for(int a=0;a<4;++a){const float*m=f+a*16;if(!b485_finite16(m))continue;++o.finiteMatrices;const double det=std::fabs(b485_det4(m));o.bestDetAbs=(std::max)(o.bestDetAbs,det);if(det>1.0e-10)++o.invertible;const double affineRow=std::fabs((double)m[3])+std::fabs((double)m[7])+std::fabs((double)m[11])+std::fabs((double)m[15]-1.0);const double affineCol=std::fabs((double)m[12])+std::fabs((double)m[13])+std::fabs((double)m[14])+std::fabs((double)m[15]-1.0);if((std::min)(affineRow,affineCol)<0.05)++o.affineLike;}
 for(int a=0;a<4;++a){const float*ma=f+a*16;if(!b485_finite16(ma))continue;for(int b=a+1;b<4;++b){const float*mb=f+b*16;if(!b485_finite16(mb))continue;const double ab=b485_inverse_pair_residual(ma,mb),ba=b485_inverse_pair_residual(mb,ma),res=(std::max)(ab,ba);if(res<o.bestInverseResidual){o.bestInverseResidual=res;o.bestInverseA=a;o.bestInverseB=b;}}}
 return o;
}
static void b484_capture_watch(int slot,const void*p,unsigned bytes){
 if(slot<0||slot>=(int)kB484WatchSlots||!p||bytes<256)return;const unsigned u=(unsigned)slot;g_b484WatchUnmaps[u].fetch_add(1,std::memory_order_relaxed);
 const unsigned char*raw=(const unsigned char*)p;const unsigned long long fp=b475_fingerprint256(raw);bool changed=false;
 AcquireSRWLockExclusive(&g_b484WatchLock);changed=!g_b484WatchValid[u]||std::memcmp(g_b484WatchLatest[u],raw,256)!=0;if(changed){std::memcpy(g_b484WatchLatest[u],raw,256);g_b484WatchValid[u]=true;}ReleaseSRWLockExclusive(&g_b484WatchLock);
 if(changed)g_b484WatchChanges[u].fetch_add(1,std::memory_order_relaxed);g_b484WatchLatestFp[u].store(fp,std::memory_order_release);
}

// B475 diagnostic-only: identify which 16-byte lanes inside the already-selected 256-byte resource actually change. No camera selection or runtime behavior changes.
static std::atomic<unsigned> g_b475ChangedLaneMask{0};
static std::atomic<unsigned long long> g_b475ChangedWrites{0},g_b475LatestChangedSnapshotFp{0};
// B479 diagnostic-only: compare the live selected-resource Unmap stream with the legacy candidate-table snapshot.
static std::atomic<unsigned long long> g_b479LiveSelectedUnmaps{0},g_b479LiveChangedSnapshots{0},g_b479LatestLiveSnapshotFp{0},g_b479PreviousLiveSnapshotFp{0};
static unsigned long long b475_fingerprint256(const unsigned char* b){unsigned long long h=1469598103934665603ULL;for(unsigned i=0;i<256;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}return h;} 
// B324: gate expensive selected-camera Unmap processing to one eligible capture per real source-frame interval.
// The epoch advances in B175FastCaptureAtPresent; duplicate selected Unmaps bypass directly to original D3D11.
static std::atomic<unsigned long long> g_b324SourceFrameEpoch{1};
static std::atomic<unsigned long long> g_b324FirstEligibleCaptures{0},g_b324DuplicateSelectedUnmapBypasses{0};
static thread_local unsigned long long t_b324CapturedEpoch=0;
static thread_local unsigned long long t_b339SecondChanceEpoch=0;
static thread_local float t_b339FirstViewInverse[16]{};
static thread_local bool t_b339FirstViewInverseValid=false;
static thread_local bool t_b339SecondChanceCaptured=false;
static std::atomic<unsigned long long> g_b339SecondChanceCaptures{0},g_b339SecondChanceBypasses{0};
static thread_local unsigned long long t_b309Generation=0,t_b309SelectedOrdinal=0;
static thread_local unsigned char t_b309LastViewInverseSignature[64]{};
static thread_local bool t_b309LastViewInverseValid=false;
// B446: avoid taking the camera SRW lock hundreds of times per source frame for byte-identical selected-CB writes.
static thread_local unsigned char t_b446LastFullSnapshot[256]{};
static thread_local bool t_b446LastFullSnapshotValid=false;
static std::atomic<bool> g_b303StaleDiscoveryConstantBypassLogged{false};
constexpr unsigned kB292MaxFamilies=32,kB293MaxClustersPerFamily=8;
struct B293ClusterAccum{unsigned long long draws{};unsigned lastIndexCount{},cbIndex{~0u};unsigned long long writeSerial{};double sumView[16]{},sumProjection[16]{};};
struct B292FamilyAccum{std::uintptr_t vsKey{};unsigned long long draws{};unsigned lastIndexCount{},cbIndex{~0u};unsigned long long writeSerial{};double sumView[16]{},sumProjection[16]{};B293ClusterAccum clusters[kB293MaxClustersPerFamily]{};unsigned clusterCount{},clusterOverflow{};};
static B292FamilyAccum g_b292Families[kB292MaxFamilies]{};
static unsigned g_b292FamilyCount=0,g_b292FamilyOverflow=0;
static bool b287_inverse4(const double in[16],double out[16]){double q[4][8]{};for(int r=0;r<4;++r){for(int c=0;c<4;++c)q[r][c]=in[r*4+c];q[r][r+4]=1.0;}for(int c=0;c<4;++c){int p=c;for(int r=c+1;r<4;++r)if(std::fabs(q[r][c])>std::fabs(q[p][c]))p=r;if(std::fabs(q[p][c])<1e-12)return false;if(p!=c)for(int k=0;k<8;++k)std::swap(q[p][k],q[c][k]);double v=q[c][c];for(int k=0;k<8;++k)q[c][k]/=v;for(int r=0;r<4;++r)if(r!=c){v=q[r][c];for(int k=0;k<8;++k)q[r][k]-=v*q[c][k];}}for(int r=0;r<4;++r)for(int c=0;c<4;++c)out[r*4+c]=q[r][c+4];return true;}
static void b287_mul4(const double a[16],const double b[16],double out[16]){for(int r=0;r<4;++r)for(int c=0;c<4;++c){double v=0;for(int k=0;k<4;++k)v+=a[r*4+k]*b[k*4+c];out[r*4+c]=v;}}
static bool b287_identity4d(const double*m){double e=0;for(int r=0;r<4;++r)for(int c=0;c<4;++c)e=(std::max)(e,std::fabs(m[r*4+c]-(r==c?1.0:0.0)));return e<1.0e-4;}
static void b309_capture_direct_camera_write_if_needed(ID3D11Resource*r,const void*p,unsigned bytes){
 if(!r||!p||bytes<256||r!=g_b301DirectCbResource.load(std::memory_order_acquire))return;
 g_b456SelectedUnmaps.fetch_add(1,std::memory_order_relaxed);
 const unsigned long long generation=g_b309DirectGeneration.load(std::memory_order_acquire);
 if(t_b309Generation!=generation){t_b309Generation=generation;t_b309SelectedOrdinal=0;t_b309LastViewInverseValid=false;t_b446LastFullSnapshotValid=false;}
 const unsigned long long ordinal=++t_b309SelectedOrdinal;g_b309SelectedUnmaps.fetch_add(1,std::memory_order_relaxed);
 const auto*raw=static_cast<const unsigned char*>(p);const unsigned char*signature=raw+192;
 // B479: fingerprint exact live selected-resource bytes before B446/B309 dedup; diagnostics only.
 const unsigned long long b479LiveFp=b475_fingerprint256(raw);
 const unsigned long long b479PriorLiveFp=g_b479LatestLiveSnapshotFp.exchange(b479LiveFp,std::memory_order_acq_rel);
 g_b479PreviousLiveSnapshotFp.store(b479PriorLiveFp,std::memory_order_relaxed);
 g_b479LiveSelectedUnmaps.fetch_add(1,std::memory_order_relaxed);
 if(b479PriorLiveFp!=0&&b479PriorLiveFp!=b479LiveFp)g_b479LiveChangedSnapshots.fetch_add(1,std::memory_order_relaxed);
 // B446: preserve a new atomic 256-byte selected-camera snapshot only when the FULL tuple bytes change.
 // B445 proved that copying/locking on every selected Unmap can occur ~678 times per source frame and
 // saturate the render thread. The thread-local full-snapshot compare keeps Projection/VP changes visible
 // while byte-identical writes bypass the shared lock entirely.
 const bool fullSnapshotChanged=!t_b446LastFullSnapshotValid||std::memcmp(t_b446LastFullSnapshot,raw,256)!=0;
 const bool signatureChanged=!t_b309LastViewInverseValid||std::memcmp(t_b309LastViewInverseSignature,signature,64)!=0;
 const bool periodicRefresh=(ordinal&255ull)==0ull;
 if(fullSnapshotChanged){
  g_b456FullTupleChanges.fetch_add(1,std::memory_order_relaxed);
  // B475: only run the lane scan when the existing full-snapshot compare already proved bytes changed.
  // Sixteen bits map directly to byte ranges 0-15,16-31,...,240-255.
  if(t_b446LastFullSnapshotValid){unsigned laneMask=0;for(unsigned lane=0;lane<16;++lane)if(std::memcmp(t_b446LastFullSnapshot+lane*16,raw+lane*16,16)!=0)laneMask|=(1u<<lane);if(laneMask)g_b475ChangedLaneMask.fetch_or(laneMask,std::memory_order_relaxed);}
  g_b475ChangedWrites.fetch_add(1,std::memory_order_relaxed);
  g_b475LatestChangedSnapshotFp.store(b475_fingerprint256(raw),std::memory_order_relaxed);
 }
 if(signatureChanged)g_b456ViewInverseChanges.fetch_add(1,std::memory_order_relaxed);
 if(!fullSnapshotChanged&&!periodicRefresh){g_b309DuplicateSkips.fetch_add(1,std::memory_order_relaxed);return;}
 if(fullSnapshotChanged){std::memcpy(t_b446LastFullSnapshot,raw,256);t_b446LastFullSnapshotValid=true;}
 const unsigned long long handoffSerial=g_b301DirectWriteSerial.fetch_add(1,std::memory_order_relaxed)+1;
 AcquireSRWLockExclusive(&g_b300DirectWriteLock);std::memcpy(g_b445LatestSelectedSnapshot.bytes,raw,256);g_b445LatestSelectedSnapshot.serial=handoffSerial;g_b445LatestSelectedResource=r;g_b445LatestSelectedSerial=handoffSerial;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);b489_record_authoritative_publish(handoffSerial,g_b324SourceFrameEpoch.load(std::memory_order_acquire),raw);
 if(signatureChanged){std::memcpy(t_b309LastViewInverseSignature,signature,64);t_b309LastViewInverseValid=true;}
 AcquireSRWLockExclusive(&g_b300DirectWriteLock);auto&dst=g_b300DirectWrites[g_b300DirectWriteHead%kB300DirectWriteRing];std::memcpy(dst.bytes,raw,256);dst.serial=handoffSerial;++g_b300DirectWriteHead;if(g_b300DirectWriteCount<kB300DirectWriteRing)++g_b300DirectWriteCount;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);
 g_b309CandidatesCaptured.fetch_add(1,std::memory_order_relaxed);g_b456PublishedWrites.fetch_add(1,std::memory_order_relaxed);
}
static void b280_observe_constant_write(ID3D11Resource*r,const void*p,unsigned bytes){
 if(!r||!p||bytes<64)return;
 // B301: once B300 has locked the actual gameplay-camera resource, do not perform any
 // generic known-CB lookup or legacy observation on Map/Unmap. The selected pointer is
 // the entire direct-mode discriminator. Non-selected resources are bypassed in h_map/h_unmap.
 if(g_b300DirectMode.load(std::memory_order_acquire)){b309_capture_direct_camera_write_if_needed(r,p,bytes);return;}
 const int idx=b280_known_cb_index(r);if(idx<0)return;
 const unsigned width=g_known_cb_bytes[idx].load(std::memory_order_acquire);
 // B283: B281 proved CB4/12240 and CB3/960 are the only structural targets worth following.
 // Ignore every other GTA constant-buffer write and sample only 1/64 target writes so diagnostics cannot starve GTA's render thread.
 if(width!=256)return;
 const unsigned seen=g_b282TargetWriteCount[idx].fetch_add(1,std::memory_order_relaxed)+1;
 // B287: retain the latest complete 256-byte write so DrawIndexed can consume the exact live buffer contents.
 if(bytes>=256){AcquireSRWLockExclusive(&g_b287LatestLock);std::memcpy(g_b287Latest256[idx],p,256);g_b287LatestValid[idx]=true;g_b287LatestWriteSerial[idx]=seen;ReleaseSRWLockExclusive(&g_b287LatestLock);}
 if((seen&15u)!=0u)return;
 const unsigned n=bytes>kSnapshotBytes?kSnapshotBytes:bytes;
 AcquireSRWLockExclusive(&g_b280CandidateLock);
 if(g_b280PreviousValid[idx]){
  double bestRms=0.0,bestMax=0.0;unsigned bestOffset=0;double bestScore=-1.0;
  for(unsigned off=0;off+64<=n;off+=16){
   float v[16]{},pv[16]{};double sum=0.0,mx=0.0;bool finite=true;
   for(unsigned j=0;j<16;++j){std::memcpy(&v[j],(const unsigned char*)p+off+j*4,4);std::memcpy(&pv[j],g_b280PreviousBytes[idx]+off+j*4,4);if(!std::isfinite(v[j])||!std::isfinite(pv[j])||std::fabs(v[j])>1000000.0f||std::fabs(pv[j])>1000000.0f){finite=false;break;}const double d=(double)v[j]-(double)pv[j];sum+=d*d;mx=(std::max)(mx,std::fabs(d));}
   if(!finite)continue;const double rms=std::sqrt(sum/16.0);
   const double r0=std::sqrt((double)v[0]*v[0]+(double)v[1]*v[1]+(double)v[2]*v[2]);const double r1=std::sqrt((double)v[4]*v[4]+(double)v[5]*v[5]+(double)v[6]*v[6]);const double r2=std::sqrt((double)v[8]*v[8]+(double)v[9]*v[9]+(double)v[10]*v[10]);const double d01=(double)v[0]*v[4]+(double)v[1]*v[5]+(double)v[2]*v[6];const double d02=(double)v[0]*v[8]+(double)v[1]*v[9]+(double)v[2]*v[10];const double d12=(double)v[4]*v[8]+(double)v[5]*v[9]+(double)v[6]*v[10];const double orthoErr=std::fabs(r0-1.0)+std::fabs(r1-1.0)+std::fabs(r2-1.0)+std::fabs(d01)+std::fabs(d02)+std::fabs(d12);const bool ortho=orthoErr<0.35;
   // For the 12240-byte CB4 target prefer changing orthonormal windows; for CB3 retain strongest-changing transform-sized window.
   const double score=rms;
   if(score>bestScore){bestScore=score;bestRms=rms;bestMax=mx;bestOffset=off;}
  }
  auto&w=g_b280CandidateWindows[idx];w.cbIndex=(unsigned)idx;w.byteOffset=bestOffset;w.byteWidth=width;w.updates=seen;w.maxDelta=bestMax;w.rmsDelta=bestRms;w.selected=(r==(ID3D11Resource*)g_b175FastConstants.Get());for(unsigned j=0;j<16;++j)std::memcpy(&w.values[j],(const unsigned char*)p+bestOffset+j*4,4);++g_b280CandidateUpdateSerial;
 }
 std::memcpy(g_b280PreviousBytes[idx],p,n);g_b280PreviousByteCount[idx]=n;g_b280PreviousValid[idx]=true;ReleaseSRWLockExclusive(&g_b280CandidateLock);
}
static unsigned long long g_b175FastLastCaptureSerial{};
static unsigned long long g_b175FastLastColorSerial{};
static unsigned g_b175FastStalePresents{};
static ULONGLONG g_b175FastHookStart{};
struct PendingMap{ID3D11Resource* resource{};void* data{};unsigned byte_count{};bool target{};int b484WatchSlot{-1};};
static thread_local PendingMap t_pending{};
using ps_cb_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11Buffer* const*);
using om_rt_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,ID3D11RenderTargetView* const*,ID3D11DepthStencilView*);
using draw_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT);
using draw_indexed_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);
using draw_indexed_instanced_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
using draw_instanced_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,UINT);
using dispatch_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT);
using map_fn=HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE*);
using unmap_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT);
static ps_cb_fn o_vs_cb{},o_ps_cb{};static om_rt_fn o_fast_om_rt{};static draw_fn o_fast_draw{};static draw_indexed_fn o_fast_draw_indexed{};static draw_indexed_instanced_fn o_b321_draw_indexed_instanced{};static draw_instanced_fn o_b321_draw_instanced{};static dispatch_fn o_b321_dispatch{};static map_fn o_map{};static unmap_fn o_unmap{};
// B321 diagnostic-only D3D11 render workload profiler. Camera classification hooks are not restored.
static std::atomic<bool> g_b321ProfilerActive{false};
static std::atomic<unsigned long long> g_b321DrawCalls{0},g_b321DrawIndexedCalls{0},g_b321DrawIndexedInstancedCalls{0},g_b321DrawInstancedCalls{0},g_b321DispatchCalls{0};
static std::atomic<unsigned long long> g_b321DrawSampleOrdinal{0},g_b321DispatchSampleOrdinal{0},g_b321SampledDrawCalls{0},g_b321SampledDispatchCalls{0};
static std::atomic<long long> g_b321SampledDrawTicks{0},g_b321SampledDispatchTicks{0};
static constexpr unsigned long long kB321SampleStride=64ull;
// B322 diagnostic-only D3D11 state/resource CPU profiler. It wraps the already-active
// immediate-context methods after the B300 camera lock, preserving every existing target.
enum B322Group : unsigned { B322_CB=0, B322_SRV=1, B322_SHADER=2, B322_IA=3, B322_OMRS=4, B322_RESOURCE=5, B322_MAPUNMAP=6, B322_GROUP_COUNT=7 };
static std::atomic<bool> g_b322ProfilerActive{false};
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
struct B323ResourceSample { ID3D11Resource* resource{}; unsigned long long samples{}; long long totalTicks{}; long long originalTicks{}; };
static B323ResourceSample g_b323ResourceSamples[64]{}; static SRWLOCK g_b323ResourceLock=SRWLOCK_INIT;
static bool b323_should_sample(unsigned c){return ((g_b323SampleOrd[c].fetch_add(1,std::memory_order_relaxed)+1ull)%kB323SampleStride)==0ull;}
static void b323_record_sample(unsigned cat,ID3D11Resource*r,long long total,long long original){
 g_b323Samples[cat].fetch_add(1,std::memory_order_relaxed);g_b323TotalTicks[cat].fetch_add(total,std::memory_order_relaxed);g_b323OriginalTicks[cat].fetch_add(original,std::memory_order_relaxed);
 if(cat==B323_OTHER_MAP||cat==B323_OTHER_UNMAP){AcquireSRWLockExclusive(&g_b323ResourceLock);unsigned empty=64;for(unsigned i=0;i<64;++i){if(g_b323ResourceSamples[i].resource==r){g_b323ResourceSamples[i].samples++;g_b323ResourceSamples[i].totalTicks+=total;g_b323ResourceSamples[i].originalTicks+=original;ReleaseSRWLockExclusive(&g_b323ResourceLock);return;}if(!g_b323ResourceSamples[i].resource&&empty==64)empty=i;}if(empty<64){g_b323ResourceSamples[empty].resource=r;g_b323ResourceSamples[empty].samples=1;g_b323ResourceSamples[empty].totalTicks=total;g_b323ResourceSamples[empty].originalTicks=original;}ReleaseSRWLockExclusive(&g_b323ResourceLock);}
}
using srv_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11ShaderResourceView*const*);
using vs_shader_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11VertexShader*,ID3D11ClassInstance*const*,UINT);
using ps_shader_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11PixelShader*,ID3D11ClassInstance*const*,UINT);
using ia_vb_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11Buffer*const*,const UINT*,const UINT*);
using ia_ib_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Buffer*,DXGI_FORMAT,UINT);
using ia_layout_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11InputLayout*);
using ia_topology_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,D3D11_PRIMITIVE_TOPOLOGY);
using rs_viewports_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,const D3D11_VIEWPORT*);
using copy_sub_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,UINT,UINT,UINT,ID3D11Resource*,UINT,const D3D11_BOX*);
using copy_resource_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,ID3D11Resource*);
using update_sub_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
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
static std::atomic<unsigned> g_b308AbState{0}; // 0=waiting,1=Map/Unmap hooks removed,2=completed
static std::atomic<ULONGLONG> g_b308DirectLockTick{0},g_b308AbStartTick{0};
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
  if(!selected){const int b484Slot=b484_watch_slot(r);if(timed)ob=b307_qpc_now();HRESULT hr=o_map(c,r,sub,mt,flags,m);if(timed)oe=b307_qpc_now();if(b484Slot>=0){t_pending={};if(SUCCEEDED(hr)&&m&&m->pData&&mt==D3D11_MAP_WRITE_DISCARD){t_pending.resource=r;t_pending.data=m->pData;t_pending.byte_count=256;t_pending.target=true;t_pending.b484WatchSlot=b484Slot;}}if(timed)b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);return hr;}
  g_b456SelectedMaps.fetch_add(1,std::memory_order_relaxed);if(g_active)g_active->fetch_add(1);if(timed)ob=b307_qpc_now();HRESULT hr=o_map(c,r,sub,mt,flags,m);if(timed)oe=b307_qpc_now();t_pending={};
  if(SUCCEEDED(hr)&&m&&m->pData&&mt==D3D11_MAP_WRITE_DISCARD){t_pending.resource=r;t_pending.data=m->pData;t_pending.byte_count=256;t_pending.target=true;}
  if(g_active)g_active->fetch_sub(1);if(timed)b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);return hr;
 }
 const long long totalBegin=b307_qpc_now();ID3D11Resource*selectedResource=g_b301DirectCbResource.load(std::memory_order_acquire);const bool selected=r&&r==selectedResource;
 if(g_active)g_active->fetch_add(1);unsigned i=~0u;if(!g_b175FastHookActive.load(std::memory_order_acquire))i=begin_map_event(1,r,sub,mt,flags);const long long ob=b307_qpc_now();HRESULT hr=o_map(c,r,sub,mt,flags,m);const long long oe=b307_qpc_now();if(i!=~0u)finish_map_event(i,hr);t_pending={};if(SUCCEEDED(hr)&&m&&m->pData&&mt==D3D11_MAP_WRITE_DISCARD){unsigned bytes=g_b175FastHookActive.load(std::memory_order_acquire)&&r==(ID3D11Resource*)g_b175FastConstants.Get()?g_b175FastConstantBytes:known_cb_bytes(r);if(bytes){t_pending.resource=r;t_pending.data=m->pData;t_pending.byte_count=bytes>kSnapshotBytes?kSnapshotBytes:bytes;t_pending.target=true;}}if(g_active)g_active->fetch_sub(1);const long long te=b307_qpc_now();b308_record_map_timing(te-totalBegin,oe-ob,selected);return hr;
}
static void STDMETHODCALLTYPE h_unmap(ID3D11DeviceContext*c,ID3D11Resource*r,UINT sub){
 // B301 mirrors the Map fast bypass. Only the one selected camera resource enters the direct ring.
 if(g_b300DirectMode.load(std::memory_order_acquire)){
  ID3D11Resource*sel=g_b301DirectCbResource.load(std::memory_order_acquire);const bool selected=(r==sel);const unsigned cat=selected?B323_SEL_UNMAP:B323_OTHER_UNMAP;
  const bool b323Active=g_b323ProfilerActive.load(std::memory_order_relaxed);if(b323Active)g_b323Calls[cat].fetch_add(1,std::memory_order_relaxed);const bool timed=b323Active&&b323_should_sample(cat);long long tb=0,ob=0,oe=0;if(timed)tb=b307_qpc_now();
  if(!selected){if(t_pending.target&&t_pending.resource==r&&t_pending.data&&t_pending.b484WatchSlot>=0)b484_capture_watch(t_pending.b484WatchSlot,t_pending.data,t_pending.byte_count);if(timed)ob=b307_qpc_now();o_unmap(c,r,sub);if(timed){oe=b307_qpc_now();b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);}t_pending={};return;}
  const unsigned long long b324Epoch=g_b324SourceFrameEpoch.load(std::memory_order_acquire);
  const bool b324Eligible=t_pending.target&&t_pending.resource==r&&t_pending.data;
  const bool b324FirstEligible=b324Eligible&&t_b324CapturedEpoch!=b324Epoch;
  bool b339SecondChance=false;
  if(b324Eligible&&!b324FirstEligible&&t_b324CapturedEpoch==b324Epoch&&!t_b339SecondChanceCaptured&&t_pending.byte_count>=256&&t_b339FirstViewInverseValid&&t_b339SecondChanceEpoch==b324Epoch){
   const float*nowView=(const float*)((const unsigned char*)t_pending.data+192);double maxDelta=0.0;
   for(unsigned i=0;i<16;++i)maxDelta=(std::max)(maxDelta,std::fabs((double)nowView[i]-(double)t_b339FirstViewInverse[i]));
   if(std::isfinite(maxDelta)&&maxDelta>=0.001){b339SecondChance=true;t_b339SecondChanceCaptured=true;g_b339SecondChanceCaptures.fetch_add(1,std::memory_order_relaxed);}
  }
  if(!b324FirstEligible&&!b339SecondChance){
   if(b324Eligible){g_b324DuplicateSelectedUnmapBypasses.fetch_add(1,std::memory_order_relaxed);g_b339SecondChanceBypasses.fetch_add(1,std::memory_order_relaxed);}
   if(timed)ob=b307_qpc_now();o_unmap(c,r,sub);if(timed){oe=b307_qpc_now();b323_record_sample(cat,r,b307_qpc_now()-tb,oe-ob);}t_pending={};return;
  }
  if(b324FirstEligible){
   t_b324CapturedEpoch=b324Epoch;g_b324FirstEligibleCaptures.fetch_add(1,std::memory_order_relaxed);t_b339SecondChanceEpoch=b324Epoch;t_b339SecondChanceCaptured=false;t_b339FirstViewInverseValid=t_pending.byte_count>=256;
   if(t_b339FirstViewInverseValid)std::memcpy(t_b339FirstViewInverse,(const unsigned char*)t_pending.data+192,64);
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
 const unsigned b431AcquireStride=g_b430ResponsiveReacquireActive.load(std::memory_order_acquire)?2u:kB300AcquireDrawSampleStride;
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
  const bool b431RejectedClassifierResource=g_b430ResponsiveReacquireActive.load(std::memory_order_acquire)&&cb1==g_b430RejectedStaticCameraResource.load(std::memory_order_acquire);
  if(b431RejectedClassifierResource){have=false;g_b431RejectedClassifierDraws.fetch_add(1,std::memory_order_relaxed);}
  AcquireSRWLockExclusive(&g_b287FrameLock);++g_b287MainPassDraws;ReleaseSRWLockExclusive(&g_b287FrameLock);
  if(have){double m[64]{};bool finite=true;for(unsigned i=0;i<64;++i){float f=0;std::memcpy(&f,raw+i*4,4);m[i]=f;if(!std::isfinite(m[i])||std::fabs(m[i])>10000000.0)finite=false;}
   if(finite){
    const bool screen=b287_identity4d(m+16)&&b287_identity4d(m+48);
    double directView[16]{},invWorldView[16]{},directProjection[16]{},directVp[16]{};
    const bool directViewOk=b287_inverse4(m+48,directView);
    const bool invWorldViewOk=b287_inverse4(m+16,invWorldView);
    if(invWorldViewOk)b287_mul4(invWorldView,m+32,directProjection);
    if(directViewOk&&invWorldViewOk)b287_mul4(directView,directProjection,directVp);
    const bool directDerive=directViewOk&&invWorldViewOk;
    if(screen||!directDerive){AcquireSRWLockExclusive(&g_b287FrameLock);++g_b287RejectedScreenDraws;++g_b291DirectRejectDraws;ReleaseSRWLockExclusive(&g_b287FrameLock);}else{
     AcquireSRWLockExclusive(&g_b287FrameLock);if(g_b287QualifyingDraws<4096){for(unsigned i=0;i<16;++i){g_b287SumWorld[i]+=m[i];g_b287SumWorldView[i]+=m[16+i];g_b287SumWorldViewProj[i]+=m[32+i];g_b287SumViewInverse[i]+=m[48+i];g_b291SumDirectView[i]+=directView[i];g_b291SumDirectProjection[i]+=directProjection[i];}++g_b291DirectDeriveDraws;++g_b287QualifyingDraws;g_b287LastCbIndex=(unsigned)bi;g_b287LastIndexCount=count;g_b287LastVs=vs;g_b287LastWriteSerial=writeSerial;
      const std::uintptr_t familyKey=reinterpret_cast<std::uintptr_t>(vs);unsigned fi=0;for(;fi<g_b292FamilyCount;++fi)if(g_b292Families[fi].vsKey==familyKey)break;if(fi==g_b292FamilyCount){if(g_b292FamilyCount<kB292MaxFamilies){fi=g_b292FamilyCount++;g_b292Families[fi].vsKey=familyKey;}else{++g_b292FamilyOverflow;fi=kB292MaxFamilies;}}if(fi<kB292MaxFamilies){auto&fam=g_b292Families[fi];++fam.draws;fam.lastIndexCount=count;fam.cbIndex=(unsigned)bi;fam.writeSerial=writeSerial;for(unsigned i=0;i<16;++i){fam.sumView[i]+=directView[i];fam.sumProjection[i]+=directProjection[i];}
       unsigned bestCluster=~0u;double bestClusterDelta=1.0e300;for(unsigned ci=0;ci<fam.clusterCount&&ci<kB293MaxClustersPerFamily;++ci){auto&cl=fam.clusters[ci];if(!cl.draws)continue;double dmax=0.0;for(unsigned i=0;i<16;++i)dmax=(std::max)(dmax,std::fabs(directView[i]-(cl.sumView[i]/double(cl.draws))));if(dmax<bestClusterDelta){bestClusterDelta=dmax;bestCluster=ci;}}
       if(bestCluster==~0u||bestClusterDelta>250.0){if(fam.clusterCount<kB293MaxClustersPerFamily)bestCluster=fam.clusterCount++;else{++fam.clusterOverflow;bestCluster=~0u;}}
       if(bestCluster!=~0u){auto&cl=fam.clusters[bestCluster];++cl.draws;cl.lastIndexCount=count;cl.cbIndex=(unsigned)bi;cl.writeSerial=writeSerial;for(unsigned i=0;i<16;++i){cl.sumView[i]+=directView[i];cl.sumProjection[i]+=directProjection[i];}}}}ReleaseSRWLockExclusive(&g_b287FrameLock);
    }
   }
  }
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
static bool target_om(const B49OmEvent*e){if(!e||!e->r[0]||!e->dsv)return false;UINT w=0,h=0,f=0,b=0,s=0;if(!id_rtv(e->r[0],w,h,f,b,s)||w<640||h<360||w>1920||h>1080||f!=87||s!=1)return false;ComPtr<ID3D11Resource>r;e->dsv->GetResource(&r);UINT dw=0,dh=0,df=0,db=0,ds=0;return id_resource(r.Get(),dw,dh,df,db,ds)&&dw==w&&dh==h&&df==44&&ds==1;}
static bool history_pair(ID3D11ShaderResourceView*a,ID3D11ShaderResourceView*b){UINT w=0,h=0,f=0,bd=0,s=0,w2=0,h2=0,f2=0,bd2=0,s2=0;auto*x=id_srv(a,w,h,f,bd,s);auto*y=id_srv(b,w2,h2,f2,bd2,s2);return x&&x==y&&w>=640&&h>=360&&w<=1920&&h<=1080&&f==10&&s==1&&w2==w&&h2==h&&f2==f&&s2==s;}
struct ConsumerState{unsigned draw{};ID3D11ShaderResourceView*slots[kSrvs]{};const B49OmEvent*om{};bool target{};};
static bool find_consumer(const B49OmEvent*oms,UINT on,const B49PsEvent*ps,UINT pn,const B49DrawEvent*draws,UINT dn,unsigned lo,unsigned hi,ConsumerState&out){ID3D11ShaderResourceView*state[kSrvs]{};unsigned pi=0,oi=0;const B49OmEvent*curOm=nullptr;ConsumerState fallback{};bool haveFallback=false;for(unsigned di=0;di<dn;++di){const auto&d=draws[di];if(!d.ready.load())continue;while(pi<pn&&ps[pi].ready.load()&&ps[pi].ordinal<d.ordinal){const auto&e=ps[pi++];for(unsigned x=0;x<e.count;++x){unsigned slot=e.start+x;if(slot<kSrvs)state[slot]=e.r[x];}}while(oi<on&&oms[oi].ready.load()&&oms[oi].ordinal<d.ordinal)curOm=&oms[oi++];if(d.ordinal<lo||d.ordinal>=hi||!history_pair(state[9],state[14]))continue;ConsumerState q{};q.draw=d.ordinal;q.om=curOm;q.target=target_om(curOm);for(unsigned slot=0;slot<kSrvs;++slot)q.slots[slot]=state[slot];if(q.target){out=q;return true;}if(!haveFallback){fallback=q;haveFallback=true;}}if(haveFallback){out=fallback;return true;}return false;}
struct PipelineState{ID3D11Buffer*cbs[kPsCbs]{};unsigned cbLastOrdinal[kPsCbs]{};};
static void pipeline_at(unsigned drawOrdinal,PipelineState&out){unsigned cn=g_cb_next.load();if(cn>kCbEvents)cn=kCbEvents;for(unsigned i=0;i<cn;++i){auto&e=g_cb_events[i];if(!e.ready.load()||e.ordinal>=drawOrdinal)continue;unsigned n=e.count>kPsCbs?kPsCbs:e.count;for(unsigned j=0;j<n;++j){unsigned slot=e.start+j;if(slot<kPsCbs){out.cbs[slot]=e.buffers[j];out.cbLastOrdinal[slot]=e.ordinal;}}}}
static const MapEvent* last_write_discard_before(ID3D11Buffer*target,unsigned lo,unsigned hi){const MapEvent*best=nullptr;if(!target)return nullptr;unsigned n=g_map_next.load();if(n>kMapEvents)n=kMapEvents;for(unsigned i=0;i<n;++i){auto&e=g_map_events[i];if(!e.ready.load()||e.type!=1||e.map_type!=(unsigned)D3D11_MAP_WRITE_DISCARD||FAILED(e.hr)||e.resource!=(ID3D11Resource*)target||e.ordinal<lo||e.ordinal>=hi)continue;if(!best||e.ordinal>best->ordinal)best=&e;}return best;}
static const Snapshot* last_snapshot_before(ID3D11Buffer*target,unsigned lo,unsigned hi){const Snapshot*best=nullptr;if(!target)return nullptr;unsigned n=g_snapshot_next.load();if(n>kSnapshots)n=kSnapshots;for(unsigned i=0;i<n;++i){auto&e=g_snapshots[i];if(!e.ready.load()||e.resource!=(ID3D11Resource*)target||e.sequence<lo||e.sequence>=hi)continue;if(!best||e.sequence>best->sequence)best=&e;}return best;}
static void release_all(){unsigned cn=g_cb_next.load();if(cn>kCbEvents)cn=kCbEvents;for(unsigned i=0;i<cn;++i)for(auto&b:g_cb_events[i].buffers)if(b){b->Release();b=nullptr;}unsigned mn=g_map_next.load();if(mn>kMapEvents)mn=kMapEvents;for(unsigned i=0;i<mn;++i)if(g_map_events[i].resource){g_map_events[i].resource->Release();g_map_events[i].resource=nullptr;}}
static float f32(const Snapshot*s,unsigned i){float v=0;if(s&&i*4+4<=s->byte_count)std::memcpy(&v,s->bytes+i*4,4);return v;}
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

struct B160Candidate{unsigned draw{};UINT width{},height{},rtv_format{},dsv_format{},marker_width{},marker_height{},marker_format{};IUnknown*color_identity{};IUnknown*depth_identity{};IUnknown*marker_identity{};ID3D11Buffer*constant_buffer{};};
struct B160Reject{unsigned inspected{},valid{},rtv_format{},bounds{},dsv_format{},dsv_size{},samples{},marker{},identity{},top_count{};UINT observed_width{},observed_height{},observed_rtv_format{},observed_dsv_width{},observed_dsv_height{},observed_dsv_format{},observed_marker_width{},observed_marker_height{},observed_marker_format{},top_width[3]{},top_height[3]{};};
static bool b160better(const B160Candidate&a,const B160Candidate&b){const uint64_t aa=(uint64_t)a.width*a.height,ba=(uint64_t)b.width*b.height;return aa!=ba?aa>ba:a.width!=b.width?a.width>b.width:a.height!=b.height?a.height>b.height:a.draw<b.draw;}
static unsigned b160marker_error(UINT marker,UINT full){const uint64_t doubled=(uint64_t)marker*2;return (unsigned)(doubled>full?doubled-full:full-doubled);}
static void b160remember_top(B160Reject&r,const B160Candidate&candidate){for(unsigned i=0;i<r.top_count;++i)if(r.top_width[i]==candidate.width&&r.top_height[i]==candidate.height)return;const uint64_t area=(uint64_t)candidate.width*candidate.height;if(r.top_count==3&&area<=(uint64_t)r.top_width[2]*r.top_height[2])return;unsigned at=r.top_count<3?r.top_count++:2;r.top_width[at]=candidate.width;r.top_height[at]=candidate.height;for(unsigned i=at;i>0;--i){uint64_t here=(uint64_t)r.top_width[i]*r.top_height[i],prior=(uint64_t)r.top_width[i-1]*r.top_height[i-1];if(here<=prior)break;std::swap(r.top_width[i],r.top_width[i-1]);std::swap(r.top_height[i],r.top_height[i-1]);}}
static unsigned b160draw(const B49OmEvent*om,UINT on,const B49PsEvent*ps,UINT pn,const B49DrawEvent*d,UINT dn,unsigned lo,unsigned hi,const B160Candidate*expected,B160Candidate&selected,B160Reject&reject){
 ID3D11ShaderResourceView*sr[kSrvs]{};ID3D11Buffer*cbs[2][kPsCbs]{};const B49OmEvent*o=nullptr;UINT pi=0,oi=0,ci=0,cn=(std::min)(g_cb_next.load(),kCbEvents);for(UINT di=0;di<dn;++di){if(!d[di].ready.load())continue;
 while(pi<pn&&ps[pi].ready.load()&&ps[pi].ordinal<d[di].ordinal){auto&e=ps[pi++];for(UINT j=0;j<e.count&&j<kSrvs;++j){UINT s=e.start+j;if(s<kSrvs)sr[s]=e.r[j];}}
 while(ci<cn&&g_cb_events[ci].ready.load()&&g_cb_events[ci].ordinal<d[di].ordinal){auto&e=g_cb_events[ci++];if(e.stage<=1)for(UINT j=0;j<e.count&&j<kPsCbs;++j){UINT slot=e.start+j;if(slot<kPsCbs)cbs[e.stage][slot]=e.buffers[j];}}
 while(oi<on&&om[oi].ready.load()&&om[oi].ordinal<d[di].ordinal)o=&om[oi++];if(d[di].ordinal<lo||d[di].ordinal>=hi||!o)continue;++reject.inspected;
 UINT rw=0,rh=0,rf=0,rs=0,dw=0,dh=0,df=0,ds=0;IUnknown*rtv=b160view(o->r[0],rw,rh,rf,rs),*depth=b160view(o->dsv,dw,dh,df,ds);
 reject.observed_width=rw;reject.observed_height=rh;reject.observed_rtv_format=rf;reject.observed_dsv_width=dw;reject.observed_dsv_height=dh;reject.observed_dsv_format=df;
 if(!rtv||rf!=10){++reject.rtv_format;continue;}if(rw<640||rh<360||rw>1920||rh>1080){++reject.bounds;continue;}if(rs!=1||ds!=1){++reject.samples;continue;}if(!depth||df!=44){++reject.dsv_format;continue;}if(dw!=rw||dh!=rh){++reject.dsv_size;continue;}
 IUnknown*markerIdentity=nullptr;UINT mw=0,mh=0,mf=0;for(auto*v:sr){UINT vw=0,vh=0,vf=0,vs=0;IUnknown*id=b160view(v,vw,vh,vf,vs);if(id&&vf==34&&vs==1&&b160marker_error(vw,rw)<=2&&b160marker_error(vh,rh)<=2){markerIdentity=id;mw=vw;mh=vh;mf=vf;break;}if(id&&vf==34){reject.observed_marker_width=vw;reject.observed_marker_height=vh;reject.observed_marker_format=vf;}}
 if(!markerIdentity){++reject.marker;continue;}ID3D11Buffer*constant=cbs[1][1]?cbs[1][1]:cbs[0][1];if(expected&&(rw!=expected->width||rh!=expected->height||depth!=expected->depth_identity||markerIdentity!=expected->marker_identity||constant!=expected->constant_buffer)){++reject.identity;continue;}
 B160Candidate candidate{d[di].ordinal,rw,rh,rf,df,mw,mh,mf,rtv,depth,markerIdentity,constant};++reject.valid;b160remember_top(reject,candidate);if(!selected.draw||b160better(candidate,selected))selected=candidate;
 }return selected.draw;}
static const B49OmEvent* b160om_at(const B49OmEvent*om,UINT on,unsigned draw){const B49OmEvent*out=nullptr;for(UINT i=0;i<on;++i)if(om[i].ready.load()&&om[i].ordinal<draw)out=&om[i];return out;}
extern "C" __declspec(dllexport) HRESULT WINAPI B175nCheck170AnalyzeRtvProbe(const B49OmEvent*om,UINT on,const B49DrawEvent*draws,UINT dn,UINT split,const char*path){
 if(!om||!draws||!path)return E_INVALIDARG;std::ofstream out(path,std::ios::app);if(!out)return E_FAIL;
 g_b175nPreCoreProbeReady.store(false,std::memory_order_release);
 struct Probe{unsigned draw{};IUnknown*color{};IUnknown*depth{};UINT width{},height{},format{},bind{},samples{};ComPtr<ID3D11Texture2D>colorTexture;ComPtr<ID3D11Texture2D>depthTexture;};
 auto select=[&](unsigned lo,unsigned hi,Probe&selected,unsigned&matches){const B49OmEvent*state=nullptr;UINT oi=0;for(UINT di=0;di<dn;++di){const auto&draw=draws[di];if(!draw.ready.load(std::memory_order_acquire))continue;while(oi<on&&om[oi].ready.load(std::memory_order_acquire)&&om[oi].ordinal<draw.ordinal)state=&om[oi++];if(draw.ordinal<lo||draw.ordinal>=hi||draw.type!=2||draw.a!=100||draw.b!=0||!state||!state->r[0]||!state->dsv)continue;UINT w=0,h=0,f=0,b=0,s=0,dw=0,dh=0,df=0,db=0,ds=0;IUnknown*color=id_rtv(state->r[0],w,h,f,b,s);ComPtr<ID3D11Resource>colorResource,depthResource;state->r[0]->GetResource(&colorResource);state->dsv->GetResource(&depthResource);IUnknown*depth=id_resource(depthResource.Get(),dw,dh,df,db,ds);if(!color||!depth||w<640||h<360||w>1920||h>1080||f!=DXGI_FORMAT_R16G16B16A16_FLOAT||s!=1||!(b&D3D11_BIND_RENDER_TARGET)||dw!=w||dh!=h||df!=DXGI_FORMAT_R24G8_TYPELESS||ds!=1)continue;++matches;if(!selected.draw){selected.draw=draw.ordinal;selected.color=color;selected.depth=depth;selected.width=w;selected.height=h;selected.format=f;selected.bind=b;selected.samples=s;colorResource.As(&selected.colorTexture);depthResource.As(&selected.depthTexture);}}};
 Probe first{},second{};unsigned firstMatches=0,secondMatches=0;select(0,split,first,firstMatches);select(split,~0u,second,secondMatches);
 const bool depthStable=first.depth&&first.depth==second.depth,colorDistinct=first.color&&second.color&&first.color!=second.color,valid=first.draw&&second.draw&&depthStable&&colorDistinct;
 if(valid&&first.colorTexture&&second.colorTexture&&second.depthTexture){AcquireSRWLockExclusive(&g_b175FastLock);g_b175FastColorPair[0]=first.colorTexture;g_b175FastColorPair[1]=second.colorTexture;g_b175FastCurrentColor=second.colorTexture;g_b175FastDepth=second.depthTexture;g_b175FastDrawType=2;g_b175FastDrawA=100;g_b175FastDrawB=0;g_b175FastDrawC=0;g_b160w=second.width;g_b160h=second.height;ReleaseSRWLockExclusive(&g_b175FastLock);g_b175nPreCoreProbeReady.store(true,std::memory_order_release);}
 out<<"B175n: ISOLATED_RTV_PROBE_RESULT first_draw="<<first.draw<<", second_draw="<<second.draw<<", first_matches="<<firstMatches<<", second_matches="<<secondMatches<<", signature={2,100,0,0}, first_desc={"<<first.width<<"x"<<first.height<<","<<first.format<<","<<first.bind<<","<<first.samples<<"}, second_desc={"<<second.width<<"x"<<second.height<<","<<second.format<<","<<second.bind<<","<<second.samples<<"}, depth_identity_stable="<<(depthStable?1:0)<<", color_identity_distinct="<<(colorDistinct?1:0)<<", valid="<<(valid?1:0)<<", selected_color_pair_retained="<<(g_b175FastColorPair[0]&&g_b175FastColorPair[1]?1:0)<<", selected_depth_retained="<<(g_b175FastDepth?1:0)<<", xess_context_created=0, d3d12_device_created=0, d3d11on12_created=0, persistent_hooks_installed=0, dependent_prewarm_started=0\n";
 out.flush();return g_b175nPreCoreProbeReady.load(std::memory_order_acquire)?S_OK:E_FAIL;
}
static const B49DrawEvent* b175_draw_at(const B49DrawEvent*draws,UINT dn,unsigned ordinal){for(UINT i=0;i<dn;++i)if(draws[i].ready.load(std::memory_order_acquire)&&draws[i].ordinal==ordinal)return&draws[i];return nullptr;}
struct B160Pipe{ID3D11Buffer*cb[2][kPsCbs]{};};static void b160pipe(unsigned draw,B160Pipe&p){unsigned n=g_cb_next.load();if(n>kCbEvents)n=kCbEvents;for(unsigned i=0;i<n;++i){auto&e=g_cb_events[i];if(!e.ready.load()||e.ordinal>=draw||e.stage>1)continue;for(UINT j=0;j<e.count&&j<kPsCbs;++j){UINT s=e.start+j;if(s<kPsCbs)p.cb[e.stage][s]=e.buffers[j];}}}
static Snapshot g_b172Previous;static bool g_b172HavePrevious=false;
static void b172remember(const Snapshot*s){if(!s)return;g_b172Previous.byte_count=s->byte_count;std::memcpy(g_b172Previous.bytes,s->bytes,s->byte_count);g_b172HavePrevious=true;}
struct B160M4{double a[4][4]{};};
static B160M4 b160matrix(const Snapshot*s,unsigned base,bool transpose){B160M4 m{};for(int r=0;r<4;++r)for(int c=0;c<4;++c)m.a[r][c]=f32(s,base+(transpose?c*4+r:r*4+c));return m;}
static bool b160inverse(const B160M4&m,B160M4&z){double q[4][8]{};for(int r=0;r<4;++r){for(int c=0;c<4;++c)q[r][c]=m.a[r][c];q[r][r+4]=1;}for(int c=0;c<4;++c){int p=c;for(int r=c+1;r<4;++r)if(fabs(q[r][c])>fabs(q[p][c]))p=r;if(fabs(q[p][c])<1e-12)return false;if(p!=c)for(int k=0;k<8;++k){double t=q[p][k];q[p][k]=q[c][k];q[c][k]=t;}double v=q[c][c];for(int k=0;k<8;++k)q[c][k]/=v;for(int r=0;r<4;++r)if(r!=c){v=q[r][c];for(int k=0;k<8;++k)q[r][k]-=v*q[c][k];}}for(int r=0;r<4;++r)for(int c=0;c<4;++c)z.a[r][c]=q[r][c+4];return true;}
static void b160mul(const B160M4&m,const double*x,double*y){for(int r=0;r<4;++r){y[r]=0;for(int c=0;c<4;++c)y[r]+=m.a[r][c]*x[c];}}
struct B160Stats{unsigned sampled{},depth_valid{},finite{},onscreen{},reasonable{};double sum{},maximum{},sumdx{},sumdy{},neighbor_sum{};unsigned neighbors{};std::vector<double>mag,depth;};
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
struct B160MotionStats{unsigned valid{},finite{},onscreen{},nonzero{};double sum{},maximum{};};
static HRESULT b160motion_texture(ID3D11DeviceContext*c,const D3D11_MAPPED_SUBRESOURCE&previousDepth,const D3D11_MAPPED_SUBRESOURCE&depth,const Snapshot*previous,const Snapshot*current,const char*path,std::ofstream&o){
 if(!c||!previous||!current||!g_b160w||!g_b160h)return E_INVALIDARG;const UINT w=g_b160w,h=g_b160h;B160M4 pm=b160matrix(previous,32,true),cm=b160matrix(current,32,true),inv{};if(!b160inverse(cm,inv))return E_FAIL;
 std::vector<uint16_t>pixels((size_t)w*h*2);std::vector<float>firstDepthPixels((size_t)w*h),depthPixels((size_t)w*h);B160MotionStats st{};
 for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){size_t i=((size_t)y*w+x)*2;UINT firstRaw=(*(const UINT*)((const BYTE*)previousDepth.pData+(size_t)y*previousDepth.RowPitch+x*4))&0x00ffffffu;if(firstRaw&&firstRaw!=0x00ffffffu)firstDepthPixels[(size_t)y*w+x]=(float)((double)firstRaw/16777215.0);UINT raw=(*(const UINT*)((const BYTE*)depth.pData+(size_t)y*depth.RowPitch+x*4))&0x00ffffffu;if(raw==0||raw==0x00ffffffu)continue;st.valid++;double z=(double)raw/16777215.0;depthPixels[(size_t)y*w+x]=(float)z;double clip[4]={2.0*(x+.5)/w-1.0,1.0-2.0*(y+.5)/h,z,1.0},world[4],prior[4];b160mul(inv,clip,world);if(!std::isfinite(world[3])||fabs(world[3])<1e-10)continue;for(double&v:world)v/=world[3];b160mul(pm,world,prior);if(!std::isfinite(prior[3])||fabs(prior[3])<1e-10)continue;double px=prior[0]/prior[3],py=prior[1]/prior[3],dx=(px-clip[0])*(w*.5),dy=(clip[1]-py)*(h*.5);if(!std::isfinite(dx)||!std::isfinite(dy))continue;st.finite++;if(fabs(px)<=1.05&&fabs(py)<=1.05)st.onscreen++;double mag=sqrt(dx*dx+dy*dy);st.sum+=mag;if(mag>st.maximum)st.maximum=mag;if(mag>.0001)st.nonzero++;float fx=(float)(dx>65504?65504:dx<-65504?-65504:dx),fy=(float)(dy>65504?65504:dy<-65504?-65504:dy);pixels[i]=b160half(fx);pixels[i+1]=b160half(fy);}
 ComPtr<ID3D11Device>dev;c->GetDevice(&dev);D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_R16G16_FLOAT;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA init{pixels.data(),w*4,0};ComPtr<ID3D11Texture2D>motion;HRESULT createHr=dev->CreateTexture2D(&d,&init,&motion);HRESULT srvHr=E_FAIL;ComPtr<ID3D11ShaderResourceView>srv;if(SUCCEEDED(createHr))srvHr=dev->CreateShaderResourceView(motion.Get(),nullptr,&srv);
 uint64_t cpuHash=b160hash((const BYTE*)pixels.data(),w*4,w,h),gpuHash=0;HRESULT mapHr=E_FAIL;unsigned gpuNonzero=0;if(SUCCEEDED(createHr)){d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D>read;if(SUCCEEDED(dev->CreateTexture2D(&d,nullptr,&read))){c->CopyResource(read.Get(),motion.Get());D3D11_MAPPED_SUBRESOURCE m{};mapHr=c->Map(read.Get(),0,D3D11_MAP_READ,0,&m);if(SUCCEEDED(mapHr)){gpuHash=b160hash((const BYTE*)m.pData,m.RowPitch,w,h);for(UINT y=0;y<h;++y){auto*p=(const uint16_t*)((const BYTE*)m.pData+(size_t)y*m.RowPitch);for(UINT x=0;x<w;++x)if(p[x*2]||p[x*2+1])gpuNonzero++;}c->Unmap(read.Get(),0);}}}
 o<<"B171: MOTION_TEXTURE width="<<w<<", height="<<h<<", format="<<(unsigned)DXGI_FORMAT_R16G16_FLOAT<<", matrix_base=32, layout=directx_row_vector, depth=normal, valid_depth_pixels="<<st.valid<<", finite_motion_pixels="<<st.finite<<", onscreen_pixels="<<st.onscreen<<", nonzero_cpu_pixels="<<st.nonzero<<", mean_magnitude_px="<<(st.finite?st.sum/st.finite:0)<<", max_magnitude_px="<<st.maximum<<"\n";
 o<<"B171: PRIVATE_TEXTURE_VALIDATION create_hr=0x"<<std::hex<<(unsigned)createHr<<", srv_hr=0x"<<(unsigned)srvHr<<", readback_map_hr=0x"<<(unsigned)mapHr<<std::dec<<", cpu_hash="<<cpuHash<<", gpu_hash="<<gpuHash<<", hash_match="<<(cpuHash==gpuHash)<<", nonzero_gpu_pixels="<<gpuNonzero<<", gta_resource_modified=0\n";
 float firstView[16]{},firstProjection[16]{},secondView[16]{},secondProjection[16]{};
 auto makeConstants=[](const Snapshot*s,float*v,float*p){B160M4 view=b160matrix(s,16,false),viewProjection=b160matrix(s,32,false),inverseView{};if(!b160inverse(view,inverseView))return false;bool ok=true;for(int r=0;r<4;++r)for(int col=0;col<4;++col){double q=0;for(int k=0;k<4;++k)q+=inverseView.a[r][k]*viewProjection.a[k][col];v[r*4+col]=(float)view.a[r][col];p[r*4+col]=(float)q;if(!std::isfinite(v[r*4+col])||!std::isfinite(p[r*4+col]))ok=false;}return ok;};
 bool firstConstantsValid=makeConstants(previous,firstView,firstProjection),secondConstantsValid=makeConstants(current,secondView,secondProjection),constantsValid=firstConstantsValid&&secondConstantsValid;
 o<<"B171: FRAME_CONSTANTS_SOURCE sets=2, first_finite="<<(firstConstantsValid?1:0)<<", second_finite="<<(secondConstantsValid?1:0)<<", view_base=16, view_projection_base=32, row_major=1, jitter={0,0}, motion_scale={1,1}\n";
 HRESULT submitHr=(SUCCEEDED(createHr)&&SUCCEEDED(srvHr)&&SUCCEEDED(mapHr)&&cpuHash==gpuHash&&constantsValid)?B164SubmitFrameInputs(pixels.data(),w*4,firstDepthPixels.data(),w*4,depthPixels.data(),w*4,firstView,firstProjection,secondView,secondProjection,w,h,path):E_FAIL;o<<"B171: MOTION_BRIDGE_SUBMIT background=1, hr=0x"<<std::hex<<(unsigned)submitHr<<std::dec<<", present_thread_blocked=0\n";return FAILED(createHr)?createHr:FAILED(srvHr)?srvHr:FAILED(mapHr)?mapHr:submitHr;}
extern "C" __declspec(dllexport) HRESULT WINAPI B175nCheck170AnalyzeDiscovery(const B49OmEvent*om,UINT on,const B49PsEvent*ps,UINT pn,const B49DrawEvent*d,UINT dn,UINT split,const char*path){
 if(!om||!ps||!d||!path)return E_INVALIDARG;std::ofstream o(path,std::ios::app);if(!o)return E_FAIL;
 g_b175FastReady.store(false,std::memory_order_release);g_b175FastDepth.Reset();g_b175FastConstants.Reset();
 B160Candidate firstCandidate{},secondCandidate{};B160Reject firstReject{},secondReject{};
 unsigned da=b160draw(om,on,ps,pn,d,dn,0,split,nullptr,firstCandidate,firstReject);
 unsigned db=b160draw(om,on,ps,pn,d,dn,split,~0u,da?&firstCandidate:nullptr,secondCandidate,secondReject);
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
 const bool depthDescriptorObserved=fastDepth&&depthDesc.Format==DXGI_FORMAT_R24G8_TYPELESS&&depthDesc.SampleDesc.Count==1&&depthDesc.Width==firstCandidate.width&&depthDesc.Height==firstCandidate.height;
 const B49DrawEvent*firstDraw=b175_draw_at(d,dn,da),*secondDraw=b175_draw_at(d,dn,db);
 const bool drawSignatureStable=firstDraw&&secondDraw&&firstDraw->type==secondDraw->type&&firstDraw->a==secondDraw->a&&firstDraw->b==secondDraw->b&&firstDraw->c==secondDraw->c;
 const B49OmEvent*firstOm=b160om_at(om,on,da),*secondOm=b160om_at(om,on,db);ComPtr<ID3D11Resource>firstColorResource,secondColorResource;ComPtr<ID3D11Texture2D>firstColor,secondColor;
 if(firstOm&&firstOm->r[0])firstOm->r[0]->GetResource(&firstColorResource);if(firstColorResource)firstColorResource.As(&firstColor);
 if(secondOm&&secondOm->r[0])secondOm->r[0]->GetResource(&secondColorResource);if(secondColorResource)secondColorResource.As(&secondColor);
 D3D11_TEXTURE2D_DESC firstColorDesc{},secondColorDesc{};if(firstColor)firstColor->GetDesc(&firstColorDesc);if(secondColor)secondColor->GetDesc(&secondColorDesc);
 const bool firstColorValid=firstColor&&firstColorDesc.Width==firstCandidate.width&&firstColorDesc.Height==firstCandidate.height&&firstColorDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT&&firstColorDesc.SampleDesc.Count==1&&(firstColorDesc.BindFlags&D3D11_BIND_RENDER_TARGET);
 const bool secondColorValid=secondColor&&secondColorDesc.Width==secondCandidate.width&&secondColorDesc.Height==secondCandidate.height&&secondColorDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT&&secondColorDesc.SampleDesc.Count==1&&(secondColorDesc.BindFlags&D3D11_BIND_RENDER_TARGET);
 const bool colorPingPongStable=firstColorValid&&secondColorValid&&firstColor.Get()!=secondColor.Get();
 const bool exactInternalSize=sizeObserved&&firstCandidate.width>=640&&firstCandidate.height>=360&&firstCandidate.width<=1920&&firstCandidate.height<=1080;
 const bool selectionValid=exactInternalSize&&depthIdentityObserved&&markerIdentityObserved&&constantIdentityObserved&&snapshotsObserved&&depthDescriptorObserved&&colorPingPongStable&&drawSignatureStable&&markerErrorX<=2&&markerErrorY<=2&&g_cb_overflow.load()==0&&g_snapshot_overflow.load()==0;
 auto logTop=[&](const B160Reject&r){o<<"[";for(unsigned i=0;i<r.top_count;++i){if(i)o<<",";o<<r.top_width[i]<<"x"<<r.top_height[i];}o<<"]";};
 o<<"B174y: DYNAMIC_DISCOVERY_SELECTION first_draw="<<da<<", second_draw="<<db<<", selected_size="<<firstCandidate.width<<"x"<<firstCandidate.height<<", second_size="<<secondCandidate.width<<"x"<<secondCandidate.height<<", marker_size="<<firstCandidate.marker_width<<"x"<<firstCandidate.marker_height<<", marker_error_px={"<<markerErrorX<<","<<markerErrorY<<"}, depth_identity_observed="<<(depthIdentityObserved?1:0)<<", marker_identity_observed="<<(markerIdentityObserved?1:0)<<", constant_identity_observed="<<(constantIdentityObserved?1:0)<<", snapshots_observed="<<(snapshotsObserved?2:0)<<", selection_valid="<<(selectionValid?1:0)<<"\n";
 o<<"B174y: DYNAMIC_DISCOVERY_COUNTS first={inspected="<<firstReject.inspected<<",valid="<<firstReject.valid<<",rtv_format="<<firstReject.rtv_format<<",bounds="<<firstReject.bounds<<",samples="<<firstReject.samples<<",dsv_format="<<firstReject.dsv_format<<",dsv_size="<<firstReject.dsv_size<<",marker="<<firstReject.marker<<",identity="<<firstReject.identity<<",top=";logTop(firstReject);o<<"}, second={inspected="<<secondReject.inspected<<",valid="<<secondReject.valid<<",rtv_format="<<secondReject.rtv_format<<",bounds="<<secondReject.bounds<<",samples="<<secondReject.samples<<",dsv_format="<<secondReject.dsv_format<<",dsv_size="<<secondReject.dsv_size<<",marker="<<secondReject.marker<<",identity="<<secondReject.identity<<",top=";logTop(secondReject);o<<"}\n";
 o<<"B175n: INTERNAL_COLOR_RTV_DISCOVERY source=OM_RTV0, first_anchor_draw="<<da<<", second_anchor_draw="<<db<<", first_signature={"<<(firstDraw?firstDraw->type:0)<<","<<(firstDraw?firstDraw->a:0)<<","<<(firstDraw?firstDraw->b:0)<<","<<(firstDraw?firstDraw->c:0)<<"}, second_signature={"<<(secondDraw?secondDraw->type:0)<<","<<(secondDraw?secondDraw->a:0)<<","<<(secondDraw?secondDraw->b:0)<<","<<(secondDraw?secondDraw->c:0)<<"}, signature_stable="<<(drawSignatureStable?1:0)<<", first_desc={"<<firstColorDesc.Width<<"x"<<firstColorDesc.Height<<","<<(unsigned)firstColorDesc.Format<<","<<firstColorDesc.BindFlags<<"}, second_desc={"<<secondColorDesc.Width<<"x"<<secondColorDesc.Height<<","<<(unsigned)secondColorDesc.Format<<","<<secondColorDesc.BindFlags<<"}, distinct_cross_frame_ping_pong="<<(colorPingPongStable?1:0)<<", exact_anchor=selected_depth_constants_draw, valid="<<(colorPingPongStable&&drawSignatureStable?1:0)<<"\n";
 if(selectionValid){g_b160w=firstCandidate.width;g_b160h=firstCandidate.height;}
 HRESULT colorPrepareHr=E_FAIL;
 if(selectionValid){D3D11_BUFFER_DESC fastDesc{};cb->GetDesc(&fastDesc);g_b175FastConstantBytes=fastDesc.ByteWidth;g_b175FastConstants=cb;g_b175FastDepth=fastDepth;AcquireSRWLockExclusive(&g_b175FastLock);g_b175FastDrawType=secondDraw->type;g_b175FastDrawA=secondDraw->a;g_b175FastDrawB=secondDraw->b;g_b175FastDrawC=secondDraw->c;g_b175FastColorPair[0]=firstColor;g_b175FastColorPair[1]=secondColor;g_b175FastCurrentColor=g_b175FastColorPair[1];g_b175FastSnapshot.byte_count=(std::min)(sb->byte_count,kSnapshotBytes);g_b175FastSnapshot.resource=cb;std::memcpy(g_b175FastSnapshot.bytes,sb->bytes,g_b175FastSnapshot.byte_count);g_b175FastSnapshot.ready.store(1,std::memory_order_release);ReleaseSRWLockExclusive(&g_b175FastLock);colorPrepareHr=B175PrepareDiscoveredInternalColor(g_b175FastColorPair[0].Get(),path);g_b175FastReady.store(SUCCEEDED(colorPrepareHr),std::memory_order_release);}
 float firstView[16]{},firstProjection[16]{},secondView[16]{},secondProjection[16]{};
 auto makeConstants=[](const Snapshot*s,float*v,float*p){B160M4 view=b160matrix(s,16,false),viewProjection=b160matrix(s,32,false),inverseView{};if(!b160inverse(view,inverseView))return false;for(int r=0;r<4;++r)for(int col=0;col<4;++col){double q=0;for(int k=0;k<4;++k)q+=inverseView.a[r][k]*viewProjection.a[k][col];v[r*4+col]=(float)view.a[r][col];p[r*4+col]=(float)q;if(!std::isfinite(v[r*4+col])||!std::isfinite(p[r*4+col]))return false;}return true;};
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

extern "C" __declspec(dllexport) HRESULT WINAPI B175FastCaptureAtPresent(ID3D11DeviceContext*context,IDXGISwapChain*swap,unsigned sourceId,const char*path)
{
 LARGE_INTEGER b306Begin{},b306AfterPreamble{},b306AfterCamera{},b306AfterDepth{},b306AfterQueue{};QueryPerformanceCounter(&b306Begin);
 auto b306ms=[](const LARGE_INTEGER&a,const LARGE_INTEGER&b){static LARGE_INTEGER f=[](){LARGE_INTEGER x{};QueryPerformanceFrequency(&x);return x;}();return f.QuadPart?((double)(b.QuadPart-a.QuadPart)*1000.0/(double)f.QuadPart):0.0;};
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
 const bool descriptorsMatch=depthDesc.Width==g_b160w&&depthDesc.Height==g_b160h&&depthDesc.Format==DXGI_FORMAT_R24G8_TYPELESS&&constantDesc.ByteWidth==g_b175FastConstantBytes;
 if(!descriptorsMatch){B175DropFastCapture(sourceId,"resource_descriptor_validation_failed");std::ofstream out(path,std::ios::app);out<<"B174y: FAST_CAPTURE_RESOURCE_VALIDATION_FAILURE depth_match="<<(depthDesc.Width==g_b160w&&depthDesc.Height==g_b160h&&depthDesc.Format==DXGI_FORMAT_R24G8_TYPELESS?1:0)<<", constants_match="<<(constantDesc.ByteWidth==g_b175FastConstantBytes?1:0)<<", transient=1, safe_stop=0\n";out.flush();return E_FAIL;}
 const bool b303DirectCameraEstablished=g_b300DirectMode.load(std::memory_order_acquire)&&g_b301DirectCbResource.load(std::memory_order_acquire)!=nullptr;
 const bool b303LegacyConstantStale=g_b175FastStalePresents>=120||updateAgeMs>=5000;
 if(b303LegacyConstantStale&&!b303DirectCameraEstablished){B175DropFastCapture(sourceId,"identified_resource_identity_stale");std::ofstream out(path,std::ios::app);out<<"B174y: IDENTIFIED_RESOURCE_IDENTITY_STALE descriptor_match=1, stale_presents="<<g_b175FastStalePresents<<", update_age_ms="<<updateAgeMs<<", hr=DXGI_ERROR_INVALID_CALL, safe_stop=1, rediscovery=0\n";out.flush();return DXGI_ERROR_INVALID_CALL;}
 if(b303LegacyConstantStale&&b303DirectCameraEstablished&&!g_b303StaleDiscoveryConstantBypassLogged.exchange(true,std::memory_order_acq_rel)){std::ofstream out(path,std::ios::app);out<<"B303: STALE_DISCOVERY_CONSTANT_IGNORED source_frame_id="<<sourceId<<", stale_presents="<<g_b175FastStalePresents<<", update_age_ms="<<updateAgeMs<<", direct_camera_cb_locked=1, selected_camera_resource="<<g_b301DirectCbResource.load(std::memory_order_acquire)<<", action=continue_live_xess_xefg_without_safe_stop, rediscovery_required=0, b301_fast_bypass_unchanged=1, b276_x4_unchanged=1\n";out.flush();}
 Snapshot current{};ComPtr<ID3D11Texture2D>currentColor;AcquireSRWLockShared(&g_b175FastLock);current.byte_count=g_b175FastSnapshot.byte_count;current.resource=g_b175FastSnapshot.resource;std::memcpy(current.bytes,g_b175FastSnapshot.bytes,current.byte_count);current.ready.store(g_b175FastSnapshot.ready.load(std::memory_order_acquire),std::memory_order_release);currentColor=g_b175FastCurrentColor;ReleaseSRWLockShared(&g_b175FastLock);
 D3D11_TEXTURE2D_DESC colorDesc{};if(currentColor)currentColor->GetDesc(&colorDesc);const bool colorValid=currentColor&&b175_fast_color_member(currentColor.Get())&&colorDesc.Width==g_b160w&&colorDesc.Height==g_b160h&&colorDesc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT&&colorDesc.SampleDesc.Count==1;if(!colorValid){B175DropFastCapture(sourceId,"internal_color_identity_or_descriptor_mismatch");return DXGI_ERROR_INVALID_CALL;}
 if(!current.ready.load(std::memory_order_acquire)||current.byte_count<192){B175DropNoConstantSnapshot(sourceId);std::ofstream out(path,std::ios::app);out<<"B175: FAST_CAPTURE_PENDING source_frame_id="<<sourceId<<", snapshot_ready="<<(current.ready.load()?1:0)<<", snapshot_bytes="<<current.byte_count<<", disposition_logged=1, hr="<<(long)E_PENDING<<"\n";out.flush();return E_PENDING;}
 if(!B172BeginFreshFrame(sourceId))return E_FAIL;
 QueryPerformanceCounter(&b306AfterPreamble);
 B160M4 currentVp=b160matrix(&current,32,true),currentInverseVp{},currentView=b160matrix(&current,16,false),inverseView{},currentProjection{};
 bool valid=b160inverse(currentVp,currentInverseVp)&&b160inverse(currentView,inverseView);
 if(valid)for(int r=0;r<4;++r)for(int col=0;col<4;++col){double q=0;for(int k=0;k<4;++k)q+=inverseView.a[r][k]*b160matrix(&current,32,false).a[k][col];currentProjection.a[r][col]=q;}
 float currentVpF[16]{},currentInverseVpF[16]{},currentViewF[16]{},currentProjectionF[16]{};
 if(valid)for(int r=0;r<4;++r)for(int col=0;col<4;++col){const int i=r*4+col;currentVpF[i]=(float)currentVp.a[r][col];currentInverseVpF[i]=(float)currentInverseVp.a[r][col];currentViewF[i]=(float)currentView.a[r][col];currentProjectionF[i]=(float)currentProjection.a[r][col];valid=valid&&std::isfinite(currentVpF[i])&&std::isfinite(currentInverseVpF[i])&&std::isfinite(currentViewF[i])&&std::isfinite(currentProjectionF[i]);}
 // B287: frame-boundary snapshot of main-3D rage_matrices accumulated by DrawIndexed.
 unsigned long long mainDraws=0,qualifying=0,rejected=0,writeSerial=0,directDeriveDraws=0,directRejectDraws=0;unsigned cbIndex=~0u,lastIndexCount=0;ComPtr<ID3D11VertexShader> rageVs;double sw[16]{},swv[16]{},swvp[16]{},svi[16]{},sdv[16]{},sdp[16]{};B292FamilyAccum b292Families[kB292MaxFamilies]{};unsigned b292FamilyCount=0,b292FamilyOverflow=0;
 AcquireSRWLockExclusive(&g_b287FrameLock);mainDraws=g_b287MainPassDraws;qualifying=g_b287QualifyingDraws;rejected=g_b287RejectedScreenDraws;directDeriveDraws=g_b291DirectDeriveDraws;directRejectDraws=g_b291DirectRejectDraws;cbIndex=g_b287LastCbIndex;lastIndexCount=g_b287LastIndexCount;rageVs=g_b287LastVs;writeSerial=g_b287LastWriteSerial;b292FamilyCount=g_b292FamilyCount;b292FamilyOverflow=g_b292FamilyOverflow;for(unsigned fi=0;fi<b292FamilyCount&&fi<kB292MaxFamilies;++fi)b292Families[fi]=g_b292Families[fi];for(unsigned i=0;i<16;++i){sw[i]=g_b287SumWorld[i];swv[i]=g_b287SumWorldView[i];swvp[i]=g_b287SumWorldViewProj[i];svi[i]=g_b287SumViewInverse[i];sdv[i]=g_b291SumDirectView[i];sdp[i]=g_b291SumDirectProjection[i];g_b287SumWorld[i]=g_b287SumWorldView[i]=g_b287SumWorldViewProj[i]=g_b287SumViewInverse[i]=g_b291SumDirectView[i]=g_b291SumDirectProjection[i]=0;}g_b291DirectDeriveDraws=g_b291DirectRejectDraws=0;g_b287MainPassDraws=g_b287QualifyingDraws=g_b287RejectedScreenDraws=0;for(unsigned fi=0;fi<kB292MaxFamilies;++fi)g_b292Families[fi]=B292FamilyAccum{};g_b292FamilyCount=g_b292FamilyOverflow=0;ReleaseSRWLockExclusive(&g_b287FrameLock);
 static double b287PrevView[16]{},b287PrevVp[16]{};static bool b287HavePrev=false;static unsigned b287LogOrdinal=0;
 double b288DerivedView[16]{},b288DerivedVp[16]{},b288DerivedProjection[16]{},b288DerivedInverseVp[16]{};
 bool b288DerivedValid=false,b288HistoryReset=false;double b288ViewDelta=0.0,b288VpDelta=0.0;
 if(qualifying){
  // B293: retain B292's depth-owning VS family lock, then select a coherent camera cluster INSIDE that family.
  // This addresses B292's proven failure mode: 5k-20k View/VP jumps can occur while VS/depth/color identity is unchanged.
  struct B292Candidate{bool valid{};std::uintptr_t key{};unsigned long long draws{};unsigned lastIndexCount{};unsigned cbIndex{~0u};unsigned long long writeSerial{};double view[16]{},projection[16]{},vp[16]{},inverseVp[16]{};};
  B292Candidate candidates[kB292MaxFamilies]{};unsigned candidateCount=0,bestIndex=~0u;unsigned long long bestDraws=0;
  for(unsigned fi=0;fi<b292FamilyCount&&fi<kB292MaxFamilies;++fi){auto&f=b292Families[fi];if(!f.vsKey||!f.draws)continue;B292Candidate c{};c.key=f.vsKey;c.draws=f.draws;c.lastIndexCount=f.lastIndexCount;c.cbIndex=f.cbIndex;c.writeSerial=f.writeSerial;for(unsigned i=0;i<16;++i){c.view[i]=f.sumView[i]/double(f.draws);c.projection[i]=f.sumProjection[i]/double(f.draws);}b287_mul4(c.view,c.projection,c.vp);c.valid=b287_inverse4(c.vp,c.inverseVp);if(!c.valid)continue;candidates[candidateCount]=c;if(c.draws>bestDraws){bestDraws=c.draws;bestIndex=candidateCount;}++candidateCount;}
  static std::uintptr_t lockedFamily=0,pendingFamily=0;static unsigned missingLockedFrames=0,pendingFamilyFrames=0,b292LastSource=0;static bool haveAcceptedCamera=false;static double acceptedView[16]{},acceptedProjection[16]{},acceptedVp[16]{},acceptedInverseVp[16]{};
  static double pendingCutView[16]{},pendingCutProjection[16]{},pendingCutVp[16]{},pendingCutInverseVp[16]{};static unsigned pendingCutFrames=0;static std::uintptr_t pendingCutFamily=0;static unsigned b293HeldFrames=0;static unsigned b294EpochCameraFrames=0;static bool b294StartupCorrectionUsed=false;static unsigned b295PendingStartEpochFrame=0,b295PendingSignatureFrames=0;static unsigned long long b295PendingDraws=0;static unsigned b295PendingLastIndexCount=0;
  const bool newEpoch=!b292LastSource||sourceId>b292LastSource+300||sourceId<=b292LastSource;b292LastSource=sourceId;if(newEpoch){lockedFamily=pendingFamily=0;missingLockedFrames=pendingFamilyFrames=0;haveAcceptedCamera=false;pendingCutFrames=0;pendingCutFamily=0;b293HeldFrames=0;b294EpochCameraFrames=0;b294StartupCorrectionUsed=false;b295PendingStartEpochFrame=0;b295PendingSignatureFrames=0;b295PendingDraws=0;b295PendingLastIndexCount=0;b287HavePrev=false;}++b294EpochCameraFrames;
  int lockedIndex=-1;for(unsigned i=0;i<candidateCount;++i)if(candidates[i].key==lockedFamily){lockedIndex=(int)i;break;}
  bool familyHeld=false,familySwitched=false,familyAcquired=false;unsigned selectedIndex=~0u;
  if(!lockedFamily&&bestIndex!=~0u){lockedFamily=candidates[bestIndex].key;selectedIndex=bestIndex;familyAcquired=true;missingLockedFrames=0;pendingFamily=pendingFamilyFrames=0;}
  else if(lockedIndex>=0){selectedIndex=(unsigned)lockedIndex;missingLockedFrames=0;pendingFamily=pendingFamilyFrames=0;}
  else if(bestIndex!=~0u){++missingLockedFrames;const std::uintptr_t bestKey=candidates[bestIndex].key;if(bestKey==pendingFamily)++pendingFamilyFrames;else{pendingFamily=bestKey;pendingFamilyFrames=1;}if(pendingFamilyFrames>=8&&missingLockedFrames>=8){lockedFamily=bestKey;selectedIndex=bestIndex;familySwitched=true;missingLockedFrames=0;pendingFamily=pendingFamilyFrames=0;}else if(haveAcceptedCamera){familyHeld=true;}}

  // Locate the selected B292 family in the frame-local accumulation so B293 can inspect its individual camera clusters.
  B292FamilyAccum*selectedFamily=nullptr;if(selectedIndex!=~0u){for(unsigned fi=0;fi<b292FamilyCount&&fi<kB292MaxFamilies;++fi)if(b292Families[fi].vsKey==lockedFamily){selectedFamily=&b292Families[fi];break;}}
  struct B293ClusterCandidate{bool valid{};unsigned index{};unsigned long long draws{};unsigned lastIndexCount{};unsigned cbIndex{~0u};unsigned long long writeSerial{};double view[16]{},projection[16]{},vp[16]{},inverseVp[16]{};double acceptedDelta{1.0e300};};
  B293ClusterCandidate clusterCandidates[kB293MaxClustersPerFamily]{};unsigned clusterCandidateCount=0,dominantCluster=~0u,nearestCluster=~0u;unsigned long long dominantClusterDraws=0;double nearestDelta=1.0e300;
  if(selectedFamily){for(unsigned ci=0;ci<selectedFamily->clusterCount&&ci<kB293MaxClustersPerFamily;++ci){auto&cl=selectedFamily->clusters[ci];if(!cl.draws)continue;B293ClusterCandidate cc{};cc.index=ci;cc.draws=cl.draws;cc.lastIndexCount=cl.lastIndexCount;cc.cbIndex=cl.cbIndex;cc.writeSerial=cl.writeSerial;for(unsigned i=0;i<16;++i){cc.view[i]=cl.sumView[i]/double(cl.draws);cc.projection[i]=cl.sumProjection[i]/double(cl.draws);}b287_mul4(cc.view,cc.projection,cc.vp);cc.valid=b287_inverse4(cc.vp,cc.inverseVp);if(!cc.valid)continue;if(haveAcceptedCamera){cc.acceptedDelta=0.0;for(unsigned i=0;i<16;++i)cc.acceptedDelta=(std::max)(cc.acceptedDelta,std::fabs(cc.view[i]-acceptedView[i]));}clusterCandidates[clusterCandidateCount]=cc;if(cc.draws>dominantClusterDraws){dominantClusterDraws=cc.draws;dominantCluster=clusterCandidateCount;}if(cc.acceptedDelta<nearestDelta){nearestDelta=cc.acceptedDelta;nearestCluster=clusterCandidateCount;}++clusterCandidateCount;}}

  // B296: when the locked family goes implausibly far away, search every OTHER observed
  // rage-matrices family for a camera cluster that is still continuous with the last accepted
  // gameplay camera. This keeps camera motion live instead of freezing it for hundreds of frames,
  // while the B294 1500-unit plausibility gate still blocks the catastrophic alternate views.
  B293ClusterCandidate b296RescueCandidate{};bool b296RescueValid=false,b296CrossFamilyRescue=false,b299ProjectionNormalizedRescue=false;std::uintptr_t b296RescueFamily=0;double b296RescueDelta=1.0e300,b298RescueVpDelta=1.0e300,b298RescueProjectionDelta=1.0e300;
  if(haveAcceptedCamera){
   for(unsigned fi=0;fi<b292FamilyCount&&fi<kB292MaxFamilies;++fi){auto&rf=b292Families[fi];if(!rf.vsKey||rf.vsKey==lockedFamily||!rf.clusterCount)continue;
    for(unsigned ci=0;ci<rf.clusterCount&&ci<kB293MaxClustersPerFamily;++ci){auto&cl=rf.clusters[ci];if(!cl.draws)continue;B293ClusterCandidate cc{};cc.index=ci;cc.draws=cl.draws;cc.lastIndexCount=cl.lastIndexCount;cc.cbIndex=cl.cbIndex;cc.writeSerial=cl.writeSerial;for(unsigned i=0;i<16;++i){cc.view[i]=cl.sumView[i]/double(cl.draws);cc.projection[i]=cl.sumProjection[i]/double(cl.draws);}b287_mul4(cc.view,cc.projection,cc.vp);cc.valid=b287_inverse4(cc.vp,cc.inverseVp);if(!cc.valid)continue;cc.acceptedDelta=0.0;double vpDelta=0.0,projectionDelta=0.0;for(unsigned i=0;i<16;++i){cc.acceptedDelta=(std::max)(cc.acceptedDelta,std::fabs(cc.view[i]-acceptedView[i]));vpDelta=(std::max)(vpDelta,std::fabs(cc.vp[i]-acceptedVp[i]));projectionDelta=(std::max)(projectionDelta,std::fabs(cc.projection[i]-acceptedProjection[i]));}
     bool plausible=cc.acceptedDelta<=500.0&&vpDelta<=500.0&&projectionDelta<=500.0;bool normalized=false;
     if(!plausible&&cc.acceptedDelta<=500.0){B293ClusterCandidate normalizedCc=cc;for(unsigned i=0;i<16;++i)normalizedCc.projection[i]=acceptedProjection[i];b287_mul4(normalizedCc.view,normalizedCc.projection,normalizedCc.vp);if(b287_inverse4(normalizedCc.vp,normalizedCc.inverseVp)){double normalizedVpDelta=0.0;for(unsigned i=0;i<16;++i)normalizedVpDelta=(std::max)(normalizedVpDelta,std::fabs(normalizedCc.vp[i]-acceptedVp[i]));if(normalizedVpDelta<=500.0){cc=normalizedCc;vpDelta=normalizedVpDelta;projectionDelta=0.0;plausible=true;normalized=true;}}}
     if(plausible&&cc.acceptedDelta<b296RescueDelta){b296RescueDelta=cc.acceptedDelta;b298RescueVpDelta=vpDelta;b298RescueProjectionDelta=projectionDelta;b296RescueCandidate=cc;b296RescueFamily=rf.vsKey;b296RescueValid=true;b299ProjectionNormalizedRescue=normalized;}}
   }
  }

  bool clusterHeld=false,clusterCutConfirmed=false,clusterStartupCorrectionConfirmed=false,clusterAccepted=false;unsigned chosenCluster=~0u;double rejectedNearestDelta=nearestDelta;std::uintptr_t b296PriorLockedFamily=lockedFamily;
  if(selectedFamily&&clusterCandidateCount){
   if(!haveAcceptedCamera){chosenCluster=dominantCluster;clusterAccepted=chosenCluster!=~0u;pendingCutFrames=0;pendingCutFamily=0;b295PendingStartEpochFrame=0;b295PendingSignatureFrames=0;b295PendingDraws=0;b295PendingLastIndexCount=0;}
   else if(nearestCluster!=~0u&&nearestDelta<=500.0){chosenCluster=nearestCluster;clusterAccepted=true;pendingCutFrames=0;pendingCutFamily=0;b295PendingStartEpochFrame=0;b295PendingSignatureFrames=0;b295PendingDraws=0;b295PendingLastIndexCount=0;}
   else if(nearestCluster!=~0u&&b296RescueValid){clusterAccepted=true;b296CrossFamilyRescue=true;lockedFamily=b296RescueFamily;pendingFamily=0;missingLockedFrames=0;pendingFamilyFrames=0;pendingCutFrames=0;pendingCutFamily=0;b295PendingStartEpochFrame=0;b295PendingSignatureFrames=0;b295PendingDraws=0;b295PendingLastIndexCount=0;}
   else if(nearestCluster!=~0u){
    auto&nc=clusterCandidates[nearestCluster];double pendingDelta=1.0e300;if(pendingCutFrames&&pendingCutFamily==lockedFamily){pendingDelta=0.0;for(unsigned i=0;i<16;++i)pendingDelta=(std::max)(pendingDelta,std::fabs(nc.view[i]-pendingCutView[i]));}
    if(pendingCutFrames&&pendingCutFamily==lockedFamily&&pendingDelta<=250.0){++pendingCutFrames;const unsigned long long lo=(b295PendingDraws*2ull)/3ull,hi=(b295PendingDraws*3ull)/2ull+1ull;const bool sigStable=nc.draws>=lo&&nc.draws<=hi&&(!b295PendingLastIndexCount||!nc.lastIndexCount||nc.lastIndexCount==b295PendingLastIndexCount);if(sigStable)++b295PendingSignatureFrames;else b295PendingSignatureFrames=1;}else{pendingCutFrames=1;pendingCutFamily=lockedFamily;b295PendingStartEpochFrame=b294EpochCameraFrames;b295PendingSignatureFrames=1;}
    b295PendingDraws=nc.draws;b295PendingLastIndexCount=nc.lastIndexCount;
    for(unsigned i=0;i<16;++i){pendingCutView[i]=nc.view[i];pendingCutProjection[i]=nc.projection[i];pendingCutVp[i]=nc.vp[i];pendingCutInverseVp[i]=nc.inverseVp[i];}
    const bool b295StartupSequenceEligible=!b294StartupCorrectionUsed&&b295PendingStartEpochFrame>0&&b295PendingStartEpochFrame<=12;
    const bool b294StartupCorrection=b295StartupSequenceEligible&&pendingCutFrames>=3&&b295PendingSignatureFrames>=3;
    const bool b294PlausiblePersistentCut=pendingCutFrames>=8&&nearestDelta<=1500.0;
    if(b294StartupCorrection||b294PlausiblePersistentCut){chosenCluster=nearestCluster;clusterAccepted=true;clusterCutConfirmed=true;clusterStartupCorrectionConfirmed=b294StartupCorrection;if(b294StartupCorrection)b294StartupCorrectionUsed=true;pendingCutFrames=0;pendingCutFamily=0;b295PendingStartEpochFrame=0;b295PendingSignatureFrames=0;b295PendingDraws=0;b295PendingLastIndexCount=0;}else if(haveAcceptedCamera){clusterHeld=true;}
   }
  }else if(haveAcceptedCamera&&b296RescueValid){clusterAccepted=true;b296CrossFamilyRescue=true;lockedFamily=b296RescueFamily;pendingFamily=0;missingLockedFrames=0;pendingFamilyFrames=0;pendingCutFrames=0;pendingCutFamily=0;b295PendingStartEpochFrame=0;b295PendingSignatureFrames=0;b295PendingDraws=0;b295PendingLastIndexCount=0;}

  if(clusterAccepted&&(chosenCluster!=~0u||b296CrossFamilyRescue)){B293ClusterCandidate&cc=b296CrossFamilyRescue?b296RescueCandidate:clusterCandidates[chosenCluster];for(unsigned i=0;i<16;++i){b288DerivedView[i]=cc.view[i];b288DerivedProjection[i]=cc.projection[i];b288DerivedVp[i]=cc.vp[i];b288DerivedInverseVp[i]=cc.inverseVp[i];}b288DerivedValid=true;cbIndex=cc.cbIndex;lastIndexCount=cc.lastIndexCount;writeSerial=cc.writeSerial;
   if(b287HavePrev)for(unsigned i=0;i<16;++i){b288ViewDelta=(std::max)(b288ViewDelta,std::fabs(b288DerivedView[i]-b287PrevView[i]));b288VpDelta=(std::max)(b288VpDelta,std::fabs(b288DerivedVp[i]-b287PrevVp[i]));}
   // B298 final temporal safety guard: even after family/cluster selection, never submit a catastrophic
   // ViewProjection discontinuity to XeFG. Hold the last accepted camera instead and leave history intact.
   const bool b298SubmitBlocked=haveAcceptedCamera&&b288VpDelta>1500.0;
   if(b298SubmitBlocked){const double blockedVp=b288VpDelta;for(unsigned i=0;i<16;++i){b288DerivedView[i]=acceptedView[i];b288DerivedProjection[i]=acceptedProjection[i];b288DerivedVp[i]=acceptedVp[i];b288DerivedInverseVp[i]=acceptedInverseVp[i];}b288ViewDelta=b288VpDelta=0;b288HistoryReset=false;b296CrossFamilyRescue=false;clusterHeld=true;++b293HeldFrames;std::ofstream q(path,std::ios::app);q<<"B298: CATASTROPHIC_VP_SUBMIT_BLOCKED source_frame_id="<<sourceId<<", blocked_vp_delta="<<blockedVp<<", submit_limit=1500, action=hold_last_accepted_camera, history_reset=0, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";}
   else{
    // A validated B298 rescue is continuous in View, Projection and ViewProjection, so do NOT reset temporal history just for a shader-family handoff.
    b288HistoryReset=familyAcquired||familySwitched||clusterCutConfirmed;for(unsigned i=0;i<16;++i){acceptedView[i]=b288DerivedView[i];acceptedProjection[i]=b288DerivedProjection[i];acceptedVp[i]=b288DerivedVp[i];acceptedInverseVp[i]=b288DerivedInverseVp[i];}haveAcceptedCamera=true;b293HeldFrames=0;
    std::memcpy(b287PrevView,b288DerivedView,sizeof(b288DerivedView));std::memcpy(b287PrevVp,b288DerivedVp,sizeof(b288DerivedVp));b287HavePrev=true;
    if(b296CrossFamilyRescue){std::ofstream r(path,std::ios::app);r<<"B299: CROSS_FAMILY_CONTINUITY_RESCUE source_frame_id="<<sourceId<<", prior_family_vs=0x"<<std::hex<<b296PriorLockedFamily<<", rescued_family_vs=0x"<<lockedFamily<<std::dec<<", rescued_cluster_index="<<cc.index<<", rescued_cluster_draws="<<cc.draws<<", accepted_view_delta="<<cc.acceptedDelta<<", accepted_vp_delta="<<b298RescueVpDelta<<", accepted_projection_delta="<<b298RescueProjectionDelta<<", projection_normalized="<<(b299ProjectionNormalizedRescue?1:0)<<", max_view_delta=500, max_vp_delta=500, max_projection_delta=500, history_reset=0, action="<<(b299ProjectionNormalizedRescue?"follow_camera_view_with_last_safe_projection":"follow_continuous_gameplay_camera_in_alternate_shader_family")<<", consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";if(b299ProjectionNormalizedRescue)r<<"B299: PROJECTION_NORMALIZED_CAMERA_RESCUE source_frame_id="<<sourceId<<", accepted_view_delta="<<cc.acceptedDelta<<", normalized_vp_delta="<<b298RescueVpDelta<<", inherited_projection=last_accepted_gameplay_projection, history_reset=0, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";}
    static unsigned b293AcceptedOrdinal=0;++b293AcceptedOrdinal;if(b296CrossFamilyRescue||(b293AcceptedOrdinal%600)==0){std::ofstream o(path,std::ios::app);o<<"B298: CAMERA_CLUSTER_ACCEPTED source_frame_id="<<sourceId<<", family_vs=0x"<<std::hex<<lockedFamily<<std::dec<<", cluster_index="<<cc.index<<", cluster_draws="<<cc.draws<<", frame_clusters="<<clusterCandidateCount<<", cluster_overflow="<<(b296CrossFamilyRescue?0:(selectedFamily?selectedFamily->clusterOverflow:0))<<", delta_from_prior_accepted="<<cc.acceptedDelta<<", action="<<(b296CrossFamilyRescue?"cross_family_continuity_rescue":clusterStartupCorrectionConfirmed?"startup_camera_lock_correction":clusterCutConfirmed?"plausible_persistent_camera_cut_confirmed":familySwitched?"family_reacquired_cluster_selected":familyAcquired?"initial_dominant_cluster_lock":"nearest_coherent_cluster")<<", history_reset="<<(b288HistoryReset?1:0)<<", consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";}
    if(clusterCutConfirmed){std::ofstream o(path,std::ios::app);if(clusterStartupCorrectionConfirmed)o<<"B298: STARTUP_RELOCK_CONFIRMED source_frame_id="<<sourceId<<", sequence_started_inside_startup_window=1, signature_frames=3, trigger=latched_early_sequence, history_reset=1, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";o<<"B298: CAMERA_CUT_CONFIRMED source_frame_id="<<sourceId<<", family_vs=0x"<<std::hex<<lockedFamily<<std::dec<<", delta_from_prior_accepted="<<cc.acceptedDelta<<", startup_correction="<<(clusterStartupCorrectionConfirmed?1:0)<<", persistent_gate_frames="<<(clusterStartupCorrectionConfirmed?3:8)<<", plausibility_max_delta=1500, action=accept_new_coherent_cluster_and_reset_history, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";}
   }
  }else if((clusterHeld||familyHeld)&&haveAcceptedCamera){if(clusterHeld&&pendingCutFrames==1&&b295PendingStartEpochFrame<=12){std::ofstream a(path,std::ios::app);a<<"B296: STARTUP_RELOCK_SEQUENCE_ARMED source_frame_id="<<sourceId<<", epoch_camera_frame="<<b295PendingStartEpochFrame<<", required_matrix_frames=3, required_signature_frames=3, one_shot=1, b276_xefg_x4_frozen=1\n";}for(unsigned i=0;i<16;++i){b288DerivedView[i]=acceptedView[i];b288DerivedProjection[i]=acceptedProjection[i];b288DerivedVp[i]=acceptedVp[i];b288DerivedInverseVp[i]=acceptedInverseVp[i];}b288DerivedValid=true;b288ViewDelta=b288VpDelta=0;b288HistoryReset=false;++b293HeldFrames;std::ofstream o(path,std::ios::app);o<<"B296: CAMERA_CLUSTER_REJECTED source_frame_id="<<sourceId<<", locked_family_vs=0x"<<std::hex<<lockedFamily<<std::dec<<", frame_clusters="<<clusterCandidateCount<<", nearest_view_delta="<<(std::isfinite(rejectedNearestDelta)?rejectedNearestDelta:-1.0)<<", cross_family_rescue_available="<<(b296RescueValid?1:0)<<", pending_cut_frames="<<pendingCutFrames<<", required_cut_frames=8, plausibility_max_delta=1500, startup_sequence_start_epoch_frame="<<b295PendingStartEpochFrame<<", startup_signature_frames="<<b295PendingSignatureFrames<<", startup_correction_available="<<((!b294StartupCorrectionUsed&&b295PendingStartEpochFrame>0&&b295PendingStartEpochFrame<=12)?1:0)<<", family_held="<<(familyHeld?1:0)<<", action=hold_last_accepted_gameplay_camera, xefg_history_reset=0, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";if(clusterHeld&&pendingCutFrames>=8&&std::isfinite(rejectedNearestDelta)&&rejectedNearestDelta>1500.0)o<<"B296: CAMERA_CUT_PLAUSIBILITY_BLOCKED source_frame_id="<<sourceId<<", nearest_view_delta="<<rejectedNearestDelta<<", plausibility_max_delta=1500, persistent_frames="<<pendingCutFrames<<", action=continue_holding_last_accepted_gameplay_camera, history_reset=0, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";o<<"B296: CAMERA_CLUSTER_HELD source_frame_id="<<sourceId<<", held_frames="<<b293HeldFrames<<", motion_previous_equals_last_accepted=1, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";}
  static unsigned b293ProofOrdinal=0;++b293ProofOrdinal;if(b288DerivedValid&&((b293ProofOrdinal%600)==0)){std::ofstream o(path,std::ios::app);o<<"B296: DEPTH_OWNING_CAMERA_CLUSTER source_frame_id="<<sourceId<<", locked_family_vs=0x"<<std::hex<<lockedFamily<<std::dec<<", observed_families="<<candidateCount<<", frame_clusters="<<clusterCandidateCount<<", qualifying_rage_draws="<<qualifying<<", cluster_held="<<(clusterHeld?1:0)<<", family_held="<<(familyHeld?1:0)<<", view_delta_max="<<b288ViewDelta<<", vp_delta_max="<<b288VpDelta<<", history_reset="<<(b288HistoryReset?1:0)<<", retained_depth_identity=1, retained_color_identity=1, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";}
  ++b287LogOrdinal;if((b287LogOrdinal%600)==0||!b288DerivedValid){std::ofstream b287(path,std::ios::app);b287<<"B287: MAIN_3D_RAGE_FRAME source_frame_id="<<sourceId<<", indexed_draws_total="<<g_b286IndexedDraws.load()<<", main_pass_draws="<<mainDraws<<", qualifying_rage_draws="<<qualifying<<", rejected_screen_draws="<<rejected<<", vs_shader="<<(void*)rageVs.Get()<<", candidate_cb_index="<<cbIndex<<", vs_slot=1, byte_width=256, last_index_count="<<lastIndexCount<<", cb_write_serial="<<writeSerial<<", derive_valid="<<(b288DerivedValid?1:0)<<", consumed_by_runtime="<<(b288DerivedValid?1:0)<<", diagnostic_only=0, b276_runtime_frozen=1\n";if(b288DerivedValid){b287<<"B287: DERIVED_CAMERA_FRAME source_frame_id="<<sourceId<<", view={";for(unsigned i=0;i<16;++i){if(i)b287<<",";b287<<b288DerivedView[i];}b287<<"}, view_projection={";for(unsigned i=0;i<16;++i){if(i)b287<<",";b287<<b288DerivedVp[i];}b287<<"}, view_delta_max="<<b288ViewDelta<<", vp_delta_max="<<b288VpDelta<<", camera_responsive="<<((b288ViewDelta>1e-5||b288VpDelta>1e-5)?1:0)<<", consumed_by_runtime="<<(b288DerivedValid?1:0)<<", diagnostic_only=0, b276_runtime_frozen=1\n";}b287.flush();}
 }
 // B440: B439 runtime proved the direct camera can remain locked while B437 rejects the
 // submitted camera tuple as mathematically invalid. Use B437's final validation as the trigger
 // to reopen the already-proven B430-B435 reacquisition path. The current resource is quarantined;
 // only a different candidate with real same-resource temporal VP movement can seed the normal
 // three-frame B300 lock, and B438/B439 still validate the resulting tuple before motion use.
 static unsigned long long b440LastRearmSource=0;
 // B441: B440 left the expensive Draw/DrawIndexed classifier enabled indefinitely when no
 // replacement camera passed the strict promotion gate. Bound each search to six source frames
 // (twice the existing three-frame seed requirement), then fail open to the prior direct resource
 // and restore GTA's original Draw calls. Retry at most once per 600 source frames.
 static unsigned long long b441ReacquireStartSource=0;
 static ID3D11Resource* b441FallbackResource=nullptr;
 static int b441FallbackCbIndex=-1;
 const unsigned b440InvalidStreak=B440InvalidMotionCameraStreak();
 if(false&&g_b300DirectMode.load(std::memory_order_acquire)&&b440InvalidStreak>=2&&(b440LastRearmSource==0||sourceId>b440LastRearmSource+600)){
  ID3D11Resource* b440RejectedResource=g_b301DirectCbResource.load(std::memory_order_acquire);
  if(b440RejectedResource){
   b441FallbackResource=b440RejectedResource;b441FallbackCbIndex=g_b300DirectCbIndex.load(std::memory_order_acquire);b441ReacquireStartSource=sourceId;
   auto***obj=reinterpret_cast<void***>(context);if(obj&&*obj){void**vt=*obj;vt[12]=(void*)&h_fast_draw_indexed;vt[13]=(void*)&h_fast_draw;g_b300DrawHooksReleased.store(false,std::memory_order_release);}
   g_b430RejectedStaticCameraResource.store(b440RejectedResource,std::memory_order_release);g_b430ResponsiveReacquireActive.store(true,std::memory_order_release);
   g_b300DirectMode.store(false,std::memory_order_release);g_b301DirectCbResource.store(nullptr,std::memory_order_release);g_b300DirectCbIndex.store(-1,std::memory_order_release);
   AcquireSRWLockExclusive(&g_b300DirectWriteLock);g_b300DirectWriteHead=g_b300DirectWriteCount=0;g_b445LatestSelectedSnapshot={};g_b445LatestSelectedResource=nullptr;g_b445LatestSelectedSerial=0;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);AcquireSRWLockExclusive(&g_b489AuthoritativeLock);g_b489Authoritative={};ReleaseSRWLockExclusive(&g_b489AuthoritativeLock);
   b440LastRearmSource=sourceId;
   std::ofstream b440(path,std::ios::app);b440<<"B440: INVALID_DIRECT_CAMERA_REACQUIRE_ARMED source_frame_id="<<sourceId<<", invalid_motion_camera_streak="<<b440InvalidStreak<<", quarantined_resource="<<b440RejectedResource<<", action=restore_draw_classifier_search_temporally_moving_256B_camera, promotion_requirements={different_resource,same_candidate_cross_frame_vp_delta_gt_0.001,three_frame_seed,B438_B439_matrix_validation}, b441_bounded_search_frames=6, b441_retry_cooldown_frames=600, history_reset=0, zero_motion_policy=B437_unchanged, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
  }
 }
 if(false&&g_b430ResponsiveReacquireActive.load(std::memory_order_acquire)&&!g_b300DirectMode.load(std::memory_order_acquire)&&b441FallbackResource&&b441ReacquireStartSource&&sourceId>=b441ReacquireStartSource+6){
  g_b430ResponsiveReacquireActive.store(false,std::memory_order_release);g_b430RejectedStaticCameraResource.store(nullptr,std::memory_order_release);
  g_b301DirectCbResource.store(b441FallbackResource,std::memory_order_release);g_b300DirectCbIndex.store(b441FallbackCbIndex,std::memory_order_release);g_b300DirectMode.store(true,std::memory_order_release);
  AcquireSRWLockExclusive(&g_b300DirectWriteLock);g_b300DirectWriteHead=g_b300DirectWriteCount=0;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);
  auto***obj=reinterpret_cast<void***>(context);if(obj&&*obj){void**vt=*obj;vt[12]=(void*)o_fast_draw_indexed;vt[13]=(void*)o_fast_draw;g_b300DrawHooksReleased.store(true,std::memory_order_release);}
  std::ofstream b441(path,std::ios::app);b441<<"B441: BOUNDED_CAMERA_REACQUIRE_ABORTED source_frame_id="<<sourceId<<", search_frames="<<(sourceId-b441ReacquireStartSource)<<", fallback_resource="<<b441FallbackResource<<", fallback_cb_index="<<b441FallbackCbIndex<<", action=restore_prior_direct_camera_and_original_draw_calls, history_reset=0, b437_zero_motion_safety_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
  b441FallbackResource=nullptr;b441FallbackCbIndex=-1;b441ReacquireStartSource=0;
 }
 // B300: lock a stable camera constant-buffer identity, release Draw/DrawIndexed hooks, and
 // drive camera matrices from recent writes to that CB. Three consecutive accepted frames are
 // required before switching, so startup still uses the proven B299 selector.
 static unsigned b300SeedFrames=0,b300SeedCb=~0u,b300NoCandidateFrames=0,b300DirectFrames=0,b343FalseStaticFrames=0;
 static bool b344UnsafeCameraActive=false;
 bool b344ZeroMotionSafetyThisFrame=false,b344ResetThisFrame=false;
 static bool b300HaveAccepted=false;static double b300AcceptedView[16]{},b300AcceptedProjection[16]{},b300AcceptedVp[16]{},b300AcceptedInverseVp[16]{};
 if(!g_b300DirectMode.load(std::memory_order_acquire)&&b288DerivedValid&&cbIndex!=~0u){
  ID3D11Resource* b430CandidateResource=(cbIndex<kKnownCbs)?reinterpret_cast<ID3D11Resource*>(g_known_cbs[cbIndex].load(std::memory_order_acquire)):nullptr;
  const bool b430Reacquire=g_b430ResponsiveReacquireActive.load(std::memory_order_acquire);
  const bool b430RejectedSame=b430Reacquire&&b430CandidateResource&&b430CandidateResource==g_b430RejectedStaticCameraResource.load(std::memory_order_acquire);
  // B435: B434 proved that a different static resource could look "responsive" only because its
  // absolute matrix was far from the old quarantined baseline. Require actual temporal movement
  // from the SAME replacement candidate before it is allowed to seed B300 direct-camera lock.
  // B498: B497 reopened the classifier correctly, but the legacy single-candidate B435 tracker
  // discarded temporal evidence whenever the selector visited another candidate. Keep bounded
  // per-CB temporal history so interleaved candidates can prove real cross-frame VP movement.
  // Promotion still requires a different non-quarantined 256B resource plus the existing B300
  // three-frame seed. B423 freezing, B458/B453 temporal authority and XeFG presentation are untouched.
  static bool b498HavePriorCandidateVp[kKnownCbs]{};static unsigned b498MovingStreak[kKnownCbs]{};static double b498PriorCandidateVp[kKnownCbs][16]{};
  // B499: once an interleaved candidate has proved temporal movement, its normal B300 three-sample
  // promotion seed must survive visits to other candidates too. Track the seed by CB/resource identity
  // during reacquisition; the legacy global consecutive seed remains unchanged outside reacquisition.
  static unsigned b499PromotionSeed[kKnownCbs]{};static ID3D11Resource* b499PromotionResource[kKnownCbs]{};
  double b435TemporalVpDelta=0.0;bool b435ObservedTemporalMotion=false;
  if(b430Reacquire&&b430CandidateResource&&!b430RejectedSame&&cbIndex<kKnownCbs){
   if(b498HavePriorCandidateVp[cbIndex])for(unsigned i=0;i<16;++i)b435TemporalVpDelta=(std::max)(b435TemporalVpDelta,std::fabs(b288DerivedVp[i]-b498PriorCandidateVp[cbIndex][i]));
   if(b498HavePriorCandidateVp[cbIndex]&&b435TemporalVpDelta>0.001)++b498MovingStreak[cbIndex];else if(b498HavePriorCandidateVp[cbIndex])b498MovingStreak[cbIndex]=0;
   std::memcpy(b498PriorCandidateVp[cbIndex],b288DerivedVp,sizeof(b498PriorCandidateVp[cbIndex]));b498HavePriorCandidateVp[cbIndex]=true;
   b435ObservedTemporalMotion=b498MovingStreak[cbIndex]>=2;
  }
  const bool b430Responsive=!b430Reacquire||b435ObservedTemporalMotion;
  if(b430RejectedSame||!b430Responsive){if(!b430Reacquire){b300SeedFrames=0;b300SeedCb=~0u;}static unsigned b430FilterOrdinal=0;++b430FilterOrdinal;if(b430FilterOrdinal<=3||(b430FilterOrdinal%120)==0){std::ofstream b430(path,std::ios::app);b430<<"B430: RESPONSIVE_CAMERA_REACQUIRE_FILTER source_frame_id="<<sourceId<<", candidate_cb_index="<<cbIndex<<", candidate_resource="<<b430CandidateResource<<", rejected_static_resource="<<g_b430RejectedStaticCameraResource.load(std::memory_order_acquire)<<", same_rejected_resource="<<(b430RejectedSame?1:0)<<", responsive_camera="<<(b430Responsive?1:0)<<", view_delta="<<b288ViewDelta<<", vp_delta="<<b288VpDelta<<", b435_temporal_vp_delta="<<b435TemporalVpDelta<<", b435_temporal_motion_seen="<<(b435ObservedTemporalMotion?1:0)<<", b498_per_candidate_moving_streak="<<((cbIndex<kKnownCbs)?b498MovingStreak[cbIndex]:0)<<", b499_per_candidate_promotion_seed="<<((cbIndex<kKnownCbs)?b499PromotionSeed[cbIndex]:0)<<", action=continue_classifier_until_same_candidate_proves_real_temporal_motion, history_reset=0, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";}}
  if(b430Reacquire&&b435ObservedTemporalMotion&&b300SeedFrames==0){std::ofstream b435(path,std::ios::app);b435<<"B435: TEMPORALLY_RESPONSIVE_CAMERA_PROVEN source_frame_id="<<sourceId<<", cb_index="<<cbIndex<<", resource="<<b430CandidateResource<<", temporal_vp_delta="<<b435TemporalVpDelta<<", requirement=same_candidate_two_consecutive_nonzero_cross_frame_motion_samples, b498_interleaved_candidate_history=1, action=allow_three_frame_direct_lock_seed, history_reset=0, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";}
  if(b430Reacquire){static unsigned b431LogOrdinal=0;++b431LogOrdinal;if(b431LogOrdinal<=3||(b431LogOrdinal%120)==0){const auto skipped=g_b431RejectedClassifierDraws.exchange(0,std::memory_order_acq_rel);std::ofstream b431(path,std::ios::app);b431<<"B431: WIDENED_CAMERA_REACQUIRE source_frame_id="<<sourceId<<", draw_sample_stride=2, rejected_resource_draws_skipped="<<skipped<<", rejected_resource="<<g_b430RejectedStaticCameraResource.load(std::memory_order_acquire)<<", candidate_cb_index="<<cbIndex<<", candidate_resource="<<b430CandidateResource<<", action=search_alternate_256B_camera_resources, history_reset=0, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";}}
  // B432: B431 accidentally kept the selector in reacquisition forever because every reacquire
  // frame skipped B300 seed accumulation. A different responsive resource must be allowed to
  // prove itself for the normal three consecutive frames; rejected/static candidates still reset.
  if(b430CandidateResource&&!b430RejectedSame&&b430Responsive){
   if(b430Reacquire&&cbIndex<kKnownCbs){
    if(b499PromotionResource[cbIndex]!=b430CandidateResource){b499PromotionResource[cbIndex]=b430CandidateResource;b499PromotionSeed[cbIndex]=0;}
    if(b499PromotionSeed[cbIndex]<3)++b499PromotionSeed[cbIndex];
    b300SeedCb=cbIndex;b300SeedFrames=b499PromotionSeed[cbIndex];
    if(b300SeedFrames==1||b300SeedFrames==3){std::ofstream b499(path,std::ios::app);b499<<"B499: INTERLEAVED_PROMOTION_SEED source_frame_id="<<sourceId<<", cb_index="<<cbIndex<<", resource="<<b430CandidateResource<<", per_candidate_seed="<<b300SeedFrames<<", moving_streak="<<b498MovingStreak[cbIndex]<<", action=preserve_three_sample_promotion_across_interleaved_candidates, history_reset=0, protected_b458_b453_unchanged=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
   }else if(cbIndex==b300SeedCb)++b300SeedFrames;else{b300SeedCb=cbIndex;b300SeedFrames=1;}
  }
  if(b300SeedFrames>=3&&b430CandidateResource&&!b430RejectedSame&&b430Responsive&&b300SeedCb==cbIndex){
   for(unsigned i=0;i<16;++i){b300AcceptedView[i]=b288DerivedView[i];b300AcceptedProjection[i]=b288DerivedProjection[i];b300AcceptedVp[i]=b288DerivedVp[i];b300AcceptedInverseVp[i]=b288DerivedInverseVp[i];}b300HaveAccepted=true;b300NoCandidateFrames=0;b300DirectFrames=0;b343FalseStaticFrames=0;b344UnsafeCameraActive=false;
   AcquireSRWLockExclusive(&g_b300DirectWriteLock);g_b300DirectWriteHead=g_b300DirectWriteCount=0;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);
   ID3D11Resource*selectedResource=(cbIndex<kKnownCbs)?reinterpret_cast<ID3D11Resource*>(g_known_cbs[cbIndex].load(std::memory_order_acquire)):nullptr;
   g_b301DirectWriteSerial.store(0,std::memory_order_release);g_b301DirectCbResource.store(selectedResource,std::memory_order_release);g_b300DirectCbIndex.store((int)cbIndex,std::memory_order_release);b484_configure_watchlist(selectedResource);g_b300DirectMode.store(selectedResource!=nullptr,std::memory_order_release);g_b303StaleDiscoveryConstantBypassLogged.store(false,std::memory_order_release);g_b309SelectedUnmaps.store(0,std::memory_order_release);g_b309DuplicateSkips.store(0,std::memory_order_release);g_b309CandidatesCaptured.store(0,std::memory_order_release);g_b309DirectGeneration.fetch_add(1,std::memory_order_acq_rel);g_b324SourceFrameEpoch.fetch_add(1,std::memory_order_acq_rel);g_b324FirstEligibleCaptures.store(0,std::memory_order_release);g_b324DuplicateSelectedUnmapBypasses.store(0,std::memory_order_release);g_b339SecondChanceCaptures.store(0,std::memory_order_release);g_b339SecondChanceBypasses.store(0,std::memory_order_release);t_b324CapturedEpoch=0;t_b339SecondChanceEpoch=0;t_b339FirstViewInverseValid=false;t_b339SecondChanceCaptured=false;g_b308DirectLockTick.store(GetTickCount64(),std::memory_order_release);
   auto***obj=reinterpret_cast<void***>(context);if(obj&&*obj&&selectedResource){void**vt=*obj;
   // B389 freeze-only: retire the B321/B322/B323 diagnostic profilers from live gameplay.
   // B391 freeze-only: retire synchronous per-reset/per-15-frame camera logging from the GTA Present hot path; functional camera/motion/reset behavior is unchanged.
   // Direct camera mode no longer needs Draw classification; restore those calls directly to GTA's original D3D11 methods.
   // Keep the existing functional Map/Unmap and OM hooks in place because they provide the selected camera write and live input identities.
   vt[12]=(void*)o_fast_draw_indexed;vt[13]=(void*)o_fast_draw;vt[20]=(void*)o_b321_draw_indexed_instanced;vt[21]=(void*)o_b321_draw_instanced;vt[41]=(void*)o_b321_dispatch;
   g_b321ProfilerActive.store(false,std::memory_order_release);g_b322ProfilerActive.store(false,std::memory_order_release);g_b323ProfilerActive.store(false,std::memory_order_release);g_b300DrawHooksReleased.store(true,std::memory_order_release);
   }
   if(g_b430ResponsiveReacquireActive.load(std::memory_order_acquire)){std::ofstream b440(path,std::ios::app);b440<<"B440: LIVE_CAMERA_RESOURCE_PROMOTED source_frame_id="<<sourceId<<", cb_index="<<cbIndex<<", resource="<<selectedResource<<", temporal_motion_proven=1, seed_frames="<<b300SeedFrames<<", matrix_validation={B438,B439}, action=promote_into_B300_direct_camera_path, history_reset=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
   std::ofstream b300(path,std::ios::app);b300<<"B300: DIRECT_CAMERA_CB_LOCKED source_frame_id="<<sourceId<<", cb_index="<<cbIndex<<", seed_frames="<<b300SeedFrames<<", draw_hooks_released="<<(g_b300DrawHooksReleased.load()?1:0)<<", map_unmap_tracking=1, om_tracking=1, action=remove_camera_classification_restore_direct_draws_keep_functional_map_unmap_om_hooks, b389_diagnostic_profilers_retired=1, b321_profiler_active="<<(g_b321ProfilerActive.load()?1:0)<<", b322_profiler_active="<<(g_b322ProfilerActive.load()?1:0)<<", b299_safety_retained=1, b276_xefg_x4_frozen=1\n";
   b300<<"B301: SELECTED_CAMERA_CB_RESOURCE_FASTPATH_LOCKED source_frame_id="<<sourceId<<", cb_index="<<cbIndex<<", resource="<<selectedResource<<", direct_mode="<<(selectedResource?1:0)<<", non_selected_map_unmap_behavior=immediate_original_call, generic_cb_lookup_after_lock=0, legacy_snapshot_after_lock=0, b300_camera_selection_unchanged=1, b276_xefg_x4_frozen=1\n";
   if(g_b430ResponsiveReacquireActive.exchange(false,std::memory_order_acq_rel)){g_b432ValidatedReacquireBaselinePending.store(true,std::memory_order_release);std::ofstream b430(path,std::ios::app);b430<<"B430: RESPONSIVE_CAMERA_REACQUIRED source_frame_id="<<sourceId<<", cb_index="<<cbIndex<<", resource="<<selectedResource<<", seed_frames="<<b300SeedFrames<<", view_delta="<<b288ViewDelta<<", vp_delta="<<b288VpDelta<<", action=lock_different_responsive_camera, history_reset=0, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";std::ofstream b432(path,std::ios::app);b432<<"B432: VALIDATED_CAMERA_REACQUIRE_BASELINE_ARMED source_frame_id="<<sourceId<<", cb_index="<<cbIndex<<", resource="<<selectedResource<<", seed_frames="<<b300SeedFrames<<", action=authorize_one_final_submit_baseline_transition, history_reset=0, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";g_b430RejectedStaticCameraResource.store(nullptr,std::memory_order_release);}
  }
 }
 // B443: provenance for a derived camera tuple. A tuple is snapshot-coherent only when View, Projection, VP and inverse-VP were all derived from the same 256-byte GTA write.
 // B449 diagnostic-only: fingerprint the exact selected 256-byte write and each derived camera component
 // across B305/B445 -> b288Derived -> RAW_PRE_B443. No selection, matrix, reset, presentation or XeFG behavior changes.
 auto b449BytesFingerprint=[](const unsigned char*bytes,unsigned count)->unsigned long long{unsigned long long h=1469598103934665603ULL;for(unsigned i=0;i<count;++i){h^=(unsigned long long)bytes[i];h*=1099511628211ULL;}return h;};
 auto b449DoubleComponentFingerprint=[&](const double*part)->unsigned long long{float f[16]{};for(unsigned i=0;i<16;++i)f[i]=(float)part[i];return b449BytesFingerprint((const unsigned char*)f,64);};
 auto b449DoubleTupleFingerprint=[&](const double*vp,const double*inv,const double*view,const double*proj)->unsigned long long{unsigned long long h=1469598103934665603ULL;const double*parts[4]={vp,inv,view,proj};for(const double*part:parts){float f[16]{};for(unsigned i=0;i<16;++i)f[i]=(float)part[i];const unsigned char*b=(const unsigned char*)f;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}}return h;};
 bool b443DerivedSnapshotCoherent=false; unsigned long long b443DerivedWriteSerial=0; ID3D11Resource* b443DerivedResource=nullptr;
 bool b446QualifiedAtomicTuple=false; unsigned long long b446QualifiedSerial=0; ID3D11Resource* b446QualifiedResource=nullptr; double b446View[16]{},b446Projection[16]{},b446Vp[16]{},b446InvVp[16]{};
 if(g_b300DirectMode.load(std::memory_order_acquire)&&b300HaveAccepted){
  // B309: hooks stay installed continuously; selected camera writes are deduplicated in Unmap.
  B300RawCameraWrite writes[kB300DirectWriteRing]{};unsigned wc=0;static unsigned long long b445LastHandoffSerial=0;ID3D11Resource* b445HandoffResource=nullptr;unsigned long long b445HandoffSerial=0;B300RawCameraWrite b445Handoff{};
  AcquireSRWLockExclusive(&g_b300DirectWriteLock);wc=g_b300DirectWriteCount;const unsigned head=g_b300DirectWriteHead;for(unsigned n=0;n<wc;++n){const unsigned src=(head-wc+n)%kB300DirectWriteRing;writes[n]=g_b300DirectWrites[src];}g_b300DirectWriteCount=0;b445Handoff=g_b445LatestSelectedSnapshot;b445HandoffResource=g_b445LatestSelectedResource;b445HandoffSerial=g_b445LatestSelectedSerial;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);
  // B490 diagnostic-only: preserve the raw ring count before B445 may inject the latest handoff.
  // This lets the sparse capture-boundary diagnostic distinguish the actual intra-frame write phases
  // from the preserved B445 injection without changing the candidate set or selection policy.
  const unsigned b490RawRingWriteCount=wc;
  // B445: if legacy signature dedup suppressed this exact write, inject the preserved atomic snapshot into
  // this frame's bounded candidate set once. This keeps B444's same-write derivation intact without scanning.
  bool b445InjectedHandoff=false;if(b445HandoffSerial!=0&&b445HandoffSerial!=b445LastHandoffSerial&&b445HandoffResource==g_b301DirectCbResource.load(std::memory_order_acquire)){bool already=false;for(unsigned n=0;n<wc;++n)if(writes[n].serial==b445HandoffSerial){already=true;break;}if(!already&&wc<kB300DirectWriteRing){writes[wc++]=b445Handoff;b445InjectedHandoff=true;}b445LastHandoffSerial=b445HandoffSerial;}
  const unsigned long long b309SelectedUnmaps=g_b309SelectedUnmaps.exchange(0,std::memory_order_acq_rel),b309DuplicateSkips=g_b309DuplicateSkips.exchange(0,std::memory_order_acq_rel),b309CandidatesCaptured=g_b309CandidatesCaptured.exchange(0,std::memory_order_acq_rel);
  const unsigned long long b324Captures=g_b324FirstEligibleCaptures.exchange(0,std::memory_order_acq_rel),b324Bypasses=g_b324DuplicateSelectedUnmapBypasses.exchange(0,std::memory_order_acq_rel);
  static unsigned b449HandoffOrdinal=0;const bool b449LogThis=((++b449HandoffOrdinal)<=12)||(b449HandoffOrdinal%120)==0;
  // B490: correlate the already-bounded selected-resource ring phases immediately before B445/B480 authority.
  // No new Map/Unmap work, Draw scan, selection, matrix, history, freezing, or XeFG behavior is introduced.
  static unsigned b490Ordinal=0;const bool b490LogThis=((++b490Ordinal)<=24)||(b490Ordinal%30)==0;
  if(b490LogThis){
   static unsigned long long b490PrevPhaseFp[4]{};static bool b490PrevPhaseValid[4]{};
   unsigned long long phaseFp[4]{};unsigned long long phaseSerial[4]{};int handoffRawIndex=-1;
   const unsigned phaseCount=(std::min)(b490RawRingWriteCount,4u);
   for(unsigned n=0;n<phaseCount;++n){phaseFp[n]=b449BytesFingerprint(writes[n].bytes,256);phaseSerial[n]=writes[n].serial;if(writes[n].serial==b445HandoffSerial)handoffRawIndex=(int)n;}
   B489AuthoritativeHandoffSnapshot b490Auth{};B489GetAuthoritativeHandoff(&b490Auth);
   const unsigned long long publishedFp=b490Auth.publishedValid?b449BytesFingerprint(b490Auth.publishedBytes,256):0ULL;
   const unsigned long long consumedFp=b490Auth.consumedValid?b449BytesFingerprint(b490Auth.consumedBytes,256):0ULL;
   std::ofstream b490(path,std::ios::app);b490<<"B490: SELECTED_RING_PHASE_TO_HANDOFF_CORRELATION source_frame_id="<<sourceId<<", raw_ring_writes="<<b490RawRingWriteCount<<", bounded_phase_count="<<phaseCount<<", phase_serials={";
   for(unsigned n=0;n<phaseCount;++n)b490<<(n?",":"")<<phaseSerial[n];b490<<"}, phase_fingerprints={";for(unsigned n=0;n<phaseCount;++n)b490<<(n?",":"")<<phaseFp[n];
   b490<<"}, phase_changed_vs_same_slot_previous={";for(unsigned n=0;n<phaseCount;++n)b490<<(n?",":"")<<(b490PrevPhaseValid[n]&&phaseFp[n]!=b490PrevPhaseFp[n]?1:0);
   b490<<"}, b445_handoff_serial="<<b445HandoffSerial<<", b445_handoff_fingerprint="<<(b445HandoffSerial?b449BytesFingerprint(b445Handoff.bytes,256):0ULL)<<", handoff_raw_ring_index="<<handoffRawIndex<<", handoff_was_injected="<<(b445InjectedHandoff?1:0)<<", post_injection_candidate_count="<<wc<<", b489_published_source_frame="<<b490Auth.publishedSourceFrame<<", b489_published_serial="<<b490Auth.publishedSerial<<", b489_published_fingerprint="<<publishedFp<<", b489_consumed_source_frame="<<b490Auth.consumedSourceFrame<<", b489_consumed_serial="<<b490Auth.consumedSerial<<", b489_consumed_fingerprint="<<consumedFp<<", published_vs_handoff_match="<<((b445HandoffSerial&&b490Auth.publishedValid&&b490Auth.publishedSerial==b445HandoffSerial&&publishedFp==b449BytesFingerprint(b445Handoff.bytes,256))?1:0)<<", consumed_is_current_handoff="<<((b445HandoffSerial&&b490Auth.consumedValid&&b490Auth.consumedSerial==b445HandoffSerial&&consumedFp==b449BytesFingerprint(b445Handoff.bytes,256))?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0, existing_ring_unchanged=1, camera_selection_unchanged=1, broad_draw_scan=0, b480_authority_unchanged=1, b474_history_policy_unchanged=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
   for(unsigned n=0;n<4;++n){if(n<phaseCount){b490PrevPhaseFp[n]=phaseFp[n];b490PrevPhaseValid[n]=true;}else b490PrevPhaseValid[n]=false;}
  }
  const unsigned long long b456Maps=g_b456SelectedMaps.exchange(0,std::memory_order_acq_rel),b456Unmaps=g_b456SelectedUnmaps.exchange(0,std::memory_order_acq_rel),b456FullChanges=g_b456FullTupleChanges.exchange(0,std::memory_order_acq_rel),b456ViewChanges=g_b456ViewInverseChanges.exchange(0,std::memory_order_acq_rel),b456Published=g_b456PublishedWrites.exchange(0,std::memory_order_acq_rel);
  const unsigned b475LaneMask=g_b475ChangedLaneMask.exchange(0,std::memory_order_acq_rel);const unsigned long long b475ChangedWrites=g_b475ChangedWrites.exchange(0,std::memory_order_acq_rel),b475LatestChangedFp=g_b475LatestChangedSnapshotFp.load(std::memory_order_acquire);
  static unsigned long long b456PriorHandoffSerial=0,b456PriorSnapshotFp=0;const unsigned long long b456SnapshotFp=b445HandoffSerial?b449BytesFingerprint(b445Handoff.bytes,256):0ULL;const bool b456SerialAdvanced=b445HandoffSerial>b456PriorHandoffSerial;const bool b456BytesAdvanced=b456SnapshotFp!=0&&b456SnapshotFp!=b456PriorSnapshotFp;
  if(b449LogThis){std::ofstream b475(path,std::ios::app);b475<<"B475: SELECTED_RESOURCE_MATRIX_LAYOUT_DIAGNOSTIC source_frame_id="<<sourceId<<", selected_resource="<<g_b301DirectCbResource.load(std::memory_order_acquire)<<", changed_writes="<<b475ChangedWrites<<", changed_lane_mask_16B=0x"<<std::hex<<b475LaneMask<<std::dec<<", region_0_63_changed="<<((b475LaneMask&0x000Fu)?1:0)<<", region_64_127_changed="<<((b475LaneMask&0x00F0u)?1:0)<<", region_128_191_changed="<<((b475LaneMask&0x0F00u)?1:0)<<", region_192_255_changed="<<((b475LaneMask&0xF000u)?1:0)<<", latest_changed_snapshot_fingerprint="<<b475LatestChangedFp<<", handoff_snapshot_fingerprint="<<(b445HandoffSerial?b449BytesFingerprint(b445Handoff.bytes,256):0ULL)<<", diagnostic_only=1, runtime_behavior_changed=0, selected_resource_unchanged=1, broad_draw_scan=0, b474_history_policy_unchanged=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
  // B477 diagnostic-only: inspect every already-captured selected-resource write drained for this source frame.
  // The runtime candidate set and selection are untouched; this only reveals which intra-frame write actually advances the camera matrices.
  if(b449LogThis&&wc){
   std::ofstream b477(path,std::ios::app);
   const unsigned long long b477HandoffFp=b445HandoffSerial?b449BytesFingerprint(b445Handoff.bytes,256):0ULL;
   b477<<"B477: SELECTED_RESOURCE_INTRAFRAME_WRITE_DIAGNOSTIC source_frame_id="<<sourceId<<", resource="<<g_b301DirectCbResource.load(std::memory_order_acquire)<<", writes_drained="<<wc<<", handoff_serial="<<b445HandoffSerial<<", handoff_fp="<<b477HandoffFp<<", entries={";
   unsigned long long b477PriorFp=0;unsigned char b477PriorBytes[256]{};bool b477HavePrior=false;
   for(unsigned n=0;n<wc;++n){
    const unsigned long long fp=b449BytesFingerprint(writes[n].bytes,256);double fullMax=0.0,fullRms=0.0;double regionMax[4]{},regionRms[4]{};
    if(b477HavePrior){double ss=0.0;for(unsigned i=0;i<64;++i){float a=0,b=0;std::memcpy(&a,writes[n].bytes+i*4,4);std::memcpy(&b,b477PriorBytes+i*4,4);const double d=(double)a-(double)b;fullMax=(std::max)(fullMax,std::fabs(d));ss+=d*d;const unsigned r=i/16;regionMax[r]=(std::max)(regionMax[r],std::fabs(d));regionRms[r]+=d*d;}fullRms=std::sqrt(ss/64.0);for(unsigned r=0;r<4;++r)regionRms[r]=std::sqrt(regionRms[r]/16.0);}
    b477<<(n?";":"")<<"[n="<<n<<",serial="<<writes[n].serial<<",fp="<<fp<<",same_as_prior="<<(b477HavePrior&&fp==b477PriorFp?1:0)<<",same_as_handoff="<<(b477HandoffFp&&fp==b477HandoffFp?1:0)<<",delta_max="<<fullMax<<",delta_rms="<<fullRms<<",region_max={"<<regionMax[0]<<","<<regionMax[1]<<","<<regionMax[2]<<","<<regionMax[3]<<"},region_rms={"<<regionRms[0]<<","<<regionRms[1]<<","<<regionRms[2]<<","<<regionRms[3]<<"}]";
    std::memcpy(b477PriorBytes,writes[n].bytes,256);b477PriorFp=fp;b477HavePrior=true;
   }
   b477<<"}, diagnostic_only=1, runtime_behavior_changed=0, existing_b300_candidate_set_unchanged=1, camera_selection_unchanged=1, broad_draw_scan=0, b476_relationship_diagnostic_retained=1, b474_history_policy_unchanged=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, motion_scale_unchanged=1, motion_direction_unchanged=1, xefg_x4_unchanged=1\n";
  }
  // B476 diagnostic-only: fingerprint and compare each 64-byte matrix-sized region of the exact
  // selected B445 handoff. This does not alter camera selection, reconstruction, temporal history, or XeFG.
  if(b449LogThis&&b445HandoffSerial){
   float b476Region[4][16]{};for(unsigned r=0;r<4;++r)for(unsigned i=0;i<16;++i)std::memcpy(&b476Region[r][i],b445Handoff.bytes+r*64+i*4,4);
   unsigned long long b476Fp[4]{};for(unsigned r=0;r<4;++r)b476Fp[r]=b449BytesFingerprint(b445Handoff.bytes+r*64,64);
   static bool b476HavePrevious=false;static float b476Previous[4][16]{};double b476Max[4]{},b476Rms[4]{};
   for(unsigned r=0;r<4;++r){double ss=0.0;for(unsigned i=0;i<16;++i){const double d=b476HavePrevious?((double)b476Region[r][i]-(double)b476Previous[r][i]):0.0;b476Max[r]=(std::max)(b476Max[r],std::fabs(d));ss+=d*d;}b476Rms[r]=std::sqrt(ss/16.0);}
   auto b476InverseResidual=[](const float*a,const float*b)->double{double worstA=0.0,worstB=0.0;for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double ab=0.0,ba=0.0;for(unsigned k=0;k<4;++k){ab+=(double)a[r*4+k]*(double)b[k*4+c];ba+=(double)b[r*4+k]*(double)a[k*4+c];}const double target=r==c?1.0:0.0;if(!std::isfinite(ab)||!std::isfinite(ba))return 960.0;worstA=(std::max)(worstA,std::fabs(ab-target));worstB=(std::max)(worstB,std::fabs(ba-target));}return (std::min)(worstA,worstB);};
   double b476DerivedVp[16]{};bool b476DerivedValid=true;double b476R1d[16]{},b476R2d[16]{},b476R3d[16]{},b476View[16]{},b476InvWv[16]{},b476Projection[16]{};for(unsigned i=0;i<16;++i){b476R1d[i]=b476Region[1][i];b476R2d[i]=b476Region[2][i];b476R3d[i]=b476Region[3][i];}
   if(!b287_inverse4(b476R3d,b476View)||!b287_inverse4(b476R1d,b476InvWv))b476DerivedValid=false;else{b287_mul4(b476InvWv,b476R2d,b476Projection);b287_mul4(b476View,b476Projection,b476DerivedVp);}
   double b476DerivedVsRawMax[4]{},b476DerivedVsRawRms[4]{};for(unsigned r=0;r<4;++r){double ss=0.0;for(unsigned i=0;i<16;++i){const double d=b476DerivedValid?(b476DerivedVp[i]-(double)b476Region[r][i]):0.0;b476DerivedVsRawMax[r]=(std::max)(b476DerivedVsRawMax[r],std::fabs(d));ss+=d*d;}b476DerivedVsRawRms[r]=std::sqrt(ss/16.0);}
   std::ofstream b476(path,std::ios::app);b476<<"B476: SELECTED_RESOURCE_64B_MATRIX_RELATIONSHIP_DIAGNOSTIC source_frame_id="<<sourceId<<", resource="<<b445HandoffResource<<", write_serial="<<b445HandoffSerial<<", region_fp={"<<b476Fp[0]<<","<<b476Fp[1]<<","<<b476Fp[2]<<","<<b476Fp[3]<<"}, region_cross_frame_max={"<<b476Max[0]<<","<<b476Max[1]<<","<<b476Max[2]<<","<<b476Max[3]<<"}, region_cross_frame_rms={"<<b476Rms[0]<<","<<b476Rms[1]<<","<<b476Rms[2]<<","<<b476Rms[3]<<"}, inverse_pair_residuals={r0_r1:"<<b476InverseResidual(b476Region[0],b476Region[1])<<",r0_r2:"<<b476InverseResidual(b476Region[0],b476Region[2])<<",r0_r3:"<<b476InverseResidual(b476Region[0],b476Region[3])<<",r1_r2:"<<b476InverseResidual(b476Region[1],b476Region[2])<<",r1_r3:"<<b476InverseResidual(b476Region[1],b476Region[3])<<",r2_r3:"<<b476InverseResidual(b476Region[2],b476Region[3])<<"}, existing_b444_derivation_valid="<<(b476DerivedValid?1:0)<<", derived_vp_vs_raw_max={"<<b476DerivedVsRawMax[0]<<","<<b476DerivedVsRawMax[1]<<","<<b476DerivedVsRawMax[2]<<","<<b476DerivedVsRawMax[3]<<"}, derived_vp_vs_raw_rms={"<<b476DerivedVsRawRms[0]<<","<<b476DerivedVsRawRms[1]<<","<<b476DerivedVsRawRms[2]<<","<<b476DerivedVsRawRms[3]<<"}, diagnostic_only=1, runtime_behavior_changed=0, b475_layout_diagnostic_retained=1, b474_history_policy_unchanged=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
   for(unsigned r=0;r<4;++r)std::memcpy(b476Previous[r],b476Region[r],64);b476HavePrevious=true;
  }
  if(b449LogThis){std::ofstream b456(path,std::ios::app);b456<<"B456: SELECTED_UNMAP_CAMERA_PROVENANCE source_frame_id="<<sourceId<<", selected_resource="<<g_b301DirectCbResource.load(std::memory_order_acquire)<<", selected_maps="<<b456Maps<<", eligible_selected_unmaps="<<b456Unmaps<<", full_tuple_changes="<<b456FullChanges<<", view_inverse_changes="<<b456ViewChanges<<", published_writes="<<b456Published<<", latest_handoff_serial="<<b445HandoffSerial<<", prior_handoff_serial="<<b456PriorHandoffSerial<<", serial_advanced="<<(b456SerialAdvanced?1:0)<<", snapshot_fingerprint="<<b456SnapshotFp<<", snapshot_bytes_advanced="<<(b456BytesAdvanced?1:0)<<", frame_epoch="<<g_b324SourceFrameEpoch.load(std::memory_order_acquire)<<", diagnostic_only=1, unmap_file_io=0, present_ring_rescan=0, b453_atomic_temporal_rebase_retained=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}if(b445HandoffSerial){b456PriorHandoffSerial=b445HandoffSerial;b456PriorSnapshotFp=b456SnapshotFp;}
  if(b449LogThis){std::ofstream b449(path,std::ios::app);b449<<"B449: UPSTREAM_CAMERA_PROVENANCE source_frame_id="<<sourceId<<", boundary=B445_HANDOFF, resource="<<b445HandoffResource<<", write_serial="<<b445HandoffSerial<<", snapshot_fingerprint="<<(b445HandoffSerial?b449BytesFingerprint(b445Handoff.bytes,256):0ULL)<<", injected="<<(b445InjectedHandoff?1:0)<<", candidates_drained="<<wc<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}
  if(b300DirectFrames<5||((b300DirectFrames+1)%60)==0){std::ofstream b324(path,std::ios::app);b324<<"B324: SELECTED_CAMERA_ONCE_PER_SOURCE_FRAME source_frame_id="<<sourceId<<", first_eligible_captures="<<b324Captures<<", duplicate_selected_unmap_bypasses="<<b324Bypasses<<", candidates_captured="<<b309CandidatesCaptured<<", candidates_drained="<<wc<<", b445_handoff_injected="<<(b445InjectedHandoff?1:0)<<", b445_handoff_serial="<<b445HandoffSerial<<", frame_epoch="<<g_b324SourceFrameEpoch.load(std::memory_order_acquire)<<", first_eligible_only=0, b339_second_chance_on_view_change=1, second_chance_total="<<g_b339SecondChanceCaptures.load(std::memory_order_relaxed)<<", duplicate_action=immediate_original_unmap, last_safe_hold_if_no_candidate=1, xefg_x4_unchanged=1\n";}
  if(b300DirectFrames<5||((b300DirectFrames+1)%60)==0){std::ofstream b309(path,std::ios::app);b309<<"B309: DIRECT_CAMERA_WRITE_DEDUP source_frame_id="<<sourceId<<", selected_unmaps="<<b309SelectedUnmaps<<", duplicate_signature_skips="<<b309DuplicateSkips<<", candidates_captured="<<b309CandidatesCaptured<<", candidates_drained="<<wc<<", signature=gViewInverse_plus_192_64B, periodic_refresh_every=256, bounded_ring="<<kB300DirectWriteRing<<", selected_unmap_full_matrix_validation=0, validation_location=present_once_per_source_frame, map_unmap_hooks_remain_installed=1, b308_per_call_split_disabled_after_direct_lock=1\n";}
  bool found=false;double bestScore=1.0e300,bestViewDelta=1.0e300,bestProjectionDelta=1.0e300,bestVpDelta=1.0e300;double bestView[16]{},bestProjection[16]{},bestVp[16]{},bestInvVp[16]{};unsigned long long bestSerial=0;
  for(unsigned n=0;n<wc;++n){double m[64]{};bool finite=true;for(unsigned i=0;i<64;++i){float f=0;std::memcpy(&f,writes[n].bytes+i*4,4);m[i]=f;if(!std::isfinite(m[i])||std::fabs(m[i])>10000000.0){finite=false;break;}}if(!finite||b287_identity4d(m+16)&&b287_identity4d(m+48))continue;double view[16]{},invWv[16]{},projection[16]{},vp[16]{},invVp[16]{};if(!b287_inverse4(m+48,view)||!b287_inverse4(m+16,invWv))continue;b287_mul4(invWv,m+32,projection);b287_mul4(view,projection,vp);if(!b287_inverse4(vp,invVp))continue;double vd=0,pd=0,vpd=0;for(unsigned i=0;i<16;++i){vd=(std::max)(vd,std::fabs(view[i]-b300AcceptedView[i]));pd=(std::max)(pd,std::fabs(projection[i]-b300AcceptedProjection[i]));vpd=(std::max)(vpd,std::fabs(vp[i]-b300AcceptedVp[i]));}if(vd>500.0||pd>2.0||vpd>500.0)continue;const bool b340MovingNow=vd>0.001||vpd>0.001;const double score=vd+pd*64.0+vpd*0.05+(b340MovingNow?0.0:1000000000.0);if(score<bestScore){bestScore=score;bestViewDelta=vd;bestProjectionDelta=pd;bestVpDelta=vpd;bestSerial=writes[n].serial;std::memcpy(bestView,view,sizeof(view));std::memcpy(bestProjection,projection,sizeof(projection));std::memcpy(bestVp,vp,sizeof(vp));std::memcpy(bestInvVp,invVp,sizeof(invVp));found=true;}}
  bool b304ProjectionNormalized=false,b305MovingWriteSelected=false;unsigned b305MovingCandidates=0,b305StaticCandidates=0;
  // B444 ghosting fix: qualify an atomic RAGE camera tuple before the B424 projection-normalized
  // fallback can inherit Projection from a different write. All four consumed camera components
  // are derived from the same 256-byte selected-resource write: View=inverse(gViewInverse),
  // Projection=inverse(gWorldView)*gWorldViewProj, VP=View*Projection, inverseVP=inverse(VP).
  // The old pd<=2 continuity gate is deliberately not used here; instead require a bounded VP
  // transition and three consecutive same-resource atomic snapshots with real cross-frame motion.
  {
   struct B444AtomicCandidate{bool valid{};double score{1.0e300},vd{},pd{},vpd{};unsigned long long serial{};double view[16]{},projection[16]{},vp[16]{},invVp[16]{};};
   B444AtomicCandidate atomic{};
   for(unsigned n=0;n<wc;++n){double m[64]{};bool finite=true;for(unsigned i=0;i<64;++i){float f=0;std::memcpy(&f,writes[n].bytes+i*4,4);m[i]=f;if(!std::isfinite(m[i])||std::fabs(m[i])>10000000.0){finite=false;break;}}if(!finite||b287_identity4d(m+16)&&b287_identity4d(m+48))continue;double view[16]{},invWv[16]{},projection[16]{},vp[16]{},invVp[16]{};if(!b287_inverse4(m+48,view)||!b287_inverse4(m+16,invWv))continue;b287_mul4(invWv,m+32,projection);b287_mul4(view,projection,vp);if(!b287_inverse4(vp,invVp))continue;double vd=0,pd=0,vpd=0,maxAbs=0;bool tupleFinite=true;for(unsigned i=0;i<16;++i){vd=(std::max)(vd,std::fabs(view[i]-b300AcceptedView[i]));pd=(std::max)(pd,std::fabs(projection[i]-b300AcceptedProjection[i]));vpd=(std::max)(vpd,std::fabs(vp[i]-b300AcceptedVp[i]));const double vals[4]={view[i],projection[i],vp[i],invVp[i]};for(double v:vals){tupleFinite=tupleFinite&&std::isfinite(v);if(std::isfinite(v))maxAbs=(std::max)(maxAbs,std::fabs(v));}}if(!tupleFinite||maxAbs>10000000.0||vd>1500.0||vpd>1500.0)continue;const double score=vd+vpd*0.05+pd*0.001;if(!atomic.valid||score<atomic.score||(std::fabs(score-atomic.score)<1.0e-9&&writes[n].serial>atomic.serial)){atomic.valid=true;atomic.score=score;atomic.vd=vd;atomic.pd=pd;atomic.vpd=vpd;atomic.serial=writes[n].serial;std::memcpy(atomic.view,view,sizeof(view));std::memcpy(atomic.projection,projection,sizeof(projection));std::memcpy(atomic.vp,vp,sizeof(vp));std::memcpy(atomic.invVp,invVp,sizeof(invVp));}}
   static double b444PriorAtomicVp[16]{};static bool b444HavePriorAtomic=false;static unsigned b444AtomicMotionFrames=0;static ID3D11Resource* b444PriorResource=nullptr;
   ID3D11Resource* b444Resource=g_b301DirectCbResource.load(std::memory_order_acquire);double b444TemporalVpDelta=0;if(atomic.valid&&b444HavePriorAtomic&&b444PriorResource==b444Resource){for(unsigned i=0;i<16;++i)b444TemporalVpDelta=(std::max)(b444TemporalVpDelta,std::fabs(atomic.vp[i]-b444PriorAtomicVp[i]));}
   if(atomic.valid){if(b444HavePriorAtomic&&b444PriorResource==b444Resource&&b444TemporalVpDelta>0.001&&b444TemporalVpDelta<=500.0)++b444AtomicMotionFrames;else if(!b444HavePriorAtomic||b444PriorResource!=b444Resource)b444AtomicMotionFrames=1;else if(b444TemporalVpDelta>500.0)b444AtomicMotionFrames=0;std::memcpy(b444PriorAtomicVp,atomic.vp,sizeof(b444PriorAtomicVp));b444HavePriorAtomic=true;b444PriorResource=b444Resource;}else{b444AtomicMotionFrames=0;}
   if(atomic.valid&&b444AtomicMotionFrames>=3){found=true;bestScore=atomic.score;bestViewDelta=atomic.vd;bestProjectionDelta=atomic.pd;bestVpDelta=atomic.vpd;bestSerial=atomic.serial;std::memcpy(bestView,atomic.view,sizeof(bestView));std::memcpy(bestProjection,atomic.projection,sizeof(bestProjection));std::memcpy(bestVp,atomic.vp,sizeof(bestVp));std::memcpy(bestInvVp,atomic.invVp,sizeof(bestInvVp));b446QualifiedAtomicTuple=true;b446QualifiedSerial=atomic.serial;b446QualifiedResource=b444Resource;std::memcpy(b446View,atomic.view,sizeof(b446View));std::memcpy(b446Projection,atomic.projection,sizeof(b446Projection));std::memcpy(b446Vp,atomic.vp,sizeof(b446Vp));std::memcpy(b446InvVp,atomic.invVp,sizeof(b446InvVp));static unsigned b444Accepted=0;if((++b444Accepted<=3)||(b444Accepted%120)==0){std::ofstream b444(path,std::ios::app);b444<<"B444: ATOMIC_RAGE_CAMERA_SNAPSHOT_QUALIFIED source_frame_id="<<sourceId<<", resource="<<b444Resource<<", write_serial="<<atomic.serial<<", qualification_frames="<<b444AtomicMotionFrames<<", temporal_vp_delta="<<b444TemporalVpDelta<<", accepted_view_delta="<<atomic.vd<<", accepted_projection_delta="<<atomic.pd<<", accepted_vp_delta="<<atomic.vpd<<", derivation={View=inverse_gViewInverse,Projection=inverse_gWorldView_times_gWorldViewProj,VP=View_times_Projection,InverseVP=inverse_VP}, provenance=same_256B_write, action=prefer_atomic_tuple_before_projection_normalized_fallback, b443_guard_retained=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}}
   else if(atomic.valid&&(b444AtomicMotionFrames<3)){static unsigned b444Pending=0;if((++b444Pending<=3)||(b444Pending%120)==0){std::ofstream b444(path,std::ios::app);b444<<"B444: ATOMIC_RAGE_CAMERA_SNAPSHOT_PENDING source_frame_id="<<sourceId<<", resource="<<b444Resource<<", write_serial="<<atomic.serial<<", qualification_frames="<<b444AtomicMotionFrames<<", temporal_vp_delta="<<b444TemporalVpDelta<<", requirement=three_consecutive_same_resource_snapshots_with_real_temporal_motion, action=hold_existing_camera_until_qualified, b443_guard_retained=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}}
  }
  // B424 ghosting fix: evaluate projection-normalized candidates even when a strict static
  // candidate already passed. Previously `if(!found&&wc)` meant one stale/static strict
  // write could suppress a fresh moving write whose projection changed enough to miss the
  // strict pd<=2 gate. That fed B343 two false-static frames and B408 then forced zero motion.
  // Prefer a fresh valid moving View with the last accepted gameplay projection; retain the
  // existing 500-unit plausibility guard and all B343/B408 safety behavior for genuinely stale data.
  if(wc){
   struct B305RelaxedCandidate{bool valid{};double score{1.0e300},vd{1.0e300},vpd{1.0e300};unsigned long long serial{};double view[16]{},vp[16]{},invVp[16]{};};
   B305RelaxedCandidate moving{},stationary{};
   for(unsigned n=0;n<wc;++n){double m[64]{};bool finite=true;for(unsigned i=0;i<64;++i){float f=0;std::memcpy(&f,writes[n].bytes+i*4,4);m[i]=f;if(!std::isfinite(m[i])||std::fabs(m[i])>10000000.0){finite=false;break;}}if(!finite||b287_identity4d(m+16)&&b287_identity4d(m+48))continue;double view[16]{},normalizedVp[16]{},invVp[16]{};if(!b287_inverse4(m+48,view))continue;b287_mul4(view,b300AcceptedProjection,normalizedVp);if(!b287_inverse4(normalizedVp,invVp))continue;double vd=0,vpd=0;for(unsigned i=0;i<16;++i){vd=(std::max)(vd,std::fabs(view[i]-b300AcceptedView[i]));vpd=(std::max)(vpd,std::fabs(normalizedVp[i]-b300AcceptedVp[i]));}if(vd>500.0||vpd>500.0)continue;const double score=vd+vpd*0.05;const bool movingNow=vd>0.001||vpd>0.001;auto&dst=movingNow?moving:stationary;if(movingNow)++b305MovingCandidates;else ++b305StaticCandidates;if(!dst.valid||score<dst.score||(std::fabs(score-dst.score)<1.0e-9&&writes[n].serial>dst.serial)){dst.valid=true;dst.score=score;dst.vd=vd;dst.vpd=vpd;dst.serial=writes[n].serial;std::memcpy(dst.view,view,sizeof(view));std::memcpy(dst.vp,normalizedVp,sizeof(normalizedVp));std::memcpy(dst.invVp,invVp,sizeof(invVp));}}
   const bool b424StrictWinnerMoving=found&&(bestViewDelta>0.001||bestVpDelta>0.001);
   const B305RelaxedCandidate*chosen=(!b424StrictWinnerMoving&&moving.valid)?&moving:(!found&&stationary.valid?&stationary:nullptr);
   if(chosen){bestScore=chosen->score;bestViewDelta=chosen->vd;bestProjectionDelta=0;bestVpDelta=chosen->vpd;bestSerial=chosen->serial;std::memcpy(bestView,chosen->view,sizeof(bestView));std::memcpy(bestProjection,b300AcceptedProjection,sizeof(bestProjection));std::memcpy(bestVp,chosen->vp,sizeof(bestVp));std::memcpy(bestInvVp,chosen->invVp,sizeof(bestInvVp));found=true;b304ProjectionNormalized=true;b305MovingWriteSelected=moving.valid&&chosen==&moving;}
  }
  // B446: once B444 qualifies a same-write tuple, it is indivisible. Do not let the later
  // projection-normalized fallback replace Projection/VP with inherited components.
  if(b446QualifiedAtomicTuple){found=true;b304ProjectionNormalized=false;bestSerial=b446QualifiedSerial;std::memcpy(bestView,b446View,sizeof(bestView));std::memcpy(bestProjection,b446Projection,sizeof(bestProjection));std::memcpy(bestVp,b446Vp,sizeof(bestVp));std::memcpy(bestInvVp,b446InvVp,sizeof(bestInvVp));double vd=0,pd=0,vpd=0;for(unsigned i=0;i<16;++i){vd=(std::max)(vd,std::fabs(bestView[i]-b300AcceptedView[i]));pd=(std::max)(pd,std::fabs(bestProjection[i]-b300AcceptedProjection[i]));vpd=(std::max)(vpd,std::fabs(bestVp[i]-b300AcceptedVp[i]));}bestViewDelta=vd;bestProjectionDelta=pd;bestVpDelta=vpd;}
  // B451 ghosting fix: B450 proved the exact selected snapshot was still blocked by stale cross-frame VP continuity. B449 proved the projection-normalized B305 winner could be selected from one
  // 256-byte write while Projection was inherited from an older accepted camera. Once bestSerial is
  // known, reconstruct the complete camera tuple from that exact selected snapshot and keep it
  // indivisible through b288Derived. No downstream temporal, motion-vector, XeFG or presentation policy changes.
  bool b450ExactSelectedSnapshotApplied=false;
  if(found&&b304ProjectionNormalized){
   for(unsigned n=0;n<wc;++n)if(writes[n].serial==bestSerial){
    double m[64]{};bool finite=true;for(unsigned i=0;i<64;++i){float f=0;std::memcpy(&f,writes[n].bytes+i*4,4);m[i]=f;if(!std::isfinite(m[i])||std::fabs(m[i])>10000000.0){finite=false;break;}}
    double exactView[16]{},exactInvWv[16]{},exactProjection[16]{},exactVp[16]{},exactInvVp[16]{};
    if(finite&&!(b287_identity4d(m+16)&&b287_identity4d(m+48))&&b287_inverse4(m+48,exactView)&&b287_inverse4(m+16,exactInvWv)){
     b287_mul4(exactInvWv,m+32,exactProjection);b287_mul4(exactView,exactProjection,exactVp);
     if(b287_inverse4(exactVp,exactInvVp)){
      double vd=0,pd=0,vpd=0,maxAbs=0;bool tupleFinite=true;for(unsigned i=0;i<16;++i){vd=(std::max)(vd,std::fabs(exactView[i]-b300AcceptedView[i]));pd=(std::max)(pd,std::fabs(exactProjection[i]-b300AcceptedProjection[i]));vpd=(std::max)(vpd,std::fabs(exactVp[i]-b300AcceptedVp[i]));const double vals[4]={exactView[i],exactProjection[i],exactVp[i],exactInvVp[i]};for(double v:vals){tupleFinite=tupleFinite&&std::isfinite(v);if(std::isfinite(v))maxAbs=(std::max)(maxAbs,std::fabs(v));}}
      if(tupleFinite&&maxAbs<=10000000.0){std::memcpy(bestView,exactView,sizeof(bestView));std::memcpy(bestProjection,exactProjection,sizeof(bestProjection));std::memcpy(bestVp,exactVp,sizeof(bestVp));std::memcpy(bestInvVp,exactInvVp,sizeof(bestInvVp));bestViewDelta=vd;bestProjectionDelta=pd;bestVpDelta=vpd;b304ProjectionNormalized=false;b450ExactSelectedSnapshotApplied=true;b443DerivedSnapshotCoherent=true;}
     }
    }
    break;
   }
  }
  // B455 ghosting/performance fix: consume the newest already-published selected-camera snapshot
  // directly from the B445 Unmap handoff. B454 rescanned/revalidated the bounded write ring in Present;
  // this removes that extra ring walk and uses the exact newest coherent 256-byte write captured upstream.
  // B439/B437 remain the final mathematical/runtime gates and B453 temporal rebase is unchanged.
  // B480 ghosting fix: B479 proved the exact locked selected-resource Map/Unmap stream can
  // advance while the legacy candidate-table representation remains stale. Make the newest
  // mathematically valid B445 selected-Unmap snapshot authoritative for this source frame.
  // It may establish the camera tuple even when the legacy candidate path found nothing, and
  // it may replace a same/older candidate-table tuple. B439/B437 remain the final validators.
  bool b455AtomicHandoffAdvance=false;unsigned long long b455PriorBestSerial=bestSerial;
  bool b480LiveUnmapAuthorityApplied=false;
  // B496 actual ghosting fix: B495 restored the accepted B458/B453 motion boundary but left
  // B480's later selected-Unmap authority active upstream. The B495 run proved that override can
  // repeatedly reconstruct the same VP while the raw selected stream continues changing. Restore
  // the complete accepted B458-era camera-selection path by preventing B480 from replacing the
  // already-selected candidate tuple. Protected B458/B453, B423 freezing and XeFG x4 are untouched.
  const bool b496AllowB480CameraOverride=false;
  if(b496AllowB480CameraOverride&&b445HandoffSerial!=0&&b445HandoffResource==g_b301DirectCbResource.load(std::memory_order_acquire)){
   double m[64]{};bool finite=true;for(unsigned i=0;i<64;++i){float f=0;std::memcpy(&f,b445Handoff.bytes+i*4,4);m[i]=f;if(!std::isfinite(m[i])||std::fabs(m[i])>10000000.0){finite=false;break;}}
   double v[16]{},iwv[16]{},pr[16]{},vp[16]{},ivp[16]{};
   if(finite&&!(b287_identity4d(m+16)&&b287_identity4d(m+48))&&b287_inverse4(m+48,v)&&b287_inverse4(m+16,iwv)){
    b287_mul4(iwv,m+32,pr);b287_mul4(v,pr,vp);
    if(b287_inverse4(vp,ivp)){bool tupleFinite=true;double maxAbs=0;for(unsigned i=0;i<16;++i){const double vals[4]={v[i],pr[i],vp[i],ivp[i]};for(double x:vals){tupleFinite=tupleFinite&&std::isfinite(x);if(std::isfinite(x))maxAbs=(std::max)(maxAbs,std::fabs(x));}}
     if(tupleFinite&&maxAbs<=10000000.0){double vd=0,pd=0,vpd=0;for(unsigned i=0;i<16;++i){vd=(std::max)(vd,std::fabs(v[i]-b300AcceptedView[i]));pd=(std::max)(pd,std::fabs(pr[i]-b300AcceptedProjection[i]));vpd=(std::max)(vpd,std::fabs(vp[i]-b300AcceptedVp[i]));}std::memcpy(bestView,v,sizeof(bestView));std::memcpy(bestProjection,pr,sizeof(bestProjection));std::memcpy(bestVp,vp,sizeof(bestVp));std::memcpy(bestInvVp,ivp,sizeof(bestInvVp));bestViewDelta=vd;bestProjectionDelta=pd;bestVpDelta=vpd;bestSerial=b445HandoffSerial;b304ProjectionNormalized=false;b450ExactSelectedSnapshotApplied=true;b443DerivedSnapshotCoherent=true;b455AtomicHandoffAdvance=true;b480LiveUnmapAuthorityApplied=true;b489_record_authoritative_consume(b445HandoffSerial,sourceId,b445Handoff.bytes);found=true;}
    }
   }
  }
  // B491 diagnostic-only: B490 logged before B480 consumed the current B445 handoff, so its
  // consumed_is_current_handoff field necessarily described the prior consume. Verify the exact
  // B489 consume object here, after B480 has had the opportunity to consume this frame's handoff.
  if(b449LogThis&&b445HandoffSerial){
   B489AuthoritativeHandoffSnapshot b491Auth{};B489GetAuthoritativeHandoff(&b491Auth);
   const unsigned long long b491HandoffFp=b449BytesFingerprint(b445Handoff.bytes,256);
   const unsigned long long b491ConsumedFp=b491Auth.consumedValid?b449BytesFingerprint(b491Auth.consumedBytes,256):0ULL;
   const bool b491SerialMatch=b491Auth.consumedValid&&b491Auth.consumedSerial==b445HandoffSerial;
   const bool b491BytesMatch=b491Auth.consumedValid&&b491ConsumedFp==b491HandoffFp;
   std::ofstream b491(path,std::ios::app);b491<<"B491: POST_B480_CURRENT_HANDOFF_CONSUME_VERIFICATION source_frame_id="<<sourceId<<", b445_handoff_serial="<<b445HandoffSerial<<", b445_handoff_fingerprint="<<b491HandoffFp<<", b480_authority_applied="<<(b480LiveUnmapAuthorityApplied?1:0)<<", consumed_source_frame="<<b491Auth.consumedSourceFrame<<", consumed_serial="<<b491Auth.consumedSerial<<", consumed_fingerprint="<<b491ConsumedFp<<", consumed_serial_matches_current_handoff="<<(b491SerialMatch?1:0)<<", consumed_bytes_match_current_handoff="<<(b491BytesMatch?1:0)<<", exact_current_handoff_consumed="<<((b491SerialMatch&&b491BytesMatch&&b480LiveUnmapAuthorityApplied)?1:0)<<", diagnostic_order=after_b480_consume, b490_preconsume_comparison_retained=1, diagnostic_only=1, runtime_behavior_changed=0, camera_selection_unchanged=1, b474_history_policy_unchanged=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";
  }
  if(found){
   unsigned long long b449SelectedSnapshotFingerprint=0;for(unsigned n=0;n<wc;++n)if(writes[n].serial==bestSerial){b449SelectedSnapshotFingerprint=b449BytesFingerprint(writes[n].bytes,256);break;}
   if(b480LiveUnmapAuthorityApplied)b449SelectedSnapshotFingerprint=b449BytesFingerprint(b445Handoff.bytes,256);
   if(b449LogThis&&b480LiveUnmapAuthorityApplied){std::ofstream b480(path,std::ios::app);b480<<"B480: LIVE_SELECTED_UNMAP_CAMERA_AUTHORITY source_frame_id="<<sourceId<<", resource="<<b445HandoffResource<<", prior_candidate_found="<<(b455PriorBestSerial?1:0)<<", prior_candidate_serial="<<b455PriorBestSerial<<", authoritative_unmap_serial="<<bestSerial<<", snapshot_fingerprint="<<b449SelectedSnapshotFingerprint<<", action=consume_exact_latest_selected_unmap_snapshot, candidate_table_authority=0, exact_selected_resource_lock_preserved=1, broad_draw_scan=0, b439_validation_retained=1, b437_final_gate_retained=1, b474_history_policy_unchanged=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}
   if(b449LogThis&&b455AtomicHandoffAdvance){std::ofstream b455(path,std::ios::app);b455<<"B455: ATOMIC_SELECTED_CAMERA_HANDOFF source_frame_id="<<sourceId<<", prior_selected_write_serial="<<b455PriorBestSerial<<", newest_selected_write_serial="<<bestSerial<<", snapshot_fingerprint="<<b449SelectedSnapshotFingerprint<<", tuple="<<b449DoubleTupleFingerprint(bestVp,bestInvVp,bestView,bestProjection)<<", view="<<b449DoubleComponentFingerprint(bestView)<<", projection="<<b449DoubleComponentFingerprint(bestProjection)<<", vp="<<b449DoubleComponentFingerprint(bestVp)<<", inverse_vp="<<b449DoubleComponentFingerprint(bestInvVp)<<", snapshot_coherent=1, source=upstream_unmap_atomic_handoff, bounded_ring_rescan=0, broad_draw_scan=0, b453_atomic_temporal_rebase_retained=1, b439_validation_retained=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}
   // B482 diagnostic-only: correlate the exact live selected-Unmap snapshot with the reconstructed camera VP.
   // No camera selection, validation, history, motion-vector, XeFG, freezing, or presentation behavior changes.
   if(b449LogThis&&b480LiveUnmapAuthorityApplied){std::ofstream b482(path,std::ios::app);b482<<"B482: LIVE_UNMAP_TO_RECONSTRUCTED_VP source_frame_id="<<sourceId<<", resource="<<b445HandoffResource<<", authoritative_unmap_serial="<<bestSerial<<", live_unmap_fingerprint="<<g_b479LatestLiveSnapshotFp.load(std::memory_order_acquire)<<", authoritative_snapshot_fingerprint="<<b449SelectedSnapshotFingerprint<<", reconstructed_tuple_fingerprint="<<b449DoubleTupleFingerprint(bestVp,bestInvVp,bestView,bestProjection)<<", reconstructed_vp_fingerprint="<<b449DoubleComponentFingerprint(bestVp)<<", reconstructed_inverse_vp_fingerprint="<<b449DoubleComponentFingerprint(bestInvVp)<<", b480_authority_applied=1, b439_validation_retained=1, b437_final_gate_retained=1, b474_history_policy_unchanged=1, camera_selection_unchanged=1, broad_draw_scan=0, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
   if(b449LogThis&&b450ExactSelectedSnapshotApplied){std::ofstream b450(path,std::ios::app);b450<<"B451: AUTHORITATIVE_EXACT_SELECTED_SNAPSHOT_TUPLE source_frame_id="<<sourceId<<", write_serial="<<bestSerial<<", snapshot_fingerprint="<<b449SelectedSnapshotFingerprint<<", tuple="<<b449DoubleTupleFingerprint(bestVp,bestInvVp,bestView,bestProjection)<<", view="<<b449DoubleComponentFingerprint(bestView)<<", projection="<<b449DoubleComponentFingerprint(bestProjection)<<", vp="<<b449DoubleComponentFingerprint(bestVp)<<", inverse_vp="<<b449DoubleComponentFingerprint(bestInvVp)<<", snapshot_coherent=1, inherited_projection=0, action=consume_exact_selected_256B_write_without_stale_cross_frame_vp_veto, b439_validation_retained=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}
   if(b449LogThis){std::ofstream b449(path,std::ios::app);b449<<"B449: UPSTREAM_CAMERA_PROVENANCE source_frame_id="<<sourceId<<", boundary=B305_SELECTED_WRITE, resource="<<g_b301DirectCbResource.load(std::memory_order_acquire)<<", write_serial="<<bestSerial<<", snapshot_fingerprint="<<b449SelectedSnapshotFingerprint<<", tuple="<<b449DoubleTupleFingerprint(bestVp,bestInvVp,bestView,bestProjection)<<", view="<<b449DoubleComponentFingerprint(bestView)<<", projection="<<b449DoubleComponentFingerprint(bestProjection)<<", vp="<<b449DoubleComponentFingerprint(bestVp)<<", inverse_vp="<<b449DoubleComponentFingerprint(bestInvVp)<<", projection_normalized="<<(b304ProjectionNormalized?1:0)<<", atomic_qualified="<<(b446QualifiedAtomicTuple?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}
   b443DerivedSnapshotCoherent=b450ExactSelectedSnapshotApplied||b446QualifiedAtomicTuple||!b304ProjectionNormalized;b443DerivedWriteSerial=bestSerial;b443DerivedResource=b446QualifiedAtomicTuple?b446QualifiedResource:g_b301DirectCbResource.load(std::memory_order_acquire);for(unsigned i=0;i<16;++i){b288DerivedView[i]=bestView[i];b288DerivedProjection[i]=bestProjection[i];b288DerivedVp[i]=bestVp[i];b288DerivedInverseVp[i]=bestInvVp[i];b300AcceptedView[i]=bestView[i];b300AcceptedProjection[i]=bestProjection[i];b300AcceptedVp[i]=bestVp[i];b300AcceptedInverseVp[i]=bestInvVp[i];}b288DerivedValid=true;b288HistoryReset=false;b288ViewDelta=bestViewDelta;b288VpDelta=bestVpDelta;b300NoCandidateFrames=0;++b300DirectFrames;if(b449LogThis){std::ofstream b449(path,std::ios::app);b449<<"B449: UPSTREAM_CAMERA_PROVENANCE source_frame_id="<<sourceId<<", boundary=POST_B288_DERIVED_ASSIGN, write_serial="<<b443DerivedWriteSerial<<", tuple="<<b449DoubleTupleFingerprint(b288DerivedVp,b288DerivedInverseVp,b288DerivedView,b288DerivedProjection)<<", view="<<b449DoubleComponentFingerprint(b288DerivedView)<<", projection="<<b449DoubleComponentFingerprint(b288DerivedProjection)<<", vp="<<b449DoubleComponentFingerprint(b288DerivedVp)<<", inverse_vp="<<b449DoubleComponentFingerprint(b288DerivedInverseVp)<<", snapshot_coherent="<<(b443DerivedSnapshotCoherent?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}const bool b343SelectedActuallyMoving=bestViewDelta>0.001||bestVpDelta>0.001;
   // B427 ghosting fix: a finite/invertible candidate selected from the locked camera CB is
   // valid even when its transform is unchanged for consecutive source frames. B426 proved
   // that treating two such frames as "false static" immediately invalidates a successful
   // rebase, forces B408 back to zero-motion, and creates reset/source-only flash cycles.
   // Stationary camera frames now preserve accepted temporal history; only the existing
   // no-valid-candidate B340 path can mark camera data stale/unsafe.
   if(wc&&!b343SelectedActuallyMoving)++b343FalseStaticFrames;else b343FalseStaticFrames=0;
   const bool b427ValidStationaryCamera=wc&&!b343SelectedActuallyMoving;
   const bool b344FalseStaticUnsafe=false;
   if(b427ValidStationaryCamera){b344ZeroMotionSafetyThisFrame=false;b344ResetThisFrame=false;b344UnsafeCameraActive=false;b288HistoryReset=false;b288ViewDelta=b288VpDelta=0;if(b343FalseStaticFrames==2||(b343FalseStaticFrames%600)==0){std::ofstream b427(path,std::ios::app);b427<<"B427: VALID_STATIONARY_CAMERA_HISTORY_PRESERVED source_frame_id="<<sourceId<<", candidate_writes="<<wc<<", stationary_frames="<<b343FalseStaticFrames<<", selected_write_serial="<<bestSerial<<", selected_view_delta="<<bestViewDelta<<", selected_vp_delta="<<bestVpDelta<<", action=preserve_temporal_history, zero_motion_safety=0, xefg_reset_history=0, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";}
    // B429: B428 runtime proved the direct CB could keep producing fresh serials while its camera
    // transform stayed exactly static for the whole session. That is a healthy resource write but
    // not a responsive gameplay camera. Perform one bounded selector revalidation after 120 such
    // frames: restore Draw/DrawIndexed classification, leave temporal history untouched, and let
    // the proven B299/B300 selector lock the currently responsive gameplay-camera CB again.
    static bool b429StationaryRelockUsed=false;
    // B497 runtime ghosting fix: B496 proved the locked selected resource can keep publishing
    // fresh serials while the actual selected View/VP remains exactly unchanged for the entire
    // gameplay run. Restore B429's bounded one-shot responsive-camera relock instead of B471's
    // stale-lock preservation. The rejected resource is quarantined so the existing B299/B300
    // selector must prove a different responsive 256-byte gameplay-camera resource. Temporal
    // history is not reset here; protected B458/B453, B423 freezing and XeFG x4 are untouched.
    if(!b429StationaryRelockUsed&&b343FalseStaticFrames>=120){
     ID3D11Resource* b497RejectedResource=g_b301DirectCbResource.load(std::memory_order_acquire);
     const int b497RejectedCbIndex=g_b300DirectCbIndex.load(std::memory_order_acquire);
     b429StationaryRelockUsed=true;b441FallbackResource=b497RejectedResource;b441FallbackCbIndex=b497RejectedCbIndex;b441ReacquireStartSource=sourceId;
     auto***b497Obj=reinterpret_cast<void***>(context);if(b497Obj&&*b497Obj){void**vt=*b497Obj;vt[12]=(void*)&h_fast_draw_indexed;vt[13]=(void*)&h_fast_draw;g_b300DrawHooksReleased.store(false,std::memory_order_release);}
     g_b430RejectedStaticCameraResource.store(b497RejectedResource,std::memory_order_release);g_b430ResponsiveReacquireActive.store(true,std::memory_order_release);
     g_b300DirectMode.store(false,std::memory_order_release);g_b301DirectCbResource.store(nullptr,std::memory_order_release);g_b300DirectCbIndex.store(-1,std::memory_order_release);
     AcquireSRWLockExclusive(&g_b300DirectWriteLock);g_b300DirectWriteHead=g_b300DirectWriteCount=0;g_b445LatestSelectedSnapshot={};g_b445LatestSelectedResource=nullptr;g_b445LatestSelectedSerial=0;ReleaseSRWLockExclusive(&g_b300DirectWriteLock);
     AcquireSRWLockExclusive(&g_b489AuthoritativeLock);g_b489Authoritative={};ReleaseSRWLockExclusive(&g_b489AuthoritativeLock);
     b344UnsafeCameraActive=false;b288HistoryReset=false;
     std::ofstream b497(path,std::ios::app);b497<<"B497: RESPONSIVE_CAMERA_RELOCK_ARMED source_frame_id="<<sourceId<<", trigger=120_fresh_zero_delta_frames, rejected_resource="<<b497RejectedResource<<", rejected_cb_index="<<b497RejectedCbIndex<<", action=restore_B429_bounded_camera_reselection, selected_resource_cleared=1, draw_classifier_restored=1, direct_write_ring_cleared=1, rejected_resource_quarantined=1, temporal_history_reset=0, protected_b458_b453_unchanged=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
    }
   }
   else if(b343SelectedActuallyMoving)b344UnsafeCameraActive=false;if(b304ProjectionNormalized&&(b300DirectFrames<=5||(b300DirectFrames%600)==0)){std::ofstream b305(path,std::ios::app);b305<<"B305: DIRECT_CAMERA_MOVING_WRITE_SELECTION source_frame_id="<<sourceId<<", cb_index="<<g_b300DirectCbIndex.load()<<", candidate_writes="<<wc<<", moving_candidates="<<b305MovingCandidates<<", static_candidates="<<b305StaticCandidates<<", selected_write_serial="<<bestSerial<<", selected_moving="<<(b305MovingWriteSelected?1:0)<<", view_delta="<<bestViewDelta<<", normalized_vp_delta="<<bestVpDelta<<", movement_floor=0.001, inherited_projection=last_safe_gameplay_projection, zero_delta_rejected_when_moving_available=1, drawindexed_queries=0, history_reset=0, consumed_by_runtime=1\n";}else if(!b304ProjectionNormalized&&(b300DirectFrames<=3||(b300DirectFrames%600)==0)){std::ofstream b300(path,std::ios::app);b300<<"B300: DIRECT_CAMERA_CB_UPDATE source_frame_id="<<sourceId<<", cb_index="<<g_b300DirectCbIndex.load()<<", candidate_writes="<<wc<<", selected_write_serial="<<bestSerial<<", view_delta="<<bestViewDelta<<", projection_delta="<<bestProjectionDelta<<", vp_delta="<<bestVpDelta<<", drawindexed_queries=0, history_reset=0, consumed_by_runtime=1\n";}}
  else{
   b343FalseStaticFrames=0;
   // B426: presentation-neutral recovery. Keep B425 fresh-write rebase but do not request a second
   // temporal reset when exiting the unsafe episode; B408 already issues the single reset on episode entry.
   // This avoids rebase -> safe-branch B297ResetCameraHistory work that can coincide with flashing/stutter.
   // B425 basis: break the B408 zero-motion deadlock. B424 proved that the locked camera CB can
   // continue delivering fresh writes while every candidate is rejected only because the
   // previously accepted camera has become too stale for the 500-unit continuity gate.
   // In an already-unsafe episode, two consecutive frames with finite/invertible writes from
   // the locked camera resource are enough to rebase to the newest write. Projection remains
   // the last accepted gameplay projection, and temporal history is reset exactly on rebase.
   // B428: fresh-write pre-unsafe shield. B427 proved that the locked camera CB can provide two
   // fresh finite/invertible writes while the continuity selector rejects them. Do not enter
   // B340/B408 zero-motion/reset first and recover afterwards. Validate the fresh writes here,
   // hold history on the first rejected frame, then presentation-neutrally rebase on the second.
   static unsigned b428FreshRejectedWriteFrames=0;
   bool b425Rebased=false;double b425View[16]{},b425Vp[16]{},b425InvVp[16]{};unsigned long long b425Serial=0;
   bool b425Valid=false;
   if(wc){
    for(unsigned n=0;n<wc;++n){double m[64]{};bool finite=true;for(unsigned i=0;i<64;++i){float f=0;std::memcpy(&f,writes[n].bytes+i*4,4);m[i]=f;if(!std::isfinite(m[i])||std::fabs(m[i])>10000000.0){finite=false;break;}}if(!finite||b287_identity4d(m+16)&&b287_identity4d(m+48))continue;double view[16]{},vp[16]{},invVp[16]{};if(!b287_inverse4(m+48,view))continue;b287_mul4(view,b300AcceptedProjection,vp);if(!b287_inverse4(vp,invVp))continue;if(!b425Valid||writes[n].serial>b425Serial){b425Valid=true;b425Serial=writes[n].serial;std::memcpy(b425View,view,sizeof(view));std::memcpy(b425Vp,vp,sizeof(vp));std::memcpy(b425InvVp,invVp,sizeof(invVp));}}
   }
   if(b425Valid)++b428FreshRejectedWriteFrames;else b428FreshRejectedWriteFrames=0;
   if(b425Valid&&b428FreshRejectedWriteFrames>=2){for(unsigned i=0;i<16;++i){b288DerivedView[i]=b425View[i];b288DerivedProjection[i]=b300AcceptedProjection[i];b288DerivedVp[i]=b425Vp[i];b288DerivedInverseVp[i]=b425InvVp[i];b300AcceptedView[i]=b425View[i];b300AcceptedVp[i]=b425Vp[i];b300AcceptedInverseVp[i]=b425InvVp[i];}b288DerivedValid=true;b288HistoryReset=false;b288ViewDelta=b288VpDelta=0;b300NoCandidateFrames=0;b344ZeroMotionSafetyThisFrame=false;b344ResetThisFrame=false;b344UnsafeCameraActive=false;b428FreshRejectedWriteFrames=0;b425Rebased=true;std::ofstream b428(path,std::ios::app);b428<<"B428: FRESH_CAMERA_PREUNSAFE_REBASE source_frame_id="<<sourceId<<", candidate_writes="<<wc<<", selected_write_serial="<<b425Serial<<", trigger=two_consecutive_fresh_rejected_locked_camera_frames, inherited_projection=last_safe_gameplay_projection, unsafe_episode_entered=0, zero_motion=0, history_reset=0, action=rebase_before_B340_B408, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";}
   if(!b425Rebased){for(unsigned i=0;i<16;++i){b288DerivedView[i]=b300AcceptedView[i];b288DerivedProjection[i]=b300AcceptedProjection[i];b288DerivedVp[i]=b300AcceptedVp[i];b288DerivedInverseVp[i]=b300AcceptedInverseVp[i];}b288DerivedValid=true;++b300NoCandidateFrames;const bool b428FreshWriteGrace=b425Valid&&b428FreshRejectedWriteFrames==1;const bool b344MissingCameraUnsafe=!b428FreshWriteGrace&&b300NoCandidateFrames>=2;if(b428FreshWriteGrace){b300NoCandidateFrames=1;b344ZeroMotionSafetyThisFrame=false;b344ResetThisFrame=false;b288HistoryReset=false;std::ofstream b428(path,std::ios::app);b428<<"B428: FRESH_CAMERA_PREUNSAFE_HOLD source_frame_id="<<sourceId<<", candidate_writes="<<wc<<", selected_write_serial="<<b425Serial<<", grace_frame=1, unsafe_episode_entered=0, zero_motion=0, history_reset=0, action=hold_one_frame_for_fresh_write_confirmation, freezing_dll_unchanged=1\n";}if(b344MissingCameraUnsafe){b344ZeroMotionSafetyThisFrame=true;b344ResetThisFrame=true;b344UnsafeCameraActive=true;}b288HistoryReset=b344MissingCameraUnsafe;b288ViewDelta=b288VpDelta=0;if(b344MissingCameraUnsafe){std::ofstream b340(path,std::ios::app);if(b300NoCandidateFrames<=3||(b300NoCandidateFrames%600)==0)b340<<"B340: STALE_CAMERA_TEMPORAL_RESET source_frame_id="<<sourceId<<", candidate_writes="<<wc<<", stale_frames="<<b300NoCandidateFrames<<", action=zero_motion_plus_reset_history, xefg_reset_history=1, b345_visual_lock=exact_B343_repeated_reset, accepted_camera_held=1, xefg_x4_unchanged=1\n";}if(b300NoCandidateFrames<=3||(b300NoCandidateFrames%600)==0){std::ofstream b304(path,std::ios::app);b304<<"B304: DIRECT_CAMERA_PERSISTENT_HOLD source_frame_id="<<sourceId<<", cb_index="<<g_b300DirectCbIndex.load()<<", candidate_writes="<<wc<<", no_candidate_frames="<<b300NoCandidateFrames<<", draw_hooks_restored=0, selected_cb_resource_retained=1, action=hold_last_safe_camera_with_b343_blur_fix_frozen_b344_reset_state, consumed_by_runtime=1\n";}}
  }
 }
 // Keep one low-rate B286 interception marker as a compatibility sanity check.
 static unsigned b286LogOrdinal=0;if(valid&&((++b286LogOrdinal%60)==0)){
  ComPtr<ID3D11VertexShader> vs;unsigned ic=0,si=0,rw=0,rh=0,rf=0,dw=0,dh=0,df=0,mask=0,cbBytes[kPsCbs]{};int bv=0;bool colorMatch=false,depthMatch=false;
  AcquireSRWLockShared(&g_b286ProbeLock);vs=g_b286LastVs;ic=g_b286LastIndexCount;si=g_b286LastStartIndex;bv=g_b286LastBaseVertex;rw=g_b286LastRtvW;rh=g_b286LastRtvH;rf=g_b286LastRtvFormat;dw=g_b286LastDsvW;dh=g_b286LastDsvH;df=g_b286LastDsvFormat;mask=g_b286LastCbMask;for(unsigned i=0;i<kPsCbs;++i)cbBytes[i]=g_b286LastCbBytes[i];colorMatch=g_b286LastFastColorMatch;depthMatch=g_b286LastFastDepthMatch;ReleaseSRWLockShared(&g_b286ProbeLock);
  std::ofstream b286(path,std::ios::app);b286<<"B286: DRAWINDEXED_PROBE source_frame_id="<<sourceId<<", indexed_draws="<<g_b286IndexedDraws.load()<<", sampled_draws="<<g_b286ProbeSamples.load()<<", index_count="<<ic<<", start_index="<<si<<", base_vertex="<<bv<<", vs_shader="<<(void*)vs.Get()<<", rtv={"<<rw<<"x"<<rh<<",fmt="<<rf<<"}, dsv={"<<dw<<"x"<<dh<<",fmt="<<df<<"}, fast_color_match="<<(colorMatch?1:0)<<", fast_depth_match="<<(depthMatch?1:0)<<", vs_cb_mask=0x"<<std::hex<<mask<<std::dec<<", vs_cb_bytes={";for(unsigned i=0;i<kPsCbs;++i){if(i)b286<<",";b286<<cbBytes[i];}b286<<"}, matrix_classification=B300_direct_camera_cb_with_b299_reacquisition, consumed_by_runtime=0, diagnostic_only=1, b276_runtime_frozen=1\n";b286.flush();
 }
 // B284 compatibility diagnostic: every 30 accepted source frames, report the strongest changing 4x4-sized windows seen in live GTA constant-buffer writes.
 static unsigned b280DiscoveryLogOrdinal=0;if(valid&&((++b280DiscoveryLogOrdinal%30)==0)){
  std::ofstream out(path,std::ios::app);unsigned known=(std::min)(g_known_cb_count.load(),kKnownCbs),found=0;
  ID3D11Resource* b470SelectedResource=g_b301DirectCbResource.load(std::memory_order_acquire);int b470SelectedIndex=b280_known_cb_index(b470SelectedResource);unsigned b470StrongestIndex=~0u;double b470StrongestDelta=-1.0;unsigned long long b470StrongestFp=0,b470SelectedFp=0;
  auto b470BytesFingerprint=[](const unsigned char*bytes,unsigned count)->unsigned long long{unsigned long long h=1469598103934665603ULL;for(unsigned i=0;i<count;++i){h^=(unsigned long long)bytes[i];h*=1099511628211ULL;}return h;};
  // B478 diagnostic-only: sample the already-maintained bounded 256-byte candidate table across
  // source-frame intervals. This does not discover, bind, promote or consume any new resource.
  static unsigned char b478PriorSample[kKnownCbs][256]{};
  static bool b478PriorValid[kKnownCbs]{};
  static unsigned b478TemporalMovingSamples[kKnownCbs]{};
  static unsigned long long b478PriorFp[kKnownCbs]{};
  unsigned b478StrongestIndex=~0u;double b478StrongestTemporalRms=-1.0;unsigned long long b478StrongestFp=0;
  AcquireSRWLockShared(&g_b280CandidateLock);
  for(unsigned ci=0;ci<known;++ci){if(g_known_cb_bytes[ci].load(std::memory_order_acquire)!=256||!g_b280PreviousValid[ci]||g_b280PreviousByteCount[ci]<256)continue;++found;
   float m[64]{};std::memcpy(m,g_b280PreviousBytes[ci],256);
   auto finite16=[&](unsigned o){for(unsigned j=0;j<16;++j)if(!std::isfinite(m[o+j])||std::fabs(m[o+j])>10000000.0f)return false;return true;};
   auto det3=[&](unsigned o){double a=m[o],b=m[o+1],c=m[o+2],d=m[o+4],e=m[o+5],f=m[o+6],g=m[o+8],h=m[o+9],i=m[o+10];return a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);};
   double d0=det3(0),d1=det3(16),d2=det3(32),d3=det3(48);
   double delta=0.0;auto&w=g_b280CandidateWindows[ci];delta=w.rmsDelta;const unsigned long long b470Fp=b470BytesFingerprint(g_b280PreviousBytes[ci],256);const bool b470Selected=((int)ci==b470SelectedIndex);if(b470Selected)b470SelectedFp=b470Fp;if(delta>b470StrongestDelta){b470StrongestDelta=delta;b470StrongestIndex=ci;b470StrongestFp=b470Fp;}
   double b478Max=0.0,b478Rms=0.0,b478RegionMax[4]{},b478RegionRms[4]{};
   const bool b478HadPrior=b478PriorValid[ci];
   if(b478HadPrior){for(unsigned bi=0;bi<256;bi+=4){float a=0,b=0;std::memcpy(&a,b478PriorSample[ci]+bi,4);std::memcpy(&b,g_b280PreviousBytes[ci]+bi,4);double d=(std::isfinite(a)&&std::isfinite(b))?std::fabs((double)b-(double)a):0.0;unsigned r=bi/64;b478Max=(std::max)(b478Max,d);b478Rms+=d*d;b478RegionMax[r]=(std::max)(b478RegionMax[r],d);b478RegionRms[r]+=d*d;}b478Rms=std::sqrt(b478Rms/64.0);for(unsigned r=0;r<4;++r)b478RegionRms[r]=std::sqrt(b478RegionRms[r]/16.0);}
   const bool b478FpChanged=b478HadPrior&&(b470Fp!=b478PriorFp[ci]);
   if(b478FpChanged&&(b478Max>1.0e-6||b478Rms>1.0e-7))++b478TemporalMovingSamples[ci];else if(b478HadPrior)b478TemporalMovingSamples[ci]=0;
   if(b478HadPrior&&b478Rms>b478StrongestTemporalRms){b478StrongestTemporalRms=b478Rms;b478StrongestIndex=ci;b478StrongestFp=b470Fp;}
   out<<"B478: CAMERA_CANDIDATE_TEMPORAL_IDENTITY source_frame_id="<<sourceId<<", candidate_cb_index="<<ci<<", resource="<<(ID3D11Resource*)g_known_cbs[ci].load(std::memory_order_acquire)<<", selected_locked_resource="<<(b470Selected?1:0)<<", prior_sample_available="<<(b478HadPrior?1:0)<<", prior_snapshot_fingerprint="<<b478PriorFp[ci]<<", current_snapshot_fingerprint="<<b470Fp<<", fingerprint_changed="<<(b478FpChanged?1:0)<<", cross_sample_max="<<b478Max<<", cross_sample_rms="<<b478Rms<<", region_max={"<<b478RegionMax[0]<<","<<b478RegionMax[1]<<","<<b478RegionMax[2]<<","<<b478RegionMax[3]<<"}, region_rms={"<<b478RegionRms[0]<<","<<b478RegionRms[1]<<","<<b478RegionRms[2]<<","<<b478RegionRms[3]<<"}, consecutive_moving_samples="<<b478TemporalMovingSamples[ci]<<", sampled_write_updates="<<g_b282TargetWriteCount[ci].load()<<", existing_candidate_table_only=1, diagnostic_only=1, runtime_behavior_changed=0, camera_selection_unchanged=1, broad_draw_scan=0, b474_history_policy_unchanged=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
   if(b470Selected){
    const unsigned long long b479LiveFp=g_b479LatestLiveSnapshotFp.load(std::memory_order_acquire);
    const unsigned long long b479PriorLiveFp=g_b479PreviousLiveSnapshotFp.load(std::memory_order_relaxed);
    const unsigned long long b479LiveUnmaps=g_b479LiveSelectedUnmaps.load(std::memory_order_relaxed);
    const unsigned long long b479Changed=g_b479LiveChangedSnapshots.load(std::memory_order_relaxed);
    out<<"B479: LIVE_UNMAP_VS_CANDIDATE_TABLE source_frame_id="<<sourceId<<", candidate_cb_index="<<ci<<", resource="<<(ID3D11Resource*)g_known_cbs[ci].load(std::memory_order_acquire)<<", selected_locked_resource=1, live_selected_unmap_captures="<<b479LiveUnmaps<<", live_changed_snapshot_count="<<b479Changed<<", prior_live_unmap_fingerprint="<<b479PriorLiveFp<<", latest_live_unmap_fingerprint="<<b479LiveFp<<", candidate_table_fingerprint="<<b470Fp<<", live_matches_candidate_table="<<((b479LiveFp!=0&&b479LiveFp==b470Fp)?1:0)<<", candidate_table_sampled_write_updates="<<g_b282TargetWriteCount[ci].load()<<", exact_selected_map_unmap_stream=1, hot_path_file_io=0, camera_selection_unchanged=1, broad_draw_scan=0, diagnostic_only=1, runtime_behavior_changed=0, b478_candidate_diagnostic_retained=1, b474_history_policy_unchanged=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
   }
   std::memcpy(b478PriorSample[ci],g_b280PreviousBytes[ci],256);b478PriorValid[ci]=true;b478PriorFp[ci]=b470Fp;
   out<<"B470: CAMERA_CANDIDATE_IDENTITY source_frame_id="<<sourceId<<", candidate_cb_index="<<ci<<", resource="<<(ID3D11Resource*)g_known_cbs[ci].load(std::memory_order_acquire)<<", selected_locked_resource="<<(b470Selected?1:0)<<", snapshot_fingerprint="<<b470Fp<<", rms_change="<<delta<<", sampled_write_updates="<<g_b282TargetWriteCount[ci].load()<<", diagnostic_only=1, runtime_behavior_changed=0, broad_draw_scan=0, b458_ghosting_runtime_unchanged=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
   out<<"B284: RAGE_MATRICES_256 candidate_cb_index="<<ci<<", byte_width=256, bind_stage="<<g_b282LastBindStage[ci].load()<<", bind_slot="<<g_b282LastBindSlot[ci].load()<<", sampled_write_updates="<<g_b282TargetWriteCount[ci].load()<<", rms_change="<<delta<<", finite={"<<finite16(0)<<","<<finite16(16)<<","<<finite16(32)<<","<<finite16(48)<<"}, det3={"<<d0<<","<<d1<<","<<d2<<","<<d3<<"}, layout={gWorld@0,gWorldView@64,gWorldViewProj@128,gViewInverse@192}, values={";
   for(unsigned j=0;j<64;++j){if(j)out<<",";out<<m[j];}out<<"}, consumed_by_runtime=0, diagnostic_only=1, b276_runtime_frozen=1\n";
  }
  ReleaseSRWLockShared(&g_b280CandidateLock);// B481 diagnostic-only: summarize the already-known constant-buffer population by byte width while the selected 256B stream is static.
   // This deliberately does not reopen Draw hooks, capture new resources, or change camera authority. It tells the next stage whether an alternate-size CB is already known.
   {unsigned b481Known= g_known_cb_count.load(std::memory_order_acquire); if(b481Known>kKnownCbs)b481Known=kKnownCbs; unsigned b481_64=0,b481_128=0,b481_192=0,b481_256=0,b481_320=0,b481_512=0,b481Other=0;
    for(unsigned bi=0;bi<b481Known;++bi){const unsigned bw=g_known_cb_bytes[bi].load(std::memory_order_acquire);if(bw==64)++b481_64;else if(bw==128)++b481_128;else if(bw==192)++b481_192;else if(bw==256)++b481_256;else if(bw==320)++b481_320;else if(bw==512)++b481_512;else ++b481Other;}
    out<<"B481: BOUNDED_ALTERNATE_CAMERA_STREAM_DIAGNOSTIC source_frame_id="<<sourceId<<", known_constant_buffers="<<b481Known<<", width_counts={64:"<<b481_64<<",128:"<<b481_128<<",192:"<<b481_192<<",256:"<<b481_256<<",320:"<<b481_320<<",512:"<<b481_512<<",other:"<<b481Other<<"}, selected_cb_index="<<g_b300DirectCbIndex.load(std::memory_order_acquire)<<", selected_resource="<<g_b301DirectCbResource.load(std::memory_order_acquire)<<", selected_live_unmap_fingerprint="<<g_b479LatestLiveSnapshotFp.load(std::memory_order_acquire)<<", selected_live_changed_snapshot_count="<<g_b479LiveChangedSnapshots.load(std::memory_order_relaxed)<<", action=inventory_existing_bound_cb_streams_before_any_wider_probe, diagnostic_only=1, runtime_behavior_changed=0, camera_selection_unchanged=1, camera_promotion=0, broad_draw_scan=0, b480_authority_retained=1, b474_history_policy_unchanged=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n"; }
   {static unsigned char b484Prior[kB484WatchSlots][256]{};static bool b484PriorValid[kB484WatchSlots]{};
    AcquireSRWLockShared(&g_b484WatchLock);
    for(unsigned wi=0;wi<kB484WatchSlots;++wi){ID3D11Resource*wr=g_b484WatchResource[wi].load(std::memory_order_acquire);if(!wr)continue;const unsigned wb=g_b484WatchBytes[wi].load(std::memory_order_acquire);const unsigned long long wu=g_b484WatchUnmaps[wi].load(std::memory_order_relaxed),wc=g_b484WatchChanges[wi].load(std::memory_order_relaxed),wf=g_b484WatchLatestFp[wi].load(std::memory_order_acquire);double mx=0.0,rms=0.0;bool advanced=false;if(g_b484WatchValid[wi]&&b484PriorValid[wi]){for(unsigned bj=0;bj<256;++bj){const double d=(double)g_b484WatchLatest[wi][bj]-(double)b484Prior[wi][bj];mx=(std::max)(mx,std::fabs(d));rms+=d*d;}rms=std::sqrt(rms/256.0);advanced=std::memcmp(g_b484WatchLatest[wi],b484Prior[wi],256)!=0;}B485MatrixProbe b485{};if(g_b484WatchValid[wi])b485=b485_probe_first256(g_b484WatchLatest[wi]);if(g_b484WatchValid[wi]){std::memcpy(b484Prior[wi],g_b484WatchLatest[wi],256);b484PriorValid[wi]=true;}out<<"B485: BOUNDED_ALTERNATE_MATRIX_LAYOUT_PROBE source_frame_id="<<sourceId<<", watch_slot="<<wi<<", resource="<<wr<<", byte_width="<<wb<<", snapshot_fingerprint="<<wf<<", advanced_since_prior_sample="<<(advanced?1:0)<<", finite_aligned_matrices="<<b485.finiteMatrices<<", affine_like_matrices="<<b485.affineLike<<", invertible_matrices="<<b485.invertible<<", best_inverse_pair_regions={"<<b485.bestInverseA<<","<<b485.bestInverseB<<"}, best_inverse_pair_residual="<<b485.bestInverseResidual<<", best_abs_determinant="<<b485.bestDetAbs<<", first_256_bytes_only=1, aligned_64byte_regions_only=1, camera_selection_unchanged=1, camera_promotion=0, broad_draw_scan=0, diagnostic_only=1, runtime_behavior_changed=0, b484_watchlist_retained=1, b480_authority_retained=1, b474_history_policy_unchanged=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";out<<"B484: BOUNDED_LIVE_ALTERNATE_CB_STREAM source_frame_id="<<sourceId<<", watch_slot="<<wi<<", resource="<<wr<<", byte_width="<<wb<<", live_unmaps="<<wu<<", changed_snapshots="<<wc<<", latest_fingerprint="<<wf<<", advanced_since_prior_sample="<<(advanced?1:0)<<", byte_delta_max="<<mx<<", byte_delta_rms="<<rms<<", selected_resource="<<g_b301DirectCbResource.load(std::memory_order_acquire)<<", selected_live_fingerprint="<<g_b479LatestLiveSnapshotFp.load(std::memory_order_acquire)<<", exact_known_resource_identity_only=1, first_256_bytes_only=1, camera_selection_unchanged=1, camera_promotion=0, broad_draw_scan=0, diagnostic_only=1, runtime_behavior_changed=0, b480_authority_retained=1, b474_history_policy_unchanged=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
    ReleaseSRWLockShared(&g_b484WatchLock);
   }
   out<<"B478: TEMPORAL_CANDIDATE_SUMMARY source_frame_id="<<sourceId<<", selected_cb_index="<<b470SelectedIndex<<", strongest_temporal_cb_index="<<(b478StrongestIndex==~0u?-1:(int)b478StrongestIndex)<<", strongest_temporal_snapshot_fingerprint="<<b478StrongestFp<<", strongest_temporal_rms="<<b478StrongestTemporalRms<<", same_as_selected="<<((b470SelectedIndex>=0&&(unsigned)b470SelectedIndex==b478StrongestIndex)?1:0)<<", purpose=identify_continuously_advancing_camera_candidate_without_reacquisition, existing_candidate_table_only=1, diagnostic_only=1, runtime_behavior_changed=0, camera_selection_unchanged=1, broad_draw_scan=0, b474_history_policy_unchanged=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";out<<"B470: LOCKED_VS_CHANGING_CAMERA_COMPARISON source_frame_id="<<sourceId<<", selected_cb_index="<<b470SelectedIndex<<", selected_resource="<<b470SelectedResource<<", selected_snapshot_fingerprint="<<b470SelectedFp<<", strongest_changing_cb_index="<<(b470StrongestIndex==~0u?-1:(int)b470StrongestIndex)<<", strongest_snapshot_fingerprint="<<b470StrongestFp<<", strongest_rms_change="<<b470StrongestDelta<<", same_candidate="<<((b470SelectedIndex>=0&&(unsigned)b470SelectedIndex==b470StrongestIndex)?1:0)<<", purpose=prove_or_disprove_locked_camera_resource_identity, diagnostic_only=1, runtime_behavior_changed=0, broad_draw_scan=0, b458_ghosting_runtime_unchanged=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";out<<"B284: RAGE_MATRICES_DISCOVERY source_frame_id="<<sourceId<<", known_constant_buffers="<<known<<", candidates_256="<<found<<", expected_layout_bytes=256, consumed_by_runtime=0, diagnostic_only=1, b276_runtime_frozen=1\n";out.flush();
 }
 // B284: diagnostic-only camera-matrix provenance. Do not alter matrices or temporal history.
 static float b279LastCaptureVp[16]{},b279LastCaptureView[16]{},b279LastCaptureProjection[16]{};static bool b279HaveLastCapture=false;static unsigned b279CaptureOrdinal=0;
 if(valid){double vpMax=0.0,vpRms=0.0,viewMax=0.0,projMax=0.0;for(unsigned i=0;i<16;++i){const double dv=(double)currentVpF[i]-(double)b279LastCaptureVp[i];const double dview=(double)currentViewF[i]-(double)b279LastCaptureView[i];const double dp=(double)currentProjectionF[i]-(double)b279LastCaptureProjection[i];vpMax=(std::max)(vpMax,std::fabs(dv));vpRms+=dv*dv;viewMax=(std::max)(viewMax,std::fabs(dview));projMax=(std::max)(projMax,std::fabs(dp));}vpRms=std::sqrt(vpRms/16.0);++b279CaptureOrdinal;if(!b279HaveLastCapture||(b279CaptureOrdinal%600)==0||vpMax>=500.0){std::ofstream b279out(path,std::ios::app);b279out<<"B284: CAMERA_MATRIX_CAPTURE_PROVENANCE source_frame_id="<<sourceId<<", snapshot_serial="<<serial<<", vp_delta_from_prior_capture_max="<<(b279HaveLastCapture?vpMax:0.0)<<", vp_delta_from_prior_capture_rms="<<(b279HaveLastCapture?vpRms:0.0)<<", view_delta_max="<<(b279HaveLastCapture?viewMax:0.0)<<", projection_delta_max="<<(b279HaveLastCapture?projMax:0.0)<<", constant_snapshot_ready="<<(current.ready.load()?1:0)<<", diagnostic_only=1, b298_hotpath_logging_throttled=1, b276_runtime_frozen=1\n";}std::memcpy(b279LastCaptureVp,currentVpF,64);std::memcpy(b279LastCaptureView,currentViewF,64);std::memcpy(b279LastCaptureProjection,currentProjectionF,64);b279HaveLastCapture=true;}
 auto b447TupleFingerprint=[](const float*vp,const float*inv,const float*view,const float*proj)->unsigned long long{unsigned long long h=1469598103934665603ULL;const float* parts[4]={vp,inv,view,proj};for(const float*part:parts){const unsigned char*b=(const unsigned char*)part;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}}return h;};
 auto b448ComponentFingerprint=[](const float*part)->unsigned long long{unsigned long long h=1469598103934665603ULL;const unsigned char*b=(const unsigned char*)part;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}return h;};
 static unsigned b448BoundaryOrdinal=0;const bool b448LogThis=valid&&(((++b448BoundaryOrdinal)<=8)||(b448BoundaryOrdinal%120)==0);
 if(b448LogThis&&b288DerivedValid){std::ofstream b449(path,std::ios::app);b449<<"B449: UPSTREAM_CAMERA_PROVENANCE source_frame_id="<<sourceId<<", boundary=DERIVED_PRE_B443, write_serial="<<b443DerivedWriteSerial<<", tuple="<<b449DoubleTupleFingerprint(b288DerivedVp,b288DerivedInverseVp,b288DerivedView,b288DerivedProjection)<<", view="<<b449DoubleComponentFingerprint(b288DerivedView)<<", projection="<<b449DoubleComponentFingerprint(b288DerivedProjection)<<", vp="<<b449DoubleComponentFingerprint(b288DerivedVp)<<", inverse_vp="<<b449DoubleComponentFingerprint(b288DerivedInverseVp)<<", snapshot_coherent="<<(b443DerivedSnapshotCoherent?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}
 if(b448LogThis){std::ofstream b448(path,std::ios::app);b448<<"B448: CAMERA_COMPONENT_PROVENANCE source_frame_id="<<sourceId<<", boundary=RAW_PRE_B443, tuple="<<b447TupleFingerprint(currentVpF,currentInverseVpF,currentViewF,currentProjectionF)<<", view="<<b448ComponentFingerprint(currentViewF)<<", projection="<<b448ComponentFingerprint(currentProjectionF)<<", vp="<<b448ComponentFingerprint(currentVpF)<<", inverse_vp="<<b448ComponentFingerprint(currentInverseVpF)<<", b288_derived_valid="<<(b288DerivedValid?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 bool b447AtomicOverrideApplied=false;
 // B457: keep the last camera tuple that passed B439 + coherent selected-write provenance.
 // If the current selected write cannot pass that gate, never fall back to the known raw/default tuple.
 // Submit the held validated tuple only as a safe carrier while B345/B408 force zero motion; a fresh valid
 // authoritative tuple replaces the hold and B453 performs the existing temporal rebase on recovery.
 static float b457LastValidVp[16]{},b457LastValidInvVp[16]{},b457LastValidView[16]{},b457LastValidProjection[16]{};
 static bool b457HaveLastValid=false;static unsigned long long b457LastValidSerial=0;static bool b457UnsafeHoldActive=false;
 // When upstream derivation disappears entirely, route the held authoritative tuple through the same
 // B439/B443 rejection branch so B345/B408 can provide zero motion instead of exposing raw defaults.
 if(!b288DerivedValid&&b457HaveLastValid){for(unsigned i=0;i<16;++i){b288DerivedVp[i]=b457LastValidVp[i];b288DerivedInverseVp[i]=b457LastValidInvVp[i];b288DerivedView[i]=b457LastValidView[i];b288DerivedProjection[i]=b457LastValidProjection[i];}b288DerivedValid=true;b443DerivedSnapshotCoherent=false;b443DerivedWriteSerial=0;b443DerivedResource=nullptr;}
 if(b288DerivedValid){
  // B439: validate the derived tuple after double->float conversion BEFORE replacing the
  // already-coherent capture tuple. B438 exposed a hole here: this path unconditionally set
  // valid=true, so overflow/non-finite derived values could reach B437 and collapse to its 960 sentinel.
  float b439Vp[16]{},b439InvVp[16]{},b439View[16]{},b439Projection[16]{};bool b439Finite=true;double b439MaxAbs=0.0;
  for(unsigned i=0;i<16;++i){b439Vp[i]=(float)b288DerivedVp[i];b439InvVp[i]=(float)b288DerivedInverseVp[i];b439View[i]=(float)b288DerivedView[i];b439Projection[i]=(float)b288DerivedProjection[i];const float values[4]={b439Vp[i],b439InvVp[i],b439View[i],b439Projection[i]};for(float v:values){b439Finite=b439Finite&&std::isfinite(v);if(std::isfinite(v))b439MaxAbs=(std::max)(b439MaxAbs,std::fabs((double)v));}}
  double b439WorstA=0.0,b439WorstB=0.0;bool b439ProductFinite=b439Finite;if(b439Finite){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double a=0.0,b=0.0;for(unsigned k=0;k<4;++k){a+=(double)b439Vp[r*4+k]*(double)b439InvVp[k*4+c];b+=(double)b439InvVp[r*4+k]*(double)b439Vp[k*4+c];}const double target=r==c?1.0:0.0;b439ProductFinite=b439ProductFinite&&std::isfinite(a)&&std::isfinite(b);if(std::isfinite(a))b439WorstA=(std::max)(b439WorstA,std::fabs(a-target));if(std::isfinite(b))b439WorstB=(std::max)(b439WorstB,std::fabs(b-target));}}
  const double b439Residual=b439ProductFinite?(std::min)(b439WorstA,b439WorstB):960.0;const bool b439DerivedTupleValid=b439Finite&&b439ProductFinite&&std::isfinite(b439Residual)&&b439Residual<=0.01;
  const bool b443ProvenanceEligible=b439DerivedTupleValid&&b443DerivedSnapshotCoherent&&b443DerivedWriteSerial!=0&&b443DerivedResource!=nullptr;
  if(b443ProvenanceEligible){b447AtomicOverrideApplied=true;std::memcpy(currentVpF,b439Vp,64);std::memcpy(currentInverseVpF,b439InvVp,64);std::memcpy(currentViewF,b439View,64);std::memcpy(currentProjectionF,b439Projection,64);std::memcpy(b457LastValidVp,b439Vp,64);std::memcpy(b457LastValidInvVp,b439InvVp,64);std::memcpy(b457LastValidView,b439View,64);std::memcpy(b457LastValidProjection,b439Projection,64);b457HaveLastValid=true;b457LastValidSerial=b443DerivedWriteSerial;if(b457UnsafeHoldActive){std::ofstream b457(path,std::ios::app);b457<<"B457: AUTHORITATIVE_CAMERA_RECOVERED source_frame_id="<<sourceId<<", write_serial="<<b443DerivedWriteSerial<<", action=resume_fresh_validated_camera_via_B453_rebase, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}b457UnsafeHoldActive=false;static unsigned b446AcceptedLog=0;if(b446QualifiedAtomicTuple&&((++b446AcceptedLog<=3)||(b446AcceptedLog%240)==0)){std::ofstream b446(path,std::ios::app);b446<<"B446: INDIVISIBLE_ATOMIC_CAMERA_TUPLE_COMMITTED source_frame_id="<<sourceId<<", resource="<<b443DerivedResource<<", write_serial="<<b443DerivedWriteSerial<<", tuple_components={View,Projection,ViewProjection,InverseViewProjection}, provenance=same_256B_write, inherited_projection=0, full_snapshot_lock_only_on_change=1, tuple_fingerprint="<<b447TupleFingerprint(currentVpF,currentInverseVpF,currentViewF,currentProjectionF)<<", action=commit_exact_B444_tuple_to_B443_B439_B437, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}static unsigned b443AcceptedLog=0;if((++b443AcceptedLog<=3)||(b443AcceptedLog%120)==0){std::ofstream b443(path,std::ios::app);b443<<"B443: COHERENT_CAMERA_SNAPSHOT_ACCEPTED source_frame_id="<<sourceId<<", resource="<<b443DerivedResource<<", write_serial="<<b443DerivedWriteSerial<<", tuple_components={View,Projection,ViewProjection,InverseViewProjection}, provenance=same_256B_write, inverse_residual="<<b439Residual<<", action=allow_derived_tuple_override, b442_handoff_gate_retained=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}}
  else if(b439DerivedTupleValid&&!b443DerivedSnapshotCoherent){static unsigned b443MixedLog=0;if((++b443MixedLog<=3)||(b443MixedLog%120)==0){std::ofstream b443(path,std::ios::app);b443<<"B443: MIXED_CAMERA_TUPLE_REJECTED source_frame_id="<<sourceId<<", resource="<<b443DerivedResource<<", write_serial="<<b443DerivedWriteSerial<<", tuple_components={View,Projection,ViewProjection,InverseViewProjection}, provenance=mixed_or_inherited_projection, action=retain_preoverride_coherent_capture_tuple, b442_handoff_gate_retained=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}}
  else{std::ofstream b439(path,std::ios::app);b439<<"B439: DERIVED_CAMERA_TUPLE_REJECTED source_frame_id="<<sourceId<<", finite="<<(b439Finite?1:0)<<", product_finite="<<(b439ProductFinite?1:0)<<", inverse_residual="<<b439Residual<<", max_abs="<<b439MaxAbs<<", action=retain_preoverride_coherent_camera_tuple, b438_gate_retained=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
  // B457 functional fallback: a selected authoritative write that fails B439/provenance must not
  // expose the pre-override raw/default tuple to B437. Hold the last B439-valid tuple as a carrier,
  // but force the existing unsafe-camera zero-motion/reset episode until a new valid tuple arrives.
  if(!b443ProvenanceEligible&&b457HaveLastValid){std::memcpy(currentVpF,b457LastValidVp,64);std::memcpy(currentInverseVpF,b457LastValidInvVp,64);std::memcpy(currentViewF,b457LastValidView,64);std::memcpy(currentProjectionF,b457LastValidProjection,64);b344ZeroMotionSafetyThisFrame=true;b344ResetThisFrame=true;b344UnsafeCameraActive=true;static unsigned b457HoldLog=0;if(!b457UnsafeHoldActive||(++b457HoldLog%120)==0){std::ofstream b457(path,std::ios::app);b457<<"B457: INVALID_AUTHORITATIVE_CAMERA_HELD source_frame_id="<<sourceId<<", rejected_write_serial="<<b443DerivedWriteSerial<<", last_valid_write_serial="<<b457LastValidSerial<<", held_tuple="<<b447TupleFingerprint(currentVpF,currentInverseVpF,currentViewF,currentProjectionF)<<", action=hold_last_B439_valid_tuple_plus_zero_motion_until_fresh_valid_authoritative_write, b453_rebase_on_recovery=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}b457UnsafeHoldActive=true;}
  // Keep the pre-override validity when the derived tuple is rejected; B457's held tuple remains mathematically valid
  // while B345/B408 explicitly marks the frame unsafe and supplies zero motion.
  valid=valid&&(!b439DerivedTupleValid?true:b439DerivedTupleValid);
  static bool b406UnsafeEpisodeResetIssued=false;static unsigned b408StableSafeFrames=30;if(b344ZeroMotionSafetyThisFrame){B407MarkUnsafeCameraFrame();b408StableSafeFrames=0;const bool b406ResetNow=!b406UnsafeEpisodeResetIssued;B345ApplyCameraZeroMotionSafety(b406ResetNow?TRUE:FALSE);b406UnsafeEpisodeResetIssued=true;if((b343FalseStaticFrames<=3&&b343FalseStaticFrames>=2)||(b343FalseStaticFrames%600)==0||(b300NoCandidateFrames<=3&&b300NoCandidateFrames>=2)||(b300NoCandidateFrames%600)==0){std::ofstream b345(path,std::ios::app);b345<<"B408: UNSAFE_CAMERA_EPISODE_ACTIVE source_frame_id="<<sourceId<<", unsafe_camera=1, zero_motion=1, xefg_reset_history="<<(b406ResetNow?1:0)<<", episode_entry="<<(b406ResetNow?1:0)<<", stable_safe_frames=0, rearm_required_safe_frames=30, repeated_history_reset=0, menu_transition_coalescing=1, protected_ghosting_module_unchanged=1, camera_selection_unchanged=1\n";}}else{B408MarkSafeCameraFrame();if(b408StableSafeFrames<30)++b408StableSafeFrames;if(b408StableSafeFrames>=30)b406UnsafeEpisodeResetIssued=false;if(b288HistoryReset)B297ResetCameraHistory();}
  static unsigned b288RuntimeLogOrdinal=0;++b288RuntimeLogOrdinal;
  if((b288RuntimeLogOrdinal%600)==0){std::ofstream b288(path,std::ios::app);b288<<"B296: CAMERA_RUNTIME_WIRED source_frame_id="<<sourceId<<", camera_source=B287_main_3d_rage, qualifying_rage_draws="<<qualifying<<", view_delta_max="<<b288ViewDelta<<", vp_delta_max="<<b288VpDelta<<", history_reset="<<(b288HistoryReset?1:0)<<", camera_derivation=B291_direct_invariant_per_locked_render_family_and_cluster, camera_selection=depth_owning_family_cluster_plus_cross_family_continuity_rescue_plausibility_gated, depth_source=B276_retained_native, jitter={0,0}, motion_scale={1,1}, consumed_by_runtime=1, b276_xefg_x4_frozen=1\n";b288.flush();}
 }
 if(b448LogThis){std::ofstream b448(path,std::ios::app);b448<<"B448: CAMERA_COMPONENT_PROVENANCE source_frame_id="<<sourceId<<", boundary=POST_B443_SELECTION, tuple="<<b447TupleFingerprint(currentVpF,currentInverseVpF,currentViewF,currentProjectionF)<<", view="<<b448ComponentFingerprint(currentViewF)<<", projection="<<b448ComponentFingerprint(currentProjectionF)<<", vp="<<b448ComponentFingerprint(currentVpF)<<", inverse_vp="<<b448ComponentFingerprint(currentInverseVpF)<<", b443_override_applied="<<(b447AtomicOverrideApplied?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 // B298 last-chance submit guard. This sits immediately before motion reconstruction so even a
 // fallback/selector path that bypasses the family logic cannot inject a catastrophic VP jump.
 static float b298LastSafeVp[16]{},b298LastSafeInvVp[16]{},b298LastSafeView[16]{},b298LastSafeProjection[16]{};static bool b298HaveLastSafe=false; static unsigned b447BoundaryLog=0;if(b448LogThis){std::ofstream b448(path,std::ios::app);b448<<"B448: CAMERA_COMPONENT_PROVENANCE source_frame_id="<<sourceId<<", boundary=PRE_B298, tuple="<<b447TupleFingerprint(currentVpF,currentInverseVpF,currentViewF,currentProjectionF)<<", view="<<b448ComponentFingerprint(currentViewF)<<", projection="<<b448ComponentFingerprint(currentProjectionF)<<", vp="<<b448ComponentFingerprint(currentVpF)<<", inverse_vp="<<b448ComponentFingerprint(currentInverseVpF)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";} if(valid&&((++b447BoundaryLog<=6)||(b447BoundaryLog%120)==0)){std::ofstream b447(path,std::ios::app);b447<<"B447: CAMERA_TUPLE_BOUNDARY source_frame_id="<<sourceId<<", boundary=PRE_B298, tuple_fingerprint="<<b447TupleFingerprint(currentVpF,currentInverseVpF,currentViewF,currentProjectionF)<<", b446_atomic_eligible="<<(b447AtomicOverrideApplied?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 // B433: B432 armed its baseline exception too late: the final B298 guard could see the
 // replacement camera before B300 completed its direct-resource lock, then hold the old baseline
 // forever.  While the old static resource is quarantined, validate the *final* camera presented
 // to B298 for three consecutive, mutually-continuous frames.  Only that bounded transition may
 // become the new last-safe baseline; ordinary catastrophic-jump protection remains unchanged.
 static float b433PendingVp[16]{},b433PendingInvVp[16]{},b433PendingView[16]{},b433PendingProjection[16]{};static unsigned b433PendingFrames=0;
 if(valid){double submitDelta=0.0;if(b298HaveLastSafe)for(unsigned i=0;i<16;++i)submitDelta=(std::max)(submitDelta,std::fabs((double)currentVpF[i]-(double)b298LastSafeVp[i]));const bool b432ValidatedBaseline=g_b432ValidatedReacquireBaselinePending.exchange(false,std::memory_order_acq_rel);const bool b433QuarantineActive=g_b430RejectedStaticCameraResource.load(std::memory_order_acquire)!=nullptr;bool b433ValidatedFinalTransition=false;double b433ContinuityDelta=0.0;if(b298HaveLastSafe&&submitDelta>1500.0&&b433QuarantineActive&&!b432ValidatedBaseline){if(b433PendingFrames){for(unsigned i=0;i<16;++i)b433ContinuityDelta=(std::max)(b433ContinuityDelta,std::fabs((double)currentVpF[i]-(double)b433PendingVp[i]));if(b433ContinuityDelta<=500.0)++b433PendingFrames;else b433PendingFrames=1;}else b433PendingFrames=1;std::memcpy(b433PendingVp,currentVpF,64);std::memcpy(b433PendingInvVp,currentInverseVpF,64);std::memcpy(b433PendingView,currentViewF,64);std::memcpy(b433PendingProjection,currentProjectionF,64);if(b433PendingFrames>=3){
 const double b438VpResidual=[](const float*vp,const float*invVp){double a[16]{},b[16]{},worstA=0.0,worstB=0.0;for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double va=0.0,vb=0.0;for(unsigned k=0;k<4;++k){va+=(double)vp[r*4+k]*(double)invVp[k*4+c];vb+=(double)invVp[r*4+k]*(double)vp[k*4+c];}a[r*4+c]=va;b[r*4+c]=vb;const double target=r==c?1.0:0.0;worstA=(std::max)(worstA,std::fabs(va-target));worstB=(std::max)(worstB,std::fabs(vb-target));}const double residual=(std::min)(worstA,worstB);return std::isfinite(residual)?residual:960.0;}(b433PendingVp,b433PendingInvVp);
 // B442: B438 only proved stored VP <-> inverse-VP consistency. B437's real motion gate
 // reconstructs VP from View x Projection, so validate that exact tuple here before B433 can commit it.
 float b442DerivedVp[16]{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double v=0.0;for(unsigned k=0;k<4;++k)v+=(double)b433PendingView[r*4+k]*(double)b433PendingProjection[k*4+c];b442DerivedVp[r*4+c]=(float)v;}
 const double b442ViewProjectionResidual=[](const float*vp,const float*invVp){double worstA=0.0,worstB=0.0;for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double a=0.0,b=0.0;for(unsigned k=0;k<4;++k){a+=(double)vp[r*4+k]*(double)invVp[k*4+c];b+=(double)invVp[r*4+k]*(double)vp[k*4+c];}const double target=r==c?1.0:0.0;if(!std::isfinite(a)||!std::isfinite(b))return 960.0;worstA=(std::max)(worstA,std::fabs(a-target));worstB=(std::max)(worstB,std::fabs(b-target));}const double residual=(std::min)(worstA,worstB);return std::isfinite(residual)?residual:960.0;}(b442DerivedVp,b433PendingInvVp);
 const bool b438Validated=b438VpResidual<=0.01;const bool b442Validated=b438Validated&&b442ViewProjectionResidual<=0.01;
 {std::ofstream b442(path,std::ios::app);b442<<"B442: CAMERA_HANDOFF_VALIDATION source_frame_id="<<sourceId<<", validation_frames="<<b433PendingFrames<<", pending_tuple_fingerprint="<<b447TupleFingerprint(b433PendingVp,b433PendingInvVp,b433PendingView,b433PendingProjection)<<", stored_vp_inverse_residual="<<b438VpResidual<<", view_times_projection_inverse_residual="<<b442ViewProjectionResidual<<", stored_pair_valid="<<(b438Validated?1:0)<<", full_tuple_valid="<<(b442Validated?1:0)<<", action="<<(b442Validated?"allow_B433_commit":"hold_last_safe_camera")<<", b440_b441_runtime_reacquire_disabled=1, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 if(b442Validated)b433ValidatedFinalTransition=true;
 else{std::ofstream b438(path,std::ios::app);b438<<"B442: INVALID_CAMERA_HANDOFF_REJECTED source_frame_id="<<sourceId<<", validation_frames="<<b433PendingFrames<<", stored_vp_inverse_residual="<<b438VpResidual<<", view_times_projection_inverse_residual="<<b442ViewProjectionResidual<<", valid=0, action=hold_last_safe_consumed_camera, b437_final_gate_retained=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";b433PendingFrames=0;}
}}else if(submitDelta<=1500.0||!b433QuarantineActive){b433PendingFrames=0;}if(b298HaveLastSafe&&submitDelta>1500.0&&!b432ValidatedBaseline&&!b433ValidatedFinalTransition){for(unsigned i=0;i<16;++i){currentVpF[i]=b298LastSafeVp[i];currentInverseVpF[i]=b298LastSafeInvVp[i];currentViewF[i]=b298LastSafeView[i];currentProjectionF[i]=b298LastSafeProjection[i];}std::ofstream q(path,std::ios::app);q<<"B298: FINAL_CAMERA_SUBMIT_GUARD source_frame_id="<<sourceId<<", blocked_vp_delta="<<submitDelta<<", submit_limit=1500, b433_quarantine_validation_frames="<<b433PendingFrames<<", action=hold_last_safe_camera_before_motion_reconstruction, history_reset=0, consumed_by_runtime=1\n";}else{std::memcpy(b298LastSafeVp,currentVpF,64);std::memcpy(b298LastSafeInvVp,currentInverseVpF,64);std::memcpy(b298LastSafeView,currentViewF,64);std::memcpy(b298LastSafeProjection,currentProjectionF,64);b298HaveLastSafe=true;if(b433ValidatedFinalTransition){b433PendingFrames=0;g_b434ValidatedBaselineCommitPending.store(true,std::memory_order_release);std::ofstream b433(path,std::ios::app);b433<<"B433: FINAL_GUARD_VALIDATED_REACQUIRE_BASELINE_COMMITTED source_frame_id="<<sourceId<<", validation_frames=3, vp_delta_from_old_baseline="<<submitDelta<<", candidate_continuity_delta="<<b433ContinuityDelta<<", old_submit_limit=1500, action=commit_validated_replacement_before_ordinary_b298_rejection, history_reset=0, zero_motion=0, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";}else if(b432ValidatedBaseline){std::ofstream b432(path,std::ios::app);b432<<"B432: FINAL_CAMERA_BASELINE_REALIGNED source_frame_id="<<sourceId<<", validated_reacquire=1, vp_delta="<<submitDelta<<", old_submit_limit=1500, action=accept_three_frame_validated_reacquired_camera_as_new_temporal_baseline, history_reset=0, zero_motion=0, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";}}}
 QueryPerformanceCounter(&b306AfterCamera);
 if(valid&&((b447BoundaryLog<=6)||(b447BoundaryLog%120)==0)){std::ofstream b447(path,std::ios::app);b447<<"B447: CAMERA_TUPLE_BOUNDARY source_frame_id="<<static_cast<unsigned long long>(sourceId)<<", boundary=PRE_B437_SUBMIT, tuple_fingerprint="<<b447TupleFingerprint(currentVpF,currentInverseVpF,currentViewF,currentProjectionF)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
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
