#pragma once
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
using Microsoft::WRL::ComPtr;
#ifdef GHOSTING_RUNTIME_BUILD
#define GHOST_API __declspec(dllexport)
#else
#define GHOST_API __declspec(dllimport)
#endif

using Microsoft::WRL::ComPtr;


constexpr unsigned kRtvs=4,kSrvs=16,kPsCbs=14,kCbEvents=4096,kMapEvents=16384,kKnownCbs=128,kSnapshots=8192,kSnapshotBytes=352;

struct B49OmEvent{std::atomic<unsigned> ready;ID3D11RenderTargetView* r[kRtvs];ID3D11DepthStencilView* dsv;unsigned ordinal,numviews;};

struct B49PsEvent{std::atomic<unsigned> ready;ID3D11ShaderResourceView* r[kSrvs];unsigned ordinal,start,count;};

struct B49DrawEvent{std::atomic<unsigned> ready;unsigned ordinal,type,a,b;int c;};

struct CbEvent{std::atomic<unsigned> ready{0};unsigned ordinal{},stage{},start{},count{};ID3D11Buffer* buffers[kPsCbs]{};};

struct MapEvent{std::atomic<unsigned> ready{0};unsigned ordinal{},type{},subresource{},map_type{},map_flags{};HRESULT hr{};ID3D11Resource* resource{};};

struct Snapshot{std::atomic<unsigned> ready{0};unsigned sequence{},byte_count{};ID3D11Resource* resource{};unsigned char bytes[kSnapshotBytes]{};};

struct B281CandidateWindow{unsigned cbIndex{},byteOffset{},byteWidth{},updates{};double maxDelta{},rmsDelta{};bool selected{};float values[16]{};};

// B300: after a stable gameplay-camera CB is acquired, stop classifying DrawIndexed calls.
// Map/Unmap keeps a tiny ring of raw 256-byte writes for the selected CB; Present chooses the
// candidate continuous with the last accepted gameplay camera. This removes the hot DrawIndexed
// COM-query path while retaining B299/B298 safety gates.
constexpr unsigned kB300DirectWriteRing=64;

struct B300RawCameraWrite{unsigned char bytes[256]{};unsigned long long serial{};};

// B489 diagnostic-only: correlate the exact B445 authoritative handoff object at publish and consume.
// No per-Unmap hashing/timing is performed. The exact 256-byte snapshots are copied only on the existing
// bounded B445 publish/consume path; fingerprints are calculated later at the source-frame capture boundary.
struct B489AuthoritativeHandoffSnapshot{unsigned long long publishedSerial{};unsigned publishedSourceFrame{};unsigned long long consumedSerial{};unsigned consumedSourceFrame{};unsigned char publishedBytes[256]{};unsigned char consumedBytes[256]{};BOOL publishedValid{};BOOL consumedValid{};};

// B484 diagnostic-only: bounded live Map/Unmap watchlist for already-known medium constant buffers.
// B481 proved there is only one known 256-byte CB, so B484 watches at most four pre-existing
// 256..512-byte non-selected CBs. No Draw hooks are reopened and camera authority is never changed.
constexpr unsigned kB484WatchSlots=4;

struct B485MatrixProbe{unsigned finiteMatrices{};unsigned affineLike{};unsigned invertible{};double bestInverseResidual{1.0e30};int bestInverseA{-1};int bestInverseB{-1};double bestDetAbs{};};

constexpr unsigned kB292MaxFamilies=32,kB293MaxClustersPerFamily=8;

struct B293ClusterAccum{unsigned long long draws{};unsigned lastIndexCount{},cbIndex{~0u};unsigned long long writeSerial{};double sumView[16]{},sumProjection[16]{};};

struct B292FamilyAccum{std::uintptr_t vsKey{};unsigned long long draws{};unsigned lastIndexCount{},cbIndex{~0u};unsigned long long writeSerial{};double sumView[16]{},sumProjection[16]{};B293ClusterAccum clusters[kB293MaxClustersPerFamily]{};unsigned clusterCount{},clusterOverflow{};};

struct PendingMap{ID3D11Resource* resource{};void* data{};unsigned byte_count{};bool target{};int b484WatchSlot{-1};};

using ps_cb_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,ID3D11Buffer* const*);

using om_rt_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,ID3D11RenderTargetView* const*,ID3D11DepthStencilView*);

using draw_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT);

using draw_indexed_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);

using draw_indexed_instanced_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);

using draw_instanced_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,UINT);

using dispatch_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT);

using map_fn=HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE*);

using unmap_fn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT);

struct B323ResourceSample { ID3D11Resource* resource{}; unsigned long long samples{}; long long totalTicks{}; long long originalTicks{}; };

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



struct ConsumerState{unsigned draw{};ID3D11ShaderResourceView*slots[kSrvs]{};const B49OmEvent*om{};bool target{};};

struct PipelineState{ID3D11Buffer*cbs[kPsCbs]{};unsigned cbLastOrdinal[kPsCbs]{};};


struct B160Candidate{unsigned draw{};UINT width{},height{},rtv_format{},dsv_format{},marker_width{},marker_height{},marker_format{};IUnknown*color_identity{};IUnknown*depth_identity{};IUnknown*marker_identity{};ID3D11Buffer*constant_buffer{};};

struct B160Reject{unsigned inspected{},valid{},rtv_format{},bounds{},dsv_format{},dsv_size{},samples{},marker{},identity{},top_count{};UINT observed_width{},observed_height{},observed_rtv_format{},observed_dsv_width{},observed_dsv_height{},observed_dsv_format{},observed_marker_width{},observed_marker_height{},observed_marker_format{},top_width[3]{},top_height[3]{};};

struct B160Pipe{ID3D11Buffer*cb[2][kPsCbs]{};};

struct B160M4{double a[4][4]{};};

struct B160Stats{unsigned sampled{},depth_valid{},finite{},onscreen{},reasonable{};double sum{},maximum{},sumdx{},sumdy{},neighbor_sum{};unsigned neighbors{};std::vector<double>mag,depth;};

struct B160MotionStats{unsigned valid{},finite{},onscreen{},nonzero{};double sum{},maximum{};};
struct CameraCollectionState {

std::atomic<ID3D11Buffer*> g_known_cbs[kKnownCbs];

std::atomic<unsigned> g_known_cb_bytes[kKnownCbs];

std::atomic<unsigned> g_known_cb_count{0},g_cb_next{0},g_cb_overflow{0},g_map_next{0},g_map_overflow{0},g_snapshot_next{0},g_snapshot_overflow{0};

ComPtr<ID3D11Buffer> g_b175FastConstants;

// B287 keeps the proven B286 DrawIndexed interception and adds diagnostic-only main-3D rage_matrices frame capture.
// The B276 XeSS/XeFG runtime remains frozen and does not consume these matrices.
SRWLOCK g_b280CandidateLock=SRWLOCK_INIT;

unsigned char g_b280PreviousBytes[kKnownCbs][kSnapshotBytes]{};

unsigned g_b280PreviousByteCount[kKnownCbs]{};

bool g_b280PreviousValid[kKnownCbs]{};

B281CandidateWindow g_b280CandidateWindows[kKnownCbs]{};

unsigned g_b280CandidateUpdateSerial{};

std::atomic<unsigned> g_b282TargetWriteCount[kKnownCbs]{};

std::atomic<unsigned> g_b282LastBindStage[kKnownCbs]{};

std::atomic<unsigned> g_b282LastBindSlot[kKnownCbs]{};

// B286 compatibility probe remains available. B287 adds exact main-pass matrix accumulation without touching runtime inputs.
SRWLOCK g_b286ProbeLock=SRWLOCK_INIT;

std::atomic<unsigned long long> g_b286IndexedDraws{0},g_b286ProbeSamples{0};

ComPtr<ID3D11VertexShader> g_b286LastVs;

unsigned g_b286LastIndexCount{},g_b286LastStartIndex{};

int g_b286LastBaseVertex{};

unsigned g_b286LastRtvW{},g_b286LastRtvH{},g_b286LastRtvFormat{};

unsigned g_b286LastDsvW{},g_b286LastDsvH{},g_b286LastDsvFormat{};

unsigned g_b286LastCbMask{};

unsigned g_b286LastCbBytes[kPsCbs]{};

bool g_b286LastFastColorMatch{},g_b286LastFastDepthMatch{};

// B287: latest exact 256-byte rage_matrices write plus per-frame main-3D accumulation.
SRWLOCK g_b287LatestLock=SRWLOCK_INIT;

unsigned char g_b287Latest256[kKnownCbs][256]{};

bool g_b287LatestValid[kKnownCbs]{};

unsigned long long g_b287LatestWriteSerial[kKnownCbs]{};

SRWLOCK g_b287FrameLock=SRWLOCK_INIT;

unsigned long long g_b287MainPassDraws{},g_b287QualifyingDraws{},g_b287RejectedScreenDraws{};

unsigned g_b287LastCbIndex=~0u,g_b287LastIndexCount{};

ComPtr<ID3D11VertexShader> g_b287LastVs;

unsigned long long g_b287LastWriteSerial{};

double g_b287SumWorld[16]{},g_b287SumWorldView[16]{},g_b287SumWorldViewProj[16]{},g_b287SumViewInverse[16]{};

double g_b291SumDirectView[16]{},g_b291SumDirectProjection[16]{};

unsigned long long g_b291DirectDeriveDraws=0,g_b291DirectRejectDraws=0;

SRWLOCK g_b300DirectWriteLock=SRWLOCK_INIT;

B300RawCameraWrite g_b300DirectWrites[kB300DirectWriteRing]{};

unsigned g_b300DirectWriteHead=0,g_b300DirectWriteCount=0;

// B445: preserve the exact bounded selected-camera write across the capture->Present handoff.
// h_unmap is already limited by B324/B339 to at most two eligible selected-resource captures per source frame,
// so this adds no Draw scanning and no unbounded render-thread work.
B300RawCameraWrite g_b445LatestSelectedSnapshot{};

ID3D11Resource* g_b445LatestSelectedResource=nullptr;

unsigned long long g_b445LatestSelectedSerial=0;

std::atomic<bool> g_b300DirectMode{false},g_b300DrawHooksReleased{false};

std::atomic<int> g_b300DirectCbIndex{-1};

std::atomic<ID3D11Resource*> g_b301DirectCbResource{nullptr};

// B430: quarantine the exact fresh-but-direct resource during the one-shot reacquisition.
std::atomic<ID3D11Resource*> g_b430RejectedStaticCameraResource{nullptr};

std::atomic<bool> g_b430ResponsiveReacquireActive{false};

// B431: while B430 quarantine is active, widen DrawIndexed camera classification briefly and
// skip the quarantined resource so alternate 256-byte camera CBs can win the selector.
std::atomic<unsigned long long> g_b431RejectedClassifierDraws{0};

// B432: one-shot authorization for the final submit guard after a different, responsive camera
// resource survives the B430 quarantine and three-frame B300 seed confirmation.
std::atomic<bool> g_b432ValidatedReacquireBaselinePending{false};

// B434: one-shot handoff from the validated B433 final-camera commit to the motion/history consumer.
// This aligns only the deliberate reacquire transition; ordinary B329 discontinuity protection remains unchanged.
std::atomic<bool> g_b434ValidatedBaselineCommitPending{false};

std::atomic<unsigned long long> g_b301DirectWriteSerial{0};

// B309: selected-camera writes are deduplicated by the 64-byte gViewInverse block at +192.
// Only unique/periodic candidates enter the bounded ring; matrix validation stays in Present.
std::atomic<unsigned long long> g_b309DirectGeneration{1};

std::atomic<unsigned long long> g_b309SelectedUnmaps{0},g_b309DuplicateSkips{0},g_b309CandidatesCaptured{0};

// B456 diagnostic-only: prove whether the locked 256-byte selected resource continues to receive
// advancing Map/Unmap writes after B437 loses camera validity. Hot-path work is atomics/byte compares only;
// all file logging remains on the existing sparse source-frame path.
std::atomic<unsigned long long> g_b456SelectedMaps{0},g_b456SelectedUnmaps{0},g_b456FullTupleChanges{0},g_b456ViewInverseChanges{0},g_b456PublishedWrites{0};

SRWLOCK g_b489AuthoritativeLock=SRWLOCK_INIT;

B489AuthoritativeHandoffSnapshot g_b489Authoritative{};

std::atomic<ID3D11Resource*> g_b484WatchResource[kB484WatchSlots]{};

std::atomic<unsigned> g_b484WatchBytes[kB484WatchSlots]{};

std::atomic<unsigned long long> g_b484WatchUnmaps[kB484WatchSlots]{},g_b484WatchChanges[kB484WatchSlots]{},g_b484WatchLatestFp[kB484WatchSlots]{};

SRWLOCK g_b484WatchLock=SRWLOCK_INIT;

unsigned char g_b484WatchLatest[kB484WatchSlots][256]{};

bool g_b484WatchValid[kB484WatchSlots]{};


// B475 diagnostic-only: identify which 16-byte lanes inside the already-selected 256-byte resource actually change. No camera selection or runtime behavior changes.
std::atomic<unsigned> g_b475ChangedLaneMask{0};

std::atomic<unsigned long long> g_b475ChangedWrites{0},g_b475LatestChangedSnapshotFp{0};

// B479 diagnostic-only: compare the live selected-resource Unmap stream with the legacy candidate-table snapshot.
std::atomic<unsigned long long> g_b479LiveSelectedUnmaps{0},g_b479LiveChangedSnapshots{0},g_b479LatestLiveSnapshotFp{0},g_b479PreviousLiveSnapshotFp{0};
 
// B324: gate expensive selected-camera Unmap processing to one eligible capture per real source-frame interval.
// The epoch advances in B175FastCaptureAtPresent; duplicate selected Unmaps bypass directly to original D3D11.
std::atomic<unsigned long long> g_b324SourceFrameEpoch{1};

std::atomic<unsigned long long> g_b324FirstEligibleCaptures{0},g_b324DuplicateSelectedUnmapBypasses{0};

std::atomic<unsigned long long> g_b339SecondChanceCaptures{0},g_b339SecondChanceBypasses{0};

std::atomic<bool> g_b303StaleDiscoveryConstantBypassLogged{false};

B292FamilyAccum g_b292Families[kB292MaxFamilies]{};

unsigned g_b292FamilyCount=0,g_b292FamilyOverflow=0;

// B321 diagnostic-only D3D11 render workload profiler. Camera classification hooks are not restored.
std::atomic<bool> g_b321ProfilerActive{false};

std::atomic<bool> g_b322ProfilerActive{false};
 // 0=waiting,1=Map/Unmap hooks removed,2=completed
std::atomic<ULONGLONG> g_b308DirectLockTick{0},g_b308AbStartTick{0};
};
struct CameraThreadState {
float t_b339FirstViewInverse[16]{};

unsigned long long t_b324CapturedEpoch=0;

unsigned long long t_b339SecondChanceEpoch=0;

bool t_b339FirstViewInverseValid=false;

bool t_b339SecondChanceCaptured=false;

unsigned long long t_b309Generation=0,t_b309SelectedOrdinal=0;

unsigned char t_b309LastViewInverseSignature[64]{};

bool t_b309LastViewInverseValid=false;

// B446: avoid taking the camera SRW lock hundreds of times per source frame for byte-identical selected-CB writes.
unsigned char t_b446LastFullSnapshot[256]{};

bool t_b446LastFullSnapshotValid=false;
};
struct CameraResult {bool valid{};float vp[16]{},inverseVp[16]{},view[16]{},projection[16]{};LARGE_INTEGER afterCamera{};};
struct CameraTransport {
void (*drawTransport0)(ID3D11DeviceContext*,ID3D11Resource*);
void (*drawTransport1)(ID3D11DeviceContext*,ID3D11Resource*);
void (*drawTransport2)(ID3D11DeviceContext*,ID3D11Resource*);
void (*drawTransport3)(ID3D11DeviceContext*,ID3D11Resource*);
};
GHOST_API CameraCollectionState& GhostingCollection();
GHOST_API CameraThreadState& GhostingThread();
GHOST_API void GhostingBindCameraTransport(const CameraTransport&);
GHOST_API int b280_known_cb_index(ID3D11Resource*r);
extern "C" GHOST_API BOOL WINAPI B434ConsumeValidatedBaselineCommit();
GHOST_API void b489_record_authoritative_publish(unsigned long long serial,unsigned sourceFrame,const unsigned char*bytes);
GHOST_API void b489_record_authoritative_consume(unsigned long long serial,unsigned sourceFrame,const unsigned char*bytes);
extern "C" GHOST_API BOOL WINAPI B489GetAuthoritativeHandoff(B489AuthoritativeHandoffSnapshot*out);
GHOST_API int b484_watch_slot(ID3D11Resource*r);
GHOST_API void b484_configure_watchlist(ID3D11Resource*selected);
GHOST_API bool b485_finite16(const float*m);
GHOST_API double b485_det4(const float*m);
GHOST_API double b485_inverse_pair_residual(const float*a,const float*b);
GHOST_API B485MatrixProbe b485_probe_first256(const unsigned char*raw);
GHOST_API void b484_capture_watch(int slot,const void*p,unsigned bytes);
GHOST_API unsigned long long b475_fingerprint256(const unsigned char* b);
GHOST_API bool b287_inverse4(const double in[16],double out[16]);
GHOST_API void b287_mul4(const double a[16],const double b[16],double out[16]);
GHOST_API bool b287_identity4d(const double*m);
GHOST_API void b309_capture_direct_camera_write_if_needed(ID3D11Resource*r,const void*p,unsigned bytes);
GHOST_API void b280_observe_constant_write(ID3D11Resource*r,const void*p,unsigned bytes);
GHOST_API float f32(const Snapshot*s,unsigned i);
GHOST_API B160M4 b160matrix(const Snapshot*s,unsigned base,bool transpose);
GHOST_API bool b160inverse(const B160M4&m,B160M4&z);
GHOST_API bool GhostingCaptureCameraSafe();
GHOST_API void GhostingSetCaptureCameraSafe(bool);
GHOST_API void GhostingSelectCamera(ID3D11DeviceContext*,unsigned,const char*,const Snapshot&,unsigned long long,CameraResult&);
GHOST_API void GhostingClassifyDraw(const unsigned char*,int,ID3D11VertexShader*,UINT,unsigned long long);
#define g_b175FastConstants (GhostingCollection().g_b175FastConstants)
#define g_b280CandidateLock (GhostingCollection().g_b280CandidateLock)
#define g_b280CandidateUpdateSerial (GhostingCollection().g_b280CandidateUpdateSerial)
#define g_b280CandidateWindows (GhostingCollection().g_b280CandidateWindows)
#define g_b280PreviousByteCount (GhostingCollection().g_b280PreviousByteCount)
#define g_b280PreviousBytes (GhostingCollection().g_b280PreviousBytes)
#define g_b280PreviousValid (GhostingCollection().g_b280PreviousValid)
#define g_b282LastBindSlot (GhostingCollection().g_b282LastBindSlot)
#define g_b282LastBindStage (GhostingCollection().g_b282LastBindStage)
#define g_b282TargetWriteCount (GhostingCollection().g_b282TargetWriteCount)
#define g_b286IndexedDraws (GhostingCollection().g_b286IndexedDraws)
#define g_b286LastBaseVertex (GhostingCollection().g_b286LastBaseVertex)
#define g_b286LastCbBytes (GhostingCollection().g_b286LastCbBytes)
#define g_b286LastCbMask (GhostingCollection().g_b286LastCbMask)
#define g_b286LastDsvFormat (GhostingCollection().g_b286LastDsvFormat)
#define g_b286LastDsvH (GhostingCollection().g_b286LastDsvH)
#define g_b286LastDsvW (GhostingCollection().g_b286LastDsvW)
#define g_b286LastFastColorMatch (GhostingCollection().g_b286LastFastColorMatch)
#define g_b286LastFastDepthMatch (GhostingCollection().g_b286LastFastDepthMatch)
#define g_b286LastIndexCount (GhostingCollection().g_b286LastIndexCount)
#define g_b286LastRtvFormat (GhostingCollection().g_b286LastRtvFormat)
#define g_b286LastRtvH (GhostingCollection().g_b286LastRtvH)
#define g_b286LastRtvW (GhostingCollection().g_b286LastRtvW)
#define g_b286LastStartIndex (GhostingCollection().g_b286LastStartIndex)
#define g_b286LastVs (GhostingCollection().g_b286LastVs)
#define g_b286ProbeLock (GhostingCollection().g_b286ProbeLock)
#define g_b286ProbeSamples (GhostingCollection().g_b286ProbeSamples)
#define g_b287FrameLock (GhostingCollection().g_b287FrameLock)
#define g_b287LastCbIndex (GhostingCollection().g_b287LastCbIndex)
#define g_b287LastIndexCount (GhostingCollection().g_b287LastIndexCount)
#define g_b287LastVs (GhostingCollection().g_b287LastVs)
#define g_b287LastWriteSerial (GhostingCollection().g_b287LastWriteSerial)
#define g_b287Latest256 (GhostingCollection().g_b287Latest256)
#define g_b287LatestLock (GhostingCollection().g_b287LatestLock)
#define g_b287LatestValid (GhostingCollection().g_b287LatestValid)
#define g_b287LatestWriteSerial (GhostingCollection().g_b287LatestWriteSerial)
#define g_b287MainPassDraws (GhostingCollection().g_b287MainPassDraws)
#define g_b287QualifyingDraws (GhostingCollection().g_b287QualifyingDraws)
#define g_b287RejectedScreenDraws (GhostingCollection().g_b287RejectedScreenDraws)
#define g_b287SumViewInverse (GhostingCollection().g_b287SumViewInverse)
#define g_b287SumWorld (GhostingCollection().g_b287SumWorld)
#define g_b287SumWorldView (GhostingCollection().g_b287SumWorldView)
#define g_b287SumWorldViewProj (GhostingCollection().g_b287SumWorldViewProj)
#define g_b291DirectDeriveDraws (GhostingCollection().g_b291DirectDeriveDraws)
#define g_b291DirectRejectDraws (GhostingCollection().g_b291DirectRejectDraws)
#define g_b291SumDirectProjection (GhostingCollection().g_b291SumDirectProjection)
#define g_b291SumDirectView (GhostingCollection().g_b291SumDirectView)
#define g_b292Families (GhostingCollection().g_b292Families)
#define g_b292FamilyCount (GhostingCollection().g_b292FamilyCount)
#define g_b292FamilyOverflow (GhostingCollection().g_b292FamilyOverflow)
#define g_b300DirectCbIndex (GhostingCollection().g_b300DirectCbIndex)
#define g_b300DirectMode (GhostingCollection().g_b300DirectMode)
#define g_b300DirectWriteCount (GhostingCollection().g_b300DirectWriteCount)
#define g_b300DirectWriteHead (GhostingCollection().g_b300DirectWriteHead)
#define g_b300DirectWriteLock (GhostingCollection().g_b300DirectWriteLock)
#define g_b300DirectWrites (GhostingCollection().g_b300DirectWrites)
#define g_b300DrawHooksReleased (GhostingCollection().g_b300DrawHooksReleased)
#define g_b301DirectCbResource (GhostingCollection().g_b301DirectCbResource)
#define g_b301DirectWriteSerial (GhostingCollection().g_b301DirectWriteSerial)
#define g_b303StaleDiscoveryConstantBypassLogged (GhostingCollection().g_b303StaleDiscoveryConstantBypassLogged)
#define g_b308AbStartTick (GhostingCollection().g_b308AbStartTick)
#define g_b308DirectLockTick (GhostingCollection().g_b308DirectLockTick)
#define g_b309CandidatesCaptured (GhostingCollection().g_b309CandidatesCaptured)
#define g_b309DirectGeneration (GhostingCollection().g_b309DirectGeneration)
#define g_b309DuplicateSkips (GhostingCollection().g_b309DuplicateSkips)
#define g_b309SelectedUnmaps (GhostingCollection().g_b309SelectedUnmaps)
#define g_b321ProfilerActive (GhostingCollection().g_b321ProfilerActive)
#define g_b322ProfilerActive (GhostingCollection().g_b322ProfilerActive)
#define g_b324DuplicateSelectedUnmapBypasses (GhostingCollection().g_b324DuplicateSelectedUnmapBypasses)
#define g_b324FirstEligibleCaptures (GhostingCollection().g_b324FirstEligibleCaptures)
#define g_b324SourceFrameEpoch (GhostingCollection().g_b324SourceFrameEpoch)
#define g_b339SecondChanceBypasses (GhostingCollection().g_b339SecondChanceBypasses)
#define g_b339SecondChanceCaptures (GhostingCollection().g_b339SecondChanceCaptures)
#define g_b430RejectedStaticCameraResource (GhostingCollection().g_b430RejectedStaticCameraResource)
#define g_b430ResponsiveReacquireActive (GhostingCollection().g_b430ResponsiveReacquireActive)
#define g_b431RejectedClassifierDraws (GhostingCollection().g_b431RejectedClassifierDraws)
#define g_b432ValidatedReacquireBaselinePending (GhostingCollection().g_b432ValidatedReacquireBaselinePending)
#define g_b434ValidatedBaselineCommitPending (GhostingCollection().g_b434ValidatedBaselineCommitPending)
#define g_b445LatestSelectedResource (GhostingCollection().g_b445LatestSelectedResource)
#define g_b445LatestSelectedSerial (GhostingCollection().g_b445LatestSelectedSerial)
#define g_b445LatestSelectedSnapshot (GhostingCollection().g_b445LatestSelectedSnapshot)
#define g_b456FullTupleChanges (GhostingCollection().g_b456FullTupleChanges)
#define g_b456PublishedWrites (GhostingCollection().g_b456PublishedWrites)
#define g_b456SelectedMaps (GhostingCollection().g_b456SelectedMaps)
#define g_b456SelectedUnmaps (GhostingCollection().g_b456SelectedUnmaps)
#define g_b456ViewInverseChanges (GhostingCollection().g_b456ViewInverseChanges)
#define g_b475ChangedLaneMask (GhostingCollection().g_b475ChangedLaneMask)
#define g_b475ChangedWrites (GhostingCollection().g_b475ChangedWrites)
#define g_b475LatestChangedSnapshotFp (GhostingCollection().g_b475LatestChangedSnapshotFp)
#define g_b479LatestLiveSnapshotFp (GhostingCollection().g_b479LatestLiveSnapshotFp)
#define g_b479LiveChangedSnapshots (GhostingCollection().g_b479LiveChangedSnapshots)
#define g_b479LiveSelectedUnmaps (GhostingCollection().g_b479LiveSelectedUnmaps)
#define g_b479PreviousLiveSnapshotFp (GhostingCollection().g_b479PreviousLiveSnapshotFp)
#define g_b484WatchBytes (GhostingCollection().g_b484WatchBytes)
#define g_b484WatchChanges (GhostingCollection().g_b484WatchChanges)
#define g_b484WatchLatest (GhostingCollection().g_b484WatchLatest)
#define g_b484WatchLatestFp (GhostingCollection().g_b484WatchLatestFp)
#define g_b484WatchLock (GhostingCollection().g_b484WatchLock)
#define g_b484WatchResource (GhostingCollection().g_b484WatchResource)
#define g_b484WatchUnmaps (GhostingCollection().g_b484WatchUnmaps)
#define g_b484WatchValid (GhostingCollection().g_b484WatchValid)
#define g_b489Authoritative (GhostingCollection().g_b489Authoritative)
#define g_b489AuthoritativeLock (GhostingCollection().g_b489AuthoritativeLock)
#define g_cb_next (GhostingCollection().g_cb_next)
#define g_cb_overflow (GhostingCollection().g_cb_overflow)
#define g_known_cb_bytes (GhostingCollection().g_known_cb_bytes)
#define g_known_cb_count (GhostingCollection().g_known_cb_count)
#define g_known_cbs (GhostingCollection().g_known_cbs)
#define g_map_next (GhostingCollection().g_map_next)
#define g_map_overflow (GhostingCollection().g_map_overflow)
#define g_snapshot_next (GhostingCollection().g_snapshot_next)
#define g_snapshot_overflow (GhostingCollection().g_snapshot_overflow)
#define t_b309Generation (GhostingThread().t_b309Generation)
#define t_b309LastViewInverseSignature (GhostingThread().t_b309LastViewInverseSignature)
#define t_b309LastViewInverseValid (GhostingThread().t_b309LastViewInverseValid)
#define t_b309SelectedOrdinal (GhostingThread().t_b309SelectedOrdinal)
#define t_b324CapturedEpoch (GhostingThread().t_b324CapturedEpoch)
#define t_b339FirstViewInverseValid (GhostingThread().t_b339FirstViewInverseValid)
#define t_b339SecondChanceCaptured (GhostingThread().t_b339SecondChanceCaptured)
#define t_b339SecondChanceEpoch (GhostingThread().t_b339SecondChanceEpoch)
#define t_b446LastFullSnapshot (GhostingThread().t_b446LastFullSnapshot)
#define t_b446LastFullSnapshotValid (GhostingThread().t_b446LastFullSnapshotValid)

#define t_b339FirstViewInverse (GhostingThread().t_b339FirstViewInverse)
GHOST_API bool GhostingAdmitSelectedWrite(ID3D11Resource*,const PendingMap&);
GHOST_API unsigned GhostingClassifierStride();
GHOST_API bool GhostingClassifierSnapshotAllowed(ID3D11Resource*,bool);
GHOST_API bool GhostingPrewarmConstants0(const Snapshot*,float*,float*);
GHOST_API bool GhostingPrewarmConstants1(const Snapshot*,float*,float*);
