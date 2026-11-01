#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d11_4.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <pdh.h>
#include <dwmapi.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "dwmapi.lib")
#include <xess_fg/xefg_swapchain.h>
#include <xess_fg/xefg_swapchain_d3d12.h>
#include <xess/xess_d3d11.h>
#include <xell/xell.h>
#include <xell/xell_d3d12.h>
#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <wrl/client.h>

#define GTAXESS_CORE_RENDER_FG_LOCKED 1
static_assert(GTAXESS_CORE_RENDER_FG_LOCKED == 1, "B339 core render/FG lock must remain enabled");
using Microsoft::WRL::ComPtr;
using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
extern "C" HRESULT WINAPI B75AttachWriteTrace(ID3D11DeviceContext*,std::atomic<unsigned>*,std::atomic<unsigned>*);
extern "C" void WINAPI B175ResetFastFreshness();
extern "C" BOOL WINAPI B434ConsumeValidatedBaselineCommit();
struct B489AuthoritativeHandoffSnapshot{unsigned long long publishedSerial{};unsigned publishedSourceFrame{};unsigned long long consumedSerial{};unsigned consumedSourceFrame{};unsigned char publishedBytes[256]{};unsigned char consumedBytes[256]{};BOOL publishedValid{};BOOL consumedValid{};};
extern "C" BOOL WINAPI B489GetAuthoritativeHandoff(B489AuthoritativeHandoffSnapshot*);

static DrawFn g_originalDraw{};
static std::atomic<unsigned>* g_active{};
static ComPtr<ID3D11DeviceContext> g_nativeContext;
static ComPtr<ID3D11Texture2D> g_nativeTexture;
static ComPtr<ID3D11RenderTargetView> g_nativeRtv;
static ComPtr<IDXGIKeyedMutex> g_nativeMutex;
static ComPtr<ID3D12Device> g_device12;
static ComPtr<ID3D12CommandQueue> g_queue12;
static std::atomic<bool> g_b174rAllowTearing{false};
static ComPtr<ID3D11Device5> g_b173fNativeDevice5;
static ComPtr<ID3D11DeviceContext4> g_b173fNativeContext4;
static ComPtr<ID3D11Fence> g_b173fNativeFence;
static ComPtr<ID3D12Fence> g_b173fSharedFence12;
static UINT64 g_b173fFenceValue{};
static ComPtr<ID3D12Fence> g_b173nStageFence;
static std::atomic<UINT64> g_b173nStageFenceValue{0};
static UINT64 g_b173nLastNativeFenceValue{};
static UINT64 g_b173nLastOn12Marker{};
static ComPtr<ID3D11Device> g_on12Device;
static ComPtr<ID3D11DeviceContext> g_on12Context;
static ComPtr<ID3D11On12Device> g_on12;
static std::atomic<unsigned> g_b175qBridgeStage{0};
static ComPtr<IDXGIAdapter> g_b175qBridgeAdapter;
static DXGI_ADAPTER_DESC g_b175qBridgeAdapterDesc{};
// B354: split bridge prewarm. Only D3D12 device/queue creation runs off the GTA render thread.
// D3D11On12 creation remains a render-thread final handoff to avoid the prior full-background bridge failure mode.
static std::atomic<unsigned> g_b354BridgePrewarmState{0}; // 0 idle, 1 working, 2 D3D12 ready, 3 failed
static std::atomic<bool> g_b354BridgePrewarmStarted{false};
static ComPtr<ID3D11Device> g_b174eWorkerOn12Device;
static ComPtr<ID3D11DeviceContext> g_b174eWorkerOn12Context;
static ComPtr<ID3D11On12Device> g_b174eWorkerOn12;
static ComPtr<ID3D12CommandQueue> g_b174iPreparationQueue;
static ComPtr<ID3D12Fence> g_b174iPreparationFence;
static std::atomic<UINT64> g_b174iPreparationFenceValue{0};
static ComPtr<ID3D11ComputeShader> g_b174eWorkerMotionShader;
static ComPtr<ID3D11ComputeShader> g_b330WorkerNativeMotionShader;
extern "C" ID3D11Texture2D* WINAPI B330AcquireNativeMotionSource();
static ComPtr<ID3D11Buffer> g_b174eWorkerMotionConstants;
static ComPtr<ID3D11ComputeShader> g_b175bNativeMotionShader;
static ComPtr<ID3D11VertexShader> g_b175bColorConvertVs;
static ComPtr<ID3D11PixelShader> g_b175bColorConvertPs;
static ComPtr<ID3D11RasterizerState> g_b175bColorConvertRs;
static xess_context_handle_t g_b175bXess{};
static xess_quality_settings_t g_b175bXessQuality=XESS_QUALITY_SETTING_PERFORMANCE;
static UINT g_b232OutputWidth=1920,g_b232OutputHeight=1080;
static bool g_b232ConfigLoaded=false;
static std::string g_b232ResolutionMode="AUTO",g_b232QualityMode="AUTO";
static std::atomic<bool> g_b175bXessReady{false},g_b175bXessReset{true};
// B407: final-consumption unsafe-camera episode guard. Camera classification can flicker safe for
// a frame while the underlying unsafe episode continues, so the final worker reset decision uses
// a short grace window instead of trusting a per-source-frame branch latch.
static std::atomic<ULONGLONG> g_b407LastUnsafeCameraTick{0};
static std::atomic<bool> g_b407UnsafeResetConsumed{false};
static std::atomic<bool> g_b408UnsafeEpisodeActive{false};
static std::atomic<unsigned> g_b408StableSafeFrames{30};
static std::atomic<unsigned> g_b175cXessContextState{0};
static std::atomic<unsigned long long> g_b175bXessFrames{0},g_b175bXessErrors{0},g_b175bDeviceErrors{0};
static ComPtr<ID3D11Texture2D> g_openedTexture;
static ComPtr<ID3D11RenderTargetView> g_openedRtv;
static ComPtr<IDXGIKeyedMutex> g_openedMutex;
static HANDLE g_sharedHandle{};
static ULONGLONG g_until{};
static unsigned g_cycles{}, g_nativeFailures{}, g_on12Failures{};
static bool g_armed{}, g_done{};


// B334: one-shot visual forensic packet. F10 only completes after a true source-frame A/A+1 pair.
static bool g_b333F10WasDown=false;
static int g_b333CapturePhase=0;
static unsigned long long g_b333PacketId=0;
static unsigned long long g_b334FrameA=0;
static std::vector<unsigned char> g_b333ColorA;
static UINT g_b333ColorW=0,g_b333ColorH=0;
static std::string g_b333PacketDir;
static float b333_half_to_float(unsigned short h){unsigned s=(h>>15)&1,e=(h>>10)&31,m=h&1023;unsigned out;if(e==0){if(m==0)out=s<<31;else{e=1;while((m&1024)==0){m<<=1;--e;}m&=1023;out=(s<<31)|((e+112)<<23)|(m<<13);}}else if(e==31)out=(s<<31)|0x7f800000|(m<<13);else out=(s<<31)|((e+112)<<23)|(m<<13);float f;std::memcpy(&f,&out,4);return f;}
static bool b333_write_bmp32(const std::string&fn,UINT w,UINT h,const std::vector<unsigned char>&bgra){if(bgra.size()<(size_t)w*h*4)return false;BITMAPFILEHEADER fh{};BITMAPINFOHEADER ih{};fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+w*h*4;ih.biSize=sizeof(ih);ih.biWidth=(LONG)w;ih.biHeight=-(LONG)h;ih.biPlanes=1;ih.biBitCount=32;ih.biCompression=BI_RGB;std::ofstream o(fn,std::ios::binary);if(!o)return false;o.write((char*)&fh,sizeof(fh));o.write((char*)&ih,sizeof(ih));o.write((char*)bgra.data(),(std::streamsize)w*h*4);return !!o;}
static bool b333_read_bgra(ID3D11DeviceContext*ctx,ID3D11Texture2D*src,std::vector<unsigned char>&dst,UINT&w,UINT&h){if(!ctx||!src)return false;D3D11_TEXTURE2D_DESC d{};src->GetDesc(&d);if(d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||d.SampleDesc.Count!=1)return false;ComPtr<ID3D11Device>dev;ctx->GetDevice(&dev);D3D11_TEXTURE2D_DESC sd=d;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;ComPtr<ID3D11Texture2D>st;if(FAILED(dev->CreateTexture2D(&sd,nullptr,&st)))return false;ctx->CopyResource(st.Get(),src);ctx->Flush();D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(ctx->Map(st.Get(),0,D3D11_MAP_READ,0,&m)))return false;w=d.Width;h=d.Height;dst.resize((size_t)w*h*4);for(UINT y=0;y<h;++y)std::memcpy(dst.data()+(size_t)y*w*4,(unsigned char*)m.pData+(size_t)y*m.RowPitch,(size_t)w*4);ctx->Unmap(st.Get(),0);return true;}
static bool b333_capture_motion(ID3D11DeviceContext*ctx,ID3D11Texture2D*src,const std::string&dir,const char*tag,unsigned long long frame){if(!ctx||!src)return false;D3D11_TEXTURE2D_DESC d{};src->GetDesc(&d);if(d.Format!=DXGI_FORMAT_R16G16_FLOAT||d.SampleDesc.Count!=1)return false;ComPtr<ID3D11Device>dev;ctx->GetDevice(&dev);D3D11_TEXTURE2D_DESC sd=d;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;ComPtr<ID3D11Texture2D>st;if(FAILED(dev->CreateTexture2D(&sd,nullptr,&st)))return false;ctx->CopyResource(st.Get(),src);ctx->Flush();D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(ctx->Map(st.Get(),0,D3D11_MAP_READ,0,&m)))return false;std::vector<unsigned char>vis((size_t)d.Width*d.Height*4);double sumMag=0,sumVx=0,sumVy=0,sumAbsVx=0,sumAbsVy=0,maxMag=0,minVx=1.0e30,maxVx=-1.0e30,minVy=1.0e30,maxVy=-1.0e30;unsigned nz=0,valid=0,invalid=0;for(UINT y=0;y<d.Height;++y){auto*row=(unsigned short*)((unsigned char*)m.pData+(size_t)y*m.RowPitch);for(UINT x=0;x<d.Width;++x){float vx=b333_half_to_float(row[x*2]),vy=b333_half_to_float(row[x*2+1]);if(!std::isfinite(vx)||!std::isfinite(vy)){++invalid;vx=0;vy=0;}else{++valid;double mag=sqrt((double)vx*vx+(double)vy*vy);sumMag+=mag;sumVx+=vx;sumVy+=vy;sumAbsVx+=fabs((double)vx);sumAbsVy+=fabs((double)vy);if(mag>maxMag)maxMag=mag;if(vx<minVx)minVx=vx;if(vx>maxVx)maxVx=vx;if(vy<minVy)minVy=vy;if(vy>maxVy)maxVy=vy;if(mag>0.001)++nz;}double mag=sqrt((double)vx*vx+(double)vy*vy);float sx=vx*0.5f,sy=vy*0.5f;int r=(int)(128+sx*12),g=(int)(128-sy*12);int b=(int)(mag*8);r=r<0?0:r>255?255:r;g=g<0?0:g>255?255:g;b=b<0?0:b>255?255:b;size_t i=((size_t)y*d.Width+x)*4;vis[i+0]=(unsigned char)b;vis[i+1]=(unsigned char)g;vis[i+2]=(unsigned char)r;vis[i+3]=255;}}ctx->Unmap(st.Get(),0);b333_write_bmp32(dir+"\\motion_"+tag+"_falsecolor.bmp",d.Width,d.Height,vis);const double denom=valid?double(valid):1.0;std::ofstream meta(dir+"\\motion_"+tag+"_stats.txt");meta<<"B334 native motion raw-unit forensic\nframe="<<frame<<"\nsize="<<d.Width<<"x"<<d.Height<<"\nformat=R16G16_FLOAT\nvalid_vectors="<<valid<<"\ninvalid_vectors="<<invalid<<"\nmean_vx="<<(sumVx/denom)<<"\nmean_vy="<<(sumVy/denom)<<"\nmean_abs_vx="<<(sumAbsVx/denom)<<"\nmean_abs_vy="<<(sumAbsVy/denom)<<"\nmin_vx="<<(valid?minVx:0)<<"\nmax_vx="<<(valid?maxVx:0)<<"\nmin_vy="<<(valid?minVy:0)<<"\nmax_vy="<<(valid?maxVy:0)<<"\nmean_magnitude="<<(sumMag/denom)<<"\nmax_magnitude="<<maxMag<<"\nnonzero_pixels="<<nz<<"\nnonzero_ratio="<<(valid?((double)nz/denom):0.0)<<"\nB334_runtime_transform=negate_xy_no_spatial_scale\nB334_runtime_spatial_scale=1x_raw_native_values\n";return true;}
static std::string b335_gta_root(){char base[MAX_PATH]{};HMODULE exe=GetModuleHandleA("GTA5.exe");if(!exe)exe=GetModuleHandleA(nullptr);if(exe)GetModuleFileNameA(exe,base,MAX_PATH);std::string d(base);auto n=d.find_last_of("\\/");if(n!=std::string::npos)d.resize(n+1);return d;}
static std::string b333_make_packet_dir(){std::string d=b335_gta_root();SYSTEMTIME st{};GetLocalTime(&st);char leaf[128]{};sprintf_s(leaf,"GTAXeSS-B335-Debug-%04u%02u%02u-%02u%02u%02u-%llu",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,++g_b333PacketId);d+=leaf;CreateDirectoryA(d.c_str(),nullptr);return d;}
static void b333_poll_forensic_capture(ID3D11DeviceContext*ctx,ID3D11Texture2D*color,unsigned long long frame,std::ofstream&out){bool down=(GetAsyncKeyState(VK_F10)&0x8000)!=0;if(down&&!g_b333F10WasDown&&g_b333CapturePhase==0){g_b333PacketDir=b333_make_packet_dir();g_b333CapturePhase=1;g_b334FrameA=0;out<<"B335: FORENSIC_CAPTURE_ARMED source_frame_id="<<frame<<", hotkey=F10, packet_dir="<<g_b333PacketDir<<", requirement=source_frame_B_equals_A_plus_1\n";}g_b333F10WasDown=down;if(g_b333CapturePhase==0)return;std::vector<unsigned char>cur;UINT w=0,h=0;bool colorOk=b333_read_bgra(ctx,color,cur,w,h);ComPtr<ID3D11Texture2D>mv;{auto*p=B330AcquireNativeMotionSource();if(p)mv.Attach(p);}if(g_b333CapturePhase==1){if(colorOk)b333_write_bmp32(g_b333PacketDir+"\\color_A.bmp",w,h,cur);bool motionOk=mv&&b333_capture_motion(ctx,mv.Get(),g_b333PacketDir,"A",frame);g_b333ColorA=cur;g_b333ColorW=w;g_b333ColorH=h;g_b334FrameA=frame;g_b333CapturePhase=2;out<<"B335: FORENSIC_CAPTURE_FRAME tag=A, source_frame_id="<<frame<<", color="<<(colorOk?1:0)<<", motion="<<(motionOk?1:0)<<"\n";return;}if(frame!=g_b334FrameA+1){out<<"B335: FORENSIC_CAPTURE_REBASE old_A="<<g_b334FrameA<<", new_A="<<frame<<", gap="<<(frame>g_b334FrameA?frame-g_b334FrameA:0)<<", reason=not_adjacent, action=replace_A_and_wait_for_A_plus_1\n";if(colorOk)b333_write_bmp32(g_b333PacketDir+"\\color_A.bmp",w,h,cur);bool motionOk=mv&&b333_capture_motion(ctx,mv.Get(),g_b333PacketDir,"A",frame);g_b333ColorA=cur;g_b333ColorW=w;g_b333ColorH=h;g_b334FrameA=frame;out<<"B335: FORENSIC_CAPTURE_FRAME tag=A_REBASE, source_frame_id="<<frame<<", color="<<(colorOk?1:0)<<", motion="<<(motionOk?1:0)<<"\n";return;}if(colorOk)b333_write_bmp32(g_b333PacketDir+"\\color_B.bmp",w,h,cur);bool motionOk=mv&&b333_capture_motion(ctx,mv.Get(),g_b333PacketDir,"B",frame);if(colorOk&&g_b333ColorA.size()==cur.size()&&w==g_b333ColorW&&h==g_b333ColorH){std::vector<unsigned char>diff(cur.size());for(size_t i=0;i<cur.size();i+=4){int db=abs((int)cur[i]-(int)g_b333ColorA[i]),dg=abs((int)cur[i+1]-(int)g_b333ColorA[i+1]),dr=abs((int)cur[i+2]-(int)g_b333ColorA[i+2]);int v=(db+dg+dr)/3;diff[i]=diff[i+1]=diff[i+2]=(unsigned char)v;diff[i+3]=255;}b333_write_bmp32(g_b333PacketDir+"\\color_difference_A_to_B.bmp",w,h,diff);}std::ofstream note(g_b333PacketDir+"\\README.txt");note<<"GTAXeSS B335 diagnostic forensic packet (prior rejected native-motion candidate retained for evidence only)\nPress F10 created this packet.\nframe_A="<<g_b334FrameA<<"\nframe_B="<<frame<<"\nframe_delta="<<(frame-g_b334FrameA)<<"\ncolor_A/B.bmp = guaranteed source-frame A and A+1 capture\nmotion_A/B_falsecolor.bmp = raw GTA R16G16_FLOAT values before B334 runtime transform\nmotion_A/B_stats.txt = extended raw component and magnitude statistics\nB335 runtime motion = reconstructed depth/camera; captured 960x540 candidate is diagnostic-only and NOT fed to XeFG\ncolor_difference_A_to_B.bmp = absolute RGB frame difference\nSend this entire folder with the B335 ScriptHook log.\n";out<<"B335: FORENSIC_CAPTURE_COMPLETE source_frame_A="<<g_b334FrameA<<", source_frame_B="<<frame<<", frame_delta="<<(frame-g_b334FrameA)<<", packet_dir="<<g_b333PacketDir<<", color="<<(colorOk?1:0)<<", motion="<<(motionOk?1:0)<<"\n";g_b333CapturePhase=0;g_b334FrameA=0;g_b333ColorA.clear();}
static std::string log_path(){return b335_gta_root()+"GTAXeSS-B352-Bridge.log";}
static void log_line(const std::string&s){std::ofstream o(log_path(),std::ios::app);if(o)o<<s<<"\n";}
// B234: target resolution and XeSS-SR preset are user-selectable through GTAXeSS.ini.
// B225/B230 proxy/input/lifecycle architecture remains unchanged.
static std::string b232_upper(std::string v){for(char&c:v)if(c>='a'&&c<='z')c=(char)(c-'a'+'A');return v;}
static const char* b232_quality_name(xess_quality_settings_t q){
 switch(q){
  case XESS_QUALITY_SETTING_ULTRA_PERFORMANCE:return "ULTRA_PERFORMANCE";
  case XESS_QUALITY_SETTING_PERFORMANCE:return "PERFORMANCE";
  case XESS_QUALITY_SETTING_BALANCED:return "BALANCED";
  case XESS_QUALITY_SETTING_QUALITY:return "QUALITY";
  case XESS_QUALITY_SETTING_ULTRA_QUALITY:return "ULTRA_QUALITY";
  case XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS:return "ULTRA_QUALITY_PLUS";
  case XESS_QUALITY_SETTING_AA:return "NATIVE_AA";
  default:return "UNKNOWN";
 }
}
static void b232_load_config(std::ofstream&out){
 if(g_b232ConfigLoaded)return;g_b232ConfigLoaded=true;
 std::string lp=log_path();auto slash=lp.find_last_of("\\/");std::string dir=slash==std::string::npos?std::string():lp.substr(0,slash+1);std::string ini=dir+"GTAXeSS.ini";
 char resolution[64]="AUTO",quality[64]="AUTO";GetPrivateProfileStringA("XeSS","OutputResolution","AUTO",resolution,64,ini.c_str());GetPrivateProfileStringA("XeSS","Quality","AUTO",quality,64,ini.c_str());
 g_b232ResolutionMode=b232_upper(resolution);g_b232QualityMode=b232_upper(quality);
 if(g_b232ResolutionMode=="1080P"||g_b232ResolutionMode=="1920X1080"){g_b232OutputWidth=1920;g_b232OutputHeight=1080;}
 else if(g_b232ResolutionMode=="1440P"||g_b232ResolutionMode=="2560X1440"){g_b232OutputWidth=2560;g_b232OutputHeight=1440;}
 else if(g_b232ResolutionMode=="4K"||g_b232ResolutionMode=="2160P"||g_b232ResolutionMode=="3840X2160"){g_b232OutputWidth=3840;g_b232OutputHeight=2160;}
 else {g_b232ResolutionMode="AUTO"; HWND gta=FindWindowA("grcWindow",nullptr);RECT rc{};if(gta&&GetClientRect(gta,&rc)){UINT w=(UINT)(rc.right-rc.left),h=(UINT)(rc.bottom-rc.top);if(w>=3200||h>=1800){g_b232OutputWidth=3840;g_b232OutputHeight=2160;}else if(w>=2300||h>=1300){g_b232OutputWidth=2560;g_b232OutputHeight=1440;}else{g_b232OutputWidth=1920;g_b232OutputHeight=1080;}}}
 const std::string b260RequestedResolution=g_b232ResolutionMode; const UINT b260RequestedWidth=g_b232OutputWidth,b260RequestedHeight=g_b232OutputHeight;
 g_b232ResolutionMode="B260_1080P_AB";g_b232OutputWidth=1920;g_b232OutputHeight=1080;
 out<<"B260: OUTPUT_SATURATION_AB forced_output=1920x1080, requested_mode="<<b260RequestedResolution<<", requested_output="<<b260RequestedWidth<<"x"<<b260RequestedHeight<<", purpose=compare_xefg_present_gpu_queue_backpressure_against_B259_2560x1440\n";out.flush();
 out<<"B234: CONFIG resolution_mode="<<g_b232ResolutionMode<<", output="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<", quality_mode="<<g_b232QualityMode<<", config_path="<<ini<<"\n";out.flush();
}
static bool b232_parse_quality(const std::string&mode,xess_quality_settings_t&outQ){
 if(mode=="ULTRA_PERFORMANCE"||mode=="ULTRAPERFORMANCE"){outQ=XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;return true;}
 if(mode=="PERFORMANCE"){outQ=XESS_QUALITY_SETTING_PERFORMANCE;return true;}
 if(mode=="BALANCED"){outQ=XESS_QUALITY_SETTING_BALANCED;return true;}
 if(mode=="QUALITY"){outQ=XESS_QUALITY_SETTING_QUALITY;return true;}
 if(mode=="ULTRA_QUALITY"||mode=="ULTRAQUALITY"){outQ=XESS_QUALITY_SETTING_ULTRA_QUALITY;return true;}
 if(mode=="ULTRA_QUALITY_PLUS"||mode=="ULTRAQUALITYPLUS"){outQ=XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS;return true;}
 if(mode=="NATIVE_AA"||mode=="NATIVEAA"||mode=="AA"){outQ=XESS_QUALITY_SETTING_AA;return true;}
 return false;
}
static double b232_max_scale(xess_quality_settings_t q){switch(q){case XESS_QUALITY_SETTING_ULTRA_PERFORMANCE:return 3.0;case XESS_QUALITY_SETTING_PERFORMANCE:return 2.3;case XESS_QUALITY_SETTING_BALANCED:return 2.0;case XESS_QUALITY_SETTING_QUALITY:return 1.7;case XESS_QUALITY_SETTING_ULTRA_QUALITY:return 1.5;case XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS:return 1.3;case XESS_QUALITY_SETTING_AA:return 1.0;default:return 0.0;}}
static void b173l_enable_dred(std::ofstream&out)
{
 ComPtr<ID3D12DeviceRemovedExtendedDataSettings>settings;HRESULT hr=D3D12GetDebugInterface(IID_PPV_ARGS(&settings));
 if(SUCCEEDED(hr)&&settings){settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);}
 out<<"B173l: DRED_ENABLE breadcrumbs="<<(SUCCEEDED(hr)&&settings?1:0)<<", page_faults="<<(SUCCEEDED(hr)&&settings?1:0)<<", hr="<<(long)hr<<"\n";out.flush();
}
static void b173l_log_dred(std::ofstream&out,HRESULT removedReason)
{
 ComPtr<ID3D12DeviceRemovedExtendedData>dred;HRESULT queryHr=g_device12?g_device12.As(&dred):E_NOINTERFACE;
 D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs{};HRESULT breadcrumbHr=SUCCEEDED(queryHr)?dred->GetAutoBreadcrumbsOutput(&breadcrumbs):queryHr;
 unsigned breadcrumbNodes=0;
 for(auto*node=breadcrumbs.pHeadAutoBreadcrumbNode;node&&breadcrumbNodes<64;node=node->pNext,++breadcrumbNodes){
  const UINT last=node->pLastBreadcrumbValue?*node->pLastBreadcrumbValue:0;const int lastOp=(node->pCommandHistory&&last&&last<=node->BreadcrumbCount)?(int)node->pCommandHistory[last-1]:-1;
  out<<"B173l: DRED_BREADCRUMB node="<<breadcrumbNodes<<", queue="<<(node->pCommandQueueDebugNameA?node->pCommandQueueDebugNameA:"<unnamed>")<<", list="<<(node->pCommandListDebugNameA?node->pCommandListDebugNameA:"<unnamed>")<<", completed="<<last<<", count="<<node->BreadcrumbCount<<", last_op="<<lastOp<<"\n";
 }
 D3D12_DRED_PAGE_FAULT_OUTPUT pageFault{};HRESULT pageFaultHr=SUCCEEDED(queryHr)?dred->GetPageFaultAllocationOutput(&pageFault):queryHr;
 unsigned existingCount=0,recentFreedCount=0;
 for(auto*node=pageFault.pHeadExistingAllocationNode;node&&existingCount<64;node=node->pNext,++existingCount)out<<"B173l: DRED_EXISTING_ALLOCATION index="<<existingCount<<", type="<<(int)node->AllocationType<<", name="<<(node->ObjectNameA?node->ObjectNameA:"<unnamed>")<<"\n";
 for(auto*node=pageFault.pHeadRecentFreedAllocationNode;node&&recentFreedCount<64;node=node->pNext,++recentFreedCount)out<<"B173l: DRED_RECENT_FREED_ALLOCATION index="<<recentFreedCount<<", type="<<(int)node->AllocationType<<", name="<<(node->ObjectNameA?node->ObjectNameA:"<unnamed>")<<"\n";
 out<<"B173l: DRED_SUMMARY removed_reason="<<(long)removedReason<<", query_hr="<<(long)queryHr<<", breadcrumb_hr="<<(long)breadcrumbHr<<", breadcrumb_nodes="<<breadcrumbNodes<<", page_fault_hr="<<(long)pageFaultHr<<", page_fault_va="<<(unsigned long long)pageFault.PageFaultVA<<", existing_allocations="<<existingCount<<", recent_freed_allocations="<<recentFreedCount<<"\n";out.flush();
}

static void release_all()
{
    g_armed=false;
    g_openedMutex.Reset(); g_openedRtv.Reset(); g_openedTexture.Reset();
    g_on12.Reset(); g_on12Context.Reset(); g_on12Device.Reset();
    g_b173fSharedFence12.Reset(); g_b173fNativeFence.Reset(); g_b173fNativeContext4.Reset(); g_b173fNativeDevice5.Reset(); g_b173fFenceValue=0;
    g_queue12.Reset(); g_device12.Reset();
    g_nativeMutex.Reset(); g_nativeRtv.Reset(); g_nativeTexture.Reset();
    if(g_sharedHandle){CloseHandle(g_sharedHandle);g_sharedHandle=nullptr;}
}

static HRESULT prepare(ID3D11Device* nativeDevice,std::ofstream& out)
{
    if(!nativeDevice||!g_nativeContext)return E_INVALIDARG;
    auto checkpoint=[&out](const char* stage,HRESULT value){out<<"B142: stage="<<stage<<", hr="<<(long)value<<"\n";out.flush();};
    ComPtr<IDXGIDevice> dxgiDevice;ComPtr<IDXGIAdapter> adapter;
    HRESULT hr=nativeDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));checkpoint("GetNativeDxgiDevice",hr);
    if(FAILED(hr))return hr;
    hr=dxgiDevice->GetAdapter(&adapter);checkpoint("GetNativeAdapter",hr);
    if(FAILED(hr))return hr;
    DXGI_ADAPTER_DESC adapterDesc{};hr=adapter->GetDesc(&adapterDesc);checkpoint("GetNativeAdapterDesc",hr);
    if(FAILED(hr))return hr;
    b173l_enable_dred(out);
    hr=D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&g_device12));
    out<<"B142: stage=D3D12CreateDevice, same_adapter=1, vendor="<<adapterDesc.VendorId<<", device="<<adapterDesc.DeviceId<<", hr="<<(long)hr<<"\n";out.flush();
    if(FAILED(hr)||!g_device12)return FAILED(hr)?hr:E_FAIL;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr=g_device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&g_queue12));checkpoint("CreateCommandQueue",hr);
    if(FAILED(hr)||!g_queue12)return FAILED(hr)?hr:E_FAIL;
    IUnknown* queues[]{g_queue12.Get()};D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};
    hr=D3D11On12CreateDevice(g_device12.Get(),D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,queues,1,0,&g_on12Device,&g_on12Context,nullptr);
    out<<"B142: stage=D3D11On12CreateDevice, device="<<(g_on12Device?1:0)<<", context="<<(g_on12Context?1:0)<<", hr="<<(long)hr<<"\n";out.flush();
    if(FAILED(hr)||!g_on12Device||!g_on12Context)return FAILED(hr)?hr:E_FAIL;
    hr=g_on12Device.As(&g_on12);checkpoint("GetID3D11On12Device",hr);
    if(FAILED(hr)||!g_on12)return FAILED(hr)?hr:E_FAIL;
    out<<"B175: BRIDGE_CORE_READY validation_clear=0, validation_readback=0, validation_hash=0, gta_present_blocking_diagnostics=0\n";out.flush();
    return S_OK;
}

static void STDMETHODCALLTYPE hooked_draw(ID3D11DeviceContext*c,UINT a,UINT b){if(g_active)g_active->fetch_add(1);g_originalDraw(c,a,b);if(g_active)g_active->fetch_sub(1);}

extern "C" __declspec(dllexport) const char* WINAPI B76GetBuildIdentity(){return "B250 ALT-TAB AUTO-RESUME + RECOVERY SEED FIX + B249/B230/B225 FROZEN 2026-09-23";}
static std::atomic<ULONGLONG> g_b220LastHitTestLog{0};
static std::atomic<ULONGLONG> g_b220LastProxySnapshot{0};
extern "C" __declspec(dllexport) HRESULT WINAPI B50AttachWriteTrace(ID3D11DeviceContext*c,std::atomic<unsigned>*s,std::atomic<unsigned>*a)
{
    if(!c||!s||!a)return E_INVALIDARG;auto***o=reinterpret_cast<void***>(c);if(!o||!*o)return E_FAIL;
    g_originalDraw=reinterpret_cast<DrawFn>((*o)[13]);g_active=a;g_nativeContext=c;(*o)[13]=reinterpret_cast<void*>(&hooked_draw);
    HRESULT traceHr=B75AttachWriteTrace(c,s,a);log_line("B172: attach active; reconstructed-motion bridge and XeFG tag tracing, hr="+std::to_string((long)traceHr)+". No GTA resource or buffer is modified.");return traceHr;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B76LogSelectedDraws(ID3D11DeviceContext* c,unsigned f,unsigned s,const char*path)
{
    if(!path||!c)return E_INVALIDARG;g_nativeContext=c;std::ofstream out(path,std::ios::app);if(!out)return E_FAIL;
    out<<"B142: helper_identity="<<B76GetBuildIdentity()<<", base_first="<<f<<", base_second="<<s<<"\n";
    ComPtr<ID3D11Device>d;g_nativeContext->GetDevice(&d);release_all();HRESULT hr=prepare(d.Get(),out);
    out<<"B142: prepare_hr="<<(long)hr<<", armed="<<(g_armed?1:0)<<"\n";
    out<<"B142: interpretation_guard=Validates one native-D3D11 clear and D3D11On12 readback of one private 64x64 legacy-shared texture. No GTA texture is shared and this is not frame generation. No GTA texture is shared and this is not frame generation.\n";return hr;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B87PresentOverlay(ID3D11DeviceContext*,IDXGISwapChain*)
{
    if(!g_armed)return S_FALSE;
    if(GetTickCount64()>=g_until){if(!g_done){log_line("B142: COMPLETE cycles="+std::to_string(g_cycles)+", native_failures="+std::to_string(g_nativeFailures)+", on12_failures="+std::to_string(g_on12Failures)+", gta_frame_access=0, gta_display_change=0");g_done=true;}release_all();return S_FALSE;}
    HRESULT h0=g_nativeMutex->AcquireSync(0,0);if(FAILED(h0)){++g_nativeFailures;return S_FALSE;}
    const float a[4]{0.75f,0.0f,0.25f,1.0f};g_nativeContext->ClearRenderTargetView(g_nativeRtv.Get(),a);g_nativeContext->Flush();
    HRESULT h1=g_nativeMutex->ReleaseSync(1);if(FAILED(h1)){++g_nativeFailures;return h1;}
    HRESULT h2=g_openedMutex->AcquireSync(1,0);if(FAILED(h2)){++g_on12Failures;return S_FALSE;}
    const float b[4]{0.0f,0.75f,0.25f,1.0f};g_on12Context->ClearRenderTargetView(g_openedRtv.Get(),b);g_on12Context->Flush();
    HRESULT h3=g_openedMutex->ReleaseSync(0);if(FAILED(h3)){++g_on12Failures;return h3;}
    ++g_cycles;if(g_cycles==1||g_cycles==120)log_line("B142: shared_cycle milestone="+std::to_string(g_cycles)+", native_acquire_clear_release=1, on12_acquire_clear_release=1");return S_OK;
}
static xefg_swapchain_handle_t g_xefgContext{};
static xell_context_handle_t g_xellContext{};
static ComPtr<IDXGISwapChain3> g_xefgSwapChain;
static ComPtr<IDXGISwapChainMedia> g_b314SwapChainMedia;
static ComPtr<IDXGISwapChain2> g_b312SwapChain2;
static std::atomic<UINT64> g_b312LiveStartTick{0};
static std::atomic<bool> g_b312LatencyApplied{false};
static UINT g_b312InitialMaxFrameLatency=0;
static HANDLE g_b312FrameLatencyWaitableObject=nullptr;
static LARGE_INTEGER g_b313QpcFrequency{};
static std::atomic<unsigned long long> g_b313DwmSamples{0};
// B316: persist the monitor-cover host geometry proven by B315. The private XeFG
// swapchain, x4, queue, XeLL, camera, motion/depth, and presentation flags remain unchanged.
static std::atomic<bool> g_b316PersistentMonitorCover{true};
static HWND g_xefgWindow{};static std::atomic<bool> g_b171WindowVisible{false};
static HANDLE g_b161WindowReady{},g_b161WindowThread{};
static ComPtr<ID3D12Resource> g_b147ColorResource;
static ComPtr<ID3D12Resource> g_b171SecondColorResource;
static ComPtr<ID3D11Texture2D> g_b175DiscoverySecondShared;
static D3D11_TEXTURE2D_DESC g_b175DiscoveryColorDesc{};
static std::atomic<bool> g_b175DiscoveryTargetReady{false},g_b175DiscoverySecondCaptured{false};
struct B173iColorSlot
{
 ComPtr<ID3D11Texture2D> nativeSourceCopy;
 ComPtr<ID3D11Texture2D> xessInput;
 ComPtr<ID3D11RenderTargetView> xessInputRtv;
 ComPtr<ID3D11Texture2D> xessOutput;
 ComPtr<ID3D11ShaderResourceView> xessOutputSrv;
 ComPtr<ID3D11RenderTargetView> sharedRtv;
 ComPtr<ID3D11DeviceContext> conversionContext;
 ComPtr<ID3D11CommandList> conversionCommands;
 ComPtr<ID3D11Texture2D> shared;
 ComPtr<ID3D11Texture2D> opened;
 ComPtr<ID3D12Resource> output;
 ComPtr<ID3D11Resource> wrapped;
 D3D11_TEXTURE2D_DESC sourceDesc{};
 bool ready{};
};
static constexpr unsigned kB173iColorSlotCount=12;
static B173iColorSlot g_b173iColorSlots[kB173iColorSlotCount];
static std::atomic<bool> g_b173iColorPoolReady{false};
static unsigned g_b173iNextColorSlot{};
struct B173jDepthMotionSlot
{
 ComPtr<ID3D11Texture2D> privateDepth;
 ComPtr<ID3D11ShaderResourceView> nativeDepthSrv;
 ComPtr<ID3D11Texture2D> sharedDepth;
 ComPtr<ID3D11RenderTargetView> nativeDepthRtv;
 ComPtr<ID3D11DeviceContext> deferredContext;
 ComPtr<ID3D11Texture2D> nativeMotion;
 ComPtr<ID3D11Texture2D> sharedNativeMotion;
 ComPtr<ID3D11Texture2D> openedNativeMotion;
 ComPtr<ID3D11ShaderResourceView> openedNativeMotionSrv;
 ComPtr<ID3D11UnorderedAccessView> nativeMotionUav;
 ComPtr<ID3D11Buffer> nativeMotionConstants;
 ComPtr<ID3D11Texture2D> openedDepth;
 ComPtr<ID3D11ShaderResourceView> openedDepthSrv;
 ComPtr<ID3D12Resource> motionOutput;
 ComPtr<ID3D12Resource> depthOutput;
 ComPtr<ID3D11Resource> wrappedMotion;
 ComPtr<ID3D11Resource> wrappedDepth;
 ComPtr<ID3D11UnorderedAccessView> motionUav;
 ComPtr<ID3D11UnorderedAccessView> depthUav;
 ComPtr<ID3D12CommandAllocator> prefillAllocator;
 ComPtr<ID3D12GraphicsCommandList> prefillList;
 UINT width{};
 UINT height{};
 bool ready{};
 bool outputsReady{};
};
static constexpr unsigned kB173jDepthMotionSlotCount=12;
static B173jDepthMotionSlot g_b173jDepthMotionSlots[kB173jDepthMotionSlotCount];
static std::atomic<bool> g_b173jDepthMotionPoolReady{false};
static std::atomic<unsigned> g_b174ColorSlotState[kB173iColorSlotCount];
static std::atomic<unsigned> g_b174DepthSlotState[kB173jDepthMotionSlotCount];
static unsigned g_b174CaptureSlot=~0u;
static std::atomic<bool> g_b171SecondColorReady{false},g_b171SecondColorAttempted{false};
static unsigned long long g_b171FirstColorHash{},g_b171SecondColorHash{};
static std::atomic<bool> g_b171LiveReady{false},g_b171LiveBusy{false};static std::atomic<unsigned> g_b171LiveSubmitted{0};static UINT g_b171LiveWidth{},g_b171LiveHeight{};static xefg_swapchain_frame_constant_data_t g_b171LiveConstants{};
static std::atomic<bool> g_b175PipelineReady{false},g_b175PipelineFailed{false},g_b175ActivationRequested{false};static constexpr bool g_b175pHiddenOnly=false;static constexpr ULONGLONG g_b175pBoundedDurationMs=8000;static constexpr ULONGLONG g_b175rBoundedVisibleDurationMs=5000;static ULONGLONG g_b175rRevealTick{};
static std::atomic<unsigned> g_b175InvariantFailures{0};
static std::atomic<unsigned> g_b264DropResourceOwned{0},g_b264DropQueueFull{0},g_b264DropSurfaceSuspended{0},g_b264DropFastSnapshot{0},g_b264DropCaptureFailed{0},g_b264DropSlotRace{0},g_b264DropFastOther{0},g_b264DropNonMonotonic{0},g_b264DropWorkerSuspend{0},g_b264DropToggleDrain{0},g_b264DropOther{0};
// B266 diagnostic-only correlation state: no pacing, queue, or Present behavior changes.
static std::atomic<unsigned long long> g_b266LastXellSleepUs{0},g_b266LastPrivatePresentUs{0},g_b266LastWorkerTotalUs{0};
static std::atomic<unsigned> g_b266LastWorkerPending{0},g_b266QueueFullBursts{0},g_b266QueueFullDrops{0};
static std::atomic<bool> g_b266QueueFullBurstActive{false};
static std::atomic<unsigned> g_b264AcceptedDisposition{0};
static constexpr unsigned kB175DecisionCapacity=4096;
static SRWLOCK g_b175DecisionLock=SRWLOCK_INIT;static unsigned g_b175DecisionIds[kB175DecisionCapacity]{};static unsigned g_b175DecisionCount{},g_b175DecisionCursor{};
static float g_b175ConsumedViewProjection[16]{};static bool g_b175HaveConsumedViewProjection=false;
static float g_b175bXessPreviousViewProjection[16]{};static bool g_b175bHaveXessPreviousViewProjection=false;
// B474: last distinct mathematically-valid camera VP observed on the capture path. Additive temporal sampling only; protected B458/B453 authority remains byte-identical.
static float g_b474LastDistinctValidatedVp[16]{};static bool g_b474HaveLastDistinctValidatedVp=false;static SRWLOCK g_b474TemporalLock=SRWLOCK_INIT;
static SRWLOCK g_b175MotionHistoryLock=SRWLOCK_INIT;
static std::atomic<bool> g_b288CameraResetPending{false};
static std::atomic<bool> g_b174TestActive{false},g_b174VisibilityLost{false},g_b174ProxyRevealed{false},g_b174SessionEverRevealed{false},g_b174StartupBackbuffersSeeded{false},g_b174SurfaceSuspended{false},g_b174ResumeNeedsFresh{false},g_b174ShutdownRequested{false};
static std::atomic<unsigned> g_b174StopReason{0},g_b174Accepted{0},g_b174PendingCount{0},g_b174Dropped{0},g_b174Replaced{0},g_b174OutputFrames{0},g_b174GeneratedFrames{0},g_b174HiddenCompleted{0};
static std::atomic<bool> g_b352InitialValidX4Seen{false};
static std::atomic<UINT64> g_b174RevealFenceMarker{0};
static std::atomic<unsigned> g_b174oObservedSourcePresents{0},g_b174oFirstSourcePresent{0},g_b174oLastSourcePresent{0};
static std::atomic<unsigned> g_b174RateObservedBase{0},g_b174RateCompletedBase{0},g_b174RateOutputBase{0},g_b174RateWindowEpoch{0};
static std::atomic<ULONGLONG> g_b174LastCaptureStart{0};
static std::atomic<ULONGLONG> g_b270LastSuccessfulOutputTick{0};
// B273: automatic recovery for the proven long-run XeLL ~60 Hz pacing lock.
static std::atomic<unsigned> g_b273PacingLockFrames{0},g_b273PacingResyncCount{0};
static std::atomic<ULONGLONG> g_b273LastPacingResyncTick{0};
// B276: request the already-proven B216/B252 live-input rediscovery when the healthy x4 worker becomes locked near 60 accepted frames/s.
static std::atomic<unsigned> g_b276PacingLockFrames{0};
static std::atomic<bool> g_b276RediscoveryRequested{false};
// B388: dedicated confirmed-stall recovery request; consumed by the ASI on the next GTA Present.
static std::atomic<bool> g_b388StallRecoveryRequested{false};
// B405: sustained-stall latch prevents normal GTA cadence gaps from disturbing a healthy FG session.
static std::atomic<bool> g_b403ConfirmedStallLatched{false};
static std::atomic<ULONGLONG> g_b276LastRediscoveryRequestTick{0};
static std::atomic<unsigned long long> g_b174TotalAccepted{0},g_b174TotalGenerated{0},g_b174TotalDisplayed{0},g_b174TotalDropped{0},g_b174TotalReplaced{0},g_b174PresentErrors{0},g_b174TagErrors{0},g_b174StatusErrors{0},g_b174ToggleOnCount{0},g_b174ToggleOffCount{0};
static std::atomic<uint32_t> g_b174NextPresentId{4};
static ULONGLONG g_b174ActivationStart{},g_b174TestStart{};static std::atomic<ULONGLONG> g_b174StartupDeadline{0},g_b174RateWindowStart{0},g_b174LastTelemetry{0};
static LARGE_INTEGER g_b174PreviousPrivatePresentStart{};
static HWND g_b174GtaWindow{};
static RECT g_b174GtaClientRect{};
static SRWLOCK g_b174WindowLock=SRWLOCK_INIT;
static SRWLOCK g_b174WindowPollLock=SRWLOCK_INIT;
static std::atomic<ULONGLONG> g_b174LastWindowPoll{0};
static std::atomic<unsigned> g_b174WindowGeneration{0};
struct B174RateWindowSnapshot{ULONGLONG start{};unsigned observedBase{},completedBase{},outputBase{};};
static void b207_reset_realtime_fps();
static void b174_rebaseline_rate_window(ULONGLONG now)
{
 g_b174RateWindowEpoch.fetch_add(1,std::memory_order_acq_rel);
 g_b174RateObservedBase.store(g_b174oObservedSourcePresents.load(std::memory_order_acquire),std::memory_order_relaxed);
 g_b174RateCompletedBase.store(g_b171LiveSubmitted.load(std::memory_order_acquire),std::memory_order_relaxed);
 g_b174RateOutputBase.store(g_b174OutputFrames.load(std::memory_order_acquire),std::memory_order_relaxed);
 g_b174RateWindowStart.store(now,std::memory_order_relaxed);g_b174LastTelemetry.store(now,std::memory_order_relaxed);
 g_b174RateWindowEpoch.fetch_add(1,std::memory_order_release);
 b207_reset_realtime_fps();
}
static B174RateWindowSnapshot b174_rate_window_snapshot()
{
 B174RateWindowSnapshot snapshot{};unsigned before{},after{};
 do{before=g_b174RateWindowEpoch.load(std::memory_order_acquire);if(before&1u){YieldProcessor();continue;}snapshot.start=g_b174RateWindowStart.load(std::memory_order_relaxed);snapshot.observedBase=g_b174RateObservedBase.load(std::memory_order_relaxed);snapshot.completedBase=g_b174RateCompletedBase.load(std::memory_order_relaxed);snapshot.outputBase=g_b174RateOutputBase.load(std::memory_order_relaxed);after=g_b174RateWindowEpoch.load(std::memory_order_acquire);}while(before!=after);
 return snapshot;
}
static ComPtr<ID3D12Resource> g_b160MotionResource;
static ComPtr<ID3D12Resource> g_b163DepthResource;
struct B173hLiveJob
{
 ComPtr<ID3D12Resource> color;
 ComPtr<ID3D12Resource> motion;
 ComPtr<ID3D12Resource> depth;
 ComPtr<ID3D11Resource> wrappedMotion;
 ComPtr<ID3D11Resource> wrappedDepth;
 ComPtr<ID3D11UnorderedAccessView> motionUav;
 ComPtr<ID3D11UnorderedAccessView> depthUav;
 float previousViewProjection[16]{};
 float currentViewProjection[16]{};
 float currentInverseViewProjection[16]{};
 float currentView[16]{};
 float currentProjection[16]{};
 bool b434ValidatedBaselineAlignment{};
 bool b437MotionCameraValid{true};
 bool b437MotionValidityTransition{};
 xefg_swapchain_frame_constant_data_t constants{};
 LARGE_INTEGER captureTime{};
 LARGE_INTEGER queuedTime{};
 UINT64 nativeFenceValue{};
 bool b330NativeMotionActive{};
 UINT64 on12Marker{};
 UINT width{};
 UINT height{};
 unsigned slotIndex{~0u};
 unsigned ordinal{};
 unsigned sourceFrame{};
 unsigned b486DepthCameraSourceFrame{};
 unsigned b486ColorSourceFrame{};
 unsigned long long b486CameraVpFingerprint{};
 long long b486DepthCameraCaptureQpc{};
 long long b486ColorCaptureQpc{};
 unsigned long long b489PublishedSerial{};
 unsigned b489PublishedSourceFrame{};
 unsigned long long b489PublishedFp{};
 unsigned long long b489ConsumedSerial{};
 unsigned b489ConsumedSourceFrame{};
 unsigned long long b489ConsumedFp{};
 unsigned windowGeneration{};
 uint32_t presentId{};
 double nativeDepthMs{};
 double depthPackSubmitMs{};
 double motionReconstructSubmitMs{};
 double gtaThreadMs{};
 double sourceFrameRenderTimeMs{16.6667};
};
static SRWLOCK g_b173hJobLock=SRWLOCK_INIT;
static constexpr unsigned kB174oPendingCapacity=11; // B328: queue-admission A/B. One serial worker + up to 11 pending jobs fits the 12-slot resource ring; no XeFG cadence/interpolation change.
static B173hLiveJob g_b174oPendingJobs[kB174oPendingCapacity];
static unsigned g_b174oPendingHead{},g_b174oPendingTail{},g_b174oPendingSize{};
static B173hLiveJob g_b174CaptureJob;
static std::vector<B173hLiveJob> g_b174FailedJobs;
static std::atomic<bool> g_b173hJobPending{false},g_b173hJobProcessing{false};
// B317: diagnostic-only source-frame pipeline timing. No pacing or queue behavior changes.
static std::atomic<long long> g_b317LastSourcePipelineEntryQpc{0};
static std::atomic<unsigned long long> g_b317SourceProfileOrdinal{0};
// B392: zero-file-I/O hot-path flight recorder. GTA and XeFG worker publish only atomics;
// a separate watchdog thread performs any diagnostic file write after a >=20 ms held stage.
struct B392StageState{std::atomic<unsigned> stage{0},frame{0},present{0};std::atomic<long long> qpc{0};std::atomic<unsigned long long> seq{0};};
static B392StageState g_b392GtaStage,g_b392WorkerStage;
static std::atomic<long long> g_b393LastSourceEntryQpc{0};
static std::atomic<unsigned> g_b393LastSourceFrame{0};
static std::atomic<unsigned long long> g_b393SourceEntrySeq{0};
static std::atomic<bool> g_b392WatchdogStarted{false},g_b392WatchdogStop{false};
static char g_b392LogPath[MAX_PATH*2]{};
static const char* b392_stage_name(unsigned s){switch(s){case 1:return "entry";case 2:return "backbuffer_get";case 3:return "color_xess_capture";case 4:return "native_fence_signal_flush_queue_wait";case 5:return "job_queue_lock";case 11:return "xell_sleep_markers";case 12:return "worker_input_prepare";case 13:return "gpu_queue_wait_submit";case 14:return "xefg_tag_resources";case 15:return "private_backbuffer_prefill";case 16:return "xefg_private_present";case 17:return "xefg_status_and_retire";default:return "idle";}}
static void b392_set_stage(B392StageState&st,unsigned stage,unsigned frame,unsigned present){LARGE_INTEGER q{};QueryPerformanceCounter(&q);st.frame.store(frame,std::memory_order_relaxed);st.present.store(present,std::memory_order_relaxed);st.qpc.store(q.QuadPart,std::memory_order_release);st.stage.store(stage,std::memory_order_release);st.seq.fetch_add(1,std::memory_order_acq_rel);}
static void b392_clear_stage(B392StageState&st){st.stage.store(0,std::memory_order_release);st.seq.fetch_add(1,std::memory_order_acq_rel);}
static DWORD WINAPI b392_watchdog(void*){LARGE_INTEGER f{};QueryPerformanceFrequency(&f);unsigned long long lastG=0,lastW=0;while(!g_b392WatchdogStop.load(std::memory_order_acquire)){Sleep(2);LARGE_INTEGER now{};QueryPerformanceCounter(&now);auto check=[&](const char*thread,B392StageState&st,unsigned long long&last){const unsigned stage=st.stage.load(std::memory_order_acquire);if(!stage)return;const long long start=st.qpc.load(std::memory_order_acquire);const unsigned long long seq=st.seq.load(std::memory_order_acquire);if(!start||seq==last||f.QuadPart<=0)return;const double ms=(double)(now.QuadPart-start)*1000.0/(double)f.QuadPart;if(ms<20.0)return;last=seq;std::ofstream o(g_b392LogPath,std::ios::app);if(o){o<<"B392: STALL_FLIGHT_RECORDER thread="<<thread<<", stage="<<b392_stage_name(stage)<<", held_ms="<<ms<<", source_frame_id="<<st.frame.load(std::memory_order_relaxed)<<", present_id="<<st.present.load(std::memory_order_relaxed)<<", pending="<<g_b174PendingCount.load(std::memory_order_relaxed)<<", gta_stage="<<b392_stage_name(g_b392GtaStage.stage.load(std::memory_order_relaxed))<<", worker_stage="<<b392_stage_name(g_b392WorkerStage.stage.load(std::memory_order_relaxed))<<", hot_path_file_io=0, threshold_ms=20\n";o.flush();}};check("gta",g_b392GtaStage,lastG);check("worker",g_b392WorkerStage,lastW);static unsigned long long lastGapSeq=0;const long long lastEntry=g_b393LastSourceEntryQpc.load(std::memory_order_acquire);const unsigned long long entrySeq=g_b393SourceEntrySeq.load(std::memory_order_acquire);if(lastEntry>0&&entrySeq!=lastGapSeq&&g_b392GtaStage.stage.load(std::memory_order_acquire)==0&&f.QuadPart>0){const double gapMs=(double)(now.QuadPart-lastEntry)*1000.0/(double)f.QuadPart;if(gapMs>=20.0){lastGapSeq=entrySeq;std::ofstream o(g_b392LogPath,std::ios::app);if(o){o<<"B393: SOURCE_ENTRY_GAP thread=gta, stage=outside_B171PresentLiveFrame, gap_ms="<<gapMs<<", last_source_frame_id="<<g_b393LastSourceFrame.load(std::memory_order_relaxed)<<", worker_stage="<<b392_stage_name(g_b392WorkerStage.stage.load(std::memory_order_relaxed))<<", pending="<<g_b174PendingCount.load(std::memory_order_relaxed)<<", classifier=render_or_hook_path_before_helper_entry, threshold_ms=20\n";o.flush();}}}}return 0;}
static void b392_start_watchdog(const char*path){if(path&&*path&&!g_b392WatchdogStarted.exchange(true,std::memory_order_acq_rel)){strncpy_s(g_b392LogPath,path,_TRUNCATE);HANDLE h=CreateThread(nullptr,0,b392_watchdog,nullptr,0,nullptr);if(h)CloseHandle(h);else g_b392WatchdogStarted.store(false,std::memory_order_release);}}
static ComPtr<ID3D11ComputeShader> g_b173MotionShader;
static ComPtr<ID3D11Buffer> g_b173MotionConstants;
static ComPtr<ID3D11VertexShader> g_b173eNativeDepthVs;
static ComPtr<ID3D11PixelShader> g_b173eNativeDepthPs;
static ComPtr<ID3D11RasterizerState> g_b173eNativeDepthRs;
static std::atomic<bool> g_xefgWorkerStarted{false},g_xefgReady{false},g_xefgInitFinished{false},g_b166FrameAttempted{false};
static std::string g_b172dXeFGLogPath;
// B302: lightweight session runtime timer shared by the FPS HUD and a separate runtime log.
static std::atomic<unsigned long long> g_b302LastRuntimeLoggedMinute{0};
static std::string b302_runtime_log_path()
{
 std::string p=!g_b172dXeFGLogPath.empty()?g_b172dXeFGLogPath:log_path();
 auto slash=p.find_last_of("\\/");if(slash!=std::string::npos)p.resize(slash+1);else p.clear();
 return p+"GTAXeSS-Runtime.log";
}
static void b302_format_runtime(ULONGLONG elapsedMs,char*buffer,size_t bufferSize)
{
 const unsigned long long total=elapsedMs/1000ULL,hours=total/3600ULL,minutes=(total/60ULL)%60ULL,seconds=total%60ULL;
 sprintf_s(buffer,bufferSize,"%02llu:%02llu:%02llu",hours,minutes,seconds);
}
static void b302_runtime_log_event(const char*eventName,ULONGLONG elapsedMs)
{
 SYSTEMTIME st{};GetLocalTime(&st);char runtime[32]{};b302_format_runtime(elapsedMs,runtime,sizeof(runtime));
 std::ofstream o(b302_runtime_log_path(),std::ios::app);if(!o)return;
 o<<"[";o.fill('0');o.width(2);o<<st.wHour<<":";o.width(2);o<<st.wMinute<<":";o.width(2);o<<st.wSecond<<"] "<<eventName<<" - Runtime "<<runtime<<"\n";o.flush();
}
// B304/B305: once-per-second GTA process CPU/GPU/VRAM sampling plus B305 render-thread CPU telemetry.
static std::atomic<unsigned> g_b304Cpu100{0},g_b304Gpu100{0},g_b305RenderCpu100{0};
static std::atomic<unsigned long long> g_b304VramUsedBytes{0},g_b304VramTotalBytes{0};
static ULONGLONG g_b304CpuLastTick{};static ULONGLONG g_b304CpuLastKernel100ns{},g_b304CpuLastUser100ns{};
static DWORD g_b305RenderTid{};static ULONGLONG g_b305RenderLastTick{},g_b305RenderLastKernel100ns{},g_b305RenderLastUser100ns{};
static PDH_HQUERY g_b304GpuQuery{};static PDH_HCOUNTER g_b304GpuCounter{};static bool g_b304GpuQueryPrimed=false;
static std::string b304_performance_log_path()
{
 std::string p=!g_b172dXeFGLogPath.empty()?g_b172dXeFGLogPath:log_path();auto slash=p.find_last_of("\\/");if(slash!=std::string::npos)p.resize(slash+1);else p.clear();return p+"GTAXeSS-Performance.log";
}
static ULONGLONG b304_filetime_u64(const FILETIME&ft){ULARGE_INTEGER u{};u.LowPart=ft.dwLowDateTime;u.HighPart=ft.dwHighDateTime;return u.QuadPart;}
static void b304_sample_performance()
{
 const ULONGLONG now=GetTickCount64();FILETIME ct{},et{},kt{},ut{};
 if(GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut)){const ULONGLONG k=b304_filetime_u64(kt),u=b304_filetime_u64(ut);if(g_b304CpuLastTick&&now>g_b304CpuLastTick){SYSTEM_INFO si{};GetSystemInfo(&si);const unsigned logical=(std::max)(1u,(unsigned)si.dwNumberOfProcessors);const double wall100ns=(double)(now-g_b304CpuLastTick)*10000.0;const double used100ns=(double)((k-g_b304CpuLastKernel100ns)+(u-g_b304CpuLastUser100ns));double pct=wall100ns>0?100.0*used100ns/(wall100ns*logical):0.0;if(pct<0)pct=0;if(pct>100)pct=100;g_b304Cpu100.store((unsigned)(pct*100.0+0.5),std::memory_order_release);}g_b304CpuLastTick=now;g_b304CpuLastKernel100ns=k;g_b304CpuLastUser100ns=u;}
 if(!g_b304GpuQuery){if(PdhOpenQueryW(nullptr,0,&g_b304GpuQuery)==ERROR_SUCCESS&&PdhAddEnglishCounterW(g_b304GpuQuery,L"\\GPU Engine(*)\\Utilization Percentage",0,&g_b304GpuCounter)==ERROR_SUCCESS){PdhCollectQueryData(g_b304GpuQuery);g_b304GpuQueryPrimed=true;}else{if(g_b304GpuQuery){PdhCloseQuery(g_b304GpuQuery);g_b304GpuQuery=nullptr;}g_b304GpuCounter=nullptr;}}
 if(g_b304GpuQuery&&g_b304GpuCounter&&g_b304GpuQueryPrimed&&PdhCollectQueryData(g_b304GpuQuery)==ERROR_SUCCESS){DWORD bytes=0,count=0;PdhGetFormattedCounterArrayW(g_b304GpuCounter,PDH_FMT_DOUBLE,&bytes,&count,nullptr);if(bytes&&count){std::vector<unsigned char>buf(bytes);auto*items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());if(PdhGetFormattedCounterArrayW(g_b304GpuCounter,PDH_FMT_DOUBLE,&bytes,&count,items)==ERROR_SUCCESS){wchar_t token[64]{};swprintf_s(token,L"pid_%lu_",GetCurrentProcessId());double gpu=0.0;for(DWORD i=0;i<count;++i){if(items[i].szName&&wcsstr(items[i].szName,token)&&items[i].FmtValue.CStatus==ERROR_SUCCESS)gpu=(std::max)(gpu,items[i].FmtValue.doubleValue);}if(gpu<0)gpu=0;if(gpu>100)gpu=100;g_b304Gpu100.store((unsigned)(gpu*100.0+0.5),std::memory_order_release);}}}
 // B305: the GTA window thread is the same thread used by the real Present path in this integration.
 const DWORD renderTid=g_b174GtaWindow?GetWindowThreadProcessId(g_b174GtaWindow,nullptr):0;
 if(renderTid){HANDLE th=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,renderTid);if(th){FILETIME tct{},tet{},tkt{},tut{};if(GetThreadTimes(th,&tct,&tet,&tkt,&tut)){const ULONGLONG k=b304_filetime_u64(tkt),u=b304_filetime_u64(tut);if(g_b305RenderTid==renderTid&&g_b305RenderLastTick&&now>g_b305RenderLastTick){const double wall100ns=(double)(now-g_b305RenderLastTick)*10000.0;const double used100ns=(double)((k-g_b305RenderLastKernel100ns)+(u-g_b305RenderLastUser100ns));double pct=wall100ns>0?100.0*used100ns/wall100ns:0.0;if(pct<0)pct=0;if(pct>100)pct=100;g_b305RenderCpu100.store((unsigned)(pct*100.0+0.5),std::memory_order_release);}g_b305RenderTid=renderTid;g_b305RenderLastTick=now;g_b305RenderLastKernel100ns=k;g_b305RenderLastUser100ns=u;}CloseHandle(th);}}
 // B305: g_b175qBridgeAdapter is intentionally released after bridge creation, so reacquire the same adapter by LUID for VRAM telemetry.
 ComPtr<IDXGIAdapter3>a3;if(g_b175qBridgeAdapter)g_b175qBridgeAdapter.As(&a3);if(!a3&&(g_b175qBridgeAdapterDesc.AdapterLuid.HighPart||g_b175qBridgeAdapterDesc.AdapterLuid.LowPart)){ComPtr<IDXGIFactory4>f4;if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&f4)))&&f4)f4->EnumAdapterByLuid(g_b175qBridgeAdapterDesc.AdapterLuid,IID_PPV_ARGS(&a3));}if(a3){DXGI_QUERY_VIDEO_MEMORY_INFO info{};if(SUCCEEDED(a3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info))){g_b304VramUsedBytes.store(info.CurrentUsage,std::memory_order_release);g_b304VramTotalBytes.store((unsigned long long)g_b175qBridgeAdapterDesc.DedicatedVideoMemory,std::memory_order_release);}}
}
static void b304_log_performance(double sourceFps,double fgFps,double instantSource,double instantFg,ULONGLONG runtimeMs)
{
 SYSTEMTIME st{};GetLocalTime(&st);char runtime[32]{};b302_format_runtime(runtimeMs,runtime,sizeof(runtime));const double cpu=g_b304Cpu100.load()/100.0,renderCpu=g_b305RenderCpu100.load()/100.0,gpu=g_b304Gpu100.load()/100.0,used=g_b304VramUsedBytes.load()/(1024.0*1024.0*1024.0),total=g_b304VramTotalBytes.load()/(1024.0*1024.0*1024.0);std::ofstream o(b304_performance_log_path(),std::ios::app);if(!o)return;o<<"[";o.fill('0');o.width(2);o<<st.wHour<<":";o.width(2);o<<st.wMinute<<":";o.width(2);o<<st.wSecond<<"] Runtime="<<runtime<<" SourceFPS="<<sourceFps<<" XeFGFPS="<<fgFps<<" InstantSourceFPS="<<instantSource<<" InstantXeFGFPS="<<instantFg<<" CPU="<<cpu<<"% RenderThreadCPU="<<renderCpu<<"% GPU="<<gpu<<"% VRAM="<<used<<"GB/"<<total<<"GB\n";o.flush();
}

static void b172d_xefg_log_callback(const char* message,xefg_swapchain_logging_level_t level,void*)
{
    if(g_b172dXeFGLogPath.empty())return;
    std::ofstream out(g_b172dXeFGLogPath,std::ios::app);
    out<<"B172d: XEFG_INTERNAL level="<<(int)level<<", message="<<(message?message:"<null>")<<"\n";
}
struct B160InitSignal{~B160InitSignal(){g_xefgInitFinished.store(true,std::memory_order_release);}};
struct B145Work { ComPtr<ID3D12Device> device; ComPtr<ID3D12CommandQueue> queue; unsigned width{},height{}; DXGI_FORMAT format{}; std::string path; };
static constexpr UINT kB171VisibilityMessage=WM_APP+170;
static constexpr UINT kB177FpsMessage=WM_APP+171;
static HWND g_b177FpsLabel{};
static HWND g_b187FpsOverlay{};
static HFONT g_b177FpsFont{};
static HFONT g_b201OverlayFont{};
static HANDLE g_b201OverlayThread{},g_b201OverlayReady{};
static char g_b187FpsText[256]="FPS -- | FG -- | CPU -- | GPU -- | VRAM --/--GB | Run 00:00:00";
// B201: FPS overlay runs on its own UI thread and never participates in XeFG proxy presentation.
static std::atomic<unsigned> g_b191SourceFps100{0},g_b191FgFps100{0};
// B207: the visible FPS values are a short rolling measurement, not the lifetime/ramp average.
static SRWLOCK g_b207FpsLock=SRWLOCK_INIT;
static ULONGLONG g_b207FpsTick{};
static unsigned g_b207FpsObservedBase{},g_b207FpsOutputBase{};
static std::atomic<bool> g_b207FpsValid{false};
static std::atomic<bool> g_b208ExclusiveFullscreen{false};
static void b207_reset_realtime_fps()
{
 AcquireSRWLockExclusive(&g_b207FpsLock);
 g_b207FpsTick=GetTickCount64();
 g_b207FpsObservedBase=g_b174oObservedSourcePresents.load(std::memory_order_acquire);
 g_b207FpsOutputBase=g_b174OutputFrames.load(std::memory_order_acquire);
 g_b191SourceFps100.store(0,std::memory_order_release);g_b191FgFps100.store(0,std::memory_order_release);g_b207FpsValid.store(false,std::memory_order_release);
 ReleaseSRWLockExclusive(&g_b207FpsLock);
}
static void b207_update_realtime_fps()
{
 const ULONGLONG now=GetTickCount64();
 if(!TryAcquireSRWLockExclusive(&g_b207FpsLock))return;
 if(!g_b207FpsTick){g_b207FpsTick=now;g_b207FpsObservedBase=g_b174oObservedSourcePresents.load();g_b207FpsOutputBase=g_b174OutputFrames.load();ReleaseSRWLockExclusive(&g_b207FpsLock);return;}
 const ULONGLONG elapsed=now-g_b207FpsTick;
 if(elapsed<250){ReleaseSRWLockExclusive(&g_b207FpsLock);return;}
 const unsigned observed=g_b174oObservedSourcePresents.load(std::memory_order_acquire),output=g_b174OutputFrames.load(std::memory_order_acquire);
 const unsigned sourceDelta=observed>=g_b207FpsObservedBase?observed-g_b207FpsObservedBase:0;
 const unsigned outputDelta=output>=g_b207FpsOutputBase?output-g_b207FpsOutputBase:0;
 if(sourceDelta){g_b191SourceFps100.store((unsigned)((100000.0*sourceDelta/elapsed)+0.5),std::memory_order_release);g_b191FgFps100.store((unsigned)((100000.0*outputDelta/elapsed)+0.5),std::memory_order_release);g_b207FpsValid.store(true,std::memory_order_release);}
 g_b207FpsObservedBase=observed;g_b207FpsOutputBase=output;g_b207FpsTick=now;
 ReleaseSRWLockExclusive(&g_b207FpsLock);
}
static bool b174_poll_source_window(const char* origin);
static bool b315_monitor_rect(RECT& result)
{
 const HWND anchor=g_b174GtaWindow?g_b174GtaWindow:g_xefgWindow;
 if(!anchor||!IsWindow(anchor))return false;
 const HMONITOR monitor=MonitorFromWindow(anchor,MONITOR_DEFAULTTONEAREST);
 if(!monitor)return false;
 MONITORINFO info{};info.cbSize=sizeof(info);
 if(!GetMonitorInfoA(monitor,&info))return false;
 result=info.rcMonitor;
 return result.right>result.left&&result.bottom>result.top;
}
static RECT b174m_proxy_rect()
{
 if(g_b316PersistentMonitorCover.load(std::memory_order_acquire)){RECT monitor{};if(b315_monitor_rect(monitor))return monitor;}
 AcquireSRWLockShared(&g_b174WindowLock);RECT result=g_b174GtaClientRect;ReleaseSRWLockShared(&g_b174WindowLock);
 return result;
}
static void b174_hide_proxy(const char*reason)
{
 const char*effectiveReason=reason?reason:"unspecified";
 // B401: once F9 owns the fullscreen monitor-cover surface, transient FG/source recovery
 // is not allowed to change HWND visibility while GTA's source window is still alive.
 // This central guard covers old callers as well as new ones, preventing the observed
 // proxy -> GTA -> proxy fullscreen switching without changing F9-off/window-loss behavior.
 const bool b401RecoveryHide=strcmp(effectiveReason,"confirmed_live_pipeline_stalled")==0||
                             strcmp(effectiveReason,"same_device_swapchain_suspended")==0||
                             strcmp(effectiveReason,"failure_fallback")==0;
 const bool b401KeepFullscreen=b401RecoveryHide&&g_b174TestActive.load(std::memory_order_acquire)&&
                               g_b316PersistentMonitorCover.load(std::memory_order_acquire)&&
                               g_b174GtaWindow&&IsWindow(g_b174GtaWindow);
 if(b401KeepFullscreen){
  g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
  log_line("B401: FULLSCREEN_HIDE_SUPPRESSED reason="+std::string(effectiveReason)+", f9_latch=1, gta_window_valid=1, proxy_visibility_unchanged=1, recovery_internal_only=1");
  return;
 }
 const bool wasVisible=g_b171WindowVisible.load(std::memory_order_acquire);
 g_b174ProxyRevealed.store(false,std::memory_order_release);g_b174StartupBackbuffersSeeded.store(false,std::memory_order_release);g_b174HiddenCompleted.store(0,std::memory_order_release);g_b352InitialValidX4Seen.store(false,std::memory_order_release);g_b174RevealFenceMarker.store(0,std::memory_order_release);g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
 if(g_xefgWindow)PostMessageA(g_xefgWindow,kB171VisibilityMessage,0,0);
 if(wasVisible)log_line("B174z: SURFACE_HIDE reason="+std::string(effectiveReason)+", normal_gta_presentation_visible=1, stale_content_reveal=0");
}
// B396: hard fail-open boundary between GTA native rendering and the XeFG sidecar.
// After a session has genuinely produced/revealed FG output, a missing-output interval must
// not let the GTA render thread continue feeding a dead sidecar at full source rate.
// While stale, hide the proxy immediately and admit only one recovery probe every 100 ms.
// A successful XeFG output refreshes g_b270LastSuccessfulOutputTick and automatically
// returns admission to full rate without changing the F9 latch or rebuilding the contexts.
// B399: bounded hard-bypass recovery state machine.
// Once an established session loses XeFG output, GTA's Present path stops feeding the
// sidecar completely for a native-only cooldown.  Recovery is then a single bounded
// three-source-frame re-arm burst (seed + history + x4 opportunity), never a perpetual
// 10 Hz recovery-seed loop.  If that burst does not produce successful XeFG output,
// admission closes again and GTA remains native-only for a longer cooldown.
// No source rediscovery, context rebuild, fence wait, queue drain, or XeSS reset is
// requested from the GTA render thread here.
static std::atomic<unsigned> g_b399FailOpenState{0}; // 0 healthy, 1 hard bypass, 2 bounded re-arm
static std::atomic<unsigned> g_b399RearmFrames{0};
static std::atomic<ULONGLONG> g_b399StateTick{0};
static std::atomic<ULONGLONG> g_b399OutputAtFailure{0};
extern "C" __declspec(dllexport) BOOL WINAPI B396AllowFgSubmission()
{
 const ULONGLONG now=GetTickCount64();
 if(!g_b174TestActive.load(std::memory_order_acquire)||!g_b174SessionEverRevealed.load(std::memory_order_acquire))return TRUE;
 const ULONGLONG lastOutput=g_b270LastSuccessfulOutputTick.load(std::memory_order_acquire);
 unsigned state=g_b399FailOpenState.load(std::memory_order_acquire);
 if(state==0){
  if(!lastOutput||now<=lastOutput||now-lastOutput<=125)return TRUE;
  g_b399OutputAtFailure.store(lastOutput,std::memory_order_release);
  g_b399RearmFrames.store(0,std::memory_order_release);
  g_b399StateTick.store(now,std::memory_order_release);
  g_b399FailOpenState.store(1,std::memory_order_release);
  g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
  // B400: F9 owns one persistent fullscreen monitor-cover presentation surface.
  // A stale XeFG interval changes FG admission only; it must never hide/show the HWND,
  // because that visibly switches the user between the proxy and GTA fullscreen.
  if(g_b174ProxyRevealed.load(std::memory_order_acquire))
   log_line("B400: FULLSCREEN_SURFACE_RETAINED reason=b399_fg_stale_hard_native_bypass, proxy_visible=1, f9_latch=1, recovery_visibility_transition=0");
  return FALSE;
 }
 const ULONGLONG failedOutput=g_b399OutputAtFailure.load(std::memory_order_acquire);
 // A genuinely new successful XeFG output is the only automatic return to full admission.
 if(lastOutput&&lastOutput>failedOutput){
  g_b399FailOpenState.store(0,std::memory_order_release);
  g_b399RearmFrames.store(0,std::memory_order_release);
  return TRUE;
 }
 if(state==1){
  const ULONGLONG entered=g_b399StateTick.load(std::memory_order_acquire);
  if(now-entered<500)return FALSE; // uninterrupted native GTA recovery window
  unsigned expected=1;
  if(g_b399FailOpenState.compare_exchange_strong(expected,2,std::memory_order_acq_rel)){
   g_b399RearmFrames.store(0,std::memory_order_release);
   g_b399StateTick.store(now,std::memory_order_release);
   g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
  }
  state=g_b399FailOpenState.load(std::memory_order_acquire);
 }
 if(state==2){
  const unsigned frame=g_b399RearmFrames.fetch_add(1,std::memory_order_acq_rel);
  if(frame<3)return TRUE;
  // Re-arm failed: close admission before another recovery-seed train can form.
  g_b399FailOpenState.store(1,std::memory_order_release);
  g_b399StateTick.store(now,std::memory_order_release);
  g_b399RearmFrames.store(0,std::memory_order_release);
  g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
  return FALSE;
 }
 return FALSE;
}
static void b174_request_stop(unsigned reason)
{
 unsigned expected=0;const bool accepted=g_b174StopReason.compare_exchange_strong(expected,reason,std::memory_order_acq_rel);
 const unsigned effectiveReason=accepted?reason:expected;
 g_b171LiveReady.store(false,std::memory_order_release);
 b174_hide_proxy(effectiveReason==2?"f9_toggle_off":effectiveReason==3?"escape":effectiveReason==9?"legacy_bounded_stop":"failure_fallback");
 if(accepted&&reason>=4&&reason!=8&&reason!=9){g_b175PipelineReady.store(false,std::memory_order_release);g_b175PipelineFailed.store(true,std::memory_order_release);}
 else if(!accepted&&effectiveReason>0&&effectiveReason<4&&reason>=4)log_line("B175n: STOP_ESCALATION_SUPPRESSED retained_reason="+std::to_string(effectiveReason)+", rejected_reason="+std::to_string(reason)+", reusable_pipeline_preserved=1");
}
static void b189_render_fps_layered(HWND overlay)
{
 if(!overlay)return;
 constexpr int width=620,height=42;
 HDC screen=GetDC(nullptr);if(!screen)return;
 HDC memory=CreateCompatibleDC(screen);if(!memory){ReleaseDC(nullptr,screen);return;}
 BITMAPINFO bmi{};bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bmi.bmiHeader.biWidth=width;bmi.bmiHeader.biHeight=-height;bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;
 void*bits=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&bmi,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmap){DeleteDC(memory);ReleaseDC(nullptr,screen);return;}
 HGDIOBJ oldBitmap=SelectObject(memory,bitmap);RECT rc{0,0,width,height};HBRUSH bg=CreateSolidBrush(RGB(0,0,0));FillRect(memory,&rc,bg);DeleteObject(bg);
 SetBkMode(memory,TRANSPARENT);SetTextColor(memory,RGB(255,255,255));HGDIOBJ oldFont=g_b177FpsFont?SelectObject(memory,g_b177FpsFont):nullptr;RECT textRc{10,0,width-8,height};DrawTextA(memory,g_b187FpsText,-1,&textRc,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
 // UpdateLayeredWindow expects premultiplied BGRA. Make the opaque bar fully visible.
 if(bits){auto*px=static_cast<unsigned char*>(bits);for(int i=0;i<width*height;++i)px[i*4+3]=230;}
 POINT source{0,0};SIZE size{width,height};RECT target=b174m_proxy_rect();POINT dest{target.left+18,target.top+18};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
 UpdateLayeredWindow(overlay,screen,&dest,&size,memory,&source,0,&blend,ULW_ALPHA);
 if(oldFont)SelectObject(memory,oldFont);SelectObject(memory,oldBitmap);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,screen);
}
static LRESULT CALLBACK b187_fps_overlay_proc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_CREATE){g_b201OverlayFont=CreateFontA(-24,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Segoe UI");return 0;}
 if(m==WM_NCHITTEST)return HTTRANSPARENT;
 if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(m==WM_SETCURSOR)return TRUE;
 if(m==WM_ERASEBKGND)return 1;
 if(m==WM_PAINT){
  PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT rc{};GetClientRect(h,&rc);
  HBRUSH bg=CreateSolidBrush(RGB(0,0,0));FillRect(dc,&rc,bg);DeleteObject(bg);
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));HGDIOBJ oldFont=g_b201OverlayFont?SelectObject(dc,g_b201OverlayFont):nullptr;
  RECT textRc{12,0,rc.right-8,rc.bottom};DrawTextA(dc,g_b187FpsText,-1,&textRc,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
  if(oldFont)SelectObject(dc,oldFont);EndPaint(h,&ps);return 0;
 }
 if(m==WM_DESTROY){if(g_b201OverlayFont){DeleteObject(g_b201OverlayFont);g_b201OverlayFont=nullptr;}PostQuitMessage(0);return 0;}
 return DefWindowProcA(h,m,w,l);
}
static void b201_update_manual_fps_overlay()
{
 if(!g_b187FpsOverlay||!IsWindow(g_b187FpsOverlay))return;
 const double source=g_b191SourceFps100.load(std::memory_order_acquire)/100.0;
 const double fg=g_b191FgFps100.load(std::memory_order_acquire)/100.0;
 char runtime[32]{};const ULONGLONG now=GetTickCount64(),elapsed=(g_b174ActivationStart&&now>=g_b174ActivationStart)?now-g_b174ActivationStart:0;b302_format_runtime(elapsed,runtime,sizeof(runtime));const double cpu=g_b304Cpu100.load(std::memory_order_acquire)/100.0,gpu=g_b304Gpu100.load(std::memory_order_acquire)/100.0,vu=g_b304VramUsedBytes.load(std::memory_order_acquire)/(1024.0*1024.0*1024.0),vt=g_b304VramTotalBytes.load(std::memory_order_acquire)/(1024.0*1024.0*1024.0);if(g_b207FpsValid.load(std::memory_order_acquire))sprintf_s(g_b187FpsText,"FPS %.1f | FG %.1f | x%.2f | CPU %.0f%% | RT %.0f%% | GPU %.0f%% | VRAM %.1f/%.1fGB | Run %s",source,fg,source>0.01?fg/source:0.0,cpu,g_b305RenderCpu100.load(std::memory_order_acquire)/100.0,gpu,vu,vt,runtime);else sprintf_s(g_b187FpsText,"FPS -- | FG -- | x-- | CPU %.0f%% | RT %.0f%% | GPU %.0f%% | VRAM %.1f/%.1fGB | Run %s",cpu,g_b305RenderCpu100.load(std::memory_order_acquire)/100.0,gpu,vu,vt,runtime);
 const bool gtaUsable=g_b174GtaWindow&&IsWindow(g_b174GtaWindow)&&!IsIconic(g_b174GtaWindow);
 const bool pipelineVisible=g_b171LiveReady.load(std::memory_order_acquire)&&g_b171WindowVisible.load(std::memory_order_acquire);
 // B206: this is a child of the XeFG presentation HWND.  It therefore follows the exact
 // visible output surface across bordered/borderless/fullscreen-sized transitions and Alt-Tab,
 // without creating an independently activating/topmost desktop window.
 if(!gtaUsable||!pipelineVisible){ShowWindow(g_b187FpsOverlay,SW_HIDE);return;}
 SetWindowPos(g_b187FpsOverlay,HWND_TOP,18,18,1500,44,SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_SHOWWINDOW);
 InvalidateRect(g_b187FpsOverlay,nullptr,FALSE);UpdateWindow(g_b187FpsOverlay);
}
static DWORD WINAPI b201_overlay_worker(void* parameter)
{
 auto* path=static_cast<std::string*>(parameter);std::ofstream out(*path,std::ios::app);
 HINSTANCE instance=GetModuleHandleA(nullptr);WNDCLASSA owc{};owc.lpfnWndProc=b187_fps_overlay_proc;owc.hInstance=instance;owc.hCursor=nullptr;owc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);owc.lpszClassName="GTAXeSSB206ProxyChildFpsOverlay";RegisterClassA(&owc);
 HWND parent=g_xefgWindow;
 // B209: do not create a child HWND on the XeFG presentation window.  The child belonged
 // to a different UI thread and could couple Win32 input/message queues to the proxy.
 // Keep FPS telemetry in the log while presentation/input stability is verified.
 g_b187FpsOverlay=nullptr;
 out<<"B210: FPS_CHILD_OVERLAY_DISABLED created=0, parent_is_xefg_proxy="<<(parent?1:0)<<", reason=avoid_cross_thread_child_input_coupling\n";out.flush();
 if(g_b201OverlayReady)SetEvent(g_b201OverlayReady);
 if(g_b187FpsOverlay){SetTimer(g_b187FpsOverlay,205,16,nullptr);MSG msg{};while(GetMessageA(&msg,nullptr,0,0)>0){if(msg.message==WM_TIMER&&msg.hwnd==g_b187FpsOverlay&&msg.wParam==205)b201_update_manual_fps_overlay();TranslateMessage(&msg);DispatchMessageA(&msg);}KillTimer(g_b187FpsOverlay,205);}
 g_b187FpsOverlay=nullptr;delete path;return 0;
}
static void b201_stop_overlay()
{
 if(g_b187FpsOverlay&&IsWindow(g_b187FpsOverlay))PostMessageA(g_b187FpsOverlay,WM_CLOSE,0,0);
 if(g_b201OverlayThread){WaitForSingleObject(g_b201OverlayThread,3000);CloseHandle(g_b201OverlayThread);g_b201OverlayThread=nullptr;}
 if(g_b201OverlayReady){CloseHandle(g_b201OverlayReady);g_b201OverlayReady=nullptr;}
}
static LRESULT CALLBACK b145_window_proc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_NCHITTEST){
  const ULONGLONG now=GetTickCount64(),last=g_b220LastHitTestLog.load(std::memory_order_acquire);
  if(!last||now-last>=250){g_b220LastHitTestLog.store(now,std::memory_order_release);POINT p{};GetCursorPos(&p);log_line("B225: PROXY_WM_NCHITTEST hwnd="+std::to_string((uintptr_t)h)+", x="+std::to_string(p.x)+", y="+std::to_string(p.y)+", result=HTTRANSPARENT, same_thread_passthrough=1");}
  return HTTRANSPARENT;
 }
 if(m==WM_MOUSEACTIVATE){log_line("B225: PROXY_WM_MOUSEACTIVATE result=MA_NOACTIVATE");return MA_NOACTIVATE;}
 if(m==WM_SETCURSOR){log_line("B225: PROXY_WM_SETCURSOR observed=1, intercepted=0, cursor_api_mutation=0");return DefWindowProcA(h,m,w,l);}
 if(m==WM_ACTIVATE||m==WM_ACTIVATEAPP||m==WM_SETFOCUS||m==WM_KILLFOCUS){log_line("B225: PROXY_FOCUS_MESSAGE msg="+std::to_string(m)+", suppressed=1");return 0;}
 if(m==WM_CREATE){g_b177FpsFont=CreateFontA(-24,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Segoe UI");return 0;}
 if(m==kB177FpsMessage)return 0;
 if(m==WM_TIMER&&w==1)return 0;
 if(m==kB171VisibilityMessage){
  const RECT target=b174m_proxy_rect();
  if(w==1)SetWindowPos(h,HWND_TOPMOST,target.left,target.top,target.right-target.left,target.bottom-target.top,SWP_SHOWWINDOW|SWP_NOACTIVATE);
  else if(w==2){SetWindowPos(h,HWND_TOPMOST,target.left,target.top,target.right-target.left,target.bottom-target.top,SWP_NOACTIVATE);ShowWindow(h,SW_HIDE);}
  else if(w==3)SetWindowPos(h,HWND_TOPMOST,target.left,target.top,target.right-target.left,target.bottom-target.top,SWP_NOACTIVATE);
  else{ShowWindow(h,SW_HIDE);SetWindowPos(h,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);}
  g_b171WindowVisible.store(IsWindowVisible(h)!=FALSE,std::memory_order_release);
  LONG_PTR st=GetWindowLongPtrA(h,GWL_STYLE),ex=GetWindowLongPtrA(h,GWL_EXSTYLE);log_line("B225: PROXY_VISIBILITY_CHANGE action="+std::to_string((unsigned long long)w)+", visible="+std::to_string(IsWindowVisible(h)?1:0)+", enabled="+std::to_string(IsWindowEnabled(h)?1:0)+", style="+std::to_string((unsigned long long)st)+", exstyle="+std::to_string((unsigned long long)ex)+", ex_transparent="+std::to_string((ex&WS_EX_TRANSPARENT)?1:0)+", ex_noactivate="+std::to_string((ex&WS_EX_NOACTIVATE)?1:0)+", ex_toolwindow="+std::to_string((ex&WS_EX_TOOLWINDOW)?1:0));
  return 0;
 }
 if(m==WM_CLOSE){DestroyWindow(h);return 0;}
 if(m==WM_DESTROY){if(g_b177FpsFont){DeleteObject(g_b177FpsFont);g_b177FpsFont=nullptr;}g_b177FpsLabel=nullptr;log_line("B225: PROXY_DESTROY same_gta_thread=1, post_quit_message=0");return 0;}
 return DefWindowProcA(h,m,w,l);
}
static constexpr UINT kB225CreateProxyMessage=WM_APP+0x225;
static WNDPROC g_b225OriginalGtaWndProc{};
static HANDLE g_b225ProxyCreateDone{};
static std::string g_b225ProxyCreateLogPath;
static std::atomic<HRESULT> g_b225ProxyCreateHr{E_PENDING};
static HRESULT b225_create_proxy_impl_on_current_thread(const char* path)
{
 if(g_xefgWindow&&IsWindow(g_xefgWindow))return S_FALSE;
 HINSTANCE instance=GetModuleHandleA(nullptr);WNDCLASSA wc{};wc.lpfnWndProc=b145_window_proc;wc.hInstance=instance;wc.hCursor=nullptr;wc.lpszClassName="GTAXeSSB161PrivateWindow";
 RegisterClassA(&wc);
 HWND window=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,wc.lpszClassName,"GTAXeSS B327 - latency1 monitor-cover GTA-thread proxy",WS_POPUP,0,0,64,64,nullptr,nullptr,instance,nullptr);
 g_xefgWindow=window;
 std::ofstream out(path,std::ios::app);if(out){DWORD gtaTid=g_b174GtaWindow?GetWindowThreadProcessId(g_b174GtaWindow,nullptr):0;out<<"B225: PROXY_WINDOW_CREATED created="<<(window?1:0)<<", owner_thread="<<GetCurrentThreadId()<<", gta_window_thread="<<gtaTid<<", same_thread="<<((window&&gtaTid==GetCurrentThreadId())?1:0)<<", marshalled_via_gta_wndproc=1, visible=0, no_activate=1, hit_test_transparent=1, hcursor_null=1, cursor_api_mutation=0, tool_window=1\n";out.flush();out<<"B314: PROXY_DIRECT_FLIP_ELIGIBILITY_HOST created="<<(window?1:0)<<", ws_ex_transparent_removed=1, ws_ex_noactivate_retained=1, ws_ex_toolwindow_retained=1, wm_nchittest_httransparent_retained=1, wm_mouseactivate_noactivate_retained=1, popup_window=1, topmost_only_when_visible=1, b327_host_restored_to_b325=1\n";out.flush();out<<"B316: PERSISTENT_MONITOR_COVER_HOST_POLICY geometry=full_monitor_rect, activation=from_first_hidden_position_and_reveal, restore_to_gta_rect=0, private_swapchain_size_unchanged=1920x1080, content_layout=monitor_stretch_retained_for_single_change, buffer_count=5, latency1_b327=0, baseline_latency_b328=1, sync_interval=0, allow_tearing=1, xefg_x4_unchanged=1, b315_monitor_cover_result_promoted=1\n";out.flush();out<<"B328: QUEUE_ADMISSION_HEADROOM_POLICY max_frame_latency_mutation=0, xeFG_created_latency_retained=1, buffer_count=5, sync_interval=0, allow_tearing=1, queue_capacity=11, resource_slots=12, serial_worker=1, b325_profiler_retained=1, b324_camera_frozen=1, xefg_x4_unchanged=1, runtime_changes=queue_capacity_11_and_remove_b327_latency1\n";out.flush();out<<"B329: GHOSTING_TEMPORAL_HISTORY_GUARD_POLICY ghosting_only=1, abrupt_camera_history_reset=1, vp_max_threshold=0.08, vp_rms_threshold=0.030, spike_ratio=1.80, reconstructed_motion_unchanged=1, native_motion_runtime_switch=1, queue_capacity_unchanged=11, frame_latency_unchanged=1, xefg_x4_unchanged=1, input_latency_changes=0\n";out.flush();out<<"B317: SOURCE_FRAME_PIPELINE_PROFILER_POLICY diagnostic_only=1, stages=depth_pack_motion_reconstruct_xess_color_fence_queue, source_entry_gap=1, private_present_reference=1, b316_host_frozen=1, b313_vblank_retained=1, b310_present_profiler_retained=1, xefg_x4_unchanged=1, runtime_behavior_changed=0\n";out.flush();}
 return window?S_OK:HRESULT_FROM_WIN32(GetLastError());
}
static LRESULT CALLBACK b225_gta_window_proc(HWND h,UINT m,WPARAM w,LPARAM l)
{
 if(m==kB225CreateProxyMessage){
  const HRESULT hr=b225_create_proxy_impl_on_current_thread(g_b225ProxyCreateLogPath.c_str());
  g_b225ProxyCreateHr.store(hr,std::memory_order_release);
  if(g_b225ProxyCreateDone)SetEvent(g_b225ProxyCreateDone);
  return 0;
 }
 return g_b225OriginalGtaWndProc?CallWindowProcA(g_b225OriginalGtaWndProc,h,m,w,l):DefWindowProcA(h,m,w,l);
}
static HRESULT b225_create_proxy_on_gta_thread(const char* path)
{
 if(g_xefgWindow&&IsWindow(g_xefgWindow))return S_FALSE;
 if(!g_b174GtaWindow||!IsWindow(g_b174GtaWindow)||!path)return E_HANDLE;
 const DWORD gtaTid=GetWindowThreadProcessId(g_b174GtaWindow,nullptr),callerTid=GetCurrentThreadId();
 if(!gtaTid)return E_HANDLE;
 if(callerTid==gtaTid)return b225_create_proxy_impl_on_current_thread(path);
 if(!g_b225OriginalGtaWndProc){
  auto original=reinterpret_cast<WNDPROC>(GetWindowLongPtrA(g_b174GtaWindow,GWLP_WNDPROC));if(!original)return HRESULT_FROM_WIN32(GetLastError());
  g_b225OriginalGtaWndProc=original;SetLastError(0);LONG_PTR previous=SetWindowLongPtrA(g_b174GtaWindow,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&b225_gta_window_proc));
  if(!previous){DWORD error=GetLastError();g_b225OriginalGtaWndProc=nullptr;if(error)return HRESULT_FROM_WIN32(error);}
 }
 if(!g_b225ProxyCreateDone)g_b225ProxyCreateDone=CreateEventA(nullptr,TRUE,FALSE,nullptr);
 if(!g_b225ProxyCreateDone)return HRESULT_FROM_WIN32(GetLastError());
 ResetEvent(g_b225ProxyCreateDone);g_b225ProxyCreateLogPath=path;g_b225ProxyCreateHr.store(E_PENDING,std::memory_order_release);
 log_line("B225: PROXY_CREATE_MARSHAL_REQUEST caller_thread="+std::to_string(callerTid)+", gta_thread="+std::to_string(gtaTid)+", same_thread=0, timeout_ms=2000");
 if(!PostMessageA(g_b174GtaWindow,kB225CreateProxyMessage,0,0))return HRESULT_FROM_WIN32(GetLastError());
 const DWORD wait=WaitForSingleObject(g_b225ProxyCreateDone,2000);
 if(wait!=WAIT_OBJECT_0){log_line("B225: PROXY_CREATE_MARSHAL_FAILED wait="+std::to_string(wait)+", fail_closed=1");return HRESULT_FROM_WIN32(wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError());}
 const HRESULT hr=g_b225ProxyCreateHr.load(std::memory_order_acquire);DWORD proxyTid=g_xefgWindow?GetWindowThreadProcessId(g_xefgWindow,nullptr):0;
 log_line("B225: PROXY_CREATE_MARSHAL_COMPLETE hr="+std::to_string((long)hr)+", proxy_thread="+std::to_string(proxyTid)+", gta_thread="+std::to_string(gtaTid)+", same_thread="+std::to_string((proxyTid&&proxyTid==gtaTid)?1:0));
 return (SUCCEEDED(hr)&&proxyTid==gtaTid)?hr:E_FAIL;
}
static void b161_stop_window(HWND window)
{
 b201_stop_overlay();
 if(window&&IsWindow(window))PostMessageA(window,WM_CLOSE,0,0);
 g_b171WindowVisible.store(false,std::memory_order_release);
}
static bool b174_poll_source_window(const char* pollOrigin)
{
 AcquireSRWLockExclusive(&g_b174WindowPollLock);
 const ULONGLONG now=GetTickCount64();const ULONGLONG priorPoll=g_b174LastWindowPoll.exchange(now,std::memory_order_acq_rel);
 RECT client{};POINT sourceOrigin{};const HWND foreground=GetForegroundWindow();
 DWORD foregroundPid=0;if(foreground)GetWindowThreadProcessId(foreground,&foregroundPid);const bool sameProcessForeground=foreground&&foregroundPid==GetCurrentProcessId()&&foreground!=g_xefgWindow;const bool focused=g_b174GtaWindow&&(foreground==g_b174GtaWindow||sameProcessForeground);
 const bool usable=g_b174GtaWindow&&IsWindow(g_b174GtaWindow)&&IsWindowVisible(g_b174GtaWindow)&&!IsIconic(g_b174GtaWindow)&&GetClientRect(g_b174GtaWindow,&client)&&client.right>client.left&&client.bottom>client.top&&ClientToScreen(g_b174GtaWindow,&sourceOrigin);
 if(usable)OffsetRect(&client,sourceOrigin.x,sourceOrigin.y);
 if(!usable){
  const bool firstSuspend=!g_b174SurfaceSuspended.exchange(true,std::memory_order_acq_rel);
  if(firstSuspend){
   g_b174WindowGeneration.fetch_add(1,std::memory_order_acq_rel);
   b174_hide_proxy("source_window_minimized_or_invalid");
   log_line("B211: SURFACE_SUSPEND_REAL_WINDOW_LOSS poll_origin="+std::string(pollOrigin?pollOrigin:"unknown")+", focused="+std::to_string(focused?1:0)+", usable=0, minimized="+std::to_string(g_b174GtaWindow&&IsIconic(g_b174GtaWindow)?1:0)+", capture_paused=1");
  }
  ReleaseSRWLockExclusive(&g_b174WindowPollLock);
  return false;
 }
 if(!focused){
  b174_hide_proxy("source_window_not_foreground_visibility_only");
  log_line("B215: FOREGROUND_LOSS_VISIBILITY_ONLY poll_origin="+std::string(pollOrigin?pollOrigin:"unknown")+", gta_window_usable=1, same_process_gta_window_family=0, pipeline_suspended=0, window_generation_unchanged=1, f9_latch_unchanged=1");
  ReleaseSRWLockExclusive(&g_b174WindowPollLock);
  return false;
 }
 AcquireSRWLockExclusive(&g_b174WindowLock);const bool moved=!EqualRect(&client,&g_b174GtaClientRect);if(moved)g_b174GtaClientRect=client;ReleaseSRWLockExclusive(&g_b174WindowLock);
 if(moved){
  const bool firstSuspend=!g_b174SurfaceSuspended.exchange(true,std::memory_order_acq_rel);
  if(firstSuspend){
   g_b174WindowGeneration.fetch_add(1,std::memory_order_acq_rel);
   const bool wasVisible=g_b171WindowVisible.load(std::memory_order_acquire);b174_hide_proxy("source_client_rect_changed");
   if(g_xefgWindow)PostMessageA(g_xefgWindow,kB171VisibilityMessage,2,0);
   log_line("B174z: SURFACE_REPOSITION poll_origin="+std::string(pollOrigin?pollOrigin:"unknown")+", rect={"+std::to_string(client.left)+","+std::to_string(client.top)+","+std::to_string(client.right-client.left)+","+std::to_string(client.bottom-client.top)+"}, was_visible="+std::to_string(wasVisible?1:0)+", detection_latency_ms="+std::to_string(priorPoll&&now>=priorPoll?now-priorPoll:0)+", reveal_deferred_until_fresh_x4=1");
  }
  ReleaseSRWLockExclusive(&g_b174WindowPollLock);
  return false;
 }
 const bool resumed=g_b174SurfaceSuspended.exchange(false,std::memory_order_acq_rel);
 if(resumed){
  B175ResetFastFreshness();AcquireSRWLockExclusive(&g_b175MotionHistoryLock);g_b175bHaveXessPreviousViewProjection=false;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);g_b175bXessReset.store(true,std::memory_order_release);g_b174StartupDeadline.store(now+1500,std::memory_order_release);g_b174ResumeNeedsFresh.store(true,std::memory_order_release);b174_rebaseline_rate_window(now);
  log_line("B174z: SURFACE_RESUME poll_origin="+std::string(pollOrigin?pollOrigin:"unknown")+", focused=1, usable=1, fresh_input_required=1, stale_content_reveal=0");
 }
 ReleaseSRWLockExclusive(&g_b174WindowPollLock);
 return !moved&&!resumed;
}
typedef BOOL (__stdcall *B385GhostUnsafeFn)(BOOL);
typedef unsigned (__stdcall *B385FreezeUIntFn)();
typedef unsigned (__stdcall *B385FreezeFlagsFn)(BOOL);
typedef unsigned (__stdcall *B423FreezeUIntFn)();
typedef BOOL (__stdcall *B423FreezeQueueFn)(unsigned,double);
static HMODULE g_b385GhostDll=nullptr,g_b385FreezeDll=nullptr;
static B385GhostUnsafeFn g_b385GhostUnsafe=nullptr;
static B385FreezeUIntFn g_b385FreezeBuffers=nullptr,g_b385FreezeLatency=nullptr;
static B385FreezeFlagsFn g_b385FreezeFlags=nullptr;
static B423FreezeUIntFn g_b423FreezeApiVersion=nullptr,g_b423FreezePendingLimit=nullptr,g_b423FreezeMaxQueueAgeMs=nullptr;
static B423FreezeQueueFn g_b423FreezeCollapseQueue=nullptr,g_b423FreezeCollapseSourceGap=nullptr;
static bool b385_load_protected_modules(){
 if(!g_b385GhostDll)g_b385GhostDll=LoadLibraryW(L"GTAXeSS.GhostingFix.dll");
 if(!g_b385FreezeDll)g_b385FreezeDll=LoadLibraryW(L"GTAXeSS.FreezingFix.dll");
 if(g_b385GhostDll&&!g_b385GhostUnsafe)g_b385GhostUnsafe=(B385GhostUnsafeFn)GetProcAddress(g_b385GhostDll,"GTAXeSSGhostingUnsafeFramePolicy");
 if(g_b385FreezeDll&&!g_b385FreezeBuffers){g_b385FreezeBuffers=(B385FreezeUIntFn)GetProcAddress(g_b385FreezeDll,"GTAXeSSFreezeBufferCount");g_b385FreezeLatency=(B385FreezeUIntFn)GetProcAddress(g_b385FreezeDll,"GTAXeSSFreezeMaximumFrameLatency");g_b385FreezeFlags=(B385FreezeFlagsFn)GetProcAddress(g_b385FreezeDll,"GTAXeSSFreezePresentFlags");g_b423FreezeApiVersion=(B423FreezeUIntFn)GetProcAddress(g_b385FreezeDll,"GTAXeSSFreezingApiVersion");g_b423FreezePendingLimit=(B423FreezeUIntFn)GetProcAddress(g_b385FreezeDll,"GTAXeSSFreezePendingAdmissionLimit");g_b423FreezeMaxQueueAgeMs=(B423FreezeUIntFn)GetProcAddress(g_b385FreezeDll,"GTAXeSSFreezeMaximumQueuedAgeMs");g_b423FreezeCollapseQueue=(B423FreezeQueueFn)GetProcAddress(g_b385FreezeDll,"GTAXeSSFreezeCollapseQueuedWorkerJobs");g_b423FreezeCollapseSourceGap=(B423FreezeQueueFn)GetProcAddress(g_b385FreezeDll,"GTAXeSSFreezeCollapsePendingOnSourceGap");}
 return g_b385GhostUnsafe&&g_b385FreezeBuffers&&g_b385FreezeLatency&&g_b385FreezeFlags&&g_b423FreezeApiVersion&&g_b423FreezeApiVersion()==3&&g_b423FreezePendingLimit&&g_b423FreezeMaxQueueAgeMs&&g_b423FreezeCollapseQueue&&g_b423FreezeCollapseSourceGap;
}


static DWORD WINAPI b145_worker(void* parameter)
{
    // B245: keep the serial XeFG presentation worker responsive without using dangerous TIME_CRITICAL priority.
    const BOOL b245PriorityRaised=SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
    B160InitSignal initSignal;g_xefgInitFinished.store(false,std::memory_order_release);g_xefgReady.store(false,std::memory_order_release);
    auto* work=static_cast<B145Work*>(parameter);std::ofstream out(work->path,std::ios::app);
    out<<"B246: WORKER_SCHEDULING priority=THREAD_PRIORITY_HIGHEST, priority_set="<<(b245PriorityRaised?1:0)<<", serial_xefg_present_preserved=1, extra_worker_threads=0\n";out<<"B147: worker_begin present_thread_blocked=0\n";out.flush();
    xefg_swapchain_version_t version{};auto versionResult=xefgSwapChainGetVersion(&version);
    out<<"B147: stage=XeFGGetVersion, result="<<(int)versionResult<<", version="<<version.major<<"."<<version.minor<<"."<<version.patch<<"\n";out.flush();
    xell_version_t xellVersion{};auto xellVersionResult=xellGetVersion(&xellVersion);
    out<<"B147: stage=XeLLGetVersion, result="<<(int)xellVersionResult<<", version="<<xellVersion.major<<"."<<xellVersion.minor<<"."<<xellVersion.patch<<"\n";out.flush();
    xell_context_handle_t xell{};auto xellCreateResult=xellD3D12CreateContext(work->device.Get(),&xell);
    out<<"B147: stage=XeLLD3D12CreateContext, context="<<(xell?1:0)<<", result="<<(int)xellCreateResult<<"\n";out.flush();
    if(xellCreateResult!=XELL_RESULT_SUCCESS||!xell){delete work;return 1;}
    xell_sleep_params_t sleep{};sleep.bLowLatencyMode=1;sleep.minimumIntervalUs=0;
    auto sleepResult=xellSetSleepMode(xell,&sleep);
    out<<"B147: stage=XeLLSetSleepMode, low_latency=1, result="<<(int)sleepResult<<"\n";out.flush();
    if(sleepResult!=XELL_RESULT_SUCCESS){xellDestroyContext(xell);delete work;return 2;}
    xefg_swapchain_handle_t context{};auto createResult=xefgSwapChainD3D12CreateContext(work->device.Get(),&context);
    out<<"B147: stage=XeFGD3D12CreateContext, context="<<(context?1:0)<<", result="<<(int)createResult<<"\n";out.flush();
    if(createResult!=XEFG_SWAPCHAIN_RESULT_SUCCESS||!context){xellDestroyContext(xell);delete work;return 3;}
    g_b172dXeFGLogPath=work->path;
    auto loggingResult=xefgSwapChainSetLoggingCallback(context,XEFG_SWAPCHAIN_LOGGING_LEVEL_ERROR,b172d_xefg_log_callback,nullptr);
    out<<"B258: stage=XeFGSetLoggingCallback, level=ERROR, debug_callback_hot_path_disabled=1, result="<<(int)loggingResult<<"\n";out.flush();
    auto latencyResult=xefgSwapChainSetLatencyReduction(context,xell);
    out<<"B147: stage=XeFGSetLatencyReduction, attached="<<(latencyResult==XEFG_SWAPCHAIN_RESULT_SUCCESS?1:0)<<", result="<<(int)latencyResult<<"\n";out.flush();
    if(latencyResult!=XEFG_SWAPCHAIN_RESULT_SUCCESS){xefgSwapChainDestroy(context);xellDestroyContext(xell);delete work;return 4;}
    // B172c: The optional eager pipeline-build entry point hangs in the tested
    // Intel driver even when requested in non-blocking mode. Intel's normal
    // quick-start sequence permits omitting it: InitFromSwapChainDesc owns any
    // required compilation and waits for it there.
    out<<"B172d: stage=XeFGBuildPipelines, action=skipped_optional_prebuild, init_owns_pipeline_build=1\n";out.flush();
    xefg_swapchain_properties_t properties{};auto propertiesResult=xefgSwapChainGetProperties(context,&properties);
    const UINT selectedInterpolations=(properties.maxSupportedInterpolations<3u)?properties.maxSupportedInterpolations:3u;
    out<<"B172d: stage=XeFGGetProperties, result="<<(int)propertiesResult<<", max_supported_interpolations="<<properties.maxSupportedInterpolations<<", requested_interpolations=3, selected_interpolations="<<selectedInterpolations<<"\n";out.flush();
    if((int)propertiesResult<0||selectedInterpolations!=3){out<<"B174y: X4_REQUIREMENT_FAILED selected_interpolations="<<selectedInterpolations<<", required_interpolations=3, proxy_created=0\n";out.flush();xefgSwapChainDestroy(context);xellDestroyContext(xell);delete work;return 5;}
    ComPtr<IDXGIFactory2> factory;HRESULT factoryHr=CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    out<<"B147: stage=CreateDXGIFactory, created="<<(factory?1:0)<<", hr="<<(long)factoryHr<<"\n";out.flush();
    if(FAILED(factoryHr)){xefgSwapChainDestroy(context);xellDestroyContext(xell);delete work;return 6;}
    BOOL allowTearing=FALSE;ComPtr<IDXGIFactory5> factory5;HRESULT factory5Hr=factory.As(&factory5);HRESULT tearingHr=SUCCEEDED(factory5Hr)?factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&allowTearing,sizeof(allowTearing)):factory5Hr;
    out<<"B174r: TEARING_SUPPORT factory5="<<(factory5?1:0)<<", query_hr="<<(long)tearingHr<<", allow_tearing="<<(allowTearing?1:0)<<", required=1\n";out.flush();
    if(FAILED(tearingHr)||!allowTearing){xefgSwapChainDestroy(context);xellDestroyContext(xell);delete work;return 10;}
    HWND window=g_xefgWindow;DWORD proxyTid=window?GetWindowThreadProcessId(window,nullptr):0;DWORD gtaTid=g_b174GtaWindow?GetWindowThreadProcessId(g_b174GtaWindow,nullptr):0;
    out<<"B225: stage=UseMarshalledGtaThreadWindowHost, created="<<(window?1:0)<<", proxy_thread="<<proxyTid<<", gta_thread="<<gtaTid<<", same_thread="<<((window&&proxyTid==gtaTid)?1:0)<<", separate_message_pump=0\n";out.flush();
    if(!window||!IsWindow(window)||!gtaTid||proxyTid!=gtaTid){b161_stop_window(window);xefgSwapChainDestroy(context);xellDestroyContext(xell);delete work;return 7;}
    out<<"B214: FPS_OVERLAY_THREAD_DISABLED created=0, child_hwnd=0, cross_thread_input_surface=0, fps_path=private_backbuffer_visual_only\n";out.flush();
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=work->width;desc.Height=work->height;desc.Format=work->format;desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;if(!b385_load_protected_modules())return E_FAIL;desc.BufferCount=g_b385FreezeBuffers();desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;desc.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    out<<"B340: PRIVATE_SWAPCHAIN_BUFFER_DEPTH_AB requested_buffer_count=3, previous_b339_buffer_count=5, sync_interval=0, allow_tearing=1, goal=isolate_private_present_blocking_without_queue_policy_change\n";out.flush();
    out<<"B311: PRIVATE_SWAPCHAIN_BUFFER_DEPTH_AB historical_test_buffer_count=5, B340_active_buffer_count=3, sync_interval=0, allow_tearing=1, xefg_interpolations=3, x4_unchanged=1, camera_identity_unchanged=1, queue_unchanged=1\n";out.flush();
    xefg_swapchain_d3d12_init_params_t init{};init.pApplicationSwapChain=nullptr;init.initFlags=XEFG_SWAPCHAIN_INIT_FLAG_NONE;
    init.maxInterpolatedFrames=selectedInterpolations;init.creationNodeMask=1;init.visibleNodeMask=1;
    // B193: the proxy backbuffer contains the measured FPS HUD while the separately tagged
    // HUDLESS_COLOR remains clean scene color. Let XeFG extract the UI from the backbuffer
    // and composite it after interpolation so the text is present on real and generated frames.
    init.uiMode=XEFG_SWAPCHAIN_UI_MODE_NONE;
    out<<"B172: stage=XeFGInitPrivateSwapChainBegin, caller_thread="<<GetCurrentThreadId()<<", window_thread_separate=0, window_thread_same_as_gta=1, size="<<work->width<<"x"<<work->height<<", interpolations="<<selectedInterpolations<<", ui_mode=NONE, ui_composition=disabled, hud_source=none\n";out.flush();
    auto initResult=xefgSwapChainD3D12InitFromSwapChainDesc(context,window,&desc,nullptr,work->queue.Get(),factory.Get(),&init);
    out<<"B172: stage=XeFGInitPrivateSwapChainEnd, size="<<work->width<<"x"<<work->height<<", format="<<(unsigned)work->format<<", interpolations="<<selectedInterpolations<<", swapchain_allow_tearing=1, result="<<(int)initResult<<"\n";out.flush();
    if(initResult!=XEFG_SWAPCHAIN_RESULT_SUCCESS){b161_stop_window(window);xefgSwapChainDestroy(context);xellDestroyContext(xell);delete work;return 8;}
    auto uiCompositionResult=xefgSwapChainSetUiCompositionState(context,XEFG_SWAPCHAIN_UI_COMPOSITION_STATE_DISABLED);
    out<<"B213: XEFG_UI_COMPOSITION_STATE enabled=0, mode=NONE, ui_interpolation_default=1, result="<<(int)uiCompositionResult<<"\n";out.flush();
    if(uiCompositionResult!=XEFG_SWAPCHAIN_RESULT_SUCCESS){b161_stop_window(window);xefgSwapChainDestroy(context);xellDestroyContext(xell);delete work;return 11;}
    ComPtr<IDXGISwapChain3> swap;auto pointerResult=xefgSwapChainD3D12GetSwapChainPtr(context,IID_PPV_ARGS(&swap));
    out<<"B147: stage=XeFGGetPrivateSwapChain, obtained="<<(swap?1:0)<<", result="<<(int)pointerResult<<"\n";out.flush();
    ComPtr<IDXGISwapChain2> b312Swap2;HRESULT b312Swap2Hr=swap?swap.As(&b312Swap2):E_NOINTERFACE;UINT b312InitialMaxLatency=0;HRESULT b312GetLatencyHr=SUCCEEDED(b312Swap2Hr)&&b312Swap2?b312Swap2->GetMaximumFrameLatency(&b312InitialMaxLatency):b312Swap2Hr;HANDLE b312Waitable=SUCCEEDED(b312Swap2Hr)&&b312Swap2?b312Swap2->GetFrameLatencyWaitableObject():nullptr;
    DXGI_SWAP_CHAIN_DESC actualSwapDesc{};HRESULT actualSwapDescHr=swap?swap->GetDesc(&actualSwapDesc):E_NOINTERFACE;
    const bool actualSwapSize=SUCCEEDED(actualSwapDescHr)&&actualSwapDesc.BufferDesc.Width==work->width&&actualSwapDesc.BufferDesc.Height==work->height;
    out<<"B312: FRAME_LATENCY_CAPABILITY swapchain2="<<(b312Swap2?1:0)<<", query_interface_hr="<<(long)b312Swap2Hr<<", get_max_latency_hr="<<(long)b312GetLatencyHr<<", initial_max_frame_latency="<<b312InitialMaxLatency<<", waitable_object_present="<<(b312Waitable?1:0)<<", swap_effect="<<(unsigned)actualSwapDesc.SwapEffect<<", flags="<<(unsigned)actualSwapDesc.Flags<<", buffer_count="<<actualSwapDesc.BufferCount<<", sync_interval=0, allow_tearing=1\n";out.flush();
    UINT b313BeforeLatency=0,b313AfterLatency=0;HRESULT b313BeforeHr=b312Swap2?b312Swap2->GetMaximumFrameLatency(&b313BeforeLatency):E_NOINTERFACE;HRESULT b313SetHr=(b312Swap2&&SUCCEEDED(b313BeforeHr)&&b313BeforeLatency!=3)?b312Swap2->SetMaximumFrameLatency(3):S_FALSE;HRESULT b313AfterHr=b312Swap2?b312Swap2->GetMaximumFrameLatency(&b313AfterLatency):E_NOINTERFACE;
    QueryPerformanceFrequency(&g_b313QpcFrequency);
    const bool b339LatencyRestored=SUCCEEDED(b313AfterHr)&&b313AfterLatency==3;g_b312LatencyApplied.store(false,std::memory_order_release);
    out<<"B340: PRIVATE_PRESENT_LATENCY_BASELINE_RETAINED before_query_hr="<<(long)b313BeforeHr<<", before_max_frame_latency="<<b313BeforeLatency<<", set_hr="<<(long)b313SetHr<<", requested_max_frame_latency=3, after_query_hr="<<(long)b313AfterHr<<", after_max_frame_latency="<<b313AfterLatency<<", restored="<<(b339LatencyRestored?1:0)<<", b338_latency2_rejected=1, buffer_count=3, sync_interval=0, allow_tearing=1, queue_capacity="<<kB174oPendingCapacity<<", core_render_fg_locked=1\n";out.flush();
    ComPtr<IDXGISwapChainMedia> b314SwapMedia;HRESULT b314MediaHr=swap?swap.As(&b314SwapMedia):E_NOINTERFACE;
    out<<"B314: PRESENT_MODE_CAPABILITY swapchain_media="<<(b314SwapMedia?1:0)<<", query_interface_hr="<<(long)b314MediaHr<<", frame_statistics_media_sampling=1, composition_mode_mapping={0:COMPOSED,1:OVERLAY,2:NONE,3:COMPOSITION_FAILURE}, independent_or_direct_flip_candidate_when_mode_noncomposed=1\n";out.flush();
    out<<"B174y: XEFG_PROXY_INPUT_OUTPUT_VERIFICATION proxy_swapchain="<<actualSwapDesc.BufferDesc.Width<<"x"<<actualSwapDesc.BufferDesc.Height<<", proxy_format="<<(unsigned)actualSwapDesc.BufferDesc.Format<<", input_resources="<<work->width<<"x"<<work->height<<", input_format=BGRA8, required="<<work->width<<"x"<<work->height<<"_BGRA8, selected_interpolations="<<selectedInterpolations<<", interpolation_mode=x4, exact_match="<<(actualSwapSize&&actualSwapDesc.BufferDesc.Format==DXGI_FORMAT_B8G8R8A8_UNORM&&work->width==g_b232OutputWidth&&work->height==g_b232OutputHeight?1:0)<<", scaling=0, private_proxy_equals_input=1, private_swapchain_pointer="<<(uintptr_t)swap.Get()<<", hwnd="<<(uintptr_t)window<<", actual_buffer_count="<<actualSwapDesc.BufferCount<<", requested_buffer_count=3, get_desc_hr="<<(long)actualSwapDescHr<<"\n";out.flush();
    auto disabledResult=xefgSwapChainSetEnabled(context,0);
    out<<"B147: stage=XeFGSetEnabled, enabled=0, result="<<(int)disabledResult<<"\n";out.flush();
    xefg_swapchain_d3d12_resource_data_t colorTag{};colorTag.type=XEFG_SWAPCHAIN_RES_HUDLESS_COLOR;
    colorTag.validity=XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;colorTag.resourceBase={0,0};
    colorTag.resourceSize={work->width,work->height};colorTag.pResource=nullptr;
    colorTag.incomingState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    auto tagResult=XEFG_SWAPCHAIN_RESULT_SUCCESS;
    out<<"B174y: stage=XeFGInitialColorTag, present_id=1, action=skipped_until_fresh_native_size_live_input, input_size="<<work->width<<"x"<<work->height<<", proxy_swapchain_size="<<work->width<<"x"<<work->height<<", interpolation_mode=x4, selected_interpolations="<<selectedInterpolations<<", no_scaling=1, api_called=0, result=0\n";out.flush();
    const bool ready=pointerResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&swap&&actualSwapSize&&actualSwapDesc.BufferDesc.Format==DXGI_FORMAT_B8G8R8A8_UNORM&&work->width==g_b232OutputWidth&&work->height==g_b232OutputHeight&&selectedInterpolations==3&&disabledResult==XEFG_SWAPCHAIN_RESULT_SUCCESS;
    if(ready){g_xellContext=xell;g_xefgContext=context;g_xefgSwapChain=swap;g_b312SwapChain2=b312Swap2;g_b312InitialMaxFrameLatency=b312InitialMaxLatency;g_b312FrameLatencyWaitableObject=b312Waitable;g_b314SwapChainMedia=b314SwapMedia;g_b312LiveStartTick.store(GetTickCount64(),std::memory_order_release);g_b312LatencyApplied.store(false,std::memory_order_release);g_xefgWindow=window;g_b174rAllowTearing.store(true,std::memory_order_release);g_xefgReady.store(true,std::memory_order_release);}
    else{swap.Reset();b161_stop_window(window);xefgSwapChainDestroy(context);xellDestroyContext(xell);}
    out<<"B147: PRIVATE_PROXY_INITIALIZATION_COMPLETE ready="<<(ready?1:0)<<", present_id=1, color_tagged=0, tag_api_called=0, depth_tagged=0, motion_vectors_tagged=0, retained="<<(ready?1:0)<<", enabled=0, generated_frames=0, gta_display_change=0\n";out.flush();
    delete work;return ready?0:9;
}
static HRESULT start_b146_worker(unsigned width,unsigned height,DXGI_FORMAT format,const char* path)
{
    if(width!=g_b232OutputWidth||height!=g_b232OutputHeight){log_line("B234: XeFG worker initialization rejected because requested output dimensions do not match configured XeSS output.");return E_INVALIDARG;}
    if(g_xefgWorkerStarted.exchange(true))return S_FALSE;
    auto* work=new B145Work{};work->device=g_device12;work->queue=g_queue12;work->width=width;work->height=height;work->format=format;work->path=path;
    HANDLE thread=CreateThread(nullptr,0,b145_worker,work,0,nullptr);
    if(!thread){g_xefgWorkerStarted=false;delete work;return HRESULT_FROM_WIN32(GetLastError());}
    CloseHandle(thread);{std::ostringstream m;m<<"B234: background XeFG initialization for "<<width<<"x"<<height<<" BGRA8 converted XeSS output started; F9 Present callback released.";log_line(m.str());}
    return S_OK;
}
static ComPtr<ID3D12CommandAllocator> g_b174oSeedAllocator;
static ComPtr<ID3D12GraphicsCommandList> g_b174oSeedList;
static std::vector<ComPtr<ID3D12Resource>> g_b174oSeedBackBuffers;
// B191: FPS HUD is rendered directly into the XeFG swapchain backbuffer before Present.
static ComPtr<ID3D12DescriptorHeap> g_b191RtvHeap;
static std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_b191BackBufferRtvs;
static std::atomic<bool> g_b191HudReady{false};
static std::atomic<unsigned long long> g_b192HudDraws{0};
static std::atomic<unsigned long long> g_b214HudDrawCalls{0},g_b214HudLastTick{0};
static void b192_draw_post_present_hud(HWND target)
{
 if(!target||!IsWindow(target))return;
 HDC dc=GetDC(target);if(!dc)return;
 RECT client{};GetClientRect(target,&client);
 RECT bar{18,18,(std::min)(client.right-1,1550L),(std::min)(client.bottom-1,66L)};
 HBRUSH brush=CreateSolidBrush(RGB(0,0,0));if(brush){FillRect(dc,&bar,brush);DeleteObject(brush);}
 const double sf=g_b191SourceFps100.load(std::memory_order_acquire)/100.0,ff=g_b191FgFps100.load(std::memory_order_acquire)/100.0;
 char runtime[32]{},text[256]{};const ULONGLONG now=GetTickCount64(),elapsed=(g_b174ActivationStart&&now>=g_b174ActivationStart)?now-g_b174ActivationStart:0;b302_format_runtime(elapsed,runtime,sizeof(runtime));const double cpu=g_b304Cpu100.load()/100.0,gpu=g_b304Gpu100.load()/100.0,vu=g_b304VramUsedBytes.load()/(1024.0*1024.0*1024.0),vt=g_b304VramTotalBytes.load()/(1024.0*1024.0*1024.0);sprintf_s(text,"FPS %.1f | FG %.1f | x%.2f | CPU %.0f%% | RT %.0f%% | GPU %.0f%% | VRAM %.1f/%.1fGB | Run %s",sf,ff,sf>0.01?ff/sf:0.0,cpu,g_b305RenderCpu100.load(std::memory_order_acquire)/100.0,gpu,vu,vt,runtime);
 SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
 HFONT font=CreateFontA(24,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Segoe UI");
 HGDIOBJ old=font?SelectObject(dc,font):nullptr;TextOutA(dc,28,28,text,(int)strlen(text));if(old)SelectObject(dc,old);if(font)DeleteObject(font);GdiFlush();ReleaseDC(target,dc);g_b192HudDraws.fetch_add(1,std::memory_order_relaxed);
}
static unsigned char b191_glyph(char c,unsigned row)
{
 static const struct { char c; unsigned char r[7]; } g[]={
  {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},
  {'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},{'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},
  {'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,1,14}},{'.',{0,0,0,0,0,6,6}},{':',{0,6,6,0,6,6,0}},
  {'|',{4,4,4,4,4,4,4}},{'x',{0,0,17,10,4,10,17}},{'O',{14,17,17,17,17,17,14}},{'r',{0,0,22,25,16,16,16}},
  {'i',{4,0,12,4,4,4,14}},{'g',{0,0,15,17,15,1,14}},{'n',{0,0,30,17,17,17,17}},{'a',{0,0,14,1,15,17,15}},
  {'l',{12,4,4,4,4,4,14}},{'F',{31,16,16,30,16,16,16}},{'P',{30,17,17,30,16,16,16}},{'S',{15,16,16,14,1,1,30}},
  {'X',{17,17,10,4,10,17,17}},{'R',{30,17,17,30,20,18,17}},{'C',{14,17,16,16,16,17,14}},{'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},{'A',{14,17,17,31,17,17,17}},{'M',{17,27,21,21,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'%',{25,26,2,4,8,11,19}},{'/',{1,2,2,4,8,8,16}},{'u',{0,0,17,17,17,19,13}},{'e',{0,0,14,17,31,16,14}},{'G',{14,17,16,23,17,17,15}},{' ',{0,0,0,0,0,0,0}}
 };
 if(row>=7)return 0;for(const auto&v:g)if(v.c==c)return v.r[row];return 0;
}
static void b191_draw_fps_hud(ID3D12GraphicsCommandList*list,UINT backBufferIndex,UINT width,UINT height)
{
 if(!list||!g_b191HudReady.load(std::memory_order_acquire)||backBufferIndex>=g_b191BackBufferRtvs.size())return;
 const auto rtv=g_b191BackBufferRtvs[backBufferIndex];
 const float black[4]{0.0f,0.0f,0.0f,1.0f},white[4]{1.0f,1.0f,1.0f,1.0f};
 D3D12_RECT bg{18,18,(LONG)(width>18?(std::min)(width-1u,1550u):width),(LONG)(height>18?(std::min)(height-1u,62u):height)};
 if(bg.right>bg.left&&bg.bottom>bg.top)list->ClearRenderTargetView(rtv,black,1,&bg);
 const double sf=g_b191SourceFps100.load(std::memory_order_acquire)/100.0,ff=g_b191FgFps100.load(std::memory_order_acquire)/100.0;
 char runtime[32]{},text[256]{};const ULONGLONG now=GetTickCount64(),elapsed=(g_b174ActivationStart&&now>=g_b174ActivationStart)?now-g_b174ActivationStart:0;b302_format_runtime(elapsed,runtime,sizeof(runtime));const double cpu=g_b304Cpu100.load()/100.0,gpu=g_b304Gpu100.load()/100.0,vu=g_b304VramUsedBytes.load()/(1024.0*1024.0*1024.0),vt=g_b304VramTotalBytes.load()/(1024.0*1024.0*1024.0);sprintf_s(text,"FPS %.1f | FG %.1f | x%.2f | CPU %.0f%% | RT %.0f%% | GPU %.0f%% | VRAM %.1f/%.1fGB | Run %s",sf,ff,sf>0.01?ff/sf:0.0,cpu,g_b305RenderCpu100.load(std::memory_order_acquire)/100.0,gpu,vu,vt,runtime);
 constexpr LONG scale=2,advance=12;LONG x0=28,y0=29;std::vector<D3D12_RECT> rects;rects.reserve(1024);
 for(unsigned ci=0;text[ci]&&ci<120;++ci){LONG bx=x0+(LONG)ci*advance;for(unsigned row=0;row<7;++row){unsigned bits=b191_glyph(text[ci],row);for(unsigned col=0;col<5;++col)if(bits&(1u<<(4-col))){LONG x=bx+(LONG)col*scale,y=y0+(LONG)row*scale;rects.push_back({x,y,x+scale,y+scale});}}}
 if(!rects.empty())list->ClearRenderTargetView(rtv,white,(UINT)rects.size(),rects.data());
}

// B207: burn the measured HUD into the 1920x1080 XeSS transport color before the
// worker copies/tags it for XeFG. This path is part of the actual presented image and
// therefore does not depend on Win32 overlay composition in exclusive fullscreen.
static void b207_draw_fullscreen_transport_hud(ID3D11DeviceContext*context,ID3D11RenderTargetView*rtv)
{
 if(!context||!rtv||!g_b208ExclusiveFullscreen.load(std::memory_order_acquire))return;
 ComPtr<ID3D11DeviceContext1> context1;if(FAILED(context->QueryInterface(IID_PPV_ARGS(&context1)))||!context1)return;
 const FLOAT black[4]{0.0f,0.0f,0.0f,1.0f},white[4]{1.0f,1.0f,1.0f,1.0f};
 D3D11_RECT bg{18,18,1550,64};context1->ClearView(rtv,black,&bg,1);
 char runtime[32]{},text[256]{};const double sf=g_b191SourceFps100.load(std::memory_order_acquire)/100.0,ff=g_b191FgFps100.load(std::memory_order_acquire)/100.0;const ULONGLONG now=GetTickCount64(),elapsed=(g_b174ActivationStart&&now>=g_b174ActivationStart)?now-g_b174ActivationStart:0;b302_format_runtime(elapsed,runtime,sizeof(runtime));const double cpu=g_b304Cpu100.load()/100.0,gpu=g_b304Gpu100.load()/100.0,vu=g_b304VramUsedBytes.load()/(1024.0*1024.0*1024.0),vt=g_b304VramTotalBytes.load()/(1024.0*1024.0*1024.0);
 if(g_b207FpsValid.load(std::memory_order_acquire))sprintf_s(text,"FPS %.1f | FG %.1f | x%.2f | CPU %.0f%% | RT %.0f%% | GPU %.0f%% | VRAM %.1f/%.1fGB | Run %s",sf,ff,sf>0.01?ff/sf:0.0,cpu,g_b305RenderCpu100.load(std::memory_order_acquire)/100.0,gpu,vu,vt,runtime);else sprintf_s(text,"FPS -- | FG -- | x-- | CPU %.0f%% | RT %.0f%% | GPU %.0f%% | VRAM %.1f/%.1fGB | Run %s",cpu,g_b305RenderCpu100.load(std::memory_order_acquire)/100.0,gpu,vu,vt,runtime);
 constexpr LONG scale=2,advance=12;std::vector<D3D11_RECT> rects;rects.reserve(1024);LONG x0=28,y0=29;
 for(unsigned ci=0;text[ci]&&ci<120;++ci){LONG bx=x0+(LONG)ci*advance;for(unsigned row=0;row<7;++row){unsigned bits=b191_glyph(text[ci],row);for(unsigned col=0;col<5;++col)if(bits&(1u<<(4-col))){LONG x=bx+(LONG)col*scale,y=y0+(LONG)row*scale;rects.push_back({x,y,x+scale,y+scale});}}}
 if(!rects.empty())context1->ClearView(rtv,white,rects.data(),(UINT)rects.size());
}

struct B174InFlightFrame
{
 UINT64 marker{};
 unsigned sourceFrame{};
 uint32_t presentId{};
 unsigned slotIndex{~0u};
 ComPtr<ID3D12Resource> color;
 ComPtr<ID3D12Resource> motion;
 ComPtr<ID3D12Resource> depth;
 ComPtr<ID3D11Resource> wrappedMotion;
 ComPtr<ID3D11Resource> wrappedDepth;
 ComPtr<ID3D11UnorderedAccessView> motionUav;
 ComPtr<ID3D11UnorderedAccessView> depthUav;
};
static std::vector<B174InFlightFrame> g_b174InFlightFrames;
struct B174ReplacedPending
{
 UINT64 nativeFenceValue{};
 unsigned slotIndex{~0u};
 unsigned sourceFrame{};
};
static std::vector<B174ReplacedPending> g_b174ReplacedPending;
static std::atomic<unsigned> g_b174LastAcceptedSource{0},g_b174LastPresentedSource{0};
static double b173g_elapsed_ms(const LARGE_INTEGER&begin,const LARGE_INTEGER&end)
{
 LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
 return frequency.QuadPart?1000.0*(double)(end.QuadPart-begin.QuadPart)/(double)frequency.QuadPart:0.0;
}
static double b310_current_thread_cpu_ms()
{
 FILETIME c{},e{},k{},u{};if(!GetThreadTimes(GetCurrentThread(),&c,&e,&k,&u))return 0.0;
 ULARGE_INTEGER kk{},uu{};kk.LowPart=k.dwLowDateTime;kk.HighPart=k.dwHighDateTime;uu.LowPart=u.dwLowDateTime;uu.HighPart=u.dwHighDateTime;
 return (double)(kk.QuadPart+uu.QuadPart)/10000.0;
}
static HRESULT b173n_prepare_stage_fence(std::ofstream&out)
{
 if(g_b173nStageFence)return S_OK;
 HRESULT hr=g_device12?g_device12->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&g_b173nStageFence)):E_NOINTERFACE;
 if(SUCCEEDED(hr))g_b173nStageFenceValue.store(0,std::memory_order_release);
 out<<"B173n: GPU_STAGE_FENCE_PREPARED created="<<(g_b173nStageFence?1:0)<<", nonblocking=1, hr="<<(long)hr<<"\n";out.flush();return hr;
}
static HRESULT b173n_signal_stage(UINT64&value)
{
 if(!g_queue12||!g_b173nStageFence)return E_NOINTERFACE;
 value=g_b173nStageFenceValue.fetch_add(1,std::memory_order_acq_rel)+1;
 return g_queue12->Signal(g_b173nStageFence.Get(),value);
}
static HRESULT b174d_wait_for_stage(UINT64 value,DWORD timeoutMs,double&waitMs,UINT64&completed)
{
 LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);completed=g_b173nStageFence?g_b173nStageFence->GetCompletedValue():0;
 if(!g_b173nStageFence||!value)return E_INVALIDARG;
 HRESULT hr=S_OK;
 if(completed<value){
  HANDLE eventHandle=CreateEventA(nullptr,FALSE,FALSE,nullptr);if(!eventHandle)return HRESULT_FROM_WIN32(GetLastError());
  hr=g_b173nStageFence->SetEventOnCompletion(value,eventHandle);
  if(SUCCEEDED(hr)){DWORD waitResult=WaitForSingleObject(eventHandle,timeoutMs);if(waitResult==WAIT_TIMEOUT)hr=HRESULT_FROM_WIN32(ERROR_TIMEOUT);else if(waitResult!=WAIT_OBJECT_0)hr=HRESULT_FROM_WIN32(GetLastError());}
  CloseHandle(eventHandle);completed=g_b173nStageFence->GetCompletedValue();
 }
 if(completed==UINT64_MAX&&g_device12){HRESULT removed=g_device12->GetDeviceRemovedReason();if(FAILED(removed))hr=removed;}
 QueryPerformanceCounter(&end);waitMs=b173g_elapsed_ms(begin,end);return SUCCEEDED(hr)&&completed>=value?S_OK:FAILED(hr)?hr:E_FAIL;
}
static bool b240_log_sample(unsigned long long frameId);
static void b174_retire_completed_frames(std::ofstream&out)
{
 const UINT64 completed=g_b173nStageFence?g_b173nStageFence->GetCompletedValue():0;
 for(auto it=g_b174InFlightFrames.begin();it!=g_b174InFlightFrames.end();){
  if(it->marker&&it->marker<=completed){if(it->slotIndex<kB173iColorSlotCount){g_b174ColorSlotState[it->slotIndex].store(0,std::memory_order_release);g_b174DepthSlotState[it->slotIndex].store(0,std::memory_order_release);}if(b240_log_sample(it->sourceFrame))out<<"B246: POST_PRESENT_FENCE_RETIREMENT source_frame_id="<<it->sourceFrame<<", present_id="<<it->presentId<<", marker="<<it->marker<<", completed_value="<<completed<<", slot="<<it->slotIndex<<", resources_released=1, sampled_fast_path=1\n";it=g_b174InFlightFrames.erase(it);}
  else ++it;
 }
 const UINT64 nativeCompleted=g_b173fSharedFence12?g_b173fSharedFence12->GetCompletedValue():0;
 AcquireSRWLockExclusive(&g_b173hJobLock);
 for(auto it=g_b174ReplacedPending.begin();it!=g_b174ReplacedPending.end();){
  if(it->nativeFenceValue&&it->nativeFenceValue<=nativeCompleted){if(it->slotIndex<kB173iColorSlotCount){g_b174ColorSlotState[it->slotIndex].store(0,std::memory_order_release);g_b174DepthSlotState[it->slotIndex].store(0,std::memory_order_release);}if(b240_log_sample(it->sourceFrame))out<<"B246: REPLACED_PENDING_RETIREMENT source_frame_id="<<it->sourceFrame<<", native_fence="<<it->nativeFenceValue<<", completed_value="<<nativeCompleted<<", slot="<<it->slotIndex<<", resources_released=1, sampled_fast_path=1\n";it=g_b174ReplacedPending.erase(it);}
  else ++it;
 }
 ReleaseSRWLockExclusive(&g_b173hJobLock);
}
static HRESULT b174c_seed_all_proxy_backbuffers(ID3D12Resource*source,UINT64&marker,std::ofstream&out)
{
 if(!source||!g_xefgSwapChain||!g_device12||!g_queue12)return E_INVALIDARG;
 DXGI_SWAP_CHAIN_DESC desc{};HRESULT hr=g_xefgSwapChain->GetDesc(&desc);if(FAILED(hr)||!desc.BufferCount)return FAILED(hr)?hr:E_FAIL;
 if(!g_b174oSeedAllocator||!g_b174oSeedList||g_b174oSeedBackBuffers.size()!=desc.BufferCount)return E_FAIL;
 auto sourceDesc=source->GetDesc();hr=g_b174oSeedAllocator->Reset();if(SUCCEEDED(hr))hr=g_b174oSeedList->Reset(g_b174oSeedAllocator.Get(),nullptr);if(FAILED(hr))return hr;auto*list=g_b174oSeedList.Get();
 bool allSeeded=true;
 D3D12_RESOURCE_BARRIER sourceBefore{};sourceBefore.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;sourceBefore.Transition.pResource=source;sourceBefore.Transition.StateBefore=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;sourceBefore.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;sourceBefore.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
 list->ResourceBarrier(1,&sourceBefore);
 for(UINT index=0;index<desc.BufferCount;++index){
  auto*buffer=g_b174oSeedBackBuffers[index].Get();bool descMatch=false;if(buffer){auto bd=buffer->GetDesc();descMatch=bd.Width==sourceDesc.Width&&bd.Height==sourceDesc.Height&&bd.Format==sourceDesc.Format&&bd.SampleDesc.Count==sourceDesc.SampleDesc.Count;}
  if(!buffer||!descMatch){allSeeded=false;out<<"B174c: PROXY_BACKBUFFER_SEEDED index="<<index<<", obtained="<<(buffer?1:0)<<", desc_match="<<(descMatch?1:0)<<", copy_queued=0, valid_non_black=0, hr="<<(long)E_FAIL<<"\n";continue;}
  D3D12_RESOURCE_BARRIER before{};before.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;before.Transition.pResource=buffer;before.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;before.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;before.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  list->ResourceBarrier(1,&before);list->CopyResource(buffer,source);
  std::swap(before.Transition.StateBefore,before.Transition.StateAfter);list->ResourceBarrier(1,&before);
  out<<"B174c: PROXY_BACKBUFFER_SEEDED index="<<index<<", obtained=1, desc_match=1, copy_queued=1, valid_non_black=1, hr=0\n";
 }
 std::swap(sourceBefore.Transition.StateBefore,sourceBefore.Transition.StateAfter);list->ResourceBarrier(1,&sourceBefore);
 hr=allSeeded?list->Close():E_FAIL;if(FAILED(hr))return hr;ID3D12CommandList*lists[]{list};g_queue12->ExecuteCommandLists(1,lists);hr=b173n_signal_stage(marker);
 out<<"B174c: PROXY_BACKBUFFER_SEED_COMPLETE buffers="<<desc.BufferCount<<", all_seeded="<<(allSeeded?1:0)<<", marker="<<marker<<", marker_hr="<<(long)hr<<", black_or_uninitialized_possible="<<((allSeeded&&SUCCEEDED(hr))?0:1)<<"\n";out.flush();
 return allSeeded?hr:E_FAIL;
}
static HRESULT b171_copy_color_to_private_backbuffer(ID3D12Resource*source,unsigned slotIndex,const char*label,std::ofstream&out)
{
 LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
 if(!source||!g_xefgSwapChain||!g_device12||!g_queue12||slotIndex>=kB173jDepthMotionSlotCount)return E_INVALIDARG;
 auto&slot=g_b173jDepthMotionSlots[slotIndex];if(!slot.outputsReady||!slot.prefillAllocator||!slot.prefillList)return E_FAIL;
 UINT index=g_xefgSwapChain->GetCurrentBackBufferIndex();ComPtr<ID3D12Resource>backBuffer=index<g_b174oSeedBackBuffers.size()?g_b174oSeedBackBuffers[index]:nullptr;HRESULT hr=backBuffer?S_OK:E_FAIL;
 if(FAILED(hr)||!backBuffer){out<<"B172: stage=GetPrewarmedPrivateBackBuffer, frame="<<label<<", index="<<index<<", hr="<<(long)hr<<"\n";out.flush();return FAILED(hr)?hr:E_FAIL;}
 auto sd=source->GetDesc(),bd=backBuffer->GetDesc();bool descMatch=sd.Width==bd.Width&&sd.Height==bd.Height&&sd.Format==bd.Format&&sd.SampleDesc.Count==bd.SampleDesc.Count;
 if(!descMatch)hr=E_FAIL;else hr=slot.prefillAllocator->Reset();
 if(SUCCEEDED(hr))hr=slot.prefillList->Reset(slot.prefillAllocator.Get(),nullptr);auto*list=slot.prefillList.Get();
 if(FAILED(hr)){out<<"B172: stage=CreatePrivateBackBufferCopyObjects, frame="<<label<<", desc_match="<<(descMatch?1:0)<<", hr="<<(long)hr<<"\n";out.flush();return hr;}
 list->SetName(L"GTAXeSS B173l Private Backbuffer Prefill");
 D3D12_RESOURCE_BARRIER before[2]{};before[0].Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;before[0].Transition.pResource=source;before[0].Transition.StateBefore=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;before[0].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;before[0].Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
 before[1].Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;before[1].Transition.pResource=backBuffer.Get();before[1].Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;before[1].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;before[1].Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
 list->ResourceBarrier(2,before);list->CopyResource(backBuffer.Get(),source);
 D3D12_RESOURCE_BARRIER toRender{};toRender.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;toRender.Transition.pResource=backBuffer.Get();toRender.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;toRender.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;toRender.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;list->ResourceBarrier(1,&toRender);
 // B226: restore the measured FPS HUD only after the B225 same-GTA-thread proxy
 // architecture proved that the cursor stays hidden while real XeFG x4 remains live.
 // Keep all B225 window/input/lifecycle behavior unchanged.
 b191_draw_fps_hud(list,index,(UINT)bd.Width,bd.Height);
 g_b214HudDrawCalls.fetch_add(1,std::memory_order_relaxed);
 g_b214HudLastTick.store(GetTickCount64(),std::memory_order_release);
 D3D12_RESOURCE_BARRIER after[2]{};
 after[0]=before[0];after[0].Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;after[0].Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
 after[1]=toRender;after[1].Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;after[1].Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;
 list->ResourceBarrier(2,after);
 hr=list->Close();if(FAILED(hr))return hr;ID3D12CommandList*lists[]{list};g_queue12->ExecuteCommandLists(1,lists);
 QueryPerformanceCounter(&end);
 const auto b254HudLogOrdinal=g_b214HudDrawCalls.load(std::memory_order_relaxed);if(b254HudLogOrdinal<=8||(b254HudLogOrdinal%120)==0){out<<"B234: PRIVATE_BACKBUFFER_FPS_TEXT_DRAWN frame="<<label<<", slot="<<slotIndex<<", index="<<index<<", size="<<(UINT)bd.Width<<"x"<<bd.Height<<", direct_fps_hud=1, fps_text_rendering=1, ui_mode=PRIVATE_BACKBUFFER, xefg_ui_composition=0, b225_proxy_architecture_frozen=1, cpu_ms="<<b173g_elapsed_ms(begin,end)<<", hr=0\n";out.flush();}return S_OK;
}
static bool g_b172Fresh=false;static unsigned g_b172GtaFrame=0;static LARGE_INTEGER g_b172Now{},g_b172PreviousTime{};
// B245: measure cadence from adjacent real GTA source Presents, not from the previous frame consumed by XeFG.
// This breaks the B244 positive-feedback loop where backlog/drop gaps inflated frameRenderTime and made XeFG Present block even longer.
static std::atomic<LONGLONG> g_b245PreviousObservedPresentQpc{0};
static std::atomic<unsigned long long> g_b245ObservedFrameIntervalUs{16667};
// Retain tagged resources until their post-Present fence markers complete.
static std::vector<ComPtr<ID3D12Resource>> g_b172Retained;
extern "C" __declspec(dllexport) BOOL WINAPI B174IsTestActive(){return g_b174TestActive.load(std::memory_order_acquire)?TRUE:FALSE;}
extern "C" __declspec(dllexport) void WINAPI B174RequestStop(unsigned reason){if(g_b174TestActive.load(std::memory_order_acquire))b174_request_stop(reason?reason:4u);}
extern "C" __declspec(dllexport) HRESULT WINAPI B175uBindGtaWindow(HWND window,const char*path)
{
 if(!window||!path||!IsWindow(window))return E_INVALIDARG;
 RECT client{};POINT origin{};
 const BOOL clientOk=GetClientRect(window,&client);
 const BOOL originOk=clientOk&&ClientToScreen(window,&origin);
 const bool ready=originOk&&client.right>client.left&&client.bottom>client.top;
 if(ready){OffsetRect(&client,origin.x,origin.y);AcquireSRWLockExclusive(&g_b174WindowLock);g_b174GtaWindow=window;g_b174GtaClientRect=client;ReleaseSRWLockExclusive(&g_b174WindowLock);}
 std::ofstream out(path,std::ios::app);out<<"B175u: EARLY_GTA_WINDOW_BIND hwnd=1, valid="<<(IsWindow(window)?1:0)<<", rect={"<<client.left<<","<<client.top<<","<<client.right<<","<<client.bottom<<"}, ready="<<(ready?1:0)<<", hr="<<(ready?0:(long)E_FAIL)<<"\n";out.flush();
 return ready?S_OK:E_FAIL;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B175ActivatePrewarmedPipeline()
{
 if(g_b175PipelineFailed.load(std::memory_order_acquire)||!g_b175bXessReady.load(std::memory_order_acquire))return E_FAIL;
 if(!g_b175PipelineReady.load(std::memory_order_acquire))return E_PENDING;
 if(g_b174TestActive.load(std::memory_order_acquire)||g_b175ActivationRequested.exchange(true,std::memory_order_acq_rel))return S_FALSE;
 AcquireSRWLockExclusive(&g_b175MotionHistoryLock);g_b175bHaveXessPreviousViewProjection=false;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);g_b175bXessReset.store(true,std::memory_order_release);g_b174ActivationStart=GetTickCount64();g_b302LastRuntimeLoggedMinute.store(0,std::memory_order_release);b302_runtime_log_event("XeSS/XeFG enabled",0);return S_OK;
}
extern "C" __declspec(dllexport) void WINAPI B214LogSymptomSnapshot(const char* tag)
{
 std::ofstream out(log_path(),std::ios::app);const ULONGLONG now=GetTickCount64(),hudTick=g_b214HudLastTick.load(std::memory_order_acquire);
 CURSORINFO ci{};ci.cbSize=sizeof(ci);const BOOL cursorQuery=GetCursorInfo(&ci);HWND fg=GetForegroundWindow();DWORD gtaPid=0,fgPid=0;if(g_b174GtaWindow)GetWindowThreadProcessId(g_b174GtaWindow,&gtaPid);if(fg)GetWindowThreadProcessId(fg,&fgPid);
 out<<"B216: SYMPTOM_SNAPSHOT tag="<<(tag?tag:"unknown")
    <<", live_ready="<<(g_b171LiveReady.load()?1:0)<<", proxy_visible="<<(g_b171WindowVisible.load()?1:0)
    <<", pipeline_ready="<<(g_b175PipelineReady.load()?1:0)<<", pipeline_failed="<<(g_b175PipelineFailed.load()?1:0)
    <<", surface_suspended="<<(g_b174SurfaceSuspended.load()?1:0)<<", resume_needs_fresh="<<(g_b174ResumeNeedsFresh.load()?1:0)
    <<", proxy_revealed="<<(g_b174ProxyRevealed.load()?1:0)<<", foreground_hwnd="<<(uintptr_t)fg<<", gta_hwnd="<<(uintptr_t)g_b174GtaWindow
    <<", foreground_same_gta="<<(fg==g_b174GtaWindow?1:0)<<", foreground_same_process="<<((gtaPid&&fgPid&&gtaPid==fgPid)?1:0)
    <<", cursor_query_ok="<<(cursorQuery?1:0)<<", cursor_visible="<<((cursorQuery&&(ci.flags&CURSOR_SHOWING))?1:0)
    <<", fps_valid="<<(g_b207FpsValid.load()?1:0)<<", source_fps="<<(g_b191SourceFps100.load()/100.0)<<", xefg_fps="<<(g_b191FgFps100.load()/100.0)
    <<", hud_draw_calls="<<g_b214HudDrawCalls.load()<<", hud_last_age_ms="<<(hudTick&&now>=hudTick?now-hudTick:0)
    <<", accepted="<<g_b174Accepted.load()<<", generated="<<g_b174GeneratedFrames.load()<<", displayed="<<g_b174OutputFrames.load()<<", pending="<<g_b174PendingCount.load()
    <<", window_generation="<<g_b174WindowGeneration.load()<<"\n";out.flush();
}
extern "C" __declspec(dllexport) unsigned WINAPI B175PipelineState(){return g_b175PipelineFailed.load()?2u:g_b175PipelineReady.load()?1u:0u;}
extern "C" __declspec(dllexport) void WINAPI B175SourceSwapChanged(BOOL sameDevice)
{
 g_b175ActivationRequested.store(false,std::memory_order_release);
 if(sameDevice){
  // B247 frozen lifecycle: Alt-Tab/minimize/same-device swapchain recreation is a suspension, never a user toggle-off.
  // Keep the proven XeSS/XeFG contexts and live session alive; hide stale proxy output until fresh inputs are rediscovered.
  b174_hide_proxy("same_device_swapchain_suspended");
  g_b174SurfaceSuspended.store(true,std::memory_order_release);
  g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
  g_b175bXessReset.store(true,std::memory_order_release);
 }else{
  if(g_b174TestActive.load(std::memory_order_acquire))b174_request_stop(4u);else b174_hide_proxy("source_device_replaced");
  g_b175PipelineReady.store(false,std::memory_order_release);g_b175PipelineFailed.store(true,std::memory_order_release);
 }
 log_line("B174y: SOURCE_SWAPCHAIN_REPLACED same_device="+std::to_string(sameDevice?1:0)+", generation_temporarily_suspended=1, same_device_rediscovery_supported=1, persistent_user_latch_owned_by_asi=1, reusable_pipeline="+std::to_string(sameDevice?1:0));
}

// Build identity: B395 FRESH-FRAME STALL RECOVERY + NONBLOCKING XEFG STATUS + GHOSTING UNCHANGED 2026-09-28
extern "C" __declspec(dllexport) BOOL WINAPI B276ConsumePacingRediscoveryRequest()
{
 return g_b276RediscoveryRequested.exchange(false,std::memory_order_acq_rel)?TRUE:FALSE;
}

extern "C" __declspec(dllexport) BOOL WINAPI B388ConsumeStallRecoveryRequest()
{
 return g_b388StallRecoveryRequested.exchange(false,std::memory_order_acq_rel)?TRUE:FALSE;
}

extern "C" __declspec(dllexport) void WINAPI B251ResumeAfterRediscovery()
{
 const ULONGLONG now=GetTickCount64();
 const bool wasSuspended=g_b174SurfaceSuspended.exchange(false,std::memory_order_acq_rel);
 b174_hide_proxy("b251_rediscovery_resume_handshake");
 B175ResetFastFreshness();
 AcquireSRWLockExclusive(&g_b175MotionHistoryLock);g_b175bHaveXessPreviousViewProjection=false;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 g_b175bXessReset.store(true,std::memory_order_release);
 g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
 g_b174StartupBackbuffersSeeded.store(false,std::memory_order_release);
 g_b174HiddenCompleted.store(0,std::memory_order_release);
 g_b174RevealFenceMarker.store(0,std::memory_order_release);
 g_b174StartupDeadline.store(now+1500,std::memory_order_release);
 b174_rebaseline_rate_window(now);
 log_line("B252: HELPER_REDISCOVERY_RESUME was_surface_suspended="+std::to_string(wasSuspended?1:0)+", surface_suspended=0, reset_history_rearmed=1, hidden_warmup_reset=1, proxy_hidden=1, persistent_session="+std::to_string(g_b174SessionEverRevealed.load()?1:0));
}

extern "C" __declspec(dllexport) void WINAPI B175ShutdownPersistentPipeline()
{
 g_b174ShutdownRequested.store(true,std::memory_order_release);g_b175ActivationRequested.store(false,std::memory_order_release);b174_request_stop(3);if(g_b304GpuQuery){PdhCloseQuery(g_b304GpuQuery);g_b304GpuQuery=nullptr;g_b304GpuCounter=nullptr;g_b304GpuQueryPrimed=false;}
 log_line("B175n: SHUTDOWN_TOTALS accepted="+std::to_string(g_b174TotalAccepted.load())+", successful_xess="+std::to_string(g_b175bXessFrames.load())+", generated="+std::to_string(g_b174TotalGenerated.load())+", displayed="+std::to_string(g_b174TotalDisplayed.load())+", dropped="+std::to_string(g_b174TotalDropped.load())+", replaced="+std::to_string(g_b174TotalReplaced.load())+", xess_errors="+std::to_string(g_b175bXessErrors.load())+", xefg_errors="+std::to_string(g_b174PresentErrors.load()+g_b174TagErrors.load()+g_b174StatusErrors.load())+", device_errors="+std::to_string(g_b175bDeviceErrors.load())+", toggle_on="+std::to_string(g_b174ToggleOnCount.load())+", toggle_off="+std::to_string(g_b174ToggleOffCount.load()));
}
static bool b240_log_sample(unsigned long long frameId);
static void b175_log_disposition(unsigned source,const char*action,const char*reason,unsigned slot)
{
 // B264: classify every terminal source-frame disposition without adding hot-path disk I/O.
 if(action&&strcmp(action,"accepted_current_worker")==0)g_b264AcceptedDisposition.fetch_add(1,std::memory_order_relaxed);
 else if(action&&strncmp(action,"dropped",7)==0){
  if(reason&&strcmp(reason,"frame_resource_ring_owned")==0)g_b264DropResourceOwned.fetch_add(1,std::memory_order_relaxed);
  else if(reason&&strcmp(reason,"ordered_queue_backpressure")==0)g_b264DropQueueFull.fetch_add(1,std::memory_order_relaxed);
  else if(reason&&strcmp(reason,"focus_minimize_or_geometry")==0)g_b264DropSurfaceSuspended.fetch_add(1,std::memory_order_relaxed);
  else if(reason&&strcmp(reason,"fast_snapshot_pending")==0)g_b264DropFastSnapshot.fetch_add(1,std::memory_order_relaxed);
  else if(reason&&strcmp(reason,"capture_slot_race")==0)g_b264DropSlotRace.fetch_add(1,std::memory_order_relaxed);
  else if(reason&&strcmp(reason,"source_id_not_newer")==0)g_b264DropNonMonotonic.fetch_add(1,std::memory_order_relaxed);
  else if(reason&&strcmp(reason,"worker_rejected_pre_suspend_mailbox")==0)g_b264DropWorkerSuspend.fetch_add(1,std::memory_order_relaxed);
  else if(reason&&strcmp(reason,"capture_stopped_before_worker_accept")==0)g_b264DropToggleDrain.fetch_add(1,std::memory_order_relaxed);
  else if(action&&strcmp(action,"dropped_capture_failed")==0)g_b264DropCaptureFailed.fetch_add(1,std::memory_order_relaxed);
  else if(action&&strcmp(action,"dropped_fast_capture")==0)g_b264DropFastOther.fetch_add(1,std::memory_order_relaxed);
  else g_b264DropOther.fetch_add(1,std::memory_order_relaxed);
 }
 bool duplicate=false;AcquireSRWLockExclusive(&g_b175DecisionLock);const unsigned retained=(std::min)(g_b175DecisionCount,kB175DecisionCapacity);for(unsigned i=0;i<retained;++i)if(g_b175DecisionIds[i]==source){duplicate=true;break;}if(!duplicate){g_b175DecisionIds[g_b175DecisionCursor]=source;g_b175DecisionCursor=(g_b175DecisionCursor+1)%kB175DecisionCapacity;if(g_b175DecisionCount<kB175DecisionCapacity)++g_b175DecisionCount;}ReleaseSRWLockExclusive(&g_b175DecisionLock);
 if(duplicate){g_b175InvariantFailures.fetch_add(1,std::memory_order_acq_rel);b174_request_stop(4);}
 if(duplicate||b240_log_sample(source))log_line("B175: SOURCE_FRAME_DISPOSITION source_frame_id="+std::to_string(source)+", action="+action+", reason="+reason+", slot="+(slot<kB173iColorSlotCount?std::to_string(slot):"none")+", decision_count="+std::to_string(duplicate?2:1)+", duplicate_source_id="+std::to_string(duplicate?1:0)+", invariant_failures="+std::to_string(g_b175InvariantFailures.load()));
}
static bool b174c_has_free_capture_slot()
{
 for(unsigned i=0;i<kB173iColorSlotCount;++i)if(g_b174ColorSlotState[i].load(std::memory_order_acquire)==0&&g_b174DepthSlotState[i].load(std::memory_order_acquire)==0)return true;
 return false;
}
static bool b174c_reserve_capture_slot(unsigned&slot)
{
 for(unsigned i=0;i<kB173iColorSlotCount;++i){
  unsigned colorExpected=0;if(!g_b174ColorSlotState[i].compare_exchange_strong(colorExpected,1,std::memory_order_acq_rel))continue;
  unsigned depthExpected=0;if(g_b174DepthSlotState[i].compare_exchange_strong(depthExpected,1,std::memory_order_acq_rel)){slot=i;return true;}
  g_b174ColorSlotState[i].store(0,std::memory_order_release);
 }
 return false;
}
static void b174c_release_capture_slot(unsigned slot)
{
 if(slot<kB173iColorSlotCount){g_b174ColorSlotState[slot].store(0,std::memory_order_release);g_b174DepthSlotState[slot].store(0,std::memory_order_release);}
}
extern "C" __declspec(dllexport) void WINAPI B175DropNoConstantSnapshot(unsigned gtaFrame)
{
 g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_no_constant_snapshot","fast_snapshot_pending",~0u);
}
extern "C" __declspec(dllexport) void WINAPI B175DropFastCapture(unsigned gtaFrame,const char*reason)
{
 g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_fast_capture",reason?reason:"unspecified",~0u);
}
extern "C" __declspec(dllexport) void WINAPI B172AbortFreshFrame(unsigned gtaFrame,const char*reason,HRESULT hr)
{
 if(!g_b171LiveBusy.load(std::memory_order_acquire)||g_b172GtaFrame!=gtaFrame)return;
 const unsigned slot=g_b174CaptureSlot;g_b172Fresh=false;g_b174CaptureSlot=~0u;b174c_release_capture_slot(slot);g_b171LiveBusy.store(false,std::memory_order_release);g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);
 g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_capture_failed",reason?reason:"unspecified",slot);
 log_line("B175: FRESH_FRAME_ABORT source_frame_id="+std::to_string(gtaFrame)+", reason="+(reason?reason:"unspecified")+", hr="+std::to_string((long)hr)+", busy_released=1, slot_released=1");
}
extern "C" __declspec(dllexport) BOOL WINAPI B172WantsCapture(unsigned gtaFrame){
 if(!g_b171LiveReady.load(std::memory_order_acquire)){B175DropFastCapture(gtaFrame,"live_pipeline_not_ready");return FALSE;}
 if(!b174_poll_source_window("present")){g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_surface_suspended","focus_minimize_or_geometry",~0u);return FALSE;}
 g_b174oObservedSourcePresents.fetch_add(1,std::memory_order_acq_rel);
 LARGE_INTEGER b245Now{},b245Freq{};QueryPerformanceCounter(&b245Now);QueryPerformanceFrequency(&b245Freq);
 const LONGLONG b245Prev=g_b245PreviousObservedPresentQpc.exchange(b245Now.QuadPart,std::memory_order_acq_rel);
 if(b245Prev>0&&b245Freq.QuadPart>0){double b245Us=1000000.0*(double)(b245Now.QuadPart-b245Prev)/(double)b245Freq.QuadPart;if(b245Us>=1000.0&&b245Us<=50000.0)g_b245ObservedFrameIntervalUs.store((unsigned long long)(b245Us+0.5),std::memory_order_release);}
 b207_update_realtime_fps();unsigned noFirst=0;g_b174oFirstSourcePresent.compare_exchange_strong(noFirst,gtaFrame,std::memory_order_acq_rel);g_b174oLastSourcePresent.store(gtaFrame,std::memory_order_release);
 if(g_b174ProxyRevealed.load(std::memory_order_acquire)&&!g_b171WindowVisible.load(std::memory_order_acquire)){g_b174VisibilityLost.store(true,std::memory_order_release);b174_hide_proxy("proxy_visibility_lost");return FALSE;}
 if(g_b171LiveBusy.load(std::memory_order_acquire)||!b174c_has_free_capture_slot()){g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_no_safe_slot","frame_resource_ring_owned",~0u);return FALSE;}
 return TRUE;
}
extern "C" BOOL WINAPI B172FreshFrameOwned(){return g_b171LiveBusy.load(std::memory_order_acquire);}
extern "C" __declspec(dllexport) BOOL WINAPI B172BeginFreshFrame(unsigned gtaFrame){
 if(!g_b171LiveReady.load(std::memory_order_acquire)){B175DropFastCapture(gtaFrame,"begin_after_pipeline_stop");return FALSE;}
 if(g_b171LiveBusy.exchange(true,std::memory_order_acq_rel)){B175DropFastCapture(gtaFrame,"begin_busy_race");return FALSE;}
 if(!g_b171LiveReady.load(std::memory_order_acquire)){g_b171LiveBusy.store(false,std::memory_order_release);B175DropFastCapture(gtaFrame,"begin_after_stop_race");return FALSE;}
 unsigned slot=~0u;if(!b174c_reserve_capture_slot(slot)){g_b171LiveBusy.store(false,std::memory_order_release);g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_no_safe_slot","capture_slot_race",~0u);return FALSE;}
 g_b174CaptureSlot=slot;g_b174LastCaptureStart.store(GetTickCount64(),std::memory_order_release);g_b172Fresh=false;g_b172GtaFrame=gtaFrame;QueryPerformanceCounter(&g_b172Now);return TRUE;
}
static HRESULT b172upload(const void*p,UINT pitch,UINT width,UINT height,DXGI_FORMAT format,ComPtr<ID3D12Resource>&dst){
 if(!p||pitch<width*4||!width||!height)return E_INVALIDARG;
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
 D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width;d.Height=height;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
 ComPtr<ID3D12Resource>resource;HRESULT hr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&resource));if(FAILED(hr))return hr;
 D3D11_RESOURCE_FLAGS flags{};ComPtr<ID3D11Resource>wrapped;hr=g_on12->CreateWrappedResource(resource.Get(),&flags,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&wrapped));if(FAILED(hr))return hr;
 ID3D11Resource*r[]{wrapped.Get()};g_on12->AcquireWrappedResources(r,1);g_on12Context->UpdateSubresource(wrapped.Get(),0,nullptr,p,pitch,0);g_on12->ReleaseWrappedResources(r,1);g_on12Context->Flush();dst=resource;return S_OK;
}
struct B173GpuConstants{float previousViewProjection[16];float currentInverseViewProjection[16];UINT width,height;float padding[2];};
extern "C" __declspec(dllexport) HRESULT WINAPI B175CreateXessContextOnRenderThread(ID3D11Device*device,const char*path)
{
 if(!device||!path)return E_INVALIDARG;
 unsigned expected=0;
 if(!g_b175cXessContextState.compare_exchange_strong(expected,1,std::memory_order_acq_rel))
  return expected==2&&g_b175bXess?S_FALSE:E_FAIL;
 std::ofstream out(path,std::ios::app);if(!out){g_b175cXessContextState.store(3,std::memory_order_release);return E_FAIL;}
 LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
 out<<"B175n: XESS_CREATE_ENTER owner=render_thread, thread_id="<<GetCurrentThreadId()<<", f9_callback_blocked=0\n";out.flush();
 xess_context_handle_t context{};const xess_result_t result=xessD3D11CreateContext(device,&context);
 QueryPerformanceCounter(&end);const double elapsedMs=frequency.QuadPart?1000.0*(double)(end.QuadPart-begin.QuadPart)/(double)frequency.QuadPart:0.0;
 const bool ready=result==XESS_RESULT_SUCCESS&&context;
 out<<"B175n: XESS_CREATE_RETURN owner=render_thread, thread_id="<<GetCurrentThreadId()<<", duration_ms="<<elapsedMs<<", result="<<(int)result<<", ready="<<(ready?1:0)<<"\n";out.flush();
 if(!ready){if(context)xessDestroyContext(context);g_b175cXessContextState.store(3,std::memory_order_release);return E_FAIL;}
 g_b175bXess=context;g_b175cXessContextState.store(2,std::memory_order_release);return S_OK;
}
static HRESULT b175b_prepare_xess(ID3D11Device*device,DXGI_FORMAT colorFormat,UINT sourceWidth,UINT sourceHeight,std::ofstream&out)
{
 if(!device||sourceWidth<640||sourceHeight<360)return E_INVALIDARG;
 if(g_b175bXessReady.load(std::memory_order_acquire))return S_FALSE;
 if(colorFormat!=DXGI_FORMAT_R8G8B8A8_UNORM&&colorFormat!=DXGI_FORMAT_B8G8R8A8_UNORM&&colorFormat!=DXGI_FORMAT_R16G16B16A16_FLOAT){
  out<<"B175n: XESS_PREPARE_FAILED reason=unsupported_linear_color_format format="<<(unsigned)colorFormat<<"\n";out.flush();return E_NOTIMPL;
 }
 if(g_b175cXessContextState.load(std::memory_order_acquire)!=2||!g_b175bXess){out<<"B175n: XESS_PREPARE_FAILED reason=render_thread_context_not_ready, fail_closed=1\n";out.flush();return E_FAIL;}
 xess_context_handle_t context=g_b175bXess;xess_result_t result=XESS_RESULT_SUCCESS;
 b232_load_config(out);
 out<<"B234: CONFIG_VALUE_PROOF raw_resolution_value="<<g_b232ResolutionMode
    <<", raw_quality_value="<<g_b232QualityMode
    <<", expected_build=B234, proof_before_xess_init=1\n";out.flush();
 const xess_2d_t output{g_b232OutputWidth,g_b232OutputHeight};
 const double scaleX=(double)g_b232OutputWidth/(double)sourceWidth,scaleY=(double)g_b232OutputHeight/(double)sourceHeight,requiredScale=scaleX>scaleY?scaleX:scaleY;
 xess_quality_settings_t requestedQuality{};bool manualQuality=b232_parse_quality(g_b232QualityMode,requestedQuality);
 const xess_quality_settings_t candidates[]{XESS_QUALITY_SETTING_BALANCED,XESS_QUALITY_SETTING_PERFORMANCE,XESS_QUALITY_SETTING_ULTRA_PERFORMANCE,XESS_QUALITY_SETTING_QUALITY,XESS_QUALITY_SETTING_ULTRA_QUALITY,XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS,XESS_QUALITY_SETTING_AA};
 bool found=false;xess_2d_t selectedInput{};
 if(manualQuality){const double maxScale=b232_max_scale(requestedQuality);xess_2d_t queried{};const auto query=xessGetInputResolution(context,&output,requestedQuality,&queried);const bool dynamicSupported=requiredScale<=maxScale+0.001&&requiredScale>=1.0;out<<"B234: XESS_QUALITY_REQUEST mode="<<g_b232QualityMode<<", enum="<<(int)requestedQuality<<", output="<<output.x<<"x"<<output.y<<", source="<<sourceWidth<<"x"<<sourceHeight<<", required_scale="<<requiredScale<<", max_scale="<<maxScale<<", optimal_input="<<queried.x<<"x"<<queried.y<<", query_result="<<(int)query<<", supported="<<(query==XESS_RESULT_SUCCESS&&dynamicSupported?1:0)<<"\n";if(query==XESS_RESULT_SUCCESS&&dynamicSupported){g_b175bXessQuality=requestedQuality;selectedInput=queried;found=true;}}
 else {for(auto quality:candidates){xess_2d_t queried{};const auto query=xessGetInputResolution(context,&output,quality,&queried);const double maxScale=b232_max_scale(quality);const bool supported=query==XESS_RESULT_SUCCESS&&requiredScale<=maxScale+0.001&&requiredScale>=1.0;out<<"B234: XESS_AUTO_CANDIDATE quality="<<b232_quality_name(quality)<<", enum="<<(int)quality<<", output="<<output.x<<"x"<<output.y<<", source="<<sourceWidth<<"x"<<sourceHeight<<", required_scale="<<requiredScale<<", max_scale="<<maxScale<<", optimal_input="<<queried.x<<"x"<<queried.y<<", supported="<<(supported?1:0)<<"\n";if(supported){g_b175bXessQuality=quality;selectedInput=queried;found=true;break;}}}
 if(!found){xessDestroyContext(context);g_b175bXess=nullptr;g_b175cXessContextState.store(3,std::memory_order_release);out<<"B234: XESS_PREPARE_FAILED reason=configured_resolution_quality_not_supported_by_discovered_native_source, output="<<output.x<<"x"<<output.y<<", quality="<<g_b232QualityMode<<", required_scale="<<requiredScale<<", maximum_xess_dynamic_scale=3.0, minimum_4k_source=1280x720, target_real_4k_source=1920x1080, fail_closed=1\n";out.flush();return E_INVALIDARG;}
 xess_d3d11_init_params_t init{};init.outputResolution=output;init.qualitySetting=g_b175bXessQuality;init.initFlags=XESS_INIT_FLAG_NONE;
 out<<"B234: XESS_INIT_ENTER quality="<<b232_quality_name(g_b175bXessQuality)<<", output="<<output.x<<"x"<<output.y<<", actual_input="<<sourceWidth<<"x"<<sourceHeight<<", optimal_input="<<selectedInput.x<<"x"<<selectedInput.y<<", dynamic_input=1, thread_id="<<GetCurrentThreadId()<<"\n";out.flush();
 result=xessD3D11Init(context,&init);xess_2d_t validated{};const auto validation=result==XESS_RESULT_SUCCESS?xessGetInputResolution(context,&output,g_b175bXessQuality,&validated):result;
 const auto velocityResult=validation==XESS_RESULT_SUCCESS?xessSetVelocityScale(context,1.0f,1.0f):validation;
 const bool ready=result==XESS_RESULT_SUCCESS&&validation==XESS_RESULT_SUCCESS&&velocityResult==XESS_RESULT_SUCCESS;
 out<<"B234: XESS_INITIALIZED quality="<<b232_quality_name(g_b175bXessQuality)<<", enum="<<(int)g_b175bXessQuality<<", output="<<output.x<<"x"<<output.y<<", actual_input="<<sourceWidth<<"x"<<sourceHeight<<", optimal_input="<<validated.x<<"x"<<validated.y<<", required_scale="<<requiredScale<<", init_result="<<(int)result<<", validation_result="<<(int)validation<<", velocity_result="<<(int)velocityResult<<", ready="<<(ready?1:0)<<"\n";out.flush();
 if(!ready){xessDestroyContext(context);g_b175bXess=nullptr;g_b175cXessContextState.store(3,std::memory_order_release);return E_FAIL;}g_b175bXessReady.store(true,std::memory_order_release);return S_OK;
}
static std::atomic<bool> g_b175nXessOnlyStarted{false};
static std::atomic<unsigned> g_b175nXessOnlyState{0};
extern "C" __declspec(dllexport) unsigned WINAPI B175nXessOnlyPreparationState(){return g_b175nXessOnlyState.load(std::memory_order_acquire);}
extern "C" __declspec(dllexport) HRESULT WINAPI B175nStartXessOnlyPreparation(ID3D11Device*device,const char*path)
{
 if(!device||!path)return E_INVALIDARG;
 if(device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED){std::ofstream out(path,std::ios::app);out<<"B175n: XESS_ONLY_PREPARATION_REJECTED reason=native_d3d11_device_is_single_threaded, d3d12_device_created=0, d3d11on12_created=0\n";out.flush();return E_FAIL;}
 if(g_b175nXessOnlyStarted.exchange(true,std::memory_order_acq_rel))return S_FALSE;
 std::ofstream out(path,std::ios::app);
 if(!out){g_b175nXessOnlyState.store(2,std::memory_order_release);return E_FAIL;}
 out<<"B186: XESS_ONLY_SYNCHRONOUS_BEGIN owner=render_thread, thread_id="<<GetCurrentThreadId()<<", background_worker=0, d3d12_device_created=0, d3d11on12_created=0, bridge_prepare_called=0\n";out.flush();
 const HRESULT hr=b175b_prepare_xess(device,DXGI_FORMAT_R16G16B16A16_FLOAT,1920,1080,out);
 g_b175nXessOnlyState.store(SUCCEEDED(hr)?1u:2u,std::memory_order_release);
 out<<"B186: XESS_ONLY_SYNCHRONOUS_RETURN hr="<<(long)hr<<", ready="<<(SUCCEEDED(hr)?1:0)<<", thread_id="<<GetCurrentThreadId()<<", background_worker=0, d3d12_device_created=0, d3d11on12_created=0, bridge_prepare_called=0\n";out.flush();
 return hr;
}

// CHECK-171 deliberately creates only the bridge core after CHECK-170 has
// completed XeSS initialization.  It must not allocate dependent resources,
// initialize XeFG, create a proxy, install persistent hooks, or touch GTA's
// immediate context.  State is terminal and there is no automatic retry.
static std::atomic<bool> g_b175oBridgeOnlyStarted{false};
static std::atomic<unsigned> g_b175oBridgeOnlyState{0};
struct B175oBridgeCoreResults
{
 HRESULT dxgiHr{E_FAIL},adapterHr{E_FAIL},descHr{E_FAIL},d3d12Hr{E_FAIL},queueHr{E_FAIL},on12Hr{E_FAIL},on12InterfaceHr{E_FAIL};
 DXGI_ADAPTER_DESC adapterDesc{};
};

static HRESULT b175o_prepare_bridge_core_only(ID3D11Device* nativeDevice,std::ofstream& out,B175oBridgeCoreResults& results)
{
 if(!nativeDevice)return E_INVALIDARG;
 ComPtr<IDXGIDevice>dxgiDevice;ComPtr<IDXGIAdapter>adapter;
 results.dxgiHr=nativeDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
 if(SUCCEEDED(results.dxgiHr))results.adapterHr=dxgiDevice->GetAdapter(&adapter);
 if(SUCCEEDED(results.adapterHr))results.descHr=adapter->GetDesc(&results.adapterDesc);
 out<<"B175o: BRIDGE_CORE_STAGE stage=native_adapter, thread_id="<<GetCurrentThreadId()<<", query_dxgi_hr="<<(long)results.dxgiHr<<", get_adapter_hr="<<(long)results.adapterHr<<", get_desc_hr="<<(long)results.descHr<<", vendor="<<results.adapterDesc.VendorId<<", device="<<results.adapterDesc.DeviceId<<", luid_high="<<results.adapterDesc.AdapterLuid.HighPart<<", luid_low="<<results.adapterDesc.AdapterLuid.LowPart<<"\n";out.flush();
 if(FAILED(results.dxgiHr)||FAILED(results.adapterHr)||FAILED(results.descHr))return FAILED(results.dxgiHr)?results.dxgiHr:FAILED(results.adapterHr)?results.adapterHr:results.descHr;

 results.d3d12Hr=D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&g_device12));
 out<<"B175o: BRIDGE_CORE_STAGE stage=d3d12_device, same_native_adapter=1, hr="<<(long)results.d3d12Hr<<", created="<<(g_device12?1:0)<<"\n";out.flush();
 if(FAILED(results.d3d12Hr)||!g_device12)return FAILED(results.d3d12Hr)?results.d3d12Hr:E_FAIL;

 D3D12_COMMAND_QUEUE_DESC queueDesc{};queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 results.queueHr=g_device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&g_queue12));
 out<<"B175o: BRIDGE_CORE_STAGE stage=d3d12_direct_queue, hr="<<(long)results.queueHr<<", created="<<(g_queue12?1:0)<<"\n";out.flush();
 if(FAILED(results.queueHr)||!g_queue12){g_queue12.Reset();g_device12.Reset();return FAILED(results.queueHr)?results.queueHr:E_FAIL;}

 IUnknown*queues[]{g_queue12.Get()};D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};
 results.on12Hr=D3D11On12CreateDevice(g_device12.Get(),D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,queues,1,0,&g_on12Device,&g_on12Context,nullptr);
 out<<"B175o: BRIDGE_CORE_STAGE stage=d3d11on12_device_context, hr="<<(long)results.on12Hr<<", device="<<(g_on12Device?1:0)<<", context="<<(g_on12Context?1:0)<<"\n";out.flush();
 if(FAILED(results.on12Hr)||!g_on12Device||!g_on12Context){g_on12Context.Reset();g_on12Device.Reset();g_queue12.Reset();g_device12.Reset();return FAILED(results.on12Hr)?results.on12Hr:E_FAIL;}

 results.on12InterfaceHr=g_on12Device.As(&g_on12);
 out<<"B175o: BRIDGE_CORE_STAGE stage=d3d11on12_interface, hr="<<(long)results.on12InterfaceHr<<", interface="<<(g_on12?1:0)<<"\n";out.flush();
 if(FAILED(results.on12InterfaceHr)||!g_on12){g_on12.Reset();g_on12Context.Reset();g_on12Device.Reset();g_queue12.Reset();g_device12.Reset();return FAILED(results.on12InterfaceHr)?results.on12InterfaceHr:E_FAIL;}

 out<<"B175o: BRIDGE_CORE_READY bridge_core_ready=1, worker_thread=1, d3d12_device_created=1, d3d12_queue_created=1, d3d11on12_created=1, xefg_initialization_started=0, proxy_created=0, dependent_pools_created=0, frame_ring_created=0, shaders_created=0, persistent_hooks_installed=0, gta_present_waits=0, gta_present_allocations=0\n";out.flush();
 return S_OK;
}

// B354 keeps only driver-heavy D3D12 device/queue creation on a detached worker.
// Adapter identity is captured synchronously from the native GTA D3D11 device; D3D11On12
// is deliberately completed later on the GTA render thread.
extern "C" __declspec(dllexport) unsigned WINAPI B354BridgeDevicePrewarmState(){return g_b354BridgePrewarmState.load(std::memory_order_acquire);}
extern "C" __declspec(dllexport) HRESULT WINAPI B354StartBridgeDevicePrewarm(ID3D11Device*nativeDevice,const char*path)
{
 if(!nativeDevice||!path)return E_INVALIDARG;
 if(!g_b175bXessReady.load(std::memory_order_acquire)||g_b175nXessOnlyState.load(std::memory_order_acquire)!=1)return E_PENDING;
 unsigned prior=g_b354BridgePrewarmState.load(std::memory_order_acquire);if(prior==2&&g_device12&&g_queue12)return S_OK;if(prior==2&&(!g_device12||!g_queue12)){g_b354BridgePrewarmState.store(0,std::memory_order_release);g_b354BridgePrewarmStarted.store(false,std::memory_order_release);prior=0;}if(prior==3)return E_FAIL;
 if(g_b354BridgePrewarmStarted.exchange(true,std::memory_order_acq_rel))return S_FALSE;
 ComPtr<IDXGIDevice>dxgiDevice;ComPtr<IDXGIAdapter>adapter;DXGI_ADAPTER_DESC desc{};
 HRESULT hr=nativeDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));if(SUCCEEDED(hr))hr=dxgiDevice->GetAdapter(&adapter);if(SUCCEEDED(hr))hr=adapter->GetDesc(&desc);
 if(FAILED(hr)||!adapter){g_b354BridgePrewarmState.store(3,std::memory_order_release);std::ofstream out(path,std::ios::app);out<<"B354: BACKGROUND_D3D12_PREWARM_ADAPTER_FAILED hr="<<(long)hr<<", render_thread=1\n";out.flush();return FAILED(hr)?hr:E_FAIL;}
 g_b175qBridgeAdapterDesc=desc;std::string retainedPath=path;
 std::thread([adapter,retainedPath,desc](){
  std::ofstream out(retainedPath,std::ios::app);const ULONGLONG begin=GetTickCount64();ComPtr<ID3D12Device>device12;ComPtr<ID3D12CommandQueue>queue12;
  HRESULT hr=D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device12));const ULONGLONG deviceMs=GetTickCount64()-begin;
  ULONGLONG queueMs=0;if(SUCCEEDED(hr)&&device12){D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;const ULONGLONG qb=GetTickCount64();hr=device12->CreateCommandQueue(&q,IID_PPV_ARGS(&queue12));queueMs=GetTickCount64()-qb;}
  const bool ready=SUCCEEDED(hr)&&device12&&queue12;if(ready){g_device12=device12;g_queue12=queue12;}else{g_queue12.Reset();g_device12.Reset();}
  out<<"B354: BACKGROUND_D3D12_PREWARM_COMPLETE hr="<<(long)hr<<", ready="<<(ready?1:0)<<", worker_thread_id="<<GetCurrentThreadId()<<", d3d12_device_ms="<<deviceMs<<", queue_ms="<<queueMs<<", total_ms="<<(GetTickCount64()-begin)<<", adapter_vendor="<<desc.VendorId<<", adapter_device="<<desc.DeviceId<<", gta_render_thread_blocked=0, d3d11on12_created=0\n";out.flush();
  g_b354BridgePrewarmState.store(ready?2u:3u,std::memory_order_release);
 }).detach();
 std::ofstream out(path,std::ios::app);out<<"B354: BACKGROUND_D3D12_PREWARM_STARTED render_thread_id="<<GetCurrentThreadId()<<", adapter_vendor="<<desc.VendorId<<", adapter_device="<<desc.DeviceId<<", d3d12_creation_background=1, queue_creation_background=1, d3d11on12_final_handoff_render_thread=1\n";out.flush();return S_OK;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B354CompleteBridgeOnRenderThread(const char*path)
{
 if(!path)return E_INVALIDARG;const unsigned state=g_b354BridgePrewarmState.load(std::memory_order_acquire);if(state!=2)return state==3?E_FAIL:E_PENDING;if(!g_device12||!g_queue12)return E_FAIL;if(g_on12Device&&g_on12Context&&g_on12)return S_OK;
 std::ofstream out(path,std::ios::app);const ULONGLONG begin=GetTickCount64();IUnknown*queues[]{g_queue12.Get()};D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};HRESULT hr=D3D11On12CreateDevice(g_device12.Get(),D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,queues,1,0,&g_on12Device,&g_on12Context,nullptr);if(SUCCEEDED(hr)&&g_on12Device)hr=g_on12Device.As(&g_on12);const bool ready=SUCCEEDED(hr)&&g_on12Device&&g_on12Context&&g_on12;
 if(!ready){g_on12.Reset();g_on12Context.Reset();g_on12Device.Reset();}
 out<<"B354: RENDER_THREAD_ON12_FINAL_HANDOFF_COMPLETE hr="<<(long)hr<<", ready="<<(ready?1:0)<<", duration_ms="<<(GetTickCount64()-begin)<<", render_thread_id="<<GetCurrentThreadId()<<", d3d12_device_precreated_background="<<(g_device12?1:0)<<", queue_precreated_background="<<(g_queue12?1:0)<<", on12_device="<<(g_on12Device?1:0)<<", on12_context="<<(g_on12Context?1:0)<<", on12_interface="<<(g_on12?1:0)<<"\n";out.flush();if(ready)g_b175qBridgeStage.store(5,std::memory_order_release);return ready?S_OK:(FAILED(hr)?hr:E_FAIL);
}

extern "C" __declspec(dllexport) unsigned WINAPI B175oBridgeCoreOnlyState(){return g_b175oBridgeOnlyState.load(std::memory_order_acquire);}
extern "C" __declspec(dllexport) HRESULT WINAPI B175oStartBridgeCoreOnly(ID3D11Device*device,const char*path)
{
 if(!device||!path)return E_INVALIDARG;
 if(!g_b175bXessReady.load(std::memory_order_acquire)||g_b175nXessOnlyState.load(std::memory_order_acquire)!=1)return E_PENDING;
 if(g_b175oBridgeOnlyStarted.exchange(true,std::memory_order_acq_rel))return S_FALSE;
 ComPtr<ID3D11Device>retainedDevice=device;std::string retainedPath=path;
 std::thread([retainedDevice,retainedPath](){
  std::ofstream out(retainedPath,std::ios::app);
  if(!out){g_b175oBridgeOnlyState.store(2,std::memory_order_release);return;}
  out<<"B175o: BRIDGE_CORE_ONLY_WORKER_ENTER thread_id="<<GetCurrentThreadId()<<", xess_ready=1, d3d12_device_created=0, d3d11on12_created=0, xefg_initialization_started=0, proxy_created=0\n";out.flush();
  B175oBridgeCoreResults results{};const HRESULT hr=b175o_prepare_bridge_core_only(retainedDevice.Get(),out,results);
  const bool ready=SUCCEEDED(hr)&&g_device12&&g_queue12&&g_on12Device&&g_on12Context&&g_on12;
  if(!ready){g_on12.Reset();g_on12Context.Reset();g_on12Device.Reset();g_queue12.Reset();g_device12.Reset();}
  out<<"B175o: BRIDGE_CORE_ONLY_COMPLETE hr="<<(long)hr<<", bridge_core_ready="<<(ready?1:0)<<", terminal=1, automatic_retry=0, thread_id="<<GetCurrentThreadId()<<", query_dxgi_hr="<<(long)results.dxgiHr<<", get_adapter_hr="<<(long)results.adapterHr<<", get_desc_hr="<<(long)results.descHr<<", d3d12_create_hr="<<(long)results.d3d12Hr<<", queue_create_hr="<<(long)results.queueHr<<", d3d11on12_create_hr="<<(long)results.on12Hr<<", d3d11on12_interface_hr="<<(long)results.on12InterfaceHr<<", adapter_vendor="<<results.adapterDesc.VendorId<<", adapter_device="<<results.adapterDesc.DeviceId<<", adapter_luid_high="<<results.adapterDesc.AdapterLuid.HighPart<<", adapter_luid_low="<<results.adapterDesc.AdapterLuid.LowPart<<", xess_ready=1, d3d12_device_created="<<(g_device12?1:0)<<", d3d12_queue_created="<<(g_queue12?1:0)<<", d3d11on12_device_created="<<(g_on12Device?1:0)<<", d3d11on12_context_created="<<(g_on12Context?1:0)<<", d3d11on12_interface_created="<<(g_on12?1:0)<<", xefg_initialization_started=0, proxy_created=0, dependent_pools_created=0, frame_ring_created=0, shaders_created=0, persistent_hooks_installed=0, gta_present_modified=0\n";out.flush();
  g_b175oBridgeOnlyState.store(ready?1u:2u,std::memory_order_release);
 }).detach();
 std::ofstream out(path,std::ios::app);out<<"B175o: BRIDGE_CORE_ONLY_DISPATCHED present_thread_blocking_initialization=0, worker_owned=1, xess_ready=1, xefg_initialization_started=0, proxy_created=0\n";out.flush();return S_OK;
}

// CHECK-173 advances exactly one bridge-core component on each real GTA
// Present.  CHECK-172 proved that creating the complete bridge on a detached
// worker could remain pending until shutdown on this driver.  Keeping the
// creation calls on the render thread, while limiting each Present to one
// component, preserves adapter/thread affinity without repeating discovery.
extern "C" __declspec(dllexport) HRESULT WINAPI B175qAdvanceBridgeCore(ID3D11Device*nativeDevice,const char*path)
{
 if(!nativeDevice||!path)return E_INVALIDARG;
 if(!g_b175bXessReady.load(std::memory_order_acquire)||g_b175nXessOnlyState.load(std::memory_order_acquire)!=1)return E_PENDING;
 const unsigned stage=g_b175qBridgeStage.load(std::memory_order_acquire);
 if(stage==5)return S_OK;
 if(stage>=6)return E_FAIL;
 std::ofstream out(path,std::ios::app);if(!out)return E_FAIL;
 const ULONGLONG begin=GetTickCount64();HRESULT hr=S_OK;const char*name="unknown";
 if(stage==0){
  name="native_adapter";ComPtr<IDXGIDevice>dxgiDevice;ComPtr<IDXGIAdapter>adapter;
  hr=nativeDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));if(SUCCEEDED(hr))hr=dxgiDevice->GetAdapter(&adapter);if(SUCCEEDED(hr))hr=adapter->GetDesc(&g_b175qBridgeAdapterDesc);if(SUCCEEDED(hr))g_b175qBridgeAdapter=adapter;
 }else if(stage==1){
  name="d3d12_device";hr=g_b175qBridgeAdapter?D3D12CreateDevice(g_b175qBridgeAdapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&g_device12)):E_NOINTERFACE;if(SUCCEEDED(hr)&&!g_device12)hr=E_FAIL;
 }else if(stage==2){
  name="d3d12_direct_queue";D3D12_COMMAND_QUEUE_DESC queueDesc{};queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;hr=g_device12?g_device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&g_queue12)):E_NOINTERFACE;if(SUCCEEDED(hr)&&!g_queue12)hr=E_FAIL;
 }else if(stage==3){
  name="d3d11on12_device_context";IUnknown*queues[]{g_queue12.Get()};D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};hr=g_device12&&g_queue12?D3D11On12CreateDevice(g_device12.Get(),D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,queues,1,0,&g_on12Device,&g_on12Context,nullptr):E_NOINTERFACE;if(SUCCEEDED(hr)&&(!g_on12Device||!g_on12Context))hr=E_FAIL;
 }else if(stage==4){
  name="d3d11on12_interface";hr=g_on12Device?g_on12Device.As(&g_on12):E_NOINTERFACE;if(SUCCEEDED(hr)&&!g_on12)hr=E_FAIL;
 }
 out<<"B175q: STAGED_BRIDGE_STEP stage_index="<<stage<<", stage="<<name<<", hr="<<(long)hr<<", duration_ms="<<(GetTickCount64()-begin)<<", present_thread=1, thread_id="<<GetCurrentThreadId()<<", vendor="<<g_b175qBridgeAdapterDesc.VendorId<<", device="<<g_b175qBridgeAdapterDesc.DeviceId<<", d3d12_device="<<(g_device12?1:0)<<", queue="<<(g_queue12?1:0)<<", on12_device="<<(g_on12Device?1:0)<<", on12_context="<<(g_on12Context?1:0)<<", on12_interface="<<(g_on12?1:0)<<"\n";out.flush();
 if(FAILED(hr)){
  g_b175qBridgeStage.store(6,std::memory_order_release);g_b175qBridgeAdapter.Reset();g_on12.Reset();g_on12Context.Reset();g_on12Device.Reset();g_queue12.Reset();g_device12.Reset();
  out<<"B175q: STAGED_BRIDGE_FAILED failed_stage="<<name<<", hr="<<(long)hr<<", terminal=1, automatic_retry=0, gta_present_modified=0\n";out.flush();return hr;
 }
 const unsigned next=stage+1;g_b175qBridgeStage.store(next,std::memory_order_release);
 if(next==5){g_b175qBridgeAdapter.Reset();out<<"B175q: STAGED_BRIDGE_COMPLETE bridge_core_ready=1, steps=5, present_thread_staged=1, xess_ready=1, xefg_initialization_started=0, proxy_created=0, gta_present_modified=0\n";out.flush();return S_OK;}
 return S_FALSE;
}
static HRESULT b175b_prepare_native_motion_shader(ID3D11Device*device,std::ofstream&out)
{
 if(g_b175bNativeMotionShader)return S_OK;if(!device)return E_INVALIDARG;
 static const char source[]=R"(
 cbuffer FrameData : register(b0) {
  row_major float4x4 previousViewProjection;
  row_major float4x4 currentInverseViewProjection;
  uint2 frameSize;
  float2 padding;
 };
 Texture2D<float> depthInput : register(t0);
 RWTexture2D<float2> motionOutput : register(u0);
 [numthreads(8,8,1)]
 void main(uint3 id : SV_DispatchThreadID) {
  if(id.x>=frameSize.x||id.y>=frameSize.y)return;
  float z=depthInput.Load(int3(id.xy,0));float2 motion=float2(0.0,0.0);
  if(z>0.000001&&z<0.999999){
   float2 uv=(float2(id.xy)+0.5)/float2(frameSize);float4 clip=float4(uv.x*2.0-1.0,1.0-uv.y*2.0,z,1.0);
   float4 world=mul(currentInverseViewProjection,clip);
   if(abs(world.w)>0.000001){world/=world.w;float4 prior=mul(previousViewProjection,world);if(abs(prior.w)>0.000001){float2 priorNdc=prior.xy/prior.w;motion=float2((priorNdc.x-clip.x)*(frameSize.x*0.5),(clip.y-priorNdc.y)*(frameSize.y*0.5));}}
  }
  motionOutput[id.xy]=clamp(motion,float2(-65504.0,-65504.0),float2(65504.0,65504.0));
 })";
 ComPtr<ID3DBlob>bytecode,errors;HRESULT hr=D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,0,&bytecode,&errors);
 if(SUCCEEDED(hr))hr=device->CreateComputeShader(bytecode->GetBufferPointer(),bytecode->GetBufferSize(),nullptr,&g_b175bNativeMotionShader);
 out<<"B175n: XESS_NATIVE_MOTION_PIPELINE shader="<<(g_b175bNativeMotionShader?1:0)<<", pixel_space_current_to_previous=1, hr="<<(long)hr<<"\n";out.flush();return hr;
}
static HRESULT b175b_prepare_color_conversion(ID3D11Device*device,std::ofstream&out)
{
 if(g_b175bColorConvertVs&&g_b175bColorConvertPs&&g_b175bColorConvertRs)return S_OK;if(!device)return E_INVALIDARG;
 static const char source[]=R"(
 Texture2D<float4> sourceColor : register(t0);
 struct VsOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };
 VsOut vsMain(uint id : SV_VertexID) { VsOut o;float2 p=float2((id<<1)&2,id&2);o.uv=float2(p.x,1.0-p.y);o.position=float4(p*float2(2.0,-2.0)+float2(-1.0,1.0),0.0,1.0);return o; }
 float4 psMain(VsOut input) : SV_Target {
  // B241: B240 now feeds GTA's final display-referred SDR backbuffer into XeSS.
  // Do not apply the old linear->sRGB transfer a second time at presentation.
  float4 color=sourceColor.Load(int3(int2(input.position.xy),0));
  return float4(saturate(color.rgb),saturate(color.a));
 }
 )";
 ComPtr<ID3DBlob>vs,ps,errors;HRESULT vsCompileHr=D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"vsMain","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&errors);HRESULT hr=vsCompileHr;
 if(FAILED(vsCompileHr)&&errors){out<<"B175r: COLOR_CONVERSION_COMPILE_ERROR stage=vertex, message="<<static_cast<const char*>(errors->GetBufferPointer())<<"\n";out.flush();}
 errors.Reset();HRESULT psCompileHr=SUCCEEDED(hr)?D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"psMain","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&errors):hr;hr=psCompileHr;
 if(FAILED(psCompileHr)&&errors){out<<"B175r: COLOR_CONVERSION_COMPILE_ERROR stage=pixel, message="<<static_cast<const char*>(errors->GetBufferPointer())<<"\n";out.flush();}
 HRESULT vsCreateHr=SUCCEEDED(hr)?device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&g_b175bColorConvertVs):hr;hr=vsCreateHr;
 HRESULT psCreateHr=SUCCEEDED(hr)?device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&g_b175bColorConvertPs):hr;hr=psCreateHr;
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
 HRESULT rasterHr=SUCCEEDED(hr)?device->CreateRasterizerState(&raster,&g_b175bColorConvertRs):hr;hr=rasterHr;
 out<<"B241: XESS_PRESENTATION_CONVERSION source=final_composited_SDR_encoded, destination=BGRA8_SDR_passthrough, duplicate_gamma_encode_removed=1, shader_preallocated="<<(SUCCEEDED(hr)?1:0)<<", vs_compile_hr="<<(long)vsCompileHr<<", ps_compile_hr="<<(long)psCompileHr<<", vs_create_hr="<<(long)vsCreateHr<<", ps_create_hr="<<(long)psCreateHr<<", raster_create_hr="<<(long)rasterHr<<", hr="<<(long)hr<<"\n";out.flush();return hr;
}
static HRESULT b173_prepare_compute(std::ofstream&out){
 if(g_b173MotionShader&&g_b173MotionConstants)return S_OK;
 static const char source[]=R"(
cbuffer FrameData : register(b0) {
 row_major float4x4 previousViewProjection;
 row_major float4x4 currentInverseViewProjection;
 uint2 frameSize;
 float2 padding;
};
Texture2D<float4> depthInput : register(t0);
RWTexture2D<float2> motionOutput : register(u0);
RWTexture2D<float> depthOutput : register(u1);
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
 if(id.x>=frameSize.x||id.y>=frameSize.y)return;
 float4 packedDepth=depthInput.Load(int3(id.xy,0));
 uint3 packedBytes=(uint3)round(saturate(packedDepth.rgb)*255.0);
 float z=(float(packedBytes.x)+float(packedBytes.y)*256.0+float(packedBytes.z)*65536.0)/16777215.0;
 depthOutput[id.xy]=z;
 float2 motion=float2(0.0,0.0);
 if(z>0.0&&z<1.0) {
  float4 clip=float4(2.0*((id.x+0.5)/frameSize.x)-1.0,1.0-2.0*((id.y+0.5)/frameSize.y),z,1.0);
  float4 world=mul(currentInverseViewProjection,clip);
  if(abs(world.w)>1.0e-6) {
   world/=world.w;
   float4 prior=mul(previousViewProjection,world);
   if(abs(prior.w)>1.0e-6) {
    float2 priorNdc=prior.xy/prior.w;
    motion=float2((priorNdc.x-clip.x)*(frameSize.x*0.5),(clip.y-priorNdc.y)*(frameSize.y*0.5));
    motion=clamp(motion,float2(-65504.0,-65504.0),float2(65504.0,65504.0));
   }
  }
 }
 motionOutput[id.xy]=motion;
}
)";
 ComPtr<ID3DBlob>bytecode,errors;HRESULT hr=D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,0,&bytecode,&errors);
 out<<"B173: stage=CompileGpuMotionShader, bytecode="<<(bytecode?1:0)<<", hr="<<(long)hr;
 if(errors&&errors->GetBufferPointer())out<<", compiler_message="<<(const char*)errors->GetBufferPointer();out<<"\n";out.flush();if(FAILED(hr))return hr;
 hr=g_on12Device->CreateComputeShader(bytecode->GetBufferPointer(),bytecode->GetBufferSize(),nullptr,&g_b173MotionShader);if(FAILED(hr))return hr;
 D3D11_BUFFER_DESC cb{};cb.ByteWidth=256;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 hr=g_on12Device->CreateBuffer(&cb,nullptr,&g_b173MotionConstants);out<<"B173: stage=CreateGpuMotionPipeline, shader="<<(g_b173MotionShader?1:0)<<", constants="<<(g_b173MotionConstants?1:0)<<", hr="<<(long)hr<<"\n";out.flush();return hr;
}
static HRESULT b174e_prepare_worker_on12(std::ofstream&out)
{
 if(g_b174eWorkerOn12Device&&g_b174eWorkerOn12Context&&g_b174eWorkerOn12&&g_b174iPreparationQueue&&g_b174iPreparationFence&&g_b174eWorkerMotionShader&&g_b174eWorkerMotionConstants)return S_FALSE;
 D3D12_COMMAND_QUEUE_DESC queueDesc{};queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 HRESULT hr=g_device12?g_device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&g_b174iPreparationQueue)):E_NOINTERFACE;
 if(SUCCEEDED(hr))hr=g_device12->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&g_b174iPreparationFence));
 if(SUCCEEDED(hr)){g_b174iPreparationFenceValue.store(0,std::memory_order_release);g_b174iPreparationQueue->SetName(L"GTAXeSS B174i Input Preparation Queue");g_b174iPreparationFence->SetName(L"GTAXeSS B174i Input Preparation Fence");}
 IUnknown*queues[]{g_b174iPreparationQueue.Get()};D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};
 if(SUCCEEDED(hr))hr=D3D11On12CreateDevice(g_device12.Get(),D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,queues,1,0,&g_b174eWorkerOn12Device,&g_b174eWorkerOn12Context,nullptr);
 if(SUCCEEDED(hr))hr=g_b174eWorkerOn12Device.As(&g_b174eWorkerOn12);
 static const char source[]=R"(
cbuffer FrameData : register(b0) {
 row_major float4x4 previousViewProjection;
 row_major float4x4 currentInverseViewProjection;
 uint2 frameSize;
 float2 padding;
};
Texture2D<float4> depthInput : register(t0);
RWTexture2D<float2> motionOutput : register(u0);
RWTexture2D<float> depthOutput : register(u1);
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
 if(id.x>=frameSize.x||id.y>=frameSize.y)return;
 float4 packedDepth=depthInput.Load(int3(id.xy,0));
 uint3 packedBytes=(uint3)round(saturate(packedDepth.rgb)*255.0);
 float z=(float(packedBytes.x)+float(packedBytes.y)*256.0+float(packedBytes.z)*65536.0)/16777215.0;
 depthOutput[id.xy]=z;
 float2 motion=float2(0.0,0.0);
 if(z>0.0&&z<1.0) {
  float4 clip=float4(2.0*((id.x+0.5)/frameSize.x)-1.0,1.0-2.0*((id.y+0.5)/frameSize.y),z,1.0);
  float4 world=mul(currentInverseViewProjection,clip);
  if(abs(world.w)>1.0e-6) {
   world/=world.w;
   float4 prior=mul(previousViewProjection,world);
   if(abs(prior.w)>1.0e-6) {
    float2 priorNdc=prior.xy/prior.w;
    motion=float2((priorNdc.x-clip.x)*(frameSize.x*0.5),(clip.y-priorNdc.y)*(frameSize.y*0.5));
    motion=clamp(motion,float2(-65504.0,-65504.0),float2(65504.0,65504.0));
   }
  }
 }
 motionOutput[id.xy]=motion;
}
)";
 ComPtr<ID3DBlob>bytecode,errors;if(SUCCEEDED(hr))hr=D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,0,&bytecode,&errors);
 if(SUCCEEDED(hr))hr=g_b174eWorkerOn12Device->CreateComputeShader(bytecode->GetBufferPointer(),bytecode->GetBufferSize(),nullptr,&g_b174eWorkerMotionShader);
 if(SUCCEEDED(hr)){static const char nativeMotionSource[]=R"(
cbuffer FrameData : register(b0){row_major float4x4 a;row_major float4x4 b;uint2 frameSize;float2 pad;}
Texture2D<float2> nativeMotion : register(t1);RWTexture2D<float2> motionOutput : register(u0);
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID){if(id.x>=frameSize.x||id.y>=frameSize.y)return;uint sw,sh;nativeMotion.GetDimensions(sw,sh);float2 uv=(float2(id.xy)+0.5)/float2(frameSize);uint2 sp=min(uint2(uv*float2(sw,sh)),uint2(sw-1,sh-1));float2 mv=nativeMotion.Load(int3(sp,0));motionOutput[id.xy]=clamp(-mv,float2(-65504,-65504),float2(65504,65504));}
)";ComPtr<ID3DBlob>nm,ne;HRESULT nhr=D3DCompile(nativeMotionSource,sizeof(nativeMotionSource)-1,nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&nm,&ne);if(SUCCEEDED(nhr))nhr=g_b174eWorkerOn12Device->CreateComputeShader(nm->GetBufferPointer(),nm->GetBufferSize(),nullptr,&g_b330WorkerNativeMotionShader);out<<"B331: OPTIONAL_NATIVE_MOTION_WORKER_SHADER compiled="<<(nm?1:0)<<", created="<<(g_b330WorkerNativeMotionShader?1:0)<<", hr="<<(long)nhr<<", activation_blocking=0\n";if(FAILED(nhr))g_b330WorkerNativeMotionShader.Reset();}
 D3D11_BUFFER_DESC cb{};cb.ByteWidth=256;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 if(SUCCEEDED(hr))hr=g_b174eWorkerOn12Device->CreateBuffer(&cb,nullptr,&g_b174eWorkerMotionConstants);
 unsigned colorPrepared=0,depthPrepared=0;
 for(unsigned i=0;SUCCEEDED(hr)&&i<kB173iColorSlotCount;++i){
  auto&slot=g_b173iColorSlots[i];ComPtr<IDXGIResource>shared;HANDLE handle{};hr=slot.shared.As(&shared);if(SUCCEEDED(hr))hr=shared->GetSharedHandle(&handle);
  if(SUCCEEDED(hr)&&handle)hr=g_b174eWorkerOn12Device->OpenSharedResource(handle,IID_PPV_ARGS(&slot.opened));
  D3D11_RESOURCE_FLAGS flags{};if(SUCCEEDED(hr))hr=g_b174eWorkerOn12->CreateWrappedResource(slot.output.Get(),&flags,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&slot.wrapped));
  if(SUCCEEDED(hr))++colorPrepared;
 }
 for(unsigned i=0;SUCCEEDED(hr)&&i<kB173jDepthMotionSlotCount;++i){
  auto&slot=g_b173jDepthMotionSlots[i];ComPtr<IDXGIResource>shared;HANDLE handle{};hr=slot.sharedDepth.As(&shared);if(SUCCEEDED(hr))hr=shared->GetSharedHandle(&handle);
  if(SUCCEEDED(hr)&&handle)hr=g_b174eWorkerOn12Device->OpenSharedResource(handle,IID_PPV_ARGS(&slot.openedDepth));
  if(SUCCEEDED(hr))hr=g_b174eWorkerOn12Device->CreateShaderResourceView(slot.openedDepth.Get(),nullptr,&slot.openedDepthSrv);
  HRESULT nativeMotionHr=S_FALSE;ComPtr<IDXGIResource>motionShared;HANDLE motionHandle{};if(slot.sharedNativeMotion){nativeMotionHr=slot.sharedNativeMotion.As(&motionShared);if(SUCCEEDED(nativeMotionHr))nativeMotionHr=motionShared->GetSharedHandle(&motionHandle);if(SUCCEEDED(nativeMotionHr)&&motionHandle)nativeMotionHr=g_b174eWorkerOn12Device->OpenSharedResource(motionHandle,IID_PPV_ARGS(&slot.openedNativeMotion));if(SUCCEEDED(nativeMotionHr))nativeMotionHr=g_b174eWorkerOn12Device->CreateShaderResourceView(slot.openedNativeMotion.Get(),nullptr,&slot.openedNativeMotionSrv);}if(FAILED(nativeMotionHr)){slot.openedNativeMotion.Reset();slot.openedNativeMotionSrv.Reset();}out<<"B331: OPTIONAL_NATIVE_MOTION_SLOT slot="<<i<<", ready="<<(slot.openedNativeMotionSrv?1:0)<<", hr="<<(long)nativeMotionHr<<", activation_blocking=0\n";
  if(SUCCEEDED(hr))++depthPrepared;
 }
 out<<"B174i: WORKER_ON12_CONTEXT_PREWARM device="<<(g_b174eWorkerOn12Device?1:0)<<", context="<<(g_b174eWorkerOn12Context?1:0)<<", on12="<<(g_b174eWorkerOn12?1:0)<<", preparation_queue="<<(g_b174iPreparationQueue?1:0)<<", preparation_fence="<<(g_b174iPreparationFence?1:0)<<", queue_shared_with_xefg=0, shader="<<(g_b174eWorkerMotionShader?1:0)<<", constants="<<(g_b174eWorkerMotionConstants?1:0)<<", color_slots="<<colorPrepared<<", depth_slots="<<depthPrepared<<", owner_thread="<<GetCurrentThreadId()<<", ready="<<(SUCCEEDED(hr)?1:0)<<", hr="<<(long)hr<<"\n";out.flush();
 return hr;
}
static HRESULT b174o_prepare_frame_resource_ring(UINT width,UINT height,std::ofstream&out)
{
 if(!g_device12||!g_b174eWorkerOn12||!g_b174eWorkerOn12Device||!g_xefgSwapChain||!width||!height)return E_INVALIDARG;
 LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);HRESULT hr=S_OK;unsigned prepared=0;
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
 D3D12_RESOURCE_DESC output{};output.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;output.Width=width;output.Height=height;output.DepthOrArraySize=1;output.MipLevels=1;output.SampleDesc.Count=1;output.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;output.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
 D3D11_RESOURCE_FLAGS flags{};flags.BindFlags=D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE;
 for(unsigned i=0;SUCCEEDED(hr)&&i<kB173jDepthMotionSlotCount;++i){
  auto&slot=g_b173jDepthMotionSlots[i];if(!slot.ready||slot.width!=width||slot.height!=height){hr=E_FAIL;break;}
  output.Format=DXGI_FORMAT_R16G16_FLOAT;hr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&slot.motionOutput));
  output.Format=DXGI_FORMAT_R32_FLOAT;if(SUCCEEDED(hr))hr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&slot.depthOutput));
  if(SUCCEEDED(hr))hr=g_b174eWorkerOn12->CreateWrappedResource(slot.motionOutput.Get(),&flags,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&slot.wrappedMotion));
  if(SUCCEEDED(hr))hr=g_b174eWorkerOn12->CreateWrappedResource(slot.depthOutput.Get(),&flags,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&slot.wrappedDepth));
  if(SUCCEEDED(hr))hr=g_b174eWorkerOn12Device->CreateUnorderedAccessView(slot.wrappedMotion.Get(),nullptr,&slot.motionUav);
  if(SUCCEEDED(hr))hr=g_b174eWorkerOn12Device->CreateUnorderedAccessView(slot.wrappedDepth.Get(),nullptr,&slot.depthUav);
  if(SUCCEEDED(hr))hr=g_device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&slot.prefillAllocator));
  if(SUCCEEDED(hr))hr=g_device12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,slot.prefillAllocator.Get(),nullptr,IID_PPV_ARGS(&slot.prefillList));
  if(SUCCEEDED(hr))hr=slot.prefillList->Close();
  slot.outputsReady=SUCCEEDED(hr);if(slot.outputsReady){slot.motionOutput->SetName(L"GTAXeSS B174o Ring Motion Output");slot.depthOutput->SetName(L"GTAXeSS B174o Ring Depth Output");++prepared;}
 }
 DXGI_SWAP_CHAIN_DESC swapDesc{};if(SUCCEEDED(hr))hr=g_xefgSwapChain->GetDesc(&swapDesc);
 if(SUCCEEDED(hr))hr=g_device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&g_b174oSeedAllocator));
 if(SUCCEEDED(hr))hr=g_device12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,g_b174oSeedAllocator.Get(),nullptr,IID_PPV_ARGS(&g_b174oSeedList));
 if(SUCCEEDED(hr))hr=g_b174oSeedList->Close();
 g_b174oSeedBackBuffers.clear();if(SUCCEEDED(hr))g_b174oSeedBackBuffers.reserve(swapDesc.BufferCount);
 for(UINT i=0;SUCCEEDED(hr)&&i<swapDesc.BufferCount;++i){ComPtr<ID3D12Resource>buffer;hr=g_xefgSwapChain->GetBuffer(i,IID_PPV_ARGS(&buffer));if(SUCCEEDED(hr)&&buffer)g_b174oSeedBackBuffers.push_back(buffer);else if(SUCCEEDED(hr))hr=E_FAIL;}
 g_b191RtvHeap.Reset();g_b191BackBufferRtvs.clear();g_b191HudReady.store(false,std::memory_order_release);
 if(SUCCEEDED(hr)&&swapDesc.BufferCount){D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=swapDesc.BufferCount;hr=g_device12->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&g_b191RtvHeap));}
 if(SUCCEEDED(hr)&&g_b191RtvHeap){const UINT inc=g_device12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);auto h=g_b191RtvHeap->GetCPUDescriptorHandleForHeapStart();for(UINT i=0;i<swapDesc.BufferCount&&i<g_b174oSeedBackBuffers.size();++i){g_device12->CreateRenderTargetView(g_b174oSeedBackBuffers[i].Get(),nullptr,h);g_b191BackBufferRtvs.push_back(h);h.ptr+=inc;}g_b191HudReady.store(g_b191BackBufferRtvs.size()==swapDesc.BufferCount,std::memory_order_release);}
 out<<"B196: EMBEDDED_COLOR_HUD_RESOURCES_READY rtv_heap="<<(g_b191RtvHeap?1:0)<<", rtv_count="<<g_b191BackBufferRtvs.size()<<", direct_into_xefg_backbuffer=1, xefg_ui_mode=NONE, hud_embedded_into_tagged_color_each_frame=1, separate_hud_hwnd=0, ready="<<(g_b191HudReady.load()?1:0)<<"\n";out.flush();
 QueryPerformanceCounter(&end);
 out<<"B174o: FRAME_RESOURCE_RING_PREWARM slots="<<kB173jDepthMotionSlotCount<<", prepared="<<prepared<<", size="<<width<<"x"<<height<<", d3d12_motion_outputs="<<prepared<<", d3d12_depth_outputs="<<prepared<<", wrapped_outputs="<<(prepared*2)<<", uavs="<<(prepared*2)<<", prefill_command_pairs="<<prepared<<", seed_command_pair="<<(g_b174oSeedList?1:0)<<", seed_backbuffers="<<g_b174oSeedBackBuffers.size()<<", live_d3d12_allocation_allowed=0, cpu_ms="<<b173g_elapsed_ms(begin,end)<<", ready="<<(SUCCEEDED(hr)&&prepared==kB173jDepthMotionSlotCount?1:0)<<", hr="<<(long)hr<<"\n";out.flush();
 out<<"B265: ORDERED_QUEUE_BURST_HEADROOM resource_slots="<<kB173jDepthMotionSlotCount<<", previous_resource_slots=10, ordered_queue_capacity="<<kB174oPendingCapacity<<", previous_queue_capacity=7, queue_capacity_changed=1, retirement_headroom=3, xell_low_latency=1, goal=absorb_short_private_present_stall_bursts_after_B264_queue_full_classification\n";out.flush();
 out<<"B266: QUEUE_FULL_XELL_PRESENT_CORRELATION behavior_change=0, queue_capacity="<<kB174oPendingCapacity<<", resource_slots="<<kB173jDepthMotionSlotCount<<", xell_low_latency=1, queue_full_transition_logging=1, worker_timing_snapshot=1, goal=separate_xell_sleep_pacing_from_private_present_service_time_at_backpressure\n";out.flush();
 out<<"B267: WORKER_PRIORITY_POLICY startup=THREAD_PRIORITY_HIGHEST, activation=THREAD_PRIORITY_HIGHEST, previous_activation=THREAD_PRIORITY_ABOVE_NORMAL, queue_capacity="<<kB174oPendingCapacity<<", resource_slots="<<kB173jDepthMotionSlotCount<<", xell_low_latency=1, xefg_interpolations=3, behavior_change=worker_scheduling_priority_only\n";out<<"B309: DIRECT_CAMERA_UNMAP_DEDUP_POLICY selected_resource_pointer_gate=1, gviewinverse_signature_bytes=64, duplicate_selected_unmaps_skip_full_copy=1, periodic_refresh_every_selected_unmaps=256, bounded_candidate_ring=64, expensive_matrix_validation=present_only, b308_per_call_split_disabled_after_direct_lock=1, map_unmap_hooks_remain_installed=1, xess_xefg_worker_unchanged=1, b276_x4_frozen=1\n";out<<"B306: SAFE_XELL_BACKLOG_POLICY xell_sleep_call_every_frame_id=1, low_latency_normal_when_queue_empty=1, temporary_no_wait_mode_when_pending_gt_0=1, low_latency_restored_before_frame_markers=1, queue_capacity="<<kB174oPendingCapacity<<", resource_slots="<<kB173jDepthMotionSlotCount<<", goal=prevent_startup_and_backlog_xell_sleep_from_delaying_real_x4_output\n";out.flush();
 out<<"B263: XEFG_PRESENT_STALL_CLASSIFIER threshold_ms=12, behavior_change=0, captures_every_long_present=1, records_backbuffer_rotation=1, records_stage_fence_progress=1, goal=classify_remaining_intermittent_private_present_stalls\n";out.flush();
 out<<"B386: FREEZE_ONLY_NONBLOCKING_PRESENT_POLICY private_present_do_not_wait=1, dxgi_was_still_drawing=nonfatal_drop_and_fence_retire, gta_capture_wait=0, ghosting_policy_unchanged=1, flash_policy_unchanged=1, performance_tuning=0\n";out.flush();out<<"B411: PRE_CAPTURE_BACKPRESSURE_POLICY threshold=5, admission_before_color_xess_fence=1, private_present_do_not_wait_retained=1, queue_capacity_unchanged=11, xefg_x4_request_unchanged=1, ghosting_unchanged=1\n";out.flush();out<<"B388: CONFIRMED_STALL_RECOVERY_POLICY trigger=capture_and_output_stale_over_500ms_plus_queue_drained, recovery=existing_b216_b252_live_input_rediscovery, f9_latch_preserved=1, xess_xefg_contexts_reused=1, stale_proxy_hidden_until_fresh_x4=1, ghosting_unchanged=1\n";out.flush();
 out<<"B310: PRIVATE_PRESENT_WAIT_PROFILE_POLICY thread_cpu_time=GetThreadTimes, wall_time=qpc, blocked_wait_estimate=wall_minus_thread_cpu, stage_fence_progress=1, queue_depth_before_after=1, diagnostic_only=1, camera_path_unchanged=1, xell_path_unchanged=1, xefg_x4_unchanged=1\n";out.flush();
 out<<"B392: FLIGHT_RECORDER_POLICY gta_and_worker_independent=1, hot_path_atomic_only=1, watchdog_thread_disk_io=1, stall_threshold_ms=20, stages={gta_entry,backbuffer_get,color_xess_capture,native_fence_signal_flush_queue_wait,job_queue_lock,xell_sleep_markers,worker_input_prepare,gpu_queue_wait_submit,xefg_tag_resources,private_backbuffer_prefill,xefg_private_present,xefg_status_and_retire}, behavior_change=diagnostic_only, ghosting_unchanged=1\n";out.flush();
 out<<"B393: RECORDER_CORRECTION_POLICY gta_stage_cleared_immediately_after_queue_lock_release=1, successful_path_no_stale_job_queue_lock_label=1, idle_source_entry_gap_classifier=1, source_gap_threshold_ms=20, worker_recorder_retained=1, runtime_render_behavior_unchanged=1, ghosting_unchanged=1\n";out.flush();
 out<<"B391: WHOLE_RUN_STALL_POLICY gta_render_hotpath_sync_logging_retired=1, camera_reset_logging_sparse_600_frames=1, dwm_queries_per_present=0, dxgi_stats_probes=0, getthreadtimes_per_present=0, long_present_multiline_logging=0, stall_boundary_threshold_ms=20, stall_boundary_rate_limit_ms=500, b390_fast_rebind_retained=1, ghosting_behavior_unchanged=1\n";out.flush();
 out<<"B311: PRIVATE_SWAPCHAIN_BUFFER_DEPTH_POLICY baseline_buffer_count=3, test_buffer_count=5, sync_interval=0, allow_tearing=1, only_runtime_change=private_swapchain_buffer_count, b310_profiler_retained=1, b309_camera_retained=1, b306_xell_retained=1, b276_x4_frozen=1\n";out.flush();
 out<<"B313: DWM_VBLANK_CORRELATION_POLICY dwm_get_composition_timing_info=1, qpc_vblank_before_after_present=1, refresh_period_logged=1, present_start_end_phase_logged=1, max_frame_latency_retained=16, five_buffer_swapchain_retained=1, sync_interval=0, allow_tearing=1, b310_profiler_retained=1, b309_camera_retained=1, b306_xell_retained=1, b276_x4_frozen=1\n";out.flush();
 out<<"B314: INDEPENDENT_FLIP_ELIGIBILITY_POLICY remove_ws_ex_transparent=1, retain_ws_ex_noactivate=1, retain_ws_ex_toolwindow=1, retain_httransparent_hit_test=1, retain_topmost_visibility=1, idxgi_swapchain_media_stats=1, composition_mode_mapping={0:COMPOSED,1:OVERLAY,2:NONE,3:COMPOSITION_FAILURE}, five_buffers=1, max_frame_latency=baseline_unmodified, sync_interval=0, allow_tearing=1, b313_vblank_retained=1, b310_profiler_retained=1, b309_camera_retained=1, b306_xell_retained=1, b276_x4_frozen=1\n";out.flush();
  out<<"B316: PERSISTENT_MONITOR_COVER_PRESENTATION_POLICY full_monitor_host_persistent=1, no_timed_ab=1, no_restore_transition=1, private_swapchain_size_unchanged=1, content_layout=monitor_stretch_retained_for_single_change, b314_window_style_retained=1, b313_vblank_profiler_retained=1, b327_latency1_test=0, b328_baseline_latency=1, b311_5_buffers_retained=1, b309_camera_retained=1, b306_xell_retained=1, b276_x4_frozen=1\n";out.flush();
 out<<"B325: PRIVATE_PRESENT_SATURATION_ADMISSION_PROFILER_POLICY diagnostic_only=1, sample_every_15th_present_when_queue_saturated=1, first_8_always_sampled=1, dxgi_frame_statistics_before_after=1, last_present_count_before_after=1, backbuffer_rotation_retained=1, dwm_refresh_delta_retained=1, stage_fence_progress_retained=1, thread_cpu_vs_blocked_wait_retained=1, b324_camera_optimization_frozen=1, xefg_x4_unchanged=1, profiler_retained=1, runtime_behavior_changed=0\n";out.flush();
 out<<"B328: QUEUE_ADMISSION_HEADROOM_POLICY max_frame_latency_mutation=0, xeFG_created_latency_retained=1, buffer_count=5, sync_interval=0, allow_tearing=1, queue_capacity="<<kB174oPendingCapacity<<", resource_slots="<<kB173jDepthMotionSlotCount<<", serial_worker=1, b325_profiler_retained=1, b324_camera_frozen=1, xefg_x4_unchanged=1, runtime_changes=queue_capacity_11_and_remove_b327_latency1\n";out.flush();out<<"B329: GHOSTING_TEMPORAL_HISTORY_GUARD_POLICY ghosting_only=1, abrupt_camera_history_reset=1, vp_max_threshold=0.08, vp_rms_threshold=0.030, spike_ratio=1.80, reconstructed_motion_unchanged=1, native_motion_runtime_switch=1, queue_capacity_unchanged="<<kB174oPendingCapacity<<", frame_latency_unchanged=1, xefg_x4_unchanged=1, input_latency_changes=0\n";out.flush();out<<"B330: NATIVE_MOTION_RUNTIME_POLICY ghosting_only=1, gta_native_motion=960x540_R16G16_FLOAT, runtime_switch=1, output_motion=1920x1080_R16G16_FLOAT, pixel_space_scale={2,2}, direction_assumption=current_to_previous, reconstructed_motion_fallback=1, b329_history_guard_retained=1, queue_unchanged=11, presentation_unchanged=1, xefg_x4_unchanged=1, input_latency_changes=0\n";out.flush();out<<"B331: NATIVE_MOTION_FAIL_OPEN_POLICY optional_shader=1, optional_shared_texture=1, optional_worker_open=1, activation_must_not_fail_from_native_motion=1, reconstructed_motion_fallback=1, b330_native_motion_logic_retained=1, xefg_x4_unchanged=1\n";out.flush();out<<"B332: NATIVE_MOTION_DIRECTION_AB_POLICY ghosting_only=1, camera_path_frozen=B291_B296_B309_B324, native_motion_input=960x540_R16G16_FLOAT, output_motion=1920x1080_R16G16_FLOAT, b330_direction=current_to_previous, b332_test_direction=previous_to_current, direction_transform=negate_xy, spatial_scale_retained={2,2}, reconstructed_motion_fallback=1, queue_unchanged=11, presentation_unchanged=1, xefg_x4_unchanged=1, input_latency_changes=0\n";out<<"B333: VISUAL_FORENSIC_POLICY retained_in_B335=1, runtime_change_when_idle=0, hotkey=F10, camera_path_frozen=B291_B296_B309_B324, xefg_x4_unchanged=1, presentation_unchanged=1, diagnostic_capture_may_hitch=1\n";out<<"B334: NATIVE_MOTION_RAW_SCALE_RESULT retained_as_evidence=1, candidate_960x540_R16G16_FLOAT_rejected_as_motion=1, runtime_use=0\n";out<<"B335: RECONSTRUCTED_MOTION_SAFE_FALLBACK_POLICY ghosting_only=1, native_runtime_use=0, rejected_candidate=960x540_R16G16_FLOAT, motion_source=reconstructed_depth_camera, camera_path_frozen=B291_B296_B309_B324, queue_unchanged=11, presentation_unchanged=1, xefg_x4_unchanged=1, input_latency_changes=0\n";out<<"B335: NATIVE_MOTION_REDISCOVERY_POLICY diagnostic_only=1, enumerate_vector_format_srvs=1, rejected_candidate_excluded_from_runtime=1, runtime_switch=0, no_blind_candidate_promotion=1\n";out<<"B335: GTA_ROOT_LOGGING_POLICY script_hook=GTA5_exe_directory, bridge=GTA5_exe_directory, performance=GTA5_exe_directory, runtime=GTA5_exe_directory, debug_capture=GTA5_exe_directory\n";out.flush();out<<"B340: CAMERA_CONSUMPTION_FIX_POLICY selected_camera_resource_unchanged=1, b339_second_chance_retained=1, fresh_valid_moving_candidate_preferred_over_static_candidate=1, stale_camera_reset_after_frames=2, stale_action=zero_motion_plus_reset_history, camera_identity=B291_B296_B309, b335_reconstructed_motion_retained=1\n";out.flush();out<<"B345: B343_GHOSTING_HARD_LOCK_POLICY CORE_RENDER_FG_LOCKED=1, b343_false_static_detection=frozen, zero_motion_on_unsafe_camera=frozen, repeated_reset_history_on_every_unsafe_frame=frozen, camera_resource_selection=frozen, b339_second_chance=frozen, b335_reconstructed_motion=frozen, user_directive=do_not_ever_touch_ghosting_feature, mutable_scope={private_present_and_flash_path_only}\n";out<<"B345: B343_REPEATED_RESET_RESTORED unsafe_threshold_frames=2, unsafe_action_every_frame=zero_motion_plus_reset_history_1, debounce=0, one_shot_reset=0, immutable_visual_path=1, xefg_x4_semantics_unchanged=1\n";out<<"B345: PRIVATE_PRESENT_ROLLBACK_POLICY b343_do_not_wait_removed=1, present_flags=ALLOW_TEARING_only, buffer_count=3, max_frame_latency=3, b342_no_hide_reveal_retained=1, queue_capacity=11, resource_slots=12\n";out<<"B352: B348_B349_HOLD_ROLLBACK_POLICY runtime_base=exact_B347_plus_initial_startup_quiet_reveal, reset_backbuffer_safe_seed=removed, seed_all_private_backbuffers_during_visible_reset=0, b345_ghosting_path=immutable, b342_no_hide_reveal=retained_for_established_session, private_swapchain_buffers=3, max_frame_latency=3, runtime_behavior_changed_from_B345=startup_visibility_gate_only\n";out<<"B352: DEVICE_REMOVED_FLASH_FORENSIC_POLICY diagnostic_only=1, present_error_capture=1, source_only_enter_exit_capture=1, proxy_visibility_mutation=0, reset_history_mutation=0, queue_mutation=0, present_mutation=0\n";out<<"B352: STARTUP_VALID_X4_LATCH_POLICY initial_only=1, minimum_hidden_elapsed_ms=1500, required_initial_valid_x4=1, valid_x4_latch_persists_across_result3=1, result3_does_not_restart_initial_gate=1, startup_deadline_is_soft_wait=1, established_session_recovery_required_x4=2, recovery_deadline_ms=1500, proxy_remains_hidden_until_gate=1, held_frame_prefill=0, backbuffer_runtime_copy=0, b345_ghosting_path=immutable, reset_history_policy=immutable\n";out<<"B358: B356_VISIBLE_RESET_PREFILL_SUPPRESSION_ROLLBACK_POLICY active=1, visible_reset_prefill=exact_B355, current_backbuffer_prefill_skipped=0, rotating_backbuffer_reuse_suppression=0, every_tagged_frame_prefills_current_color=1, b357_result3_reveal_retained=1, repeated_reset_exact_b343=1, ghosting_visual_path=IMMUTABLE, reset_history_policy=IMMUTABLE\n";out<<"B357: POST_LATCH_RESULT3_REVEAL_POLICY initial_only=1, prior_valid_x4_latch_required=1, minimum_hidden_elapsed_ms=1500, current_result3_allowed_for_first_reveal=1, startup_prefill_exact_b355_until_reveal=1, b356_suppression_after_reveal_only=0, b358_exact_b355_visible_prefill=1, reset_history_policy=IMMUTABLE, ghosting_visual_path=IMMUTABLE\n";out<<"B342: BLACK_FLASH_DIAGNOSTIC_POLICY retained=1, post_reveal_source_only_transition_logging=1, no_proxy_visibility_mutation=1, diagnostic_only=1\n";out<<"B340: FREEZE_AB_POLICY private_swapchain_max_frame_latency=3, buffer_count=3, previous_buffer_count=5, b339_backlog_collapse_disabled=1, sync_interval=0, allow_tearing=1, queue_capacity=11, resource_slots=12, xefg_x4_semantics_unchanged=1, core_render_fg_locked=1\n";out<<"B337: STARTUP_SEED_GATE_FIX_POLICY retained=1, initial_reset_seed_result3_is_warmup=1, proxy_stays_hidden=1, f9_latch_preserved=1, historical_startup_deadline_ms=1500, b352_initial_startup_soft_deadline_ms=5000, recovery_deadline_ms=1500, deadline_not_extended_by_seed=1, initial_reveal_requires_one_valid_x4_plus_1500ms=1, core_render_fg_locked=1\n";out.flush();
 return SUCCEEDED(hr)&&prepared==kB173jDepthMotionSlotCount?S_OK:FAILED(hr)?hr:E_FAIL;
}
static HRESULT b174i_signal_preparation(UINT64&value)
{
 if(!g_b174iPreparationQueue||!g_b174iPreparationFence)return E_NOINTERFACE;
 value=g_b174iPreparationFenceValue.fetch_add(1,std::memory_order_acq_rel)+1;
 return g_b174iPreparationQueue->Signal(g_b174iPreparationFence.Get(),value);
}
static HRESULT b173e_prepare_native_depth_shaders(ID3D11Device*device,std::ofstream&out){
 if(g_b173eNativeDepthVs&&g_b173eNativeDepthPs&&g_b173eNativeDepthRs)return S_OK;if(!device)return E_INVALIDARG;
 static const char source[]=R"(
Texture2D<float> sourceDepth : register(t0);
struct VsOut { float4 position : SV_Position; };
VsOut vsMain(uint id : SV_VertexID) {
 VsOut output;
 float2 position=float2((id==2)?3.0:-1.0,(id==1)?3.0:-1.0);
 output.position=float4(position,0.0,1.0);
 return output;
}
float4 psMain(VsOut input) : SV_Target {
 float z=saturate(sourceDepth.Load(int3(int2(input.position.xy),0)));
 uint packed=(uint)round(z*16777215.0);
 float3 bytes=float3(packed&255,(packed>>8)&255,(packed>>16)&255);
 return float4(bytes/255.0,1.0);
}
)";
 ComPtr<ID3DBlob>vsBytecode,psBytecode,errors;HRESULT hr=D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"vsMain","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vsBytecode,&errors);
 out<<"B173f: stage=CompileNativeDepthTransportVS, bytecode="<<(vsBytecode?1:0)<<", hr="<<(long)hr;if(errors&&errors->GetBufferPointer())out<<", compiler_message="<<(const char*)errors->GetBufferPointer();out<<"\n";out.flush();if(FAILED(hr))return hr;
 errors.Reset();hr=D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"psMain","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&psBytecode,&errors);
 out<<"B173f: stage=CompileNativeDepthTransportPS, bytecode="<<(psBytecode?1:0)<<", hr="<<(long)hr;if(errors&&errors->GetBufferPointer())out<<", compiler_message="<<(const char*)errors->GetBufferPointer();out<<"\n";out.flush();if(FAILED(hr))return hr;
 hr=device->CreateVertexShader(vsBytecode->GetBufferPointer(),vsBytecode->GetBufferSize(),nullptr,&g_b173eNativeDepthVs);if(FAILED(hr))return hr;
 hr=device->CreatePixelShader(psBytecode->GetBufferPointer(),psBytecode->GetBufferSize(),nullptr,&g_b173eNativeDepthPs);if(FAILED(hr))return hr;
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
 hr=device->CreateRasterizerState(&raster,&g_b173eNativeDepthRs);out<<"B173f: stage=CreateNativeDepthTransportPipeline, vs="<<(g_b173eNativeDepthVs?1:0)<<", ps="<<(g_b173eNativeDepthPs?1:0)<<", raster="<<(g_b173eNativeDepthRs?1:0)<<", packed_depth_bits=24, hr="<<(long)hr<<"\n";out.flush();return hr;
}
static HRESULT b173f_prepare_shared_fence(ID3D11Device*device,ID3D11DeviceContext*context,std::ofstream&out){
 if(g_b173fNativeDevice5&&g_b173fNativeContext4&&g_b173fNativeFence&&g_b173fSharedFence12)return S_OK;
 if(!device||!context||!g_device12)return E_INVALIDARG;
 ComPtr<ID3D11Device5>device5;ComPtr<ID3D11DeviceContext4>context4;ComPtr<ID3D11Fence>fence11;ComPtr<ID3D12Fence>fence12;
 HRESULT deviceHr=device->QueryInterface(IID_PPV_ARGS(&device5));HRESULT contextHr=context->QueryInterface(IID_PPV_ARGS(&context4));HRESULT createHr=FAILED(deviceHr)?deviceHr:FAILED(contextHr)?contextHr:device5->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&fence11));
 HANDLE sharedHandle{};HRESULT handleHr=SUCCEEDED(createHr)?fence11->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&sharedHandle):createHr;HRESULT openHr=SUCCEEDED(handleHr)&&sharedHandle?g_device12->OpenSharedHandle(sharedHandle,IID_PPV_ARGS(&fence12)):(FAILED(handleHr)?handleHr:E_FAIL);if(sharedHandle)CloseHandle(sharedHandle);
 HRESULT hr=FAILED(deviceHr)?deviceHr:FAILED(contextHr)?contextHr:FAILED(createHr)?createHr:FAILED(handleHr)?handleHr:openHr;
 out<<"B173f: stage=CreateSharedGpuFence, device5="<<(device5?1:0)<<", context4="<<(context4?1:0)<<", fence11="<<(fence11?1:0)<<", fence12="<<(fence12?1:0)<<", device_hr="<<(long)deviceHr<<", context_hr="<<(long)contextHr<<", create_hr="<<(long)createHr<<", handle_hr="<<(long)handleHr<<", open_hr="<<(long)openHr<<", handle_closed=1, hr="<<(long)hr<<"\n";out.flush();
 if(FAILED(hr))return hr;g_b173fNativeDevice5=device5;g_b173fNativeContext4=context4;g_b173fNativeFence=fence11;g_b173fSharedFence12=fence12;g_b173fFenceValue=0;return S_OK;
}
static HRESULT b173j_prepare_depth_motion_slot(B173jDepthMotionSlot&slot,ID3D11Device*nativeDevice,UINT width,UINT height)
{
 if(!nativeDevice||!g_on12Device||!g_on12||!g_device12||!width||!height)return E_INVALIDARG;
 if(slot.ready&&slot.width==width&&slot.height==height)return S_FALSE;
 slot=B173jDepthMotionSlot{};
 D3D11_TEXTURE2D_DESC privateDesc{};privateDesc.Width=width;privateDesc.Height=height;privateDesc.MipLevels=1;privateDesc.ArraySize=1;privateDesc.Format=DXGI_FORMAT_R24G8_TYPELESS;privateDesc.SampleDesc.Count=1;privateDesc.Usage=D3D11_USAGE_DEFAULT;privateDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 HRESULT hr=nativeDevice->CreateTexture2D(&privateDesc,nullptr,&slot.privateDepth);if(FAILED(hr)||!slot.privateDepth)return FAILED(hr)?hr:E_FAIL;
 D3D11_SHADER_RESOURCE_VIEW_DESC nativeSrvDesc{};nativeSrvDesc.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;nativeSrvDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;nativeSrvDesc.Texture2D.MipLevels=1;
 hr=nativeDevice->CreateShaderResourceView(slot.privateDepth.Get(),&nativeSrvDesc,&slot.nativeDepthSrv);if(FAILED(hr)||!slot.nativeDepthSrv)return FAILED(hr)?hr:E_FAIL;
 D3D11_TEXTURE2D_DESC sharedDesc{};sharedDesc.Width=width;sharedDesc.Height=height;sharedDesc.MipLevels=1;sharedDesc.ArraySize=1;sharedDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sharedDesc.SampleDesc.Count=1;sharedDesc.Usage=D3D11_USAGE_DEFAULT;sharedDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;sharedDesc.MiscFlags=D3D11_RESOURCE_MISC_SHARED;
 hr=nativeDevice->CreateTexture2D(&sharedDesc,nullptr,&slot.sharedDepth);if(FAILED(hr)||!slot.sharedDepth)return FAILED(hr)?hr:E_FAIL;
 hr=nativeDevice->CreateRenderTargetView(slot.sharedDepth.Get(),nullptr,&slot.nativeDepthRtv);if(FAILED(hr)||!slot.nativeDepthRtv)return FAILED(hr)?hr:E_FAIL;
 hr=nativeDevice->CreateDeferredContext(0,&slot.deferredContext);if(FAILED(hr)||!slot.deferredContext)return FAILED(hr)?hr:E_FAIL;
 D3D11_TEXTURE2D_DESC motionDesc{};motionDesc.Width=width;motionDesc.Height=height;motionDesc.MipLevels=1;motionDesc.ArraySize=1;motionDesc.Format=DXGI_FORMAT_R16G16_FLOAT;motionDesc.SampleDesc.Count=1;motionDesc.Usage=D3D11_USAGE_DEFAULT;motionDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
 hr=nativeDevice->CreateTexture2D(&motionDesc,nullptr,&slot.nativeMotion);if(FAILED(hr))return hr;
 hr=nativeDevice->CreateUnorderedAccessView(slot.nativeMotion.Get(),nullptr,&slot.nativeMotionUav);if(FAILED(hr))return hr;
 D3D11_TEXTURE2D_DESC nativeInputDesc{};nativeInputDesc.Width=960;nativeInputDesc.Height=540;nativeInputDesc.MipLevels=1;nativeInputDesc.ArraySize=1;nativeInputDesc.Format=DXGI_FORMAT_R16G16_FLOAT;nativeInputDesc.SampleDesc.Count=1;nativeInputDesc.Usage=D3D11_USAGE_DEFAULT;nativeInputDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;nativeInputDesc.MiscFlags=D3D11_RESOURCE_MISC_SHARED;
 HRESULT nativeInputHr=nativeDevice->CreateTexture2D(&nativeInputDesc,nullptr,&slot.sharedNativeMotion);if(FAILED(nativeInputHr))slot.sharedNativeMotion.Reset();
 D3D11_BUFFER_DESC constantsDesc{};constantsDesc.ByteWidth=256;constantsDesc.Usage=D3D11_USAGE_DEFAULT;constantsDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 hr=nativeDevice->CreateBuffer(&constantsDesc,nullptr,&slot.nativeMotionConstants);if(FAILED(hr))return hr;
 slot.width=width;slot.height=height;slot.ready=true;return S_OK;
}
static HRESULT b173j_prepare_depth_motion_pool(UINT width,UINT height,std::ofstream&out)
{
 LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);ComPtr<ID3D11Device>nativeDevice;if(g_nativeContext)g_nativeContext->GetDevice(&nativeDevice);
 HRESULT hr=nativeDevice?b173e_prepare_native_depth_shaders(nativeDevice.Get(),out):E_NOINTERFACE;
 if(SUCCEEDED(hr))hr=b175b_prepare_native_motion_shader(nativeDevice.Get(),out);
 if(SUCCEEDED(hr))hr=b173_prepare_compute(out);
 if(SUCCEEDED(hr))hr=b173f_prepare_shared_fence(nativeDevice.Get(),g_nativeContext.Get(),out);
 if(SUCCEEDED(hr))hr=b173n_prepare_stage_fence(out);
 unsigned prepared=0,reused=0;
 for(unsigned i=0;SUCCEEDED(hr)&&i<kB173jDepthMotionSlotCount;++i){HRESULT slotHr=b173j_prepare_depth_motion_slot(g_b173jDepthMotionSlots[i],nativeDevice.Get(),width,height);if(slotHr==S_FALSE)++reused;else if(SUCCEEDED(slotHr))++prepared;else hr=slotHr;}
 const bool ready=SUCCEEDED(hr);g_b173jDepthMotionPoolReady.store(ready,std::memory_order_release);if(ready)for(auto&state:g_b174DepthSlotState)state.store(0,std::memory_order_release);QueryPerformanceCounter(&end);
 out<<"B173m: NATIVE_DEPTH_POOL_PREWARM slots="<<kB173jDepthMotionSlotCount<<", prepared="<<prepared<<", reused="<<reused<<", size="<<width<<"x"<<height<<", private_depth=1, packed_depth=1, opened_depth=1, motion_output=0, depth_output=0, wrapped_outputs=0, deferred_context=1, cpu_ms="<<b173g_elapsed_ms(begin,end)<<", ready="<<(ready?1:0)<<", hr="<<(long)hr<<"\n";out.flush();return hr;
}
extern "C" __declspec(dllexport) void WINAPI B407MarkUnsafeCameraFrame(){
 g_b407LastUnsafeCameraTick.store(GetTickCount64(),std::memory_order_release);
 g_b408UnsafeEpisodeActive.store(true,std::memory_order_release);
 g_b408StableSafeFrames.store(0,std::memory_order_release);
}
extern "C" __declspec(dllexport) void WINAPI B408MarkSafeCameraFrame(){
 unsigned n=g_b408StableSafeFrames.load(std::memory_order_acquire);
 while(n<30&&!g_b408StableSafeFrames.compare_exchange_weak(n,n+1,std::memory_order_acq_rel,std::memory_order_acquire)){}
 if((n<30?n+1:n)>=30){g_b408UnsafeEpisodeActive.store(false,std::memory_order_release);g_b407UnsafeResetConsumed.store(false,std::memory_order_release);}
}
// B461: B437 is the final camera authority. Once a recovered authoritative camera has
// passed B437 and B453 has atomically rebased both motion histories, the older B408
// classifier episode must not keep the worker in its 30-safe-frame quarantine.


extern "C" __declspec(dllexport) void WINAPI B297ResetCameraHistory(){
 AcquireSRWLockExclusive(&g_b175MotionHistoryLock);
 g_b175HaveConsumedViewProjection=false;
 g_b175bHaveXessPreviousViewProjection=false;
 ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 g_b288CameraResetPending.store(true,std::memory_order_release);
 g_b175bXessReset.store(true,std::memory_order_release);
}

extern "C" __declspec(dllexport) void WINAPI B345ApplyCameraZeroMotionSafety(BOOL resetTemporalHistory){
 // B406: retain the protected zero-motion ghosting safety on every unsafe frame, but temporal
 // history is invalidated only on entry to an unsafe-camera episode. Re-clearing it every frame
 // prevented XeFG from accumulating temporal history and produced long result=3/source-only runs.
 if(resetTemporalHistory){
  AcquireSRWLockExclusive(&g_b175MotionHistoryLock);
  g_b175HaveConsumedViewProjection=false;
  g_b175bHaveXessPreviousViewProjection=false;
  ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 }
 const BOOL b384Reset=(b385_load_protected_modules()&&g_b385GhostUnsafe)?g_b385GhostUnsafe(resetTemporalHistory):FALSE;
 if(resetTemporalHistory&&b384Reset){
  g_b288CameraResetPending.store(true,std::memory_order_release);
  g_b175bXessReset.store(true,std::memory_order_release);
 }
}

static void b436_mul4x4(const float*a,const float*b,double*out){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double v=0.0;for(unsigned k=0;k<4;++k)v+=(double)a[r*4+k]*(double)b[k*4+c];out[r*4+c]=v;}}
static double b436_identity_residual(const double*m){double worst=0.0;for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){const double target=r==c?1.0:0.0;worst=(std::max)(worst,std::fabs(m[r*4+c]-target));}return worst;}
static double b437_view_projection_residual(const float*invVp,const float*view,const float*projection,float*outViewProjection){
 if(!invVp||!view||!projection)return 960.0;
 double viewProj[16]{},vpInv[16]{},invVpVp[16]{};b436_mul4x4(view,projection,viewProj);
 float vpF[16]{};for(unsigned i=0;i<16;++i){vpF[i]=(float)viewProj[i];if(outViewProjection)outViewProjection[i]=vpF[i];}
 b436_mul4x4(vpF,invVp,vpInv);b436_mul4x4(invVp,vpF,invVpVp);
 const double residual=(std::min)(b436_identity_residual(vpInv),b436_identity_residual(invVpVp));
 return std::isfinite(residual)?residual:960.0;
}
static void b436_motion_transform_diagnostic(const float*invVp,const float*view,const float*projection,unsigned long long frame,const char*path){
 if(!invVp||!view||!projection||!path||frame%120)return;
 double viewProj[16]{},projView[16]{},vpInv[16]{},invVpVp[16]{},pvInv[16]{},invPv[16]{};
 b436_mul4x4(view,projection,viewProj);b436_mul4x4(projection,view,projView);
 float vpF[16]{},pvF[16]{};for(unsigned i=0;i<16;++i){vpF[i]=(float)viewProj[i];pvF[i]=(float)projView[i];}
 b436_mul4x4(vpF,invVp,vpInv);b436_mul4x4(invVp,vpF,invVpVp);b436_mul4x4(pvF,invVp,pvInv);b436_mul4x4(invVp,pvF,invPv);
 const double rowOrder=(std::min)(b436_identity_residual(vpInv),b436_identity_residual(invVpVp));
 const double columnOrder=(std::min)(b436_identity_residual(pvInv),b436_identity_residual(invPv));
 const char*best=rowOrder<columnOrder?"view_times_projection":"projection_times_view";
 std::ofstream d(path,std::ios::app);d<<"B436: MOTION_TRANSFORM_CONVENTION_PROBE source_frame_id="<<frame<<", view_projection_inverse_residual="<<rowOrder<<", projection_view_inverse_residual="<<columnOrder<<", best_order="<<best<<", shader_decl=row_major, shader_mul=matrix_times_vector, depth_ndc_range_assumed=zero_to_one, runtime_motion_unchanged=1, diagnostic_only=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";d.flush();
}

static std::atomic<unsigned> g_b440InvalidMotionCameraStreak{0};
extern "C" __declspec(dllexport) unsigned WINAPI B440InvalidMotionCameraStreak(){return g_b440InvalidMotionCameraStreak.load(std::memory_order_acquire);}

extern "C" __declspec(dllexport) HRESULT WINAPI B173SubmitGpuFrame(ID3D11DeviceContext*nativeContext,ID3D11Texture2D*nativeDepth,const float*previousViewProjection,const float*currentInverseViewProjection,const float*currentView,const float*currentProjection,UINT width,UINT height,const char*path){
 if(!nativeContext||!nativeDepth||!previousViewProjection||!currentInverseViewProjection||!currentView||!currentProjection||!width||!height||!path||!g_device12)return E_INVALIDARG;
 const bool b434AlignValidatedBaseline=B434ConsumeValidatedBaselineCommit()!=FALSE;
 auto b447TupleFingerprint=[](const float*vp,const float*inv,const float*view,const float*proj)->unsigned long long{unsigned long long h=1469598103934665603ULL;const float*parts[4]={vp,inv,view,proj};for(const float*part:parts){const unsigned char*b=(const unsigned char*)part;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}}return h;};
 if((g_b172GtaFrame<=6)||(g_b172GtaFrame%120)==0){std::ofstream b447(path,std::ios::app);b447<<"B447: CAMERA_TUPLE_BOUNDARY source_frame_id="<<g_b172GtaFrame<<", boundary=B437_ENTRY, tuple_fingerprint="<<b447TupleFingerprint(previousViewProjection,currentInverseViewProjection,currentView,currentProjection)<<", vp_component=caller_supplied_first_tuple_component, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 b436_motion_transform_diagnostic(currentInverseViewProjection,currentView,currentProjection,g_b172GtaFrame,path);
 float b437ValidatedCurrentVp[16]{};const double b437VpResidual=b437_view_projection_residual(currentInverseViewProjection,currentView,currentProjection,b437ValidatedCurrentVp);
 const bool b437MotionCameraValid=b437VpResidual<=0.01;if(b437MotionCameraValid)g_b440InvalidMotionCameraStreak.store(0,std::memory_order_release);else g_b440InvalidMotionCameraStreak.fetch_add(1,std::memory_order_acq_rel);static std::atomic<bool> b437LastMotionCameraValid{true};const bool b437PriorValid=b437LastMotionCameraValid.exchange(b437MotionCameraValid,std::memory_order_acq_rel);const bool b437ValidityTransition=b437PriorValid!=b437MotionCameraValid;
 // B458: B437 is the final authority for camera validity. Capture the exact tuple only after
 // this final mathematical gate succeeds. If a later candidate fails, keep the frame unsafe/zero-motion
 // but use the last B437-valid inverse/view/projection as the stable reconstruction basis.
 static float b458LastValidatedVp[16]{},b458LastValidatedInvVp[16]{},b458LastValidatedView[16]{},b458LastValidatedProjection[16]{};static bool b458HaveValidatedCamera=false;static unsigned long long b458ValidatedFrame=0;
 if(b437MotionCameraValid){std::memcpy(b458LastValidatedVp,b437ValidatedCurrentVp,64);std::memcpy(b458LastValidatedInvVp,currentInverseViewProjection,64);std::memcpy(b458LastValidatedView,currentView,64);std::memcpy(b458LastValidatedProjection,currentProjection,64);b458HaveValidatedCamera=true;b458ValidatedFrame=g_b172GtaFrame;}
 const float*b458EffectiveInvVp=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedInvVp:currentInverseViewProjection;const float*b458EffectiveView=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedView:currentView;const float*b458EffectiveProjection=(!b437MotionCameraValid&&b458HaveValidatedCamera)?b458LastValidatedProjection:currentProjection;
 std::ofstream out(path,std::ios::app);
 if(!b437MotionCameraValid&&b458HaveValidatedCamera&&(b437ValidityTransition||(g_b172GtaFrame%120)==0))out<<"B458: B437_VALIDATED_AUTHORITATIVE_CAMERA_HOLD source_frame_id="<<g_b172GtaFrame<<", last_validated_source_frame_id="<<b458ValidatedFrame<<", rejected_inverse_residual="<<b437VpResidual<<", action=retain_B437_valid_reconstruction_basis_plus_existing_zero_motion_unsafe_state, b453_rebase_on_fresh_validity=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
 if(b437ValidityTransition||(g_b172GtaFrame%120)==0)out<<"B437: VALIDATED_MOTION_CAMERA_GATE source_frame_id="<<g_b172GtaFrame<<", view_projection_inverse_residual="<<b437VpResidual<<", valid="<<(b437MotionCameraValid?1:0)<<", prior_valid="<<(b437PriorValid?1:0)<<", transition="<<(b437ValidityTransition?1:0)<<", valid_order=view_times_projection, invalid_action=zero_motion_preserve_last_valid_history, return_action=rebase_then_resume, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";LARGE_INTEGER begin{},afterDepthPack{},afterMotionSubmit{},end{};QueryPerformanceCounter(&begin);auto fail=[&](const char*stage,HRESULT value){out<<"B174c: NATIVE_INPUT_CAPTURE_FAILED stage="<<stage<<", hr="<<(long)value<<"\n";out.flush();return value;};
 D3D11_TEXTURE2D_DESC sourceDesc{};nativeDepth->GetDesc(&sourceDesc);if(sourceDesc.Width!=width||sourceDesc.Height!=height||sourceDesc.Format!=DXGI_FORMAT_R24G8_TYPELESS||sourceDesc.SampleDesc.Count!=1)return fail("ValidateNativeDepth",E_INVALIDARG);
 ComPtr<ID3D11Device>nativeDevice;nativeContext->GetDevice(&nativeDevice);if(!nativeDevice)return fail("GetNativeDevice",E_NOINTERFACE);
 HRESULT hr=b173e_prepare_native_depth_shaders(nativeDevice.Get(),out);if(FAILED(hr))return fail("PrepareNativeDepthShaders",hr);
 const unsigned slotIndex=g_b174CaptureSlot;if(slotIndex>=kB173jDepthMotionSlotCount||!g_b173jDepthMotionPoolReady.load(std::memory_order_acquire))return fail("ValidateReservedDepthSlot",E_FAIL);
 auto*slot=&g_b173jDepthMotionSlots[slotIndex];if(!slot->ready||slot->width!=width||slot->height!=height)return fail("ValidateReservedDepthSlot",E_FAIL);
 nativeContext->CopyResource(slot->privateDepth.Get(),nativeDepth);
 bool b330NativeMotionCaptured=false; // B335: rejected 960x540 R16G16 candidate is diagnostic-only; never copy it into XeFG motion input.
 if((g_b172GtaFrame%60)==0)out<<"B335: RECONSTRUCTED_MOTION_SAFE_FALLBACK source_frame_id="<<g_b172GtaFrame<<", native_runtime_use=0, rejected_candidate=960x540_R16G16_FLOAT, motion_source=reconstructed_depth_camera, camera_path_frozen=B291_B296_B309_B324\n";
 D3D11_VIEWPORT viewport{};viewport.Width=(FLOAT)width;viewport.Height=(FLOAT)height;viewport.MinDepth=0.0f;viewport.MaxDepth=1.0f;
 ID3D11RenderTargetView*rtvs[]{slot->nativeDepthRtv.Get()};ID3D11ShaderResourceView*nativeSrvs[]{slot->nativeDepthSrv.Get()};
 slot->deferredContext->OMSetRenderTargets(1,rtvs,nullptr);slot->deferredContext->RSSetViewports(1,&viewport);slot->deferredContext->RSSetState(g_b173eNativeDepthRs.Get());slot->deferredContext->IASetInputLayout(nullptr);slot->deferredContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);slot->deferredContext->VSSetShader(g_b173eNativeDepthVs.Get(),nullptr,0);slot->deferredContext->PSSetShader(g_b173eNativeDepthPs.Get(),nullptr,0);slot->deferredContext->PSSetShaderResources(0,1,nativeSrvs);slot->deferredContext->Draw(3,0);
 QueryPerformanceCounter(&afterDepthPack);
 ID3D11ShaderResourceView*nullPsSrv{};slot->deferredContext->PSSetShaderResources(0,1,&nullPsSrv);
 // B453: the authoritative B451 camera can become valid before worker-side B329 sees the discontinuity.
 // Rebase every capture-side temporal baseline atomically before reconstructed motion is dispatched,
 // so the transition frame is zero-motion instead of a full-screen stale-history vector.
 bool b453AtomicTemporalRebase=false;double b453ConsumedMax=0.0,b453ConsumedRms=0.0;bool b453HadConsumed=false;bool b452HadXessPrevious=false;
 AcquireSRWLockExclusive(&g_b175MotionHistoryLock);
 b453HadConsumed=g_b175HaveConsumedViewProjection;b452HadXessPrevious=g_b175bHaveXessPreviousViewProjection;
 if(b437MotionCameraValid&&b453HadConsumed){for(unsigned i=0;i<16;++i){const double d=(double)b437ValidatedCurrentVp[i]-(double)g_b175ConsumedViewProjection[i];b453ConsumedMax=(std::max)(b453ConsumedMax,std::fabs(d));b453ConsumedRms+=d*d;}b453ConsumedRms=std::sqrt(b453ConsumedRms/16.0);}
 // B472: B471 runtime evidence showed the B453 0.08/0.030 thresholds fire during ordinary live camera motion, repeatedly forcing previous=current and XeFG history resets. Preserve transition/baseline rebases, but reserve the numeric fallback for a genuine large camera discontinuity.
 b453AtomicTemporalRebase=b437MotionCameraValid&&(b437ValidityTransition||b434AlignValidatedBaseline||(b453HadConsumed&&(b453ConsumedMax>=2.0||b453ConsumedRms>=0.500)));
 if(b453AtomicTemporalRebase){std::memcpy(g_b175ConsumedViewProjection,b437ValidatedCurrentVp,64);g_b175HaveConsumedViewProjection=true;std::memcpy(g_b175bXessPreviousViewProjection,b437ValidatedCurrentVp,64);g_b175bHaveXessPreviousViewProjection=true;}
 B173GpuConstants xessMotionConstants{};std::memcpy(xessMotionConstants.previousViewProjection,b453AtomicTemporalRebase?b437ValidatedCurrentVp:(b434AlignValidatedBaseline?previousViewProjection:(b452HadXessPrevious?g_b175bXessPreviousViewProjection:previousViewProjection)),64);
 ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);
 if(b437MotionCameraValid&&b453HadConsumed&&!b453AtomicTemporalRebase&&(b453ConsumedMax>=0.08||b453ConsumedRms>=0.030)){static unsigned b472Preserved=0;if((++b472Preserved<=12)||(b472Preserved%120)==0)out<<"B472: NORMAL_CAMERA_MOTION_HISTORY_PRESERVED source_frame_id="<<g_b172GtaFrame<<", vp_max_abs_delta="<<b453ConsumedMax<<", vp_rms_delta="<<b453ConsumedRms<<", old_b453_threshold_would_reset=1, previous_history_preserved=1, xefg_reset_history_requested=0, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, motion_scale_unchanged=1, motion_direction_unchanged=1\n";}
 if(b453AtomicTemporalRebase){g_b288CameraResetPending.store(true,std::memory_order_release);g_b175bXessReset.store(true,std::memory_order_release);out<<"B453: AUTHORITATIVE_CAMERA_ATOMIC_TEMPORAL_REBASE source_frame_id="<<g_b172GtaFrame<<", trigger="<<(b437ValidityTransition?"validity_transition":(b434AlignValidatedBaseline?"validated_baseline_alignment":"abrupt_authoritative_camera_delta"))<<", prior_consumed_available="<<(b453HadConsumed?1:0)<<", prior_xess_available="<<(b452HadXessPrevious?1:0)<<", vp_max_abs_delta="<<b453ConsumedMax<<", vp_rms_delta="<<b453ConsumedRms<<", reconstructed_motion_previous_equals_current=1, xess_previous_equals_current=1, consumed_history_rebased=1, xess_history_rebased=1, xefg_reset_history_requested=1, motion_scale_unchanged=1, motion_direction_unchanged=1, b451_camera_handoff_unchanged=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";}

 std::memcpy(xessMotionConstants.currentInverseViewProjection,b458EffectiveInvVp,64);
 // B495 runtime ghosting fix: restore the accepted B458/B453 temporal authority at the motion boundary.
 // Later B474 distinct-VP substitution and B493 synthetic exact-inverse replacement are bypassed here.
 // The current frame uses the B437/B458 validated camera tuple; previous motion comes only from the
 // accepted B453/XeSS history path above. B423 freezing and XeFG presentation are untouched.
 const bool b495AcceptedB458TemporalRuntime=true;
 static unsigned b495LogOrdinal=0;if((++b495LogOrdinal<=12)||(b495LogOrdinal%120)==0)out<<"B495: ACCEPTED_B458_GHOSTING_RUNTIME_RESTORED source_frame_id="<<g_b172GtaFrame<<", b474_distinct_vp_override=0, b493_synthetic_inverse_override=0, b458_camera_authority=1, b453_temporal_authority=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1, runtime_behavior_changed=1\n";
 float b474TemporalPreviousVp[16]{};bool b474UsedDistinctPrevious=false;double b474DistinctMax=0.0,b474DistinctRms=0.0;bool b474TupleAdvanced=false;
 std::memcpy(b474TemporalPreviousVp,xessMotionConstants.previousViewProjection,64);
 // B482 diagnostic-only: fingerprint the validated current/previous VP pair at the final B474 boundary.
 static unsigned b482MotionTraceOrdinal=0;const bool b482MotionTrace=((++b482MotionTraceOrdinal)<=24)||((b482MotionTraceOrdinal%120)==0);
 if(b482MotionTrace){auto b482Fp=[](const float*m)->unsigned long long{unsigned long long h=1469598103934665603ULL;const unsigned char*b=(const unsigned char*)m;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}return h;};out<<"B482: VALIDATED_VP_TO_MOTION_DISPATCH source_frame_id="<<g_b172GtaFrame<<", camera_valid="<<(b437MotionCameraValid?1:0)<<", validity_transition="<<(b437ValidityTransition?1:0)<<", b474_used_distinct_previous="<<(b474UsedDistinctPrevious?1:0)<<", b474_tuple_advanced="<<(b474TupleAdvanced?1:0)<<", validated_current_vp_fingerprint="<<b482Fp(b437ValidatedCurrentVp)<<", motion_previous_vp_fingerprint="<<b482Fp(xessMotionConstants.previousViewProjection)<<", vp_max_abs_delta="<<b474DistinctMax<<", vp_rms_delta="<<b474DistinctRms<<", motion_direction=current_to_previous_pixel_space, motion_scale={1,1}, b474_history_policy_unchanged=1, b437_gate_unchanged=1, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 xessMotionConstants.width=width;xessMotionConstants.height=height;
 slot->deferredContext->UpdateSubresource(slot->nativeMotionConstants.Get(),0,nullptr,&xessMotionConstants,0,0);ID3D11Buffer*motionCbs[]{slot->nativeMotionConstants.Get()};ID3D11ShaderResourceView*motionSrvs[]{slot->nativeDepthSrv.Get()};ID3D11UnorderedAccessView*motionUavs[]{slot->nativeMotionUav.Get()};
 slot->deferredContext->CSSetShader(g_b175bNativeMotionShader.Get(),nullptr,0);slot->deferredContext->CSSetConstantBuffers(0,1,motionCbs);slot->deferredContext->CSSetShaderResources(0,1,motionSrvs);slot->deferredContext->CSSetUnorderedAccessViews(0,1,motionUavs,nullptr);slot->deferredContext->Dispatch((width+7)/8,(height+7)/8,1);
 QueryPerformanceCounter(&afterMotionSubmit);
 ID3D11UnorderedAccessView*nullMotionUav{};ID3D11ShaderResourceView*nullMotionSrv{};slot->deferredContext->CSSetUnorderedAccessViews(0,1,&nullMotionUav,nullptr);slot->deferredContext->CSSetShaderResources(0,1,&nullMotionSrv);slot->deferredContext->CSSetShader(nullptr,nullptr,0);
 ComPtr<ID3D11CommandList>depthCommands;hr=slot->deferredContext->FinishCommandList(FALSE,&depthCommands);if(FAILED(hr))return fail("FinishNativeDepthMotionCommandList",hr);
 nativeContext->ExecuteCommandList(depthCommands.Get(),TRUE);
 g_b174CaptureJob=B173hLiveJob{};g_b174CaptureJob.captureTime=g_b172Now;g_b174CaptureJob.width=width;g_b174CaptureJob.height=height;g_b174CaptureJob.slotIndex=slotIndex;g_b174CaptureJob.sourceFrame=g_b172GtaFrame;
 g_b174CaptureJob.b330NativeMotionActive=b330NativeMotionCaptured;g_b174CaptureJob.b434ValidatedBaselineAlignment=b434AlignValidatedBaseline;g_b174CaptureJob.b437MotionCameraValid=b437MotionCameraValid;g_b174CaptureJob.b437MotionValidityTransition=b437ValidityTransition;
 g_b174CaptureJob.constants=g_b171LiveConstants;g_b174CaptureJob.sourceFrameRenderTimeMs=(double)g_b245ObservedFrameIntervalUs.load(std::memory_order_acquire)/1000.0;g_b174CaptureJob.constants.motionVectorScaleX=1.0f;g_b174CaptureJob.constants.motionVectorScaleY=1.0f;g_b174CaptureJob.constants.jitterOffsetX=0.0f;g_b174CaptureJob.constants.jitterOffsetY=0.0f;
 AcquireSRWLockShared(&g_b175MotionHistoryLock);std::memcpy(g_b174CaptureJob.previousViewProjection,b453AtomicTemporalRebase?b437ValidatedCurrentVp:(b434AlignValidatedBaseline?previousViewProjection:(g_b175HaveConsumedViewProjection?g_b175ConsumedViewProjection:previousViewProjection)),64);const bool b279HadConsumedVp=g_b175HaveConsumedViewProjection;ReleaseSRWLockShared(&g_b175MotionHistoryLock);if(b434AlignValidatedBaseline)out<<"B434: VALIDATED_REACQUIRE_CONSUMED_BASELINE_ALIGNED source_frame_id="<<g_b172GtaFrame<<", action=align_motion_previous_and_b329_comparison_to_validated_current_camera, xefg_reset_history=0, zero_motion=0, ordinary_b329_guard_retained=1, freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";if(b437MotionCameraValid)std::memcpy(g_b174CaptureJob.currentViewProjection,b437ValidatedCurrentVp,64);else std::memcpy(g_b174CaptureJob.currentViewProjection,g_b174CaptureJob.previousViewProjection,64);if(b437MotionCameraValid&&b437ValidityTransition)std::memcpy(g_b174CaptureJob.previousViewProjection,g_b174CaptureJob.currentViewProjection,64);std::memcpy(g_b174CaptureJob.currentInverseViewProjection,b458EffectiveInvVp,64);std::memcpy(g_b174CaptureJob.currentView,b458EffectiveView,64);std::memcpy(g_b174CaptureJob.currentProjection,b458EffectiveProjection,64);
 // B452: diagnostic-only proof of the exact temporal pair entering reconstructed motion. No history, matrix, shader, or XeFG behavior changes.
 static unsigned b452PairOrdinal=0;const bool b452LogThis=((++b452PairOrdinal)<=12)||((b452PairOrdinal%120)==0);
 if(b452LogThis){
  auto b452Fp=[](const float*m)->unsigned long long{unsigned long long h=1469598103934665603ULL;const unsigned char*b=(const unsigned char*)m;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}return h;};
  double b452VpMax=0.0,b452VpRms=0.0,b452XessVsJobMax=0.0;for(unsigned i=0;i<16;++i){const double d=(double)g_b174CaptureJob.currentViewProjection[i]-(double)g_b174CaptureJob.previousViewProjection[i];b452VpMax=(std::max)(b452VpMax,std::fabs(d));b452VpRms+=d*d;b452XessVsJobMax=(std::max)(b452XessVsJobMax,std::fabs((double)xessMotionConstants.previousViewProjection[i]-(double)g_b174CaptureJob.previousViewProjection[i]));}b452VpRms=std::sqrt(b452VpRms/16.0);
  auto b452SampleMotion=[&](float x,float y,float z,double&ox,double&oy){double clip[4]{x,y,z,1.0},world[4]{},prior[4]{};for(int r=0;r<4;++r)for(int c=0;c<4;++c)world[r]+=(double)g_b174CaptureJob.currentInverseViewProjection[r*4+c]*clip[c];if(std::fabs(world[3])<=1.0e-9){ox=oy=0.0;return;}for(int i=0;i<4;++i)world[i]/=world[3];for(int r=0;r<4;++r)for(int c=0;c<4;++c)prior[r]+=(double)g_b174CaptureJob.previousViewProjection[r*4+c]*world[c];if(std::fabs(prior[3])<=1.0e-9){ox=oy=0.0;return;}const double px=prior[0]/prior[3],py=prior[1]/prior[3];ox=(px-(double)x)*((double)width*0.5);oy=((double)y-py)*((double)height*0.5);};
  double centerX=0.0,centerY=0.0,maxSampleMag=0.0;b452SampleMotion(0.0f,0.0f,0.5f,centerX,centerY);const float b452Pts[4][2]={{-0.75f,-0.75f},{0.75f,-0.75f},{-0.75f,0.75f},{0.75f,0.75f}};for(const auto&pt:b452Pts){double mx=0.0,my=0.0;b452SampleMotion(pt[0],pt[1],0.5f,mx,my);maxSampleMag=(std::max)(maxSampleMag,std::sqrt(mx*mx+my*my));}maxSampleMag=(std::max)(maxSampleMag,std::sqrt(centerX*centerX+centerY*centerY));
  out<<"B452: MOTION_HISTORY_PAIR_PROVENANCE source_frame_id="<<g_b172GtaFrame<<", camera_valid="<<(b437MotionCameraValid?1:0)<<", validity_transition="<<(b437ValidityTransition?1:0)<<", baseline_alignment="<<(b434AlignValidatedBaseline?1:0)<<", b453_atomic_temporal_rebase="<<(b453AtomicTemporalRebase?1:0)<<", consumed_history_available="<<(b279HadConsumedVp?1:0)<<", xess_history_available="<<(b452HadXessPrevious?1:0)<<", current_vp_fp="<<b452Fp(g_b174CaptureJob.currentViewProjection)<<", worker_previous_vp_fp="<<b452Fp(g_b174CaptureJob.previousViewProjection)<<", xess_previous_vp_fp="<<b452Fp(xessMotionConstants.previousViewProjection)<<", current_vs_worker_previous_max="<<b452VpMax<<", current_vs_worker_previous_rms="<<b452VpRms<<", xess_previous_vs_worker_previous_max="<<b452XessVsJobMax<<", predicted_center_motion_px={"<<centerX<<","<<centerY<<"}, predicted_sample_max_motion_px="<<maxSampleMag<<", prediction_depth_ndc=0.5, motion_convention=current_to_previous_pixel_space, motion_scale={1,1}, diagnostic_only=1, gpu_motion_output_readback=0, runtime_behavior_changed=0, b451_camera_handoff_unchanged=1, b423_freezing_unchanged=1, ghosting_fix_unchanged=1, xefg_x4_unchanged=1\n";
 }
 // B283: provenance only. Measure the incoming captured VP against the consumed-history VP before queueing.
 double b279SubmitVpMax=0.0,b279SubmitVpRms=0.0;for(unsigned i=0;i<16;++i){const double d=(double)g_b174CaptureJob.currentViewProjection[i]-(double)g_b174CaptureJob.previousViewProjection[i];b279SubmitVpMax=(std::max)(b279SubmitVpMax,std::fabs(d));b279SubmitVpRms+=d*d;}b279SubmitVpRms=std::sqrt(b279SubmitVpRms/16.0);
 if((g_b172GtaFrame%60)==0||b279SubmitVpMax>=500.0){out<<"B283: CAMERA_MATRIX_SUBMIT_PROVENANCE source_frame_id="<<g_b172GtaFrame<<", consumed_history_available="<<(b279HadConsumedVp?1:0)<<", incoming_vs_consumed_vp_max="<<b279SubmitVpMax<<", incoming_vs_consumed_vp_rms="<<b279SubmitVpRms<<", current_vp_source=B287_main_3d_rage_or_fallback, previous_vp_source="<<(b279HadConsumedVp?"last_successfully_presented_job":"incoming_fallback")<<", diagnostic_only=1, b298_hotpath_logging_throttled=1, b276_runtime_frozen=1\n";}
 g_b172Fresh=true;QueryPerformanceCounter(&end);g_b174CaptureJob.depthPackSubmitMs=b173g_elapsed_ms(begin,afterDepthPack);g_b174CaptureJob.motionReconstructSubmitMs=b173g_elapsed_ms(afterDepthPack,afterMotionSubmit);g_b174CaptureJob.nativeDepthMs=b173g_elapsed_ms(begin,end);
 if(b240_log_sample(g_b172GtaFrame)){out<<"B174c: NATIVE_DEPTH_CAPTURE source_frame_id="<<g_b172GtaFrame<<", slot="<<slotIndex<<", native_copy_pack_ms="<<g_b174CaptureJob.nativeDepthMs<<", depth_pack_submit_ms="<<g_b174CaptureJob.depthPackSubmitMs<<", motion_reconstruct_submit_ms="<<g_b174CaptureJob.motionReconstructSubmitMs<<", output_allocation_ms=0, wrapped_resource_ms=0, on12_compute_ms=0, gta_thread_only=1, ready=1, hr=0\n";out.flush();}return S_OK;
}
static HRESULT b174c_prepare_worker_inputs(B173hLiveJob&job,std::ofstream&out)
{
 LARGE_INTEGER begin{},afterColor{},afterOutputAlloc{},afterWrap{},afterComputeSetup{},afterComputeFlush{};QueryPerformanceCounter(&begin);
 auto fail=[&](const char*stage,HRESULT value){out<<"B174c: WORKER_INPUT_PREPARATION_FAILED source_frame_id="<<job.sourceFrame<<", stage="<<stage<<", hr="<<(long)value<<"\n";out.flush();return value;};
 if(job.slotIndex>=kB173iColorSlotCount||job.slotIndex>=kB173jDepthMotionSlotCount)return fail("ValidateSlot",E_INVALIDARG);
 auto&colorSlot=g_b173iColorSlots[job.slotIndex];auto&depthSlot=g_b173jDepthMotionSlots[job.slotIndex];
 if(!colorSlot.ready||!depthSlot.ready||!depthSlot.outputsReady)return fail("ValidatePreparedFrameResource",E_FAIL);
 job.color=colorSlot.output;
 ID3D11Resource*colorResources[]{colorSlot.wrapped.Get()};g_b174eWorkerOn12->AcquireWrappedResources(colorResources,1);g_b174eWorkerOn12Context->CopyResource(colorSlot.wrapped.Get(),colorSlot.opened.Get());g_b174eWorkerOn12->ReleaseWrappedResources(colorResources,1);g_b174eWorkerOn12Context->Flush();QueryPerformanceCounter(&afterColor);
 job.motion=depthSlot.motionOutput;job.depth=depthSlot.depthOutput;job.wrappedMotion=depthSlot.wrappedMotion;job.wrappedDepth=depthSlot.wrappedDepth;job.motionUav=depthSlot.motionUav;job.depthUav=depthSlot.depthUav;QueryPerformanceCounter(&afterOutputAlloc);QueryPerformanceCounter(&afterWrap);
 if(!g_b174eWorkerMotionShader||!g_b174eWorkerMotionConstants)return fail("PrepareWorkerOn12MotionShader",E_NOINTERFACE);B173GpuConstants constants{};std::memcpy(constants.previousViewProjection,job.previousViewProjection,64);std::memcpy(constants.currentInverseViewProjection,job.currentInverseViewProjection,64);constants.width=job.width;constants.height=job.height;
 // B483 diagnostic-only: prove the exact camera constants that survive the queue handoff into the worker GPU dispatch.
 auto b483Fp64=[](const float*m)->unsigned long long{unsigned long long h=1469598103934665603ULL;const unsigned char*b=(const unsigned char*)m;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}return h;};
 static unsigned b483WorkerOrdinal=0;const bool b483WorkerTrace=((++b483WorkerOrdinal)<=24)||((b483WorkerOrdinal%120)==0);
 const unsigned long long b483PrevFp=b483Fp64(constants.previousViewProjection),b483InvFp=b483Fp64(constants.currentInverseViewProjection),b483CurrentVpFp=b483Fp64(job.currentViewProjection);
 if(b483WorkerTrace)out<<"B486: WORKER_SOURCE_PHASE_CORRELATION source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", depth_camera_source_frame="<<job.b486DepthCameraSourceFrame<<", color_source_frame="<<job.b486ColorSourceFrame<<", frame_delta_color_minus_depth_camera="<<((long long)job.b486ColorSourceFrame-(long long)job.b486DepthCameraSourceFrame)<<", camera_vp_fingerprint="<<job.b486CameraVpFingerprint<<", depth_camera_capture_qpc="<<job.b486DepthCameraCaptureQpc<<", color_capture_qpc="<<job.b486ColorCaptureQpc<<", queue_handoff_preserved=1, exact_same_source_frame="<<(job.b486DepthCameraSourceFrame==job.b486ColorSourceFrame?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
 if(b483WorkerTrace)out<<"B483: WORKER_MOTION_CONSTANT_BUFFER_INPUT source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", previous_vp_fingerprint="<<b483PrevFp<<", current_inverse_vp_fingerprint="<<b483InvFp<<", current_vp_fingerprint="<<b483CurrentVpFp<<", camera_valid="<<(job.b437MotionCameraValid?1:0)<<", width="<<job.width<<", height="<<job.height<<", queue_handoff_survived=1, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
 g_b174eWorkerOn12Context->UpdateSubresource(g_b174eWorkerMotionConstants.Get(),0,nullptr,&constants,0,0);QueryPerformanceCounter(&afterComputeSetup);
 if(job.nativeFenceValue&&g_b173fSharedFence12)g_b174iPreparationQueue->Wait(g_b173fSharedFence12.Get(),job.nativeFenceValue);
 ID3D11Resource*wrapped[]{job.wrappedMotion.Get(),job.wrappedDepth.Get()};g_b174eWorkerOn12->AcquireWrappedResources(wrapped,2);ID3D11UnorderedAccessView*uavs[]{job.motionUav.Get(),job.depthUav.Get()};ID3D11ShaderResourceView*srvs[]{depthSlot.openedDepthSrv.Get()};ID3D11Buffer*cbs[]{g_b174eWorkerMotionConstants.Get()};g_b174eWorkerOn12Context->CSSetShader(g_b174eWorkerMotionShader.Get(),nullptr,0);g_b174eWorkerOn12Context->CSSetConstantBuffers(0,1,cbs);g_b174eWorkerOn12Context->CSSetShaderResources(0,1,srvs);g_b174eWorkerOn12Context->CSSetUnorderedAccessViews(0,2,uavs,nullptr);g_b174eWorkerOn12Context->Dispatch((job.width+7)/8,(job.height+7)/8,1);if(!job.b437MotionCameraValid){const FLOAT b437ZeroMotion[4]{0.0f,0.0f,0.0f,0.0f};g_b174eWorkerOn12Context->ClearUnorderedAccessViewFloat(job.motionUav.Get(),b437ZeroMotion);}if(b483WorkerTrace)out<<"B483: GPU_MOTION_DISPATCH_RESOURCE source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", motion_resource="<<(void*)job.motion.Get()<<", motion_uav="<<(void*)job.motionUav.Get()<<", previous_vp_fingerprint="<<b483PrevFp<<", current_inverse_vp_fingerprint="<<b483InvFp<<", current_vp_fingerprint="<<b483CurrentVpFp<<", dispatch_groups={"<<((job.width+7)/8)<<","<<((job.height+7)/8)<<",1}, format=R16G16_FLOAT, camera_valid="<<(job.b437MotionCameraValid?1:0)<<", invalid_camera_zero_clear="<<(job.b437MotionCameraValid?0:1)<<", synchronous_gpu_readback=0, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";
 if(job.b330NativeMotionActive&&g_b330WorkerNativeMotionShader&&depthSlot.openedNativeMotionSrv){ID3D11ShaderResourceView*null0{};g_b174eWorkerOn12Context->CSSetShaderResources(0,1,&null0);ID3D11ShaderResourceView*nms[]{depthSlot.openedNativeMotionSrv.Get()};g_b174eWorkerOn12Context->CSSetShader(g_b330WorkerNativeMotionShader.Get(),nullptr,0);g_b174eWorkerOn12Context->CSSetShaderResources(1,1,nms);g_b174eWorkerOn12Context->CSSetUnorderedAccessViews(0,1,uavs,nullptr);g_b174eWorkerOn12Context->Dispatch((job.width+7)/8,(job.height+7)/8,1);ID3D11ShaderResourceView*null1{};g_b174eWorkerOn12Context->CSSetShaderResources(1,1,&null1);out<<"B334: NATIVE_MOTION_RUNTIME_ACTIVE source_frame_id="<<job.sourceFrame<<", native_size=960x540, output_size="<<job.width<<"x"<<job.height<<", runtime_transform=negate_xy_no_spatial_scale, spatial_scale={1,1}, direction_retained_from_B332=previous_to_current_via_negate_xy, source_direction_hypothesis=current_to_previous, xefg_tag=current_to_previous, raw_native_units_test=1, fallback=0\n";}else if(b240_log_sample(job.sourceFrame))out<<"B335: RECONSTRUCTED_MOTION_RUNTIME_ACTIVE source_frame_id="<<job.sourceFrame<<", native_runtime_use=0, rejected_candidate=960x540_R16G16_FLOAT, motion_source=reconstructed_depth_camera, camera_path_frozen=B291_B296_B309_B324\n";ID3D11UnorderedAccessView*nullUavs[2]{};ID3D11ShaderResourceView*nullSrvs[1]{};g_b174eWorkerOn12Context->CSSetUnorderedAccessViews(0,2,nullUavs,nullptr);g_b174eWorkerOn12Context->CSSetShaderResources(0,1,nullSrvs);g_b174eWorkerOn12Context->CSSetShader(nullptr,nullptr,0);g_b174eWorkerOn12->ReleaseWrappedResources(wrapped,2);g_b174eWorkerOn12Context->Flush();UINT64 on12Marker=0;HRESULT markerHr=b174i_signal_preparation(on12Marker);if(FAILED(markerHr))return fail("SignalPreparationMarker",markerHr);QueryPerformanceCounter(&afterComputeFlush);
 job.on12Marker=on12Marker;std::memcpy(job.constants.viewMatrix,job.currentView,64);std::memcpy(job.constants.projectionMatrix,job.currentProjection,64);
 double frameMs=job.sourceFrameRenderTimeMs;if(frameMs<1.0||frameMs>50.0)frameMs=16.6667;
 const bool resumeNeedsFresh=g_b174ResumeNeedsFresh.exchange(false,std::memory_order_acq_rel);
 const bool b407RawCameraReset=g_b288CameraResetPending.exchange(false,std::memory_order_acq_rel);
 const ULONGLONG b407Now=GetTickCount64();
 const ULONGLONG b407LastUnsafe=g_b407LastUnsafeCameraTick.load(std::memory_order_acquire);
 const bool b407UnsafeEpisode=g_b408UnsafeEpisodeActive.load(std::memory_order_acquire);
 const unsigned b408StableSafeFrames=g_b408StableSafeFrames.load(std::memory_order_acquire);
 bool b288CameraReset=b407RawCameraReset;
 if(b407RawCameraReset&&b407UnsafeEpisode){
  const bool b407AlreadyConsumed=g_b407UnsafeResetConsumed.exchange(true,std::memory_order_acq_rel);
  if(b407AlreadyConsumed){
   b288CameraReset=false;
   if(!resumeNeedsFresh)g_b175bXessReset.store(false,std::memory_order_release);
   out<<"B407: FINAL_CAMERA_RESET_COALESCED source_frame_id="<<job.sourceFrame<<", raw_reset=1, final_reset=0, unsafe_episode=1, unsafe_age_ms="<<(b407LastUnsafe&&b407Now>=b407LastUnsafe?b407Now-b407LastUnsafe:0)<<", stable_safe_frames="<<b408StableSafeFrames<<", rearm_required_safe_frames=30, classifier_flicker_tolerated=1, menu_transition_coalesced=1, zero_motion_retained=1, xess_history_preserved=1, xefg_history_preserved=1\n";
  }else out<<"B407: FINAL_CAMERA_RESET_ACCEPTED source_frame_id="<<job.sourceFrame<<", raw_reset=1, final_reset=1, unsafe_episode=1, episode_entry=1, stable_safe_frames="<<b408StableSafeFrames<<", rearm_required_safe_frames=30, zero_motion_retained=1\n";
 }
 // B329: Ghosting-only temporal guard.  B328 already measured camera VP deltas but only logged them.
 // Reset XeFG history on abrupt camera transitions so stale temporal history cannot smear high-contrast
 // world details (signs/edges) across a rapid camera change.  Sustained ordinary pans are left alone.
 double b329VpMax=0.0,b329VpRms=0.0;
 for(unsigned i=0;i<16;++i){const double d=(double)job.currentViewProjection[i]-(double)job.previousViewProjection[i];const double ad=std::fabs(d);if(ad>b329VpMax)b329VpMax=ad;b329VpRms+=d*d;}
 b329VpRms=std::sqrt(b329VpRms/16.0);
 static double b329LastVpMax=0.0;
 const bool b329AbruptCamera=(b329VpMax>=2.0&&b329VpMax>((std::max)(1.0,b329LastVpMax*1.80)))||b329VpRms>=0.500;
 b329LastVpMax=b329VpMax;
 if(!b329AbruptCamera&&b329VpRms>=0.030){static unsigned b473Preserved=0;if((++b473Preserved<=12)||(b473Preserved%120)==0)out<<"B473: B329_NORMAL_CAMERA_HISTORY_PRESERVED source_frame_id="<<job.sourceFrame<<", vp_max_abs_delta="<<b329VpMax<<", vp_rms_delta="<<b329VpRms<<", old_b329_rms_threshold_would_reset=1, xefg_reset_history_requested=0, b472_history_policy_aligned=1, b471_camera_lock_preserved=1, b423_freezing_unchanged=1, motion_scale_unchanged=1, motion_direction_unchanged=1\n";}
 job.constants.frameRenderTime=(float)frameMs;job.constants.resetHistory=(g_b171LiveSubmitted.load(std::memory_order_acquire)==0||resumeNeedsFresh||b288CameraReset||b329AbruptCamera||job.b437MotionValidityTransition)?1:0;if(job.b437MotionValidityTransition)out<<"B437: MOTION_VALIDITY_TRANSITION source_frame_id="<<job.sourceFrame<<", camera_valid="<<(job.b437MotionCameraValid?1:0)<<", zero_motion="<<(job.b437MotionCameraValid?0:1)<<", xefg_reset_history=1, previous_equals_current_on_valid_return="<<(job.b437MotionCameraValid?1:0)<<", freezing_dll_unchanged=1, xefg_x4_unchanged=1\n";
 if(b288CameraReset)out<<"B297: CAMERA_HISTORY_RESET source_frame_id="<<job.sourceFrame<<", reason=first_valid_or_genuine_later_discontinuity, xefg_reset_history=1, motion_previous_equals_current=1, b276_presentation_frozen=1\n";
 if(b329AbruptCamera)out<<"B329: GHOSTING_TEMPORAL_HISTORY_GUARD source_frame_id="<<job.sourceFrame<<", vp_max_abs_delta="<<b329VpMax<<", vp_rms_delta="<<b329VpRms<<", action=xefg_reset_history, threshold_policy=B473_genuine_discontinuity_only, discontinuity_max_threshold=2.0, discontinuity_rms_threshold=0.500, reconstructed_motion_unchanged=1, motion_scale_unchanged=1, jitter_unchanged=1, queue_unchanged=1, x4_unchanged=1\n";
 if(b240_log_sample(job.sourceFrame))out<<"B246: SOURCE_CADENCE current_source_id="<<job.sourceFrame<<", timing_source=adjacent_raw_gta_present, frame_render_time_ms="<<frameMs<<", backlog_independent=1, prior_consumed_source_id="<<g_b174LastPresentedSource.load(std::memory_order_acquire)<<", sampled_fast_path=1\n";
 if(b240_log_sample(job.sourceFrame)){out<<"B174o: WORKER_INPUT_PREPARATION source_frame_id="<<job.sourceFrame<<", slot="<<job.slotIndex<<", color_on12_ms="<<b173g_elapsed_ms(begin,afterColor)<<", output_allocation_ms=0, wrap_and_uav_ms=0, frame_resource_ring_hit=1, live_d3d12_allocation=0, compute_setup_ms="<<b173g_elapsed_ms(afterWrap,afterComputeSetup)<<", compute_dispatch_flush_ms="<<b173g_elapsed_ms(afterComputeSetup,afterComputeFlush)<<", worker_total_ms="<<b173g_elapsed_ms(begin,afterComputeFlush)<<", preparation_marker="<<on12Marker<<", preparation_queue_separate=1, reset_history="<<job.constants.resetHistory<<", ready=1\n";out.flush();}return S_OK;
}
static bool b173h_take_live_job(B173hLiveJob&job)
{
 // B422: bound latency by age, not by another queue-depth threshold.  If the serial XeFG
 // worker has fallen more than 25 ms behind, retire only queued (never worker-owned) stale
 // jobs and consume the newest queued source.  This targets the B421 50-60 ms queue-delay
 // bursts while leaving B421 private-Present semantics and the 5-frame admission guard intact.
 unsigned b422Dropped=0; unsigned b422DroppedFrames[kB174oPendingCapacity]{}; unsigned b422DroppedSlots[kB174oPendingCapacity]{};
 double b422OldestAgeMs=0.0;
 AcquireSRWLockExclusive(&g_b173hJobLock);
 const bool available=g_b174oPendingSize!=0;
 if(available&&g_b174oPendingSize>1&&b385_load_protected_modules()){
  LARGE_INTEGER now{};QueryPerformanceCounter(&now);
  const auto& oldest=g_b174oPendingJobs[g_b174oPendingHead];
  b422OldestAgeMs=oldest.queuedTime.QuadPart?b173g_elapsed_ms(oldest.queuedTime,now):0.0;
  if(g_b423FreezeCollapseQueue(g_b174oPendingSize,b422OldestAgeMs)){
   while(g_b174oPendingSize>1){
    auto& stale=g_b174oPendingJobs[g_b174oPendingHead];
    if(stale.slotIndex<kB173iColorSlotCount){
     if(b422Dropped<kB174oPendingCapacity){b422DroppedFrames[b422Dropped]=stale.sourceFrame;b422DroppedSlots[b422Dropped]=stale.slotIndex;}
     B174ReplacedPending retired{};retired.nativeFenceValue=stale.nativeFenceValue;retired.slotIndex=stale.slotIndex;retired.sourceFrame=stale.sourceFrame;g_b174ReplacedPending.push_back(retired);
     g_b174ColorSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174DepthSlotState[retired.slotIndex].store(4,std::memory_order_release);
     g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);++b422Dropped;
    }
    stale=B173hLiveJob{};g_b174oPendingHead=(g_b174oPendingHead+1)%kB174oPendingCapacity;--g_b174oPendingSize;
   }
   // The newest job may no longer be temporally adjacent to the last presented source.
   // Reset XeFG history once on this discontinuity; do not alter camera classification.
   g_b174oPendingJobs[g_b174oPendingHead].constants.resetHistory=1;
  }
 }
 if(available){job=g_b174oPendingJobs[g_b174oPendingHead];g_b174oPendingJobs[g_b174oPendingHead]=B173hLiveJob{};g_b174oPendingHead=(g_b174oPendingHead+1)%kB174oPendingCapacity;--g_b174oPendingSize;job.presentId=g_b174NextPresentId.fetch_add(1,std::memory_order_acq_rel);job.ordinal=g_b174Accepted.fetch_add(1,std::memory_order_acq_rel);g_b174TotalAccepted.fetch_add(1,std::memory_order_acq_rel);g_b173hJobProcessing.store(true,std::memory_order_release);g_b173hJobPending.store(g_b174oPendingSize!=0,std::memory_order_release);g_b174PendingCount.store(g_b174oPendingSize,std::memory_order_release);if(job.slotIndex<kB173iColorSlotCount){g_b174ColorSlotState[job.slotIndex].store(3,std::memory_order_release);g_b174DepthSlotState[job.slotIndex].store(3,std::memory_order_release);}}
 ReleaseSRWLockExclusive(&g_b173hJobLock);
 for(unsigned i=0;i<b422Dropped&&i<kB174oPendingCapacity;++i)b175_log_disposition(b422DroppedFrames[i],"dropped_b422_stale_worker_queue","queue_age_ge_25ms_keep_newest",b422DroppedSlots[i]);
 if(available)b175_log_disposition(job.sourceFrame,"accepted_current_worker","ordered_frame_queue",job.slotIndex);
 return available;
}
static void b174_discard_suspended_pending(std::ofstream&out)
{
 AcquireSRWLockExclusive(&g_b173hJobLock);
 while(g_b174oPendingSize){
  auto&pending=g_b174oPendingJobs[g_b174oPendingHead];
  if(pending.slotIndex<kB173iColorSlotCount){
   B174ReplacedPending retired{};retired.nativeFenceValue=pending.nativeFenceValue;retired.slotIndex=pending.slotIndex;retired.sourceFrame=pending.sourceFrame;g_b174ReplacedPending.push_back(retired);
   g_b174ColorSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174DepthSlotState[retired.slotIndex].store(4,std::memory_order_release);
   g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(pending.sourceFrame,"dropped_surface_suspended","worker_rejected_pre_suspend_mailbox",pending.slotIndex);
   out<<"B174z: SUSPENDED_PENDING_DRAIN source_frame_id="<<pending.sourceFrame<<", slot="<<pending.slotIndex<<", stale_content_presented=0\n";
  }
  pending=B173hLiveJob{};g_b174oPendingHead=(g_b174oPendingHead+1)%kB174oPendingCapacity;--g_b174oPendingSize;
 }
 g_b174oPendingHead=0;g_b174oPendingTail=0;g_b173hJobPending.store(false,std::memory_order_release);g_b174PendingCount.store(0,std::memory_order_release);
 ReleaseSRWLockExclusive(&g_b173hJobLock);out.flush();
}
static HRESULT b173h_present_live_job(B173hLiveJob&job,std::ofstream&out)
{
 LARGE_INTEGER workerBegin{},afterSleep{},afterPreparation{},afterInputReady{},afterTags{},afterPrefill{},presentStart{},afterPresent{},end{};QueryPerformanceCounter(&workerBegin);b392_set_stage(g_b392WorkerStage,11,job.sourceFrame,job.presentId);
 const bool b242Verbose=b240_log_sample(job.sourceFrame);
 // B283: diagnostic-only camera-matrix provenance. B276 runtime behavior is frozen.
 // Compare the consumed previous VP against the current VP without changing motion resources,
 // XeFG constants, history, pacing, queues, or presentation. This tells the next build whether
 // visible high-speed ghosting correlates with large camera/reprojection deltas.
 double b277VpMaxAbsDelta=0.0,b277VpRmsDelta=0.0;
 for(unsigned i=0;i<16;++i){const double d=(double)job.currentViewProjection[i]-(double)job.previousViewProjection[i];const double ad=std::fabs(d);if(ad>b277VpMaxAbsDelta)b277VpMaxAbsDelta=ad;b277VpRmsDelta+=d*d;}
 b277VpRmsDelta=std::sqrt(b277VpRmsDelta/16.0);
 const bool b277FastMotion=b277VpMaxAbsDelta>=0.02||b277VpRmsDelta>=0.005;
 if((b277FastMotion&&((job.ordinal+1)%30)==0)||b242Verbose){out<<"B283: CAMERA_MATRIX_WORKER_PROVENANCE source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", vp_max_abs_delta="<<b277VpMaxAbsDelta<<", vp_rms_delta="<<b277VpRmsDelta<<", source_frame_render_ms="<<job.sourceFrameRenderTimeMs<<", fast_motion_candidate="<<(b277FastMotion?1:0)<<", motion_source=reconstructed_depth_B287_main_3d_rage, native_gta_motion_vectors=1, pixel_space_current_to_previous=1, motion_scale={1,1}, jitter={0,0}, diagnostic_only=1, b276_runtime_frozen=1\n";out.flush();}
 b174_retire_completed_frames(out);
 const uint32_t xellFrameId=(uint32_t)job.presentId;
 // B268: Keep XeLL low-latency mode and the XeFG latency-reduction attachment enabled, but do not
 // let XeLL sleep throttle the serial XeFG worker while real GTA frames are already waiting.
 // B261 proved disabling XeLL low-latency mode breaks XeFG enable; this does NOT disable it.
 const unsigned b268PendingBeforeSleep=g_b174PendingCount.load(std::memory_order_acquire);
 // B273: Intel XeLL contract still requires xellSleep to be the first XeLL call for every
 // frame ID when low latency is enabled. B268/B269 skipped it under backlog; the B269 run eventually
 // failed at present_id=5309 with sleep=-4 and the following markers=-8, which forced F9 off.
 // Performance tuning is frozen here: no queue, Present, interpolation, priority, resource, or XeFG
 // cadence changes are made. This call only restores the required XeLL frame lifecycle.
 xell_result_t b306BacklogDisableResult=XELL_RESULT_SUCCESS,b306BacklogRestoreResult=XELL_RESULT_SUCCESS;bool b306BacklogBypass=false;
 xell_result_t xellSleepResult=XELL_RESULT_ERROR_INVALID_CONTEXT;
 if(g_xellContext&&b268PendingBeforeSleep>0){
  xell_sleep_params_t b306NoWait{};b306NoWait.bLowLatencyMode=0;b306NoWait.minimumIntervalUs=0;
  b306BacklogDisableResult=xellSetSleepMode(g_xellContext,&b306NoWait);
  if(b306BacklogDisableResult==XELL_RESULT_SUCCESS){xellSleepResult=xellSleep(g_xellContext,xellFrameId);xell_sleep_params_t b306Restore{};b306Restore.bLowLatencyMode=1;b306Restore.minimumIntervalUs=0;b306BacklogRestoreResult=xellSetSleepMode(g_xellContext,&b306Restore);b306BacklogBypass=xellSleepResult==XELL_RESULT_SUCCESS&&b306BacklogRestoreResult==XELL_RESULT_SUCCESS;}
  else xellSleepResult=xellSleep(g_xellContext,xellFrameId);
 }else if(g_xellContext)xellSleepResult=xellSleep(g_xellContext,xellFrameId);
 QueryPerformanceCounter(&afterSleep);
 if(job.ordinal<8||b240_log_sample(job.sourceFrame)||b306BacklogBypass)out<<"B306: XELL_BACKLOG_SLEEP_POLICY ordinal="<<(job.ordinal+1)<<", present_id="<<job.presentId<<", pending_before_sleep="<<b268PendingBeforeSleep<<", xell_sleep_called=1, backlog_no_wait_mode="<<(b306BacklogBypass?1:0)<<", temporary_low_latency_disable_result="<<(int)b306BacklogDisableResult<<", restore_low_latency_result="<<(int)b306BacklogRestoreResult<<", frame_markers_unchanged=1, present_id_unchanged=1, sleep_ms="<<b173g_elapsed_ms(workerBegin,afterSleep)<<"\n";
 const bool b306SleepLifecycleReady=xellSleepResult==XELL_RESULT_SUCCESS&&b306BacklogRestoreResult==XELL_RESULT_SUCCESS;
 const auto simulationStartResult=b306SleepLifecycleReady?xellAddMarkerData(g_xellContext,xellFrameId,XELL_SIMULATION_START):XELL_RESULT_ERROR_INVALID_CONTEXT;
 const auto renderStartResult=simulationStartResult==XELL_RESULT_SUCCESS?xellAddMarkerData(g_xellContext,xellFrameId,XELL_RENDERSUBMIT_START):XELL_RESULT_ERROR_INVALID_CONTEXT;
 b392_set_stage(g_b392WorkerStage,12,job.sourceFrame,job.presentId);HRESULT preparationHr=renderStartResult==XELL_RESULT_SUCCESS?b174c_prepare_worker_inputs(job,out):E_FAIL;QueryPerformanceCounter(&afterPreparation);
 // B274: XeLL permits phases to overlap. End SIMULATION after the CPU-side frame preparation
 // instead of immediately after SIMULATION_START. The old zero-length simulation marker pair fed
 // XeLL a worker-local timing model that did not describe the real per-frame preparation interval.
 const auto simulationEndResult=(SUCCEEDED(preparationHr)&&renderStartResult==XELL_RESULT_SUCCESS)?xellAddMarkerData(g_xellContext,xellFrameId,XELL_SIMULATION_END):XELL_RESULT_ERROR_INVALID_CONTEXT;
 if(FAILED(preparationHr)||xellSleepResult!=XELL_RESULT_SUCCESS||b306BacklogRestoreResult!=XELL_RESULT_SUCCESS||simulationStartResult!=XELL_RESULT_SUCCESS||simulationEndResult!=XELL_RESULT_SUCCESS||renderStartResult!=XELL_RESULT_SUCCESS){out<<"B174o: XELL_FRAME_LIFECYCLE_FAILED present_id="<<job.presentId<<", sleep="<<(int)xellSleepResult<<", backlog_restore="<<(int)b306BacklogRestoreResult<<", simulation_start="<<(int)simulationStartResult<<", simulation_end="<<(int)simulationEndResult<<", render_submit_start="<<(int)renderStartResult<<"\n";out.flush();g_b174FailedJobs.push_back(job);job.slotIndex=~0u;b174_request_stop(4);return FAILED(preparationHr)?preparationHr:E_FAIL;}
 b392_set_stage(g_b392WorkerStage,13,job.sourceFrame,job.presentId);LARGE_INTEGER queueWaitBegin{},queueWaitEnd{};QueryPerformanceCounter(&queueWaitBegin);UINT64 inputCompleted=g_b174iPreparationFence?g_b174iPreparationFence->GetCompletedValue():0;HRESULT inputWaitHr=g_queue12&&g_b174iPreparationFence?g_queue12->Wait(g_b174iPreparationFence.Get(),job.on12Marker):E_NOINTERFACE;QueryPerformanceCounter(&queueWaitEnd);const double inputWaitMs=b173g_elapsed_ms(queueWaitBegin,queueWaitEnd);QueryPerformanceCounter(&afterInputReady);
 if(b242Verbose)out<<"B174i: WORKER_INPUT_COMPLETION_GATE source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", preparation_marker="<<job.on12Marker<<", completed_value_at_enqueue="<<inputCompleted<<", worker_cpu_wait_ms="<<inputWaitMs<<", gpu_queue_wait=1, cpu_blocked=0, gta_thread_wait=0, ready="<<(SUCCEEDED(inputWaitHr)?1:0)<<", hr="<<(long)inputWaitHr<<"\n";out.flush();
 if(FAILED(inputWaitHr)){g_b174FailedJobs.push_back(job);job.slotIndex=~0u;HRESULT removed=g_device12?g_device12->GetDeviceRemovedReason():S_OK;if(FAILED(removed))b173l_log_dred(out,removed);b174_request_stop(4);return inputWaitHr;}
 const UINT64 nativeCompletedAtStart=g_b173fSharedFence12?g_b173fSharedFence12->GetCompletedValue():0;
 const UINT64 stageCompletedAtStart=g_b173nStageFence?g_b173nStageFence->GetCompletedValue():0;
 b392_set_stage(g_b392WorkerStage,14,job.sourceFrame,job.presentId);xefg_swapchain_d3d12_resource_data_t color{};color.type=XEFG_SWAPCHAIN_RES_HUDLESS_COLOR;color.validity=XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;color.resourceBase={0,0};auto cd=job.color?job.color->GetDesc():D3D12_RESOURCE_DESC{};color.resourceSize={(uint32_t)cd.Width,cd.Height};color.pResource=job.color.Get();color.incomingState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
 xefg_swapchain_d3d12_resource_data_t motion{};motion.type=XEFG_SWAPCHAIN_RES_MOTION_VECTOR;motion.validity=XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;motion.resourceBase={0,0};motion.resourceSize={g_b171LiveWidth,g_b171LiveHeight};motion.pResource=job.motion.Get();motion.incomingState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
 xefg_swapchain_d3d12_resource_data_t depth{};depth.type=XEFG_SWAPCHAIN_RES_DEPTH;depth.validity=XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;depth.resourceBase={0,0};depth.resourceSize={g_b171LiveWidth,g_b171LiveHeight};depth.pResource=job.depth.Get();depth.incomingState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
 auto frameConstants=job.constants;frameConstants.motionVectorScaleX=1.0f;frameConstants.motionVectorScaleY=1.0f;frameConstants.jitterOffsetX=0.0f;frameConstants.jitterOffsetY=0.0f;auto constantsResult=xefgSwapChainTagFrameConstants(g_xefgContext,job.presentId,&frameConstants);
 if(b242Verbose)out<<"B174y: MOTION_VECTOR_SCALE color_size="<<(uint32_t)cd.Width<<"x"<<cd.Height<<", motion_size="<<job.width<<"x"<<job.height<<", scales={"<<frameConstants.motionVectorScaleX<<","<<frameConstants.motionVectorScaleY<<"}, semantics=pixel_space_vectors_scale_1\n";
 auto motionResult=xefgSwapChainD3D12TagFrameResource(g_xefgContext,nullptr,job.presentId,&motion);
 static unsigned b483TagOrdinal=0;const bool b483TagTrace=((++b483TagOrdinal)<=24)||((b483TagOrdinal%120)==0);if(b483TagTrace){auto b483TagFp=[](const float*m)->unsigned long long{unsigned long long h=1469598103934665603ULL;const unsigned char*b=(const unsigned char*)m;for(unsigned i=0;i<64;++i){h^=(unsigned long long)b[i];h*=1099511628211ULL;}return h;};out<<"B483: XEFG_MOTION_RESOURCE_TAG_CORRELATION source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", tagged_motion_resource="<<(void*)motion.pResource<<", job_motion_resource="<<(void*)job.motion.Get()<<", exact_dispatch_resource_reused="<<((motion.pResource==job.motion.Get())?1:0)<<", previous_vp_fingerprint="<<b483TagFp(job.previousViewProjection)<<", current_inverse_vp_fingerprint="<<b483TagFp(job.currentInverseViewProjection)<<", current_vp_fingerprint="<<b483TagFp(job.currentViewProjection)<<", motion_tag_result="<<(int)motionResult<<", motion_scale={1,1}, pixel_space=1, current_to_previous=1, synchronous_gpu_readback=0, diagnostic_only=1, runtime_behavior_changed=0, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}
 auto depthResult=xefgSwapChainD3D12TagFrameResource(g_xefgContext,nullptr,job.presentId,&depth);
 auto colorResult=XEFG_SWAPCHAIN_RESULT_SUCCESS; // B213: UI_MODE_NONE does not consume HUD-less color tags.
 const bool tagged=constantsResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&motionResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&depthResult==XEFG_SWAPCHAIN_RESULT_SUCCESS;
 QueryPerformanceCounter(&afterTags);b392_set_stage(g_b392WorkerStage,15,job.sourceFrame,job.presentId);HRESULT prefillHr=tagged?b171_copy_color_to_private_backbuffer(job.color.Get(),job.slotIndex,"live-worker",out):E_FAIL;UINT64 prefillMarker=0;HRESULT prefillMarkerHr=SUCCEEDED(prefillHr)?b173n_signal_stage(prefillMarker):prefillHr;QueryPerformanceCounter(&afterPrefill);
 // B256: XeFG was initialized with g_queue12, the same D3D12 queue used by the
 // D3D11On12 prefill and this fence signal. Queue submission order already guarantees
 // the prefill executes before XeFG's subsequent Present work. B255 proved the CPU
 // fence wait consumed ~7-14 ms on many frames while the actual Present was often
 // below 1 ms. Do not stall the worker CPU for completion of work already ordered on
 // the same queue; retain the marker for retirement/diagnostics.
 UINT64 hudCompleted=g_b173nStageFence?g_b173nStageFence->GetCompletedValue():0;
 const double hudWaitMs=0.0;
 const HRESULT hudWaitHr=(SUCCEEDED(prefillHr)&&SUCCEEDED(prefillMarkerHr))?S_OK:E_FAIL;
 const UINT64 stageCompletedBeforePresent=hudCompleted;
 if(b242Verbose)out<<"B258: BACKBUFFER_PREFILL_QUEUE_ORDER source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", marker="<<prefillMarker<<", completed_at_submit="<<hudCompleted<<", cpu_wait_ms=0, same_queue=1, queue_order_guarantees_before_xefg_present=1, hr="<<(long)hudWaitHr<<"\n";out.flush();
 auto idResult=SUCCEEDED(prefillHr)&&SUCCEEDED(prefillMarkerHr)?xefgSwapChainSetPresentId(g_xefgContext,job.presentId):(xefg_swapchain_result_t)-1;
 const auto renderEndResult=xellAddMarkerData(g_xellContext,xellFrameId,XELL_RENDERSUBMIT_END);
 const auto presentStartResult=xellAddMarkerData(g_xellContext,xellFrameId,XELL_PRESENT_START);
 DWM_TIMING_INFO b313DwmBefore{};HRESULT b313DwmBeforeHr=S_FALSE; // B391: retire DWM query from every XeFG Present
 b392_set_stage(g_b392WorkerStage,16,job.sourceFrame,job.presentId);QueryPerformanceCounter(&presentStart);
 const double presentIntervalMs=g_b174PreviousPrivatePresentStart.QuadPart?b173g_elapsed_ms(g_b174PreviousPrivatePresentStart,presentStart):0.0;g_b174PreviousPrivatePresentStart=presentStart;
 const UINT b263BackBufferBefore=g_xefgSwapChain?g_xefgSwapChain->GetCurrentBackBufferIndex():~0u;
 const unsigned b310PendingBeforePresent=g_b174PendingCount.load(std::memory_order_acquire);
 if(job.ordinal<8||b240_log_sample(job.sourceFrame)){out<<"B422: AGE_BOUNDED_WORKER_QUEUE source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", queue_delay_ms="<<b173g_elapsed_ms(job.queuedTime,workerBegin)<<", max_stale_queue_age_ms="<<(g_b423FreezeMaxQueueAgeMs?g_b423FreezeMaxQueueAgeMs():0)<<", collapse_policy=locked_freezing_dll, worker_owned_untouched=1, newest_reset_history_on_collapse=1, b421_present_semantics=1\n";out.flush();}
 if(job.ordinal<8){out<<"B421: SAFE_PRIVATE_PRESENT_POLICY source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", pending_before="<<b310PendingBeforePresent<<", synthetic_backlog_skip=0, dxgi_do_not_wait_fail_open=1, b411_present_semantics=1, freezing_policy_external_locked_dll=1\n";out.flush();}
 const bool b343NonBlockingAdmission=true; // B386 freeze-only: private Present must never block the serial XeFG worker.
 const UINT presentFlags=(b385_load_protected_modules()&&g_b385FreezeFlags)?g_b385FreezeFlags(g_b174rAllowTearing.load(std::memory_order_acquire)?TRUE:FALSE):0;
 const bool b325SampleWanted=false; // B391: retire DXGI statistics probes from live worker
 DXGI_FRAME_STATISTICS b325StatsBefore{};HRESULT b325StatsBeforeHr=b325SampleWanted&&g_xefgSwapChain?g_xefgSwapChain->GetFrameStatistics(&b325StatsBefore):(b325SampleWanted?E_NOINTERFACE:S_FALSE);
 UINT b325LastPresentBefore=0;HRESULT b325LastPresentBeforeHr=b325SampleWanted&&g_xefgSwapChain?g_xefgSwapChain->GetLastPresentCount(&b325LastPresentBefore):(b325SampleWanted?E_NOINTERFACE:S_FALSE);
 const double b310ThreadCpuBefore=0.0; // B391: retire GetThreadTimes from every Present
 // B415: extend B386's proven fail-open behavior to backlog-saturated worker Presents.
 // When the worker is already carrying four or more pending source jobs, do not enter another private XeFG Present.
 // Reuse the existing DXGI_ERROR_WAS_STILL_DRAWING nonfatal-drop/fence-retirement path instead of blocking GTA behind FG.
 // B421: retire B415's synthetic backlog skip. B420 proved that repeatedly skipping XeFG Presents can leave
 // the private swapchain in a state where the next real Present blocks and the D3D12 device is removed.
 // Keep B386's native DXGI_PRESENT_DO_NOT_WAIT fail-open: call XeFG Present and only drop when DXGI itself
 // reports DXGI_ERROR_WAS_STILL_DRAWING. This restores B411 private-Present semantics without changing x4,
 // F9, camera/history, or the protected freeze/ghosting modules.
 const bool b415FailOpenBeforePrivatePresent=false;
 HRESULT presentHr=b415FailOpenBeforePrivatePresent?DXGI_ERROR_WAS_STILL_DRAWING:((renderEndResult==XELL_RESULT_SUCCESS&&presentStartResult==XELL_RESULT_SUCCESS&&idResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&g_xefgSwapChain&&(presentFlags&DXGI_PRESENT_ALLOW_TEARING))?g_xefgSwapChain->Present(0,presentFlags):E_FAIL);
 if(b415FailOpenBeforePrivatePresent){out<<"B415: WORKER_PRESENT_FAIL_OPEN source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", pending_before="<<b310PendingBeforePresent<<", threshold=4, private_present_called=0, reused_b386_was_still_drawing_retirement=1, gta_source_continues=1, xefg_context_reset=0, history_reset_added=0\n";out.flush();}
 const bool b343WouldBlock=(presentHr==DXGI_ERROR_WAS_STILL_DRAWING);b392_set_stage(g_b392WorkerStage,17,job.sourceFrame,job.presentId); // B386: drop this generated presentation attempt without blocking; retain resources until the GPU fence retires them.
 const double b310ThreadCpuAfter=0.0; // B391: retire GetThreadTimes from every Present
 const UINT64 b259StageCompletedOnPresentReturn=g_b173nStageFence?g_b173nStageFence->GetCompletedValue():0;
 const UINT b263BackBufferAfter=g_xefgSwapChain?g_xefgSwapChain->GetCurrentBackBufferIndex():~0u;
 DXGI_FRAME_STATISTICS b325StatsAfter{};HRESULT b325StatsAfterHr=b325SampleWanted&&g_xefgSwapChain?g_xefgSwapChain->GetFrameStatistics(&b325StatsAfter):(b325SampleWanted?E_NOINTERFACE:S_FALSE);
 UINT b325LastPresentAfter=0;HRESULT b325LastPresentAfterHr=b325SampleWanted&&g_xefgSwapChain?g_xefgSwapChain->GetLastPresentCount(&b325LastPresentAfter):(b325SampleWanted?E_NOINTERFACE:S_FALSE);
 if(SUCCEEDED(presentHr)){const auto draws=g_b192HudDraws.fetch_add(1,std::memory_order_relaxed)+1;if(draws<=3||draws%120==0){out<<"B213: XEFG_UI_INTERPOLATED_PRESENT source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", backbuffer_prefilled_before_present=1, ui_mode=NONE, xefg_ui_composition=0, separate_ui_hwnd=0, presents="<<draws<<"\n";out.flush();}}
 const auto presentEndResult=xellAddMarkerData(g_xellContext,xellFrameId,XELL_PRESENT_END);
 UINT64 presentMarker=0;HRESULT presentMarkerHr=SUCCEEDED(presentHr)?b173n_signal_stage(presentMarker):presentHr;QueryPerformanceCounter(&afterPresent);
 // B416: intentional saturation fail-open must retire before status/reveal failure evaluation.
 if(b343WouldBlock){const UINT64 safeMarker=prefillMarker?prefillMarker:job.on12Marker;if(safeMarker){B174InFlightFrame retained{};retained.marker=safeMarker;retained.sourceFrame=job.sourceFrame;retained.presentId=job.presentId;retained.slotIndex=job.slotIndex;retained.color=job.color;retained.motion=job.motion;retained.depth=job.depth;retained.wrappedMotion=job.wrappedMotion;retained.wrappedDepth=job.wrappedDepth;retained.motionUav=job.motionUav;retained.depthUav=job.depthUav;g_b174InFlightFrames.push_back(retained);job.slotIndex=~0u;}g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);g_b264DropOther.fetch_add(1,std::memory_order_relaxed);g_b174ResumeNeedsFresh.store(true,std::memory_order_release);out<<"B416: WORKER_PRESENT_FAIL_OPEN_NONFATAL source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", queue_before="<<b310PendingBeforePresent<<", queue_after="<<g_b174PendingCount.load(std::memory_order_acquire)<<", safe_retire_marker="<<safeMarker<<", f9_latch_preserved=1, pipeline_stop_requested=0, generation_failure=0, xefg_context_kept_enabled=1\n";out<<"B419: NONFATAL_FAIL_OPEN_CONSUMED source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", duplicate_disposition_call=0, accepted_worker_disposition_retained=1, dropped_counter_incremented=1, pipeline_failed="<<(g_b175PipelineFailed.load(std::memory_order_acquire)?1:0)<<", stop_reason="<<g_b174StopReason.load(std::memory_order_acquire)<<", fatal_teardown=0\n";out.flush();g_b266LastXellSleepUs.store((unsigned long long)(b173g_elapsed_ms(workerBegin,afterSleep)*1000.0),std::memory_order_release);g_b266LastPrivatePresentUs.store((unsigned long long)(b173g_elapsed_ms(presentStart,afterPresent)*1000.0),std::memory_order_release);g_b266LastWorkerTotalUs.store((unsigned long long)(b173g_elapsed_ms(workerBegin,afterPresent)*1000.0),std::memory_order_release);g_b266LastWorkerPending.store(g_b174PendingCount.load(std::memory_order_acquire),std::memory_order_release);b392_clear_stage(g_b392WorkerStage);return S_FALSE;}
 DWM_TIMING_INFO b313DwmAfter{};HRESULT b313DwmAfterHr=S_FALSE; // B391: retire DWM query from every XeFG Present
 HRESULT deviceRemovedReason=(FAILED(prefillHr)||(FAILED(presentHr)&&!b343WouldBlock))&&g_device12?g_device12->GetDeviceRemovedReason():S_OK;if(FAILED(deviceRemovedReason))g_b175bDeviceErrors.fetch_add(1,std::memory_order_acq_rel);
 if(FAILED(prefillHr)||(FAILED(presentHr)&&!b343WouldBlock))b173l_log_dred(out,deviceRemovedReason);
 if(FAILED(prefillHr)||(FAILED(presentHr)&&!b343WouldBlock)){out<<"B352: PRESENT_DEVICE_REMOVED_FORENSIC source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", prefill_hr="<<(long)prefillHr<<", present_hr="<<(long)presentHr<<", device_removed_reason="<<(long)deviceRemovedReason<<", reset_history="<<(frameConstants.resetHistory?1:0)<<", proxy_visible="<<(g_b174ProxyRevealed.load(std::memory_order_acquire)?1:0)<<", queue_before="<<b310PendingBeforePresent<<", queue_now="<<g_b174PendingCount.load(std::memory_order_acquire)<<", stage_before="<<stageCompletedBeforePresent<<", stage_on_return="<<b259StageCompletedOnPresentReturn<<", backbuffer_before="<<b263BackBufferBefore<<", backbuffer_after="<<b263BackBufferAfter<<", diagnostic_only=1, runtime_behavior_changed=0\n";out.flush();}
 xefg_swapchain_present_status_t status{};const bool b395StatusRequired=!g_b174SessionEverRevealed.load(std::memory_order_acquire)||frameConstants.resetHistory||FAILED(presentHr);auto statusResult=b395StatusRequired?(SUCCEEDED(presentHr)?xefgSwapChainGetLastPresentStatus(g_xefgContext,&status):(xefg_swapchain_result_t)-1):XEFG_SWAPCHAIN_RESULT_SUCCESS;if(!b395StatusRequired){status.framesPresented=4;status.isFrameGenEnabled=true;status.frameGenResult=XEFG_SWAPCHAIN_RESULT_SUCCESS;}QueryPerformanceCounter(&end);const UINT64 stageCompletedAfterStatus=g_b173nStageFence?g_b173nStageFence->GetCompletedValue():0;
 const double b263PresentMs=b173g_elapsed_ms(presentStart,afterPresent);
 // B391: one sparse boundary record for genuine whole-run hitches. No DWM/DXGI/thread-time queries.
 static std::atomic<ULONGLONG> b391LastStallLogMs{0};
 const bool b391StallBoundary=(job.sourceFrameRenderTimeMs>=20.0)||(b263PresentMs>=20.0);
 if(b391StallBoundary){const ULONGLONG now=GetTickCount64();ULONGLONG prior=b391LastStallLogMs.load(std::memory_order_relaxed);if(now-prior>=500&&b391LastStallLogMs.compare_exchange_strong(prior,now,std::memory_order_acq_rel)){out<<"B391: WHOLE_RUN_STALL_BOUNDARY source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", source_interval_ms="<<job.sourceFrameRenderTimeMs<<", private_present_ms="<<b263PresentMs<<", queue_before="<<b310PendingBeforePresent<<", queue_after="<<g_b174PendingCount.load(std::memory_order_acquire)<<", stage_before="<<stageCompletedBeforePresent<<", stage_on_return="<<b259StageCompletedOnPresentReturn<<", present_hr="<<(long)presentHr<<", was_still_drawing="<<(b343WouldBlock?1:0)<<", diagnostic_queries_retired=1, rate_limit_ms=500\n";out.flush();}}
 const double b310ThreadCpuMs=(b310ThreadCpuAfter>=b310ThreadCpuBefore)?(b310ThreadCpuAfter-b310ThreadCpuBefore):0.0;
 const double b310BlockedWaitMs=b263PresentMs>b310ThreadCpuMs?b263PresentMs-b310ThreadCpuMs:0.0;
 const double b310BlockedRatio=b263PresentMs>0.0?b310BlockedWaitMs/b263PresentMs:0.0;
 const UINT64 b310StageAdvance=b259StageCompletedOnPresentReturn>=stageCompletedBeforePresent?b259StageCompletedOnPresentReturn-stageCompletedBeforePresent:0;
 const unsigned b310PendingAfterPresent=g_b174PendingCount.load(std::memory_order_acquire);
 const double b313QpcToMs=g_b313QpcFrequency.QuadPart?1000.0/(double)g_b313QpcFrequency.QuadPart:0.0;
 const LONGLONG b313StartFromVblankQpc=SUCCEEDED(b313DwmBeforeHr)?(presentStart.QuadPart-(LONGLONG)b313DwmBefore.qpcVBlank):0;
 const LONGLONG b313EndFromVblankQpc=SUCCEEDED(b313DwmAfterHr)?(afterPresent.QuadPart-(LONGLONG)b313DwmAfter.qpcVBlank):0;
 const double b313StartFromVblankMs=(double)b313StartFromVblankQpc*b313QpcToMs;
 const double b313EndFromVblankMs=(double)b313EndFromVblankQpc*b313QpcToMs;
 const double b313RefreshHz=(SUCCEEDED(b313DwmAfterHr)&&b313DwmAfter.rateRefresh.uiDenominator)?((double)b313DwmAfter.rateRefresh.uiNumerator/(double)b313DwmAfter.rateRefresh.uiDenominator):0.0;
 const double b313RefreshMs=b313RefreshHz>0.0?1000.0/b313RefreshHz:0.0;
 const UINT64 b313Sample=g_b313DwmSamples.fetch_add(1,std::memory_order_relaxed)+1;
 // B342 diagnostic only: B341 runtime hide/reveal was rolled back. Observe post-reveal
 // source-only XeFG transitions without changing camera history, proxy visibility, queueing, or Present.
 static bool b342WasPostRevealSourceOnly=false;
 static unsigned b342PostRevealSourceOnlyRun=0;
 const bool b342PostRevealSourceOnly=g_b174SessionEverRevealed.load(std::memory_order_acquire)&&statusResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&status.isFrameGenEnabled&&status.framesPresented==1&&(int)status.frameGenResult==3;
 if(b342PostRevealSourceOnly){++b342PostRevealSourceOnlyRun;if(!b342WasPostRevealSourceOnly){out<<"B352: VISIBLE_SOURCE_ONLY_FLASH_FORENSIC state=enter, source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", reset_history="<<(frameConstants.resetHistory?1:0)<<", proxy_visible="<<(g_b174ProxyRevealed.load(std::memory_order_acquire)?1:0)<<", backbuffer_before="<<b263BackBufferBefore<<", backbuffer_after="<<b263BackBufferAfter<<", diagnostic_only=1, runtime_behavior_changed=0\n";out.flush();}if(!b342WasPostRevealSourceOnly||b342PostRevealSourceOnlyRun<=3||(b342PostRevealSourceOnlyRun%60)==0){const long long b342DwmRefreshDelta=(SUCCEEDED(b313DwmBeforeHr)&&SUCCEEDED(b313DwmAfterHr))?((long long)b313DwmAfter.cRefresh-(long long)b313DwmBefore.cRefresh):-1;out<<"B342: POST_REVEAL_SOURCE_ONLY_DIAGNOSTIC state="<<(b342WasPostRevealSourceOnly?"continue":"enter")<<", run_frames="<<b342PostRevealSourceOnlyRun<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", reset_history="<<(frameConstants.resetHistory?1:0)<<", proxy_visible="<<(g_b174ProxyRevealed.load(std::memory_order_acquire)?1:0)<<", frames_presented="<<status.framesPresented<<", generated_frames=0, frame_gen_result="<<(int)status.frameGenResult<<", present_wall_ms="<<b263PresentMs<<", worker_cpu_ms="<<b310ThreadCpuMs<<", blocked_wait_ms="<<b310BlockedWaitMs<<", queue_before="<<b310PendingBeforePresent<<", queue_after="<<b310PendingAfterPresent<<", stage_before="<<stageCompletedBeforePresent<<", stage_on_return="<<b259StageCompletedOnPresentReturn<<", dwm_refresh_delta="<<b342DwmRefreshDelta<<", backbuffer_before="<<b263BackBufferBefore<<", backbuffer_after="<<b263BackBufferAfter<<", diagnostic_only=1, runtime_behavior_changed=0\n";out.flush();}}
 else if(b342WasPostRevealSourceOnly){out<<"B352: VISIBLE_SOURCE_ONLY_FLASH_FORENSIC state=exit, run_frames="<<b342PostRevealSourceOnlyRun<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", frame_gen_result="<<(int)status.frameGenResult<<", reset_history="<<(frameConstants.resetHistory?1:0)<<", proxy_visible="<<(g_b174ProxyRevealed.load(std::memory_order_acquire)?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0\n";out.flush();out<<"B342: POST_REVEAL_SOURCE_ONLY_DIAGNOSTIC state=exit, run_frames="<<b342PostRevealSourceOnlyRun<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", frame_gen_result="<<(int)status.frameGenResult<<", reset_history="<<(frameConstants.resetHistory?1:0)<<", proxy_visible="<<(g_b174ProxyRevealed.load(std::memory_order_acquire)?1:0)<<", diagnostic_only=1, runtime_behavior_changed=0\n";out.flush();b342PostRevealSourceOnlyRun=0;}
 b342WasPostRevealSourceOnly=b342PostRevealSourceOnly;
 if(b325SampleWanted){const long long b325PresentCountDelta=(SUCCEEDED(b325StatsBeforeHr)&&SUCCEEDED(b325StatsAfterHr))?((long long)b325StatsAfter.PresentCount-(long long)b325StatsBefore.PresentCount):-1;const long long b325SyncRefreshDelta=(SUCCEEDED(b325StatsBeforeHr)&&SUCCEEDED(b325StatsAfterHr))?((long long)b325StatsAfter.SyncRefreshCount-(long long)b325StatsBefore.SyncRefreshCount):-1;const long long b325PresentRefreshDelta=(SUCCEEDED(b325StatsBeforeHr)&&SUCCEEDED(b325StatsAfterHr))?((long long)b325StatsAfter.PresentRefreshCount-(long long)b325StatsBefore.PresentRefreshCount):-1;const long long b325LastPresentDelta=(SUCCEEDED(b325LastPresentBeforeHr)&&SUCCEEDED(b325LastPresentAfterHr))?((long long)b325LastPresentAfter-(long long)b325LastPresentBefore):-1;const long long b325DwmRefreshDelta=(SUCCEEDED(b313DwmBeforeHr)&&SUCCEEDED(b313DwmAfterHr))?((long long)b313DwmAfter.cRefresh-(long long)b313DwmBefore.cRefresh):-1;const UINT64 b325StageDelta=b259StageCompletedOnPresentReturn>=stageCompletedBeforePresent?b259StageCompletedOnPresentReturn-stageCompletedBeforePresent:0;const char*b325Class=(b310BlockedRatio>=0.80?(b325StageDelta>0?(b325DwmRefreshDelta>0?"blocked_with_gpu_and_dwm_progress":"blocked_with_gpu_progress_no_dwm_tick"):(b325PresentCountDelta>0||b325LastPresentDelta>0?"blocked_dxgi_admission_or_compositor":"blocked_no_observed_progress")):"cpu_or_mixed_present");out<<"B325: PRIVATE_PRESENT_SATURATION_ADMISSION_PROFILE ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", queue_depth_before="<<b310PendingBeforePresent<<", queue_depth_after="<<b310PendingAfterPresent<<", queue_saturated="<<(b310PendingBeforePresent>=kB174oPendingCapacity?1:0)<<", present_wall_ms="<<b263PresentMs<<", worker_thread_cpu_ms="<<b310ThreadCpuMs<<", blocked_wait_ms="<<b310BlockedWaitMs<<", blocked_ratio="<<b310BlockedRatio<<", stats_before_hr="<<(long)b325StatsBeforeHr<<", stats_after_hr="<<(long)b325StatsAfterHr<<", dxgi_present_count_delta="<<b325PresentCountDelta<<", dxgi_present_refresh_delta="<<b325PresentRefreshDelta<<", dxgi_sync_refresh_delta="<<b325SyncRefreshDelta<<", last_present_before_hr="<<(long)b325LastPresentBeforeHr<<", last_present_after_hr="<<(long)b325LastPresentAfterHr<<", last_present_count_delta="<<b325LastPresentDelta<<", dwm_refresh_delta="<<b325DwmRefreshDelta<<", stage_fence_delta="<<b325StageDelta<<", backbuffer_before="<<b263BackBufferBefore<<", backbuffer_after="<<b263BackBufferAfter<<", backbuffer_rotated="<<(b263BackBufferBefore!=b263BackBufferAfter?1:0)<<", classification="<<b325Class<<", diagnostic_only=1, runtime_behavior_changed=0, b324_camera_frozen=1, xefg_x4_unchanged=1\n";out.flush();}
 if(false&&(b263PresentMs>=8.0||job.ordinal<8||(b313Sample%60)==0)){out<<"B313: DWM_VBLANK_PRESENT_CORRELATION ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", present_wall_ms="<<b263PresentMs<<", dwm_before_hr="<<(long)b313DwmBeforeHr<<", dwm_after_hr="<<(long)b313DwmAfterHr<<", qpc_present_start="<<presentStart.QuadPart<<", qpc_present_end="<<afterPresent.QuadPart<<", qpc_vblank_before="<<(SUCCEEDED(b313DwmBeforeHr)?(LONGLONG)b313DwmBefore.qpcVBlank:0)<<", qpc_vblank_after="<<(SUCCEEDED(b313DwmAfterHr)?(LONGLONG)b313DwmAfter.qpcVBlank:0)<<", start_from_vblank_ms="<<b313StartFromVblankMs<<", end_from_vblank_ms="<<b313EndFromVblankMs<<", refresh_hz="<<b313RefreshHz<<", refresh_period_ms="<<b313RefreshMs<<", cRefresh="<<(SUCCEEDED(b313DwmAfterHr)?b313DwmAfter.cRefresh:0)<<", cDXRefresh="<<(SUCCEEDED(b313DwmAfterHr)?b313DwmAfter.cDXRefresh:0)<<", backbuffer_before="<<b263BackBufferBefore<<", backbuffer_after="<<b263BackBufferAfter<<", queue_depth_before="<<b310PendingBeforePresent<<", queue_depth_after="<<b310PendingAfterPresent<<", max_latency=3, buffer_count=3, b316_persistent_monitor_cover="<<(g_b316PersistentMonitorCover.load(std::memory_order_acquire)?1:0)<<", diagnostic_only=1\n";out.flush();}
 if(false&&(b263PresentMs>=8.0||job.ordinal<8||((job.ordinal+1)%120)==0)){out<<"B310: PRIVATE_PRESENT_WAIT_PROFILE ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", wall_present_ms="<<b263PresentMs<<", worker_thread_cpu_ms="<<b310ThreadCpuMs<<", blocked_wait_estimate_ms="<<b310BlockedWaitMs<<", blocked_ratio="<<b310BlockedRatio<<", classification="<<(b310BlockedRatio>=0.80?"blocked_driver_gpu_or_compositor_wait":(b310BlockedRatio<=0.35?"cpu_execution_dominant":"mixed_cpu_wait"))<<", queue_depth_before="<<b310PendingBeforePresent<<", queue_depth_after="<<b310PendingAfterPresent<<", stage_before="<<stageCompletedBeforePresent<<", stage_on_return="<<b259StageCompletedOnPresentReturn<<", stage_advance_during_present="<<b310StageAdvance<<", prefill_marker="<<prefillMarker<<", prefill_complete_on_return="<<(prefillMarker&&b259StageCompletedOnPresentReturn>=prefillMarker?1:0)<<", backbuffer_before="<<b263BackBufferBefore<<", backbuffer_after="<<b263BackBufferAfter<<", b312_latency_ab_applied="<<(g_b312LatencyApplied.load(std::memory_order_acquire)?1:0)<<", diagnostic_only=1, xefg_x4_unchanged=1\n";out.flush();}
 if(false&&b263PresentMs>=12.0){const UINT64 b263StageAdvance=b259StageCompletedOnPresentReturn>=stageCompletedBeforePresent?b259StageCompletedOnPresentReturn-stageCompletedBeforePresent:0;out<<"B263: XEFG_PRESENT_STALL ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", present_call_ms="<<b263PresentMs<<", source_interval_ms="<<job.sourceFrameRenderTimeMs<<", private_present_start_interval_ms="<<presentIntervalMs<<", queue_depth="<<g_b174PendingCount.load(std::memory_order_acquire)<<", backbuffer_before="<<b263BackBufferBefore<<", backbuffer_after="<<b263BackBufferAfter<<", backbuffer_rotated="<<(b263BackBufferBefore!=b263BackBufferAfter?1:0)<<", stage_before="<<stageCompletedBeforePresent<<", stage_on_return="<<b259StageCompletedOnPresentReturn<<", stage_advance="<<b263StageAdvance<<", prefill_marker="<<prefillMarker<<", prefill_complete_on_return="<<(prefillMarker&&b259StageCompletedOnPresentReturn>=prefillMarker?1:0)<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", present_hr="<<(long)presentHr<<", frame_gen_result="<<(int)status.frameGenResult<<"\n";out.flush();}
 if(false&&b263PresentMs>=30.0){const long long b336DwmRefreshDelta=(SUCCEEDED(b313DwmBeforeHr)&&SUCCEEDED(b313DwmAfterHr))?((long long)b313DwmAfter.cRefresh-(long long)b313DwmBefore.cRefresh):-1;const UINT64 b336StageDelta=b259StageCompletedOnPresentReturn>=stageCompletedBeforePresent?b259StageCompletedOnPresentReturn-stageCompletedBeforePresent:0;out<<"B343: PRIVATE_PRESENT_FREEZE_EVENT ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", present_wall_ms="<<b263PresentMs<<", worker_cpu_ms="<<b310ThreadCpuMs<<", blocked_wait_ms="<<b310BlockedWaitMs<<", blocked_ratio="<<b310BlockedRatio<<", queue_before="<<b310PendingBeforePresent<<", queue_after="<<b310PendingAfterPresent<<", stage_before="<<stageCompletedBeforePresent<<", stage_on_return="<<b259StageCompletedOnPresentReturn<<", stage_delta="<<b336StageDelta<<", dwm_refresh_delta="<<b336DwmRefreshDelta<<", backbuffer_before="<<b263BackBufferBefore<<", backbuffer_after="<<b263BackBufferAfter<<", backbuffer_rotated="<<(b263BackBufferBefore!=b263BackBufferAfter?1:0)<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", frame_gen_result="<<(int)status.frameGenResult<<", present_hr="<<(long)presentHr<<", reset_history="<<(frameConstants.resetHistory?1:0)<<", core_render_fg_locked=1, diagnostic_only=1\n";out.flush();}
 const bool lifecycleCurrent=job.windowGeneration==g_b174WindowGeneration.load(std::memory_order_acquire)&&!g_b174SurfaceSuspended.load(std::memory_order_acquire);
 if(!lifecycleCurrent){
  const UINT64 safeMarker=presentMarker?presentMarker:(prefillMarker?prefillMarker:job.on12Marker);
  if(safeMarker){B174InFlightFrame retained{};retained.marker=safeMarker;retained.sourceFrame=job.sourceFrame;retained.presentId=job.presentId;retained.slotIndex=job.slotIndex;retained.color=job.color;retained.motion=job.motion;retained.depth=job.depth;retained.wrappedMotion=job.wrappedMotion;retained.wrappedDepth=job.wrappedDepth;retained.motionUav=job.motionUav;retained.depthUav=job.depthUav;g_b174InFlightFrames.push_back(retained);job.slotIndex=~0u;}
  b174_hide_proxy("surface_changed_during_worker_present");out<<"B174z: STALE_WORKER_OUTPUT_SUPPRESSED source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", job_window_generation="<<job.windowGeneration<<", current_window_generation="<<g_b174WindowGeneration.load(std::memory_order_acquire)<<", proxy_visible=0, resume_requires_fresh_reset=1\n";out.flush();return S_FALSE;
 }
 const bool fresh=job.sourceFrame>g_b174LastPresentedSource.load(std::memory_order_acquire);
 const bool expectedStatus=frameConstants.resetHistory?(status.framesPresented==1&&(int)status.frameGenResult==3):(status.framesPresented==4&&status.frameGenResult==XEFG_SWAPCHAIN_RESULT_SUCCESS);
 const bool xellOk=xellSleepResult==XELL_RESULT_SUCCESS&&simulationStartResult==XELL_RESULT_SUCCESS&&simulationEndResult==XELL_RESULT_SUCCESS&&renderStartResult==XELL_RESULT_SUCCESS&&renderEndResult==XELL_RESULT_SUCCESS&&presentStartResult==XELL_RESULT_SUCCESS&&presentEndResult==XELL_RESULT_SUCCESS;
 if(!tagged)g_b174TagErrors.fetch_add(1,std::memory_order_acq_rel);if((FAILED(presentHr)&&!b343WouldBlock)||(FAILED(presentMarkerHr)&&!b343WouldBlock))g_b174PresentErrors.fetch_add(1,std::memory_order_acq_rel);if(!b343WouldBlock&&(statusResult!=XEFG_SWAPCHAIN_RESULT_SUCCESS||!status.isFrameGenEnabled||!expectedStatus))g_b174StatusErrors.fetch_add(1,std::memory_order_acq_rel);
 const bool presentOk=fresh&&xellOk&&tagged&&SUCCEEDED(prefillHr)&&SUCCEEDED(prefillMarkerHr)&&idResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&SUCCEEDED(presentHr)&&SUCCEEDED(presentMarkerHr)&&statusResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&status.isFrameGenEnabled&&expectedStatus;
 bool revealReady=true;
 if(presentOk&&g_b175pHiddenOnly){out<<"B175p: HIDDEN_LIVE_OUTPUT source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", frame_gen_result="<<(int)status.frameGenResult<<", private_transport_visible=0, gta_presentation_modified=0\n";out.flush();revealReady=true;}
 else if(presentOk&&!g_b174ProxyRevealed.load(std::memory_order_acquire)){
  const UINT64 completed=g_b173nStageFence?g_b173nStageFence->GetCompletedValue():0;const UINT64 priorRevealMarker=g_b174RevealFenceMarker.load(std::memory_order_acquire);const bool priorMarkerComplete=priorRevealMarker&&completed>=priorRevealMarker;
  const unsigned completedBefore=g_b174HiddenCompleted.load(std::memory_order_acquire);
  out<<"B174f: HIDDEN_LIVE_PRESENT_COMPLETE source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", live_sequence="<<(completedBefore+1)<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", frame_gen_result="<<(int)status.frameGenResult<<", marker="<<presentMarker<<", prior_reveal_marker="<<priorRevealMarker<<", completed_value="<<completed<<", prior_marker_complete="<<(priorMarkerComplete?1:0)<<", cpu_wait=0, elapsed_from_f9_ms="<<(g_b174ActivationStart?GetTickCount64()-g_b174ActivationStart:0)<<"\n";out.flush();
  if(completedBefore==0){
   UINT64 seedMarker=0;HRESULT seedHr=b174c_seed_all_proxy_backbuffers(job.color.Get(),seedMarker,out);
   revealReady=SUCCEEDED(seedHr)&&seedMarker&&!g_b174StopReason.load(std::memory_order_acquire);g_b174StartupBackbuffersSeeded.store(revealReady,std::memory_order_release);if(revealReady){g_b174RevealFenceMarker.store(seedMarker,std::memory_order_release);g_b174HiddenCompleted.store(1,std::memory_order_release);}
   out<<"B174f: HIDDEN_WARMUP_SEED_QUEUED source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", seed_marker="<<seedMarker<<", queue_ordered_after_present_marker=1, all_backbuffers_seeded="<<(SUCCEEDED(seedHr)?1:0)<<", awaiting_completed_x4_marker=1, proxy_visible=0, cpu_wait=0, black_or_uninitialized_possible=0\n";out.flush();
   if(!revealReady){
    if(g_b174SessionEverRevealed.load(std::memory_order_acquire)){const ULONGLONG recoveryNow=GetTickCount64();g_b174StartupDeadline.store(recoveryNow+1500,std::memory_order_release);g_b174ResumeNeedsFresh.store(true,std::memory_order_release);g_b174HiddenCompleted.store(0,std::memory_order_release);g_b174StartupBackbuffersSeeded.store(false,std::memory_order_release);g_b174RevealFenceMarker.store(0,std::memory_order_release);revealReady=true;out<<"B250: PERSISTENT_RECOVERY_REARM reason=warmup_or_seed_failure, established_session=1, toggle_off=0, proxy_remained_hidden=1, reset_hidden_warmup=1, retry_deadline_ms=1500\n";out.flush();}
    else{out<<"B174f: PROXY_REVEAL_ABORTED reason=warmup_or_seed_failure, proxy_remained_hidden=1, black_flash_possible=0\n";out.flush();b174_request_stop(5);}
   }
  }else if(g_b174StartupBackbuffersSeeded.load(std::memory_order_acquire)&&!frameConstants.resetHistory&&status.framesPresented==4&&status.frameGenResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&!g_b174StopReason.load(std::memory_order_acquire)){
   const bool b352InitialStartup=!g_b174SessionEverRevealed.load(std::memory_order_acquire);
   const ULONGLONG b352ElapsedFromActivation=g_b174ActivationStart?GetTickCount64()-g_b174ActivationStart:0;
   constexpr ULONGLONG b352InitialQuietRevealDelayMs=1500;
   const unsigned requiredHiddenX4Frames=b352InitialStartup?1u:2u;
   unsigned hiddenX4Streak=completedBefore;
   if(b352InitialStartup){
    g_b352InitialValidX4Seen.store(true,std::memory_order_release);
    g_b174RevealFenceMarker.store(presentMarker,std::memory_order_release);
    if(hiddenX4Streak<2u){hiddenX4Streak=2u;g_b174HiddenCompleted.store(2u,std::memory_order_release);}
    out<<"B352: INITIAL_VALID_X4_LATCHED source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", frames_presented=4, generated_frames=3, result=0, elapsed_from_activation_ms="<<b352ElapsedFromActivation<<", latch_persists_across_result3=1, proxy_visible=0\n";out.flush();
   }else if(hiddenX4Streak<requiredHiddenX4Frames){
    g_b174RevealFenceMarker.store(presentMarker,std::memory_order_release);g_b174HiddenCompleted.store(completedBefore+1,std::memory_order_release);
    out<<"B174o: HIDDEN_X4_STABILITY_PROGRESS source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", consecutive_x4="<<(completedBefore+1)<<", required_x4="<<requiredHiddenX4Frames<<", prior_marker_complete="<<(priorMarkerComplete?1:0)<<", proxy_visible=0, reveal_ready=0, cpu_wait=0\n";out.flush();
   }
   if(b352InitialStartup&&b352ElapsedFromActivation<b352InitialQuietRevealDelayMs){
    out<<"B352: STARTUP_REVEAL_DEFERRED source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", reason=quiet_window, elapsed_from_activation_ms="<<b352ElapsedFromActivation<<", minimum_ms="<<b352InitialQuietRevealDelayMs<<", valid_x4_seen="<<(g_b352InitialValidX4Seen.load(std::memory_order_acquire)?1:0)<<", proxy_visible=0, ghosting_path_unchanged=1, reset_history_unchanged=1\n";out.flush();
   }else if(!b352InitialStartup&&hiddenX4Streak<requiredHiddenX4Frames){
    revealReady=true;
   }else if(!b352InitialStartup&&!priorMarkerComplete){
    out<<"B174o: HIDDEN_X4_MARKER_PENDING source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", reveal_marker="<<priorRevealMarker<<", completed_value="<<completed<<", proxy_visible=0, cpu_wait=0\n";out.flush();
   }else if(!b174_poll_source_window("worker")){
    out<<"B174z: SURFACE_REVEAL_DEFERRED source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", reason=worker_window_gate, proxy_visible=0, fresh_reset_required=1\n";out.flush();
   }else{
    const bool revealRequested=g_xefgWindow&&PostMessageA(g_xefgWindow,kB171VisibilityMessage,1,0)!=FALSE;for(unsigned i=0;i<50&&revealRequested&&!g_b171WindowVisible.load(std::memory_order_acquire);++i)Sleep(10);
    RECT proxyRect{};if(g_xefgWindow)GetWindowRect(g_xefgWindow,&proxyRect);const RECT targetRect=b174m_proxy_rect();const bool rectMatch=EqualRect(&proxyRect,&targetRect)!=FALSE;const bool visible=g_b171WindowVisible.load(std::memory_order_acquire);
    revealReady=revealRequested&&visible&&rectMatch;g_b174ProxyRevealed.store(revealReady,std::memory_order_release);if(revealReady){g_b174SessionEverRevealed.store(true,std::memory_order_release);g_b175rRevealTick=GetTickCount64();}
    out<<"B175r: GTA_ALIGNED_REPLACEMENT_REVEAL source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", consecutive_x4="<<hiddenX4Streak<<", required_x4="<<requiredHiddenX4Frames<<", requested="<<(revealRequested?1:0)<<", visible="<<(visible?1:0)<<", exact_gta_client_rect="<<(rectMatch?1:0)<<", inset_pixels=0, completed_post_present_marker="<<priorRevealMarker<<", prior_all_backbuffers_seeded=1, black_or_uninitialized_possible=0, elapsed_from_toggle_ms="<<(g_b174ActivationStart?GetTickCount64()-g_b174ActivationStart:0)<<"\n";out.flush();
    if(!revealReady){
     b174_hide_proxy("proxy_reveal_verification_failed");g_b174StartupDeadline.store(GetTickCount64()+1500,std::memory_order_release);for(unsigned i=0;i<50&&g_b171WindowVisible.load(std::memory_order_acquire);++i)Sleep(10);const bool hidden=!g_b171WindowVisible.load(std::memory_order_acquire);
     out<<"B174y: SURFACE_REVEAL_RETRY reason=visibility_or_geometry_verification_failed, proxy_hidden="<<(hidden?1:0)<<", fresh_warmup_required=1, pipeline_failed="<<(hidden?0:1)<<", black_or_uninitialized_possible=0\n";out.flush();
     revealReady=hidden;if(!hidden)b174_request_stop(5);
    }
   }
  }else{
   const bool persistentRecovery=g_b174SessionEverRevealed.load(std::memory_order_acquire);
   if(persistentRecovery){
    const bool recoverySeedFrame=frameConstants.resetHistory&&status.framesPresented==1&&(int)status.frameGenResult==3;
    if(recoverySeedFrame){
     // B250: a reset-history frame is XeFG's expected one-frame seed. B249 re-armed
     // resetHistory here, causing every following hidden frame to become another seed
     // and preventing the 4/3 stability streak (and FPS HUD) from ever returning.
     // Consume this seed exactly once; the next fresh frame runs with resetHistory=0.
     g_b174ResumeNeedsFresh.store(false,std::memory_order_release);
     g_b174StartupDeadline.store(GetTickCount64()+1500,std::memory_order_release);
     revealReady=true;
     out<<"B250: RECOVERY_SEED_ACCEPTED frames_presented=1, generated_frames=0, frame_gen_result="<<(int)status.frameGenResult<<", reset_history_consumed=1, rearm_reset=0, next_frame_requires_x4=1, proxy_remained_hidden=1, persistent_session=1\n";out.flush();
    }else{
     const ULONGLONG recoveryNow=GetTickCount64();
     g_b174StartupDeadline.store(recoveryNow+1500,std::memory_order_release);
     g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
     g_b174HiddenCompleted.store(0,std::memory_order_release);
     g_b174StartupBackbuffersSeeded.store(false,std::memory_order_release);
     g_b174RevealFenceMarker.store(0,std::memory_order_release);
     revealReady=true;
     out<<"B250: PERSISTENT_RECOVERY_REARM reason=hidden_x4_or_seed_validation_failed, established_session=1, toggle_off=0, proxy_remained_hidden=1, reset_hidden_warmup=1, retry_deadline_ms=1500, elapsed_from_f9_ms="<<(g_b174ActivationStart?recoveryNow-g_b174ActivationStart:0)<<"\n";out.flush();
    }
   }else{
    const ULONGLONG startupNow=GetTickCount64();
    const bool b352InitialStartup=!g_b174SessionEverRevealed.load(std::memory_order_acquire);
    const bool initialStartupSeedFrame=b352InitialStartup&&frameConstants.resetHistory&&status.framesPresented==1&&(int)status.frameGenResult==3&&!g_b174StopReason.load(std::memory_order_acquire);
    if(initialStartupSeedFrame){
     // B357: B352 already proved at least one valid hidden x4 frame. If the immutable B343/B345
     // safety path is now legitimately producing repeated reset/result3 frames, do not deadlock
     // first reveal waiting for another non-reset x4 after the 1500 ms quiet window. Reveal from
     // the already seeded/verified private swapchain state. B356 reset-prefill suppression remains
     // disabled until g_b174ProxyRevealed becomes true, so startup still uses the exact B355 prefill.
     g_b174StartupBackbuffersSeeded.store(true,std::memory_order_release);
     if(!g_b352InitialValidX4Seen.load(std::memory_order_acquire))g_b174HiddenCompleted.store(1,std::memory_order_release);
     if(presentMarker)g_b174RevealFenceMarker.store(presentMarker,std::memory_order_release);
     const ULONGLONG b357Elapsed=g_b174ActivationStart?startupNow-g_b174ActivationStart:0;
     const bool b357LatchedRevealEligible=g_b352InitialValidX4Seen.load(std::memory_order_acquire)&&b357Elapsed>=1500&&g_b174StartupBackbuffersSeeded.load(std::memory_order_acquire)&&!g_b174StopReason.load(std::memory_order_acquire);
     if(b357LatchedRevealEligible){
      if(!b174_poll_source_window("worker")){
       revealReady=true;
       out<<"B357: POST_LATCH_RESULT3_REVEAL_DEFERRED source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", reason=worker_window_gate, elapsed_from_activation_ms="<<b357Elapsed<<", valid_x4_seen=1, seeded=1, proxy_visible=0, reset_history_unchanged=1, ghosting_path_unchanged=1\n";out.flush();
      }else{
       const bool revealRequested=g_xefgWindow&&PostMessageA(g_xefgWindow,kB171VisibilityMessage,1,0)!=FALSE;for(unsigned i=0;i<50&&revealRequested&&!g_b171WindowVisible.load(std::memory_order_acquire);++i)Sleep(10);
       RECT proxyRect{};if(g_xefgWindow)GetWindowRect(g_xefgWindow,&proxyRect);const RECT targetRect=b174m_proxy_rect();const bool rectMatch=EqualRect(&proxyRect,&targetRect)!=FALSE;const bool visible=g_b171WindowVisible.load(std::memory_order_acquire);
       const bool b357RevealOk=revealRequested&&visible&&rectMatch;g_b174ProxyRevealed.store(b357RevealOk,std::memory_order_release);if(b357RevealOk){g_b174SessionEverRevealed.store(true,std::memory_order_release);g_b175rRevealTick=GetTickCount64();}
       out<<"B357: POST_LATCH_RESULT3_REVEAL source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", elapsed_from_activation_ms="<<b357Elapsed<<", valid_x4_seen=1, seeded=1, current_result3=1, requested="<<(revealRequested?1:0)<<", visible="<<(visible?1:0)<<", exact_gta_client_rect="<<(rectMatch?1:0)<<", reveal_ok="<<(b357RevealOk?1:0)<<", b356_suppression_activates_after_reveal=0, b358_exact_b355_visible_prefill=1, reset_history_unchanged=1, ghosting_path_unchanged=1\n";out.flush();
       if(!b357RevealOk){b174_hide_proxy("b357_post_latch_reveal_verification_failed");g_b174StartupDeadline.store(GetTickCount64()+1500,std::memory_order_release);}
       revealReady=true;
      }
     }else{
      revealReady=true;
      out<<"B352: INITIAL_RESULT3_TOLERATED source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", frames_presented="<<status.framesPresented<<", generated_frames=0, frame_gen_result="<<(int)status.frameGenResult<<", reset_history=1, proxy_remained_hidden=1, f9_latch_preserved=1, valid_x4_seen="<<(g_b352InitialValidX4Seen.load(std::memory_order_acquire)?1:0)<<", startup_timeout_is_soft_wait=1, core_render_fg_locked=1\n";out.flush();
     }
    }else{
     // B420: an initial hidden-frame validation miss is a warm-up outcome, not a fatal pipeline error.
     // Keep the F9-owned session alive and re-arm the hidden warm-up exactly as established-session recovery does.
     g_b174StartupDeadline.store(startupNow+1500,std::memory_order_release);
     g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
     g_b174HiddenCompleted.store(0,std::memory_order_release);
     g_b174StartupBackbuffersSeeded.store(false,std::memory_order_release);
     g_b174RevealFenceMarker.store(0,std::memory_order_release);
     revealReady=true;
     out<<"B420: INITIAL_ACTIVATION_REARM reason=hidden_x4_or_seed_validation_failed, proxy_remained_hidden=1, f9_latch_preserved=1, pipeline_stop_requested=0, pipeline_failed="<<(g_b175PipelineFailed.load(std::memory_order_acquire)?1:0)<<", retry_deadline_ms=1500, elapsed_from_f9_ms="<<(g_b174ActivationStart?startupNow-g_b174ActivationStart:0)<<"\n";out.flush();
    }
   }
  }
 }
 const bool ok=presentOk&&revealReady;
 if(ok){
  const unsigned completedFrames=g_b171LiveSubmitted.fetch_add(1,std::memory_order_acq_rel)+1;const unsigned outputFrames=g_b174OutputFrames.fetch_add(status.framesPresented,std::memory_order_acq_rel)+status.framesPresented;g_b270LastSuccessfulOutputTick.store(GetTickCount64(),std::memory_order_release);b207_update_realtime_fps();g_b174GeneratedFrames.fetch_add(status.framesPresented-1,std::memory_order_acq_rel);g_b174TotalGenerated.fetch_add(status.framesPresented-1,std::memory_order_acq_rel);g_b174TotalDisplayed.fetch_add(status.framesPresented,std::memory_order_acq_rel);g_b174LastPresentedSource.store(job.sourceFrame,std::memory_order_release);g_b172PreviousTime=job.captureTime;AcquireSRWLockExclusive(&g_b175MotionHistoryLock);std::memcpy(g_b175ConsumedViewProjection,job.currentViewProjection,64);g_b175HaveConsumedViewProjection=true;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);B174InFlightFrame retained{};retained.marker=presentMarker;retained.sourceFrame=job.sourceFrame;retained.presentId=job.presentId;retained.slotIndex=job.slotIndex;retained.color=job.color;retained.motion=job.motion;retained.depth=job.depth;retained.wrappedMotion=job.wrappedMotion;retained.wrappedDepth=job.wrappedDepth;retained.motionUav=job.motionUav;retained.depthUav=job.depthUav;g_b174InFlightFrames.push_back(retained);
  const auto rateWindow=b174_rate_window_snapshot();const ULONGLONG rateElapsedMs=rateWindow.start?GetTickCount64()-rateWindow.start:0;const unsigned observedTotal=g_b174oObservedSourcePresents.load(std::memory_order_acquire);const unsigned sourcePresents=observedTotal>=rateWindow.observedBase?observedTotal-rateWindow.observedBase:0;const unsigned completedInWindow=completedFrames>=rateWindow.completedBase?completedFrames-rateWindow.completedBase:0;const unsigned outputInWindow=outputFrames>=rateWindow.outputBase?outputFrames-rateWindow.outputBase:0;const double sourceRate=rateElapsedMs?1000.0*sourcePresents/rateElapsedMs:0.0;const double generatedPresentationRate=rateElapsedMs?1000.0*outputInWindow/rateElapsedMs:0.0;
  if(completedInWindow>=30&&rateElapsedMs>=3000&&generatedPresentationRate+1.0<sourceRate){
   // B271 Intel-reference persistence rule: a temporary measured-rate anomaly is telemetry, not an FG lifecycle event.
   // Intel's integration controls FG through the explicit enable state; frame pacing/rate variation is not a reason
   // to disable the XeFG context. Keep F9 ownership and the proxy alive and let the normal per-frame status plus
   // genuine device/swapchain recovery paths decide whether reconstruction is required.
   out<<"GENERATED_RATE_DIAGNOSTIC: source_presents="<<sourcePresents<<", completed_source_frames="<<completedInWindow<<", output_frames="<<outputInWindow<<", elapsed_ms="<<rateElapsedMs<<", gta_source_rate_fps="<<sourceRate<<", generated_presentation_rate_fps="<<generatedPresentationRate<<", below_source=1, action=keep_f9_enabled, xefg_disable_called=0, performance_policy=FROZEN\n";out.flush();
  }
 }
 else{const UINT64 safeMarker=presentMarker?presentMarker:(prefillMarker?prefillMarker:job.on12Marker);if(safeMarker){B174InFlightFrame retained{};retained.marker=safeMarker;retained.sourceFrame=job.sourceFrame;retained.presentId=job.presentId;retained.slotIndex=job.slotIndex;retained.color=job.color;retained.motion=job.motion;retained.depth=job.depth;retained.wrappedMotion=job.wrappedMotion;retained.wrappedDepth=job.wrappedDepth;retained.motionUav=job.motionUav;retained.depthUav=job.depthUav;g_b174InFlightFrames.push_back(retained);job.slotIndex=~0u;}b174_request_stop(g_b174StopReason.load(std::memory_order_acquire)?g_b174StopReason.load(std::memory_order_acquire):4);}
 g_b266LastXellSleepUs.store((unsigned long long)(b173g_elapsed_ms(workerBegin,afterSleep)*1000.0),std::memory_order_release);g_b266LastPrivatePresentUs.store((unsigned long long)(b173g_elapsed_ms(presentStart,afterPresent)*1000.0),std::memory_order_release);g_b266LastWorkerTotalUs.store((unsigned long long)(b173g_elapsed_ms(workerBegin,end)*1000.0),std::memory_order_release);g_b266LastWorkerPending.store(g_b174PendingCount.load(std::memory_order_acquire),std::memory_order_release);
 // B274: do not mutate XeFG/XeLL state in response to measured rate. B273 proved that
 // disable/re-enable + sleep-mode reapply only restarts warmup and the ~60 Hz-like XeLL state returns.
 // Keep one xellSleep and all six markers for every present ID. This build changes the XeLL
 // timing model below by making the simulation phase represent the real GTA source-frame interval
 // rather than an effectively zero-duration worker-local phase. No sleep skipping or FG toggling.
 const double b274SleepMs=b173g_elapsed_ms(workerBegin,afterSleep);
 const double b274WorkerMs=b173g_elapsed_ms(workerBegin,end);
 const unsigned b274Pending=g_b174PendingCount.load(std::memory_order_acquire);
 const double b274SourceFps=g_b191SourceFps100.load(std::memory_order_acquire)/100.0;
 const bool b276Locked=status.framesPresented==4&&status.frameGenResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&SUCCEEDED(presentHr)&&xellOk&&b274SourceFps>=80.0&&b274Pending>=8&&b274SleepMs>=8.0&&b274WorkerMs>=14.0;
 if(b276Locked){
  const unsigned lockedFrames=g_b276PacingLockFrames.fetch_add(1,std::memory_order_acq_rel)+1;
  const ULONGLONG now=GetTickCount64(),last=g_b276LastRediscoveryRequestTick.load(std::memory_order_acquire);
  if(lockedFrames>=90&&(last==0||now-last>=30000)&&!g_b276RediscoveryRequested.exchange(true,std::memory_order_acq_rel)){
   g_b276LastRediscoveryRequestTick.store(now,std::memory_order_release);g_b276PacingLockFrames.store(0,std::memory_order_release);
   out<<"B276: PACING_LOCK_REDISCOVERY_REQUEST source_fps="<<b274SourceFps<<", xell_sleep_ms="<<b274SleepMs<<", worker_total_ms="<<b274WorkerMs<<", pending="<<b274Pending<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", f9_latch_preserved=1, xefg_toggle=0, xell_sleep_skipped=0, action=request_existing_b216_b252_rediscovery\n";out.flush();
  }
 }else g_b276PacingLockFrames.store(0,std::memory_order_release);
 if(b242Verbose)out<<"B174o: XELL_FRAME_LIFECYCLE ordinal="<<job.ordinal+1<<", present_id="<<job.presentId<<", sleep="<<(int)xellSleepResult<<", simulation_start="<<(int)simulationStartResult<<", simulation_end="<<(int)simulationEndResult<<", render_submit_start="<<(int)renderStartResult<<", render_submit_end="<<(int)renderEndResult<<", present_start="<<(int)presentStartResult<<", present_end="<<(int)presentEndResult<<", exact_sample_order=1, all_required_markers="<<(xellOk?1:0)<<", xell_sleep_ms="<<b173g_elapsed_ms(workerBegin,afterSleep)<<"\n";
 if(b242Verbose)out<<"B174r: PRIVATE_PRESENT_CADENCE ordinal="<<job.ordinal+1<<", present_id="<<job.presentId<<", source_present_interval_ms="<<presentIntervalMs<<", sync_interval=0, present_flags=DXGI_PRESENT_ALLOW_TEARING, swapchain_allow_tearing=1, refresh_rate_lock=0, vsync_lock=0, xell_sleep_called=1, xell_markers_called=6\n";
 if(b242Verbose)out<<"B173h: WORKER_STAGE_TIMING ordinal="<<job.ordinal+1<<", queue_delay_ms="<<b173g_elapsed_ms(job.queuedTime,workerBegin)<<", gta_thread_ms="<<job.gtaThreadMs<<", xell_sleep_ms="<<b173g_elapsed_ms(workerBegin,afterSleep)<<", worker_input_ms="<<b173g_elapsed_ms(afterSleep,afterPreparation)<<", worker_input_wait_ms="<<b173g_elapsed_ms(afterPreparation,afterInputReady)<<", tag_ms="<<b173g_elapsed_ms(afterInputReady,afterTags)<<", prefill_ms="<<b173g_elapsed_ms(afterTags,afterPrefill)<<", source_present_interval_ms="<<presentIntervalMs<<", private_present_ms="<<b173g_elapsed_ms(presentStart,afterPresent)<<", status_ms="<<b173g_elapsed_ms(afterPresent,end)<<", worker_total_ms="<<b173g_elapsed_ms(workerBegin,end)<<"\n";
 if(b242Verbose)out<<"B173n: GPU_STAGE_COMPLETION ordinal="<<job.ordinal+1<<", native_fence="<<job.nativeFenceValue<<", native_completed_at_worker_start="<<nativeCompletedAtStart<<", on12_marker="<<job.on12Marker<<", stage_completed_at_worker_start="<<stageCompletedAtStart<<", prefill_marker="<<prefillMarker<<", prefill_marker_hr="<<(long)prefillMarkerHr<<", stage_completed_before_present="<<stageCompletedBeforePresent<<", present_marker="<<presentMarker<<", present_marker_hr="<<(long)presentMarkerHr<<", stage_completed_after_status="<<stageCompletedAfterStatus<<", cpu_fence_wait=0\n";
 if(job.ordinal<8||((job.ordinal+1)%120)==0){
  const double b255SleepMs=b173g_elapsed_ms(workerBegin,afterSleep);
  const double b255PrepareMs=b173g_elapsed_ms(afterSleep,afterPreparation);
  const double b255InputWaitMs=b173g_elapsed_ms(afterPreparation,afterInputReady);
  const double b255TagMs=b173g_elapsed_ms(afterInputReady,afterTags);
  const double b255PrefillMs=b173g_elapsed_ms(afterTags,afterPrefill);
  const double b255PrePresentGapMs=b173g_elapsed_ms(afterPrefill,presentStart);
  const double b255PresentMs=b173g_elapsed_ms(presentStart,afterPresent);
  const double b255StatusMs=b173g_elapsed_ms(afterPresent,end);
  const double b255WorkerMs=b173g_elapsed_ms(workerBegin,end);
  const double b255AccountedMs=b255SleepMs+b255PrepareMs+b255InputWaitMs+b255TagMs+b255PrefillMs+b255PrePresentGapMs+b255PresentMs+b255StatusMs;
  const double b255UnaccountedMs=b255WorkerMs>b255AccountedMs?b255WorkerMs-b255AccountedMs:0.0;
  const double b255OutputFps=presentIntervalMs>0.0?((double)status.framesPresented*1000.0/presentIntervalMs):0.0;
  out<<"B258: WORKER_CRITICAL_PATH ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", queue_delay_ms="<<b173g_elapsed_ms(job.queuedTime,workerBegin)<<", source_interval_ms="<<job.sourceFrameRenderTimeMs<<", private_present_start_interval_ms="<<presentIntervalMs<<", xell_sleep_ms="<<b255SleepMs<<", prepare_inputs_ms="<<b255PrepareMs<<", input_wait_submit_ms="<<b255InputWaitMs<<", tag_ms="<<b255TagMs<<", prefill_ms="<<b255PrefillMs<<", pre_present_gap_ms="<<b255PrePresentGapMs<<", private_present_call_ms="<<b255PresentMs<<", status_ms="<<b255StatusMs<<", worker_total_ms="<<b255WorkerMs<<", accounted_ms="<<b255AccountedMs<<", unaccounted_ms="<<b255UnaccountedMs<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", instantaneous_output_capacity_fps="<<b255OutputFps<<", pending_after="<<g_b174PendingCount.load(std::memory_order_acquire)<<", present_hr="<<(long)presentHr<<", status_result="<<(int)statusResult<<", frame_gen_result="<<(int)status.frameGenResult<<"\n";out.flush();
  const double b257PresentVsSource=(job.sourceFrameRenderTimeMs>0.0)?(b255PresentMs/job.sourceFrameRenderTimeMs):0.0;
  out<<"B258: PRESENT_BACKPRESSURE ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", source_interval_ms="<<job.sourceFrameRenderTimeMs<<", present_call_ms="<<b255PresentMs<<", present_to_source_ratio="<<b257PresentVsSource<<", worker_total_ms="<<b255WorkerMs<<", queue_depth="<<g_b174PendingCount.load(std::memory_order_acquire)<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", xefg_debug_callback_level=ERROR, present_is_dominant="<<(b255PresentMs>b255WorkerMs*0.75?1:0)<<"\n";out.flush();
  const bool b259PrefillPendingBeforePresent=prefillMarker&&stageCompletedBeforePresent<prefillMarker;
  const bool b259PrefillCompleteOnReturn=prefillMarker&&b259StageCompletedOnPresentReturn>=prefillMarker;
  const UINT64 b259StageAdvance=(b259StageCompletedOnPresentReturn>=stageCompletedBeforePresent)?(b259StageCompletedOnPresentReturn-stageCompletedBeforePresent):0;
  out<<"B259: XEFG_PRESENT_GPU_QUEUE_PROBE ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", prefill_marker="<<prefillMarker<<", stage_completed_before_present="<<stageCompletedBeforePresent<<", stage_completed_on_present_return="<<b259StageCompletedOnPresentReturn<<", stage_advance_during_present="<<b259StageAdvance<<", prefill_pending_before_present="<<(b259PrefillPendingBeforePresent?1:0)<<", prefill_complete_on_present_return="<<(b259PrefillCompleteOnReturn?1:0)<<", present_call_ms="<<b255PresentMs<<", source_interval_ms="<<job.sourceFrameRenderTimeMs<<", queue_depth="<<g_b174PendingCount.load(std::memory_order_acquire)<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", interpretation=prefill_pending_before_plus_complete_on_return_means_present_spanned_gpu_queue_progress\n";out.flush();
 }
 if(job.ordinal<8||((job.ordinal+1)%120)==0)out<<"B173h: LIVE_FRAME_RESULT ordinal="<<job.ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id="<<job.presentId<<", fresh="<<(fresh?1:0)<<", queued=1, worker_thread=1, temporal_source=reconstructed_depth_B287_main_3d_rage, native_gta_motion_vectors=1, jitter_source=forced_zero, motion_scale={"<<frameConstants.motionVectorScaleX<<","<<frameConstants.motionVectorScaleY<<"}, constants="<<(int)constantsResult<<", motion="<<(int)motionResult<<", depth="<<(int)depthResult<<", color="<<(int)colorResult<<", prefill_hr="<<(long)prefillHr<<", set_id="<<(int)idResult<<", present_hr="<<(long)presentHr<<", device_removed_reason="<<(long)deviceRemovedReason<<", status="<<(int)statusResult<<", frames_presented="<<status.framesPresented<<", generated_frames="<<(status.framesPresented?status.framesPresented-1:0)<<", frame_gen_result="<<(int)status.frameGenResult<<", ready="<<(ok?1:0)<<"\n";out.flush();
 b392_clear_stage(g_b392WorkerStage);return ok?S_OK:E_FAIL;
}
struct B160MotionWork{std::vector<unsigned char> motionBytes,firstDepthBytes,secondDepthBytes;float firstView[16]{},firstProjection[16]{},secondView[16]{},secondProjection[16]{};UINT width{},height{},motionPitch{},firstDepthPitch{},secondDepthPitch{};std::string path;};
static DWORD WINAPI b160_motion_worker(void*parameter){
 auto*w=static_cast<B160MotionWork*>(parameter);std::ofstream out(w->path,std::ios::app);unsigned waited=0;while(!g_xefgInitFinished.load(std::memory_order_acquire)){Sleep(10);waited+=10;}bool ready=g_xefgReady.load(std::memory_order_acquire);
 const bool discoveryResourcesReady=g_b173iColorPoolReady.load(std::memory_order_acquire)&&g_b171SecondColorReady.load(std::memory_order_acquire)&&g_b171SecondColorResource;
 out<<"B172: bridge_worker init_finished=1, ready="<<(ready?1:0)<<", discovery_resources_ready="<<(discoveryResourcesReady?1:0)<<", color_pool_ready="<<(g_b173iColorPoolReady.load()?1:0)<<", second_color_ready="<<(g_b171SecondColorReady.load()?1:0)<<", wait_ms="<<waited<<", timeout_ms=0, motion_bytes="<<w->motionBytes.size()<<", first_depth_bytes="<<w->firstDepthBytes.size()<<", second_depth_bytes="<<w->secondDepthBytes.size()<<"\n";out.flush();
 if(!ready||!discoveryResourcesReady||!g_device12||!g_on12||!g_on12Context||!g_xefgContext){g_b175PipelineReady.store(false,std::memory_order_release);g_b175PipelineFailed.store(true,std::memory_order_release);out<<"B172: MOTION_TAG_COMPLETE ready=0, reason="<<(!ready?"xefg_initialization_failed":"discovery_frame_resources_not_ready")<<", enabled=0, generated_frames=0\n";delete w;return 1;}
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w->width;d.Height=w->height;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_R16G16_FLOAT;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
 g_b160MotionResource.Reset();HRESULT createHr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&g_b160MotionResource));
 out<<"B172: stage=CreateD3D12Motion, size="<<w->width<<"x"<<w->height<<", format="<<(unsigned)d.Format<<", created="<<(g_b160MotionResource?1:0)<<", hr="<<(long)createHr<<"\n";out.flush();
 D3D11_RESOURCE_FLAGS wrappedFlags{};ComPtr<ID3D11Resource>wrapped;HRESULT wrapHr=FAILED(createHr)?createHr:g_on12->CreateWrappedResource(g_b160MotionResource.Get(),&wrappedFlags,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&wrapped));
 out<<"B172: stage=CreateWrappedMotion, created="<<(wrapped?1:0)<<", incoming=COPY_DEST, outgoing=NON_PIXEL_SHADER_RESOURCE, hr="<<(long)wrapHr<<"\n";out.flush();
 if(SUCCEEDED(wrapHr)&&wrapped){ID3D11Resource*r[]{wrapped.Get()};g_on12->AcquireWrappedResources(r,1);g_on12Context->UpdateSubresource(wrapped.Get(),0,nullptr,w->motionBytes.data(),w->motionPitch,0);g_on12->ReleaseWrappedResources(r,1);g_on12Context->Flush();}
 D3D12_RESOURCE_DESC depthDesc=d;depthDesc.Format=DXGI_FORMAT_R32_FLOAT;g_b163DepthResource.Reset();
 HRESULT depthCreateHr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&depthDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&g_b163DepthResource));
 out<<"B172: stage=CreateD3D12Depth, size="<<w->width<<"x"<<w->height<<", format="<<(unsigned)depthDesc.Format<<", created="<<(g_b163DepthResource?1:0)<<", hr="<<(long)depthCreateHr<<"\n";out.flush();
 D3D11_RESOURCE_FLAGS depthFlags{};ComPtr<ID3D11Resource>wrappedDepth;HRESULT depthWrapHr=FAILED(depthCreateHr)?depthCreateHr:g_on12->CreateWrappedResource(g_b163DepthResource.Get(),&depthFlags,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&wrappedDepth));
 out<<"B172: stage=CreateWrappedDepth, created="<<(wrappedDepth?1:0)<<", incoming=COPY_DEST, outgoing=NON_PIXEL_SHADER_RESOURCE, hr="<<(long)depthWrapHr<<"\n";out.flush();
 if(SUCCEEDED(depthWrapHr)&&wrappedDepth){ID3D11Resource*r[]{wrappedDepth.Get()};g_on12->AcquireWrappedResources(r,1);g_on12Context->UpdateSubresource(wrappedDepth.Get(),0,nullptr,w->secondDepthBytes.data(),w->secondDepthPitch,0);g_on12->ReleaseWrappedResources(r,1);g_on12Context->Flush();}
 ComPtr<ID3D12Resource> firstMotionResource,firstDepthResource;std::vector<unsigned char> zeroMotion((size_t)w->motionPitch*w->height,0);
 HRESULT firstMotionCreateHr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&firstMotionResource));
 ComPtr<ID3D11Resource>wrappedFirstMotion;HRESULT firstMotionWrapHr=FAILED(firstMotionCreateHr)?firstMotionCreateHr:g_on12->CreateWrappedResource(firstMotionResource.Get(),&wrappedFlags,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&wrappedFirstMotion));
 if(SUCCEEDED(firstMotionWrapHr)&&wrappedFirstMotion){ID3D11Resource*r[]{wrappedFirstMotion.Get()};g_on12->AcquireWrappedResources(r,1);g_on12Context->UpdateSubresource(wrappedFirstMotion.Get(),0,nullptr,zeroMotion.data(),w->motionPitch,0);g_on12->ReleaseWrappedResources(r,1);g_on12Context->Flush();}
 HRESULT firstDepthCreateHr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&depthDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&firstDepthResource));
 ComPtr<ID3D11Resource>wrappedFirstDepth;HRESULT firstDepthWrapHr=FAILED(firstDepthCreateHr)?firstDepthCreateHr:g_on12->CreateWrappedResource(firstDepthResource.Get(),&depthFlags,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&wrappedFirstDepth));
 if(SUCCEEDED(firstDepthWrapHr)&&wrappedFirstDepth){ID3D11Resource*r[]{wrappedFirstDepth.Get()};g_on12->AcquireWrappedResources(r,1);g_on12Context->UpdateSubresource(wrappedFirstDepth.Get(),0,nullptr,w->firstDepthBytes.data(),w->firstDepthPitch,0);g_on12->ReleaseWrappedResources(r,1);g_on12Context->Flush();}
 out<<"B172: stage=CreateFirstTemporalInputs, zero_motion_created="<<(firstMotionResource?1:0)<<", first_depth_created="<<(firstDepthResource?1:0)<<", zero_motion_hr="<<(long)firstMotionCreateHr<<", first_depth_hr="<<(long)firstDepthCreateHr<<"\n";out.flush();
 auto disabled=xefgSwapChainSetEnabled(g_xefgContext,0);xefg_swapchain_d3d12_resource_data_t motion{};motion.type=XEFG_SWAPCHAIN_RES_MOTION_VECTOR;motion.validity=XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;motion.resourceBase={0,0};motion.resourceSize={w->width,w->height};motion.pResource=firstMotionResource.Get();motion.incomingState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
 auto motionResult=SUCCEEDED(firstMotionWrapHr)?xefgSwapChainD3D12TagFrameResource(g_xefgContext,nullptr,2,&motion):(xefg_swapchain_result_t)-1;
 xefg_swapchain_d3d12_resource_data_t depth{};depth.type=XEFG_SWAPCHAIN_RES_DEPTH;depth.validity=XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;depth.resourceBase={0,0};depth.resourceSize={w->width,w->height};depth.pResource=firstDepthResource.Get();depth.incomingState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
 auto depthResult=SUCCEEDED(firstDepthWrapHr)?xefgSwapChainD3D12TagFrameResource(g_xefgContext,nullptr,2,&depth):(xefg_swapchain_result_t)-1;
 xefg_swapchain_d3d12_resource_data_t color{};color.type=XEFG_SWAPCHAIN_RES_HUDLESS_COLOR;color.validity=XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;color.resourceBase={0,0};auto cd=g_b147ColorResource?g_b147ColorResource->GetDesc():D3D12_RESOURCE_DESC{};color.resourceSize={(uint32_t)cd.Width,cd.Height};color.pResource=g_b147ColorResource.Get();color.incomingState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
 auto colorResult=XEFG_SWAPCHAIN_RESULT_SUCCESS;
 xefg_swapchain_frame_constant_data_t constants{};std::memcpy(constants.viewMatrix,w->firstView,sizeof(constants.viewMatrix));std::memcpy(constants.projectionMatrix,w->firstProjection,sizeof(constants.projectionMatrix));
 constants.jitterOffsetX=0.0f;constants.jitterOffsetY=0.0f;constants.motionVectorScaleX=1.0f;constants.motionVectorScaleY=1.0f;constants.resetHistory=1;constants.frameRenderTime=16.6667f;
 auto constantsResult=xefgSwapChainTagFrameConstants(g_xefgContext,2,&constants);
 out<<"B172: stage=XeFGTagFrameConstants, present_id=2, row_major=1, jitter={0,0}, color_size="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<", motion_size="<<w->width<<"x"<<w->height<<", motion_scale={"<<constants.motionVectorScaleX<<","<<constants.motionVectorScaleY<<"}, reset_history=1, frame_render_time_ms=16.6667, result="<<(int)constantsResult<<"\n";
 out<<"B172: stage=XeFGTagMotion, present_id=2, type=MOTION_VECTOR, pixel_space=1, current_to_previous=1, low_resolution=1, size="<<w->width<<"x"<<w->height<<", result="<<(int)motionResult<<"\n";
 out<<"B172: stage=XeFGTagDepth, present_id=2, type=DEPTH, size="<<w->width<<"x"<<w->height<<", format="<<(unsigned)depthDesc.Format<<", result="<<(int)depthResult<<"\n";
 out<<"B172: stage=XeFGRetagColor, present_id=2, type=HUDLESS_COLOR, size="<<(uint32_t)cd.Width<<"x"<<cd.Height<<", result="<<(int)colorResult<<"\n";
 bool inputsOk=SUCCEEDED(createHr)&&SUCCEEDED(wrapHr)&&SUCCEEDED(depthCreateHr)&&SUCCEEDED(depthWrapHr)&&disabled==XEFG_SWAPCHAIN_RESULT_SUCCESS&&motionResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&depthResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&colorResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&constantsResult==XEFG_SWAPCHAIN_RESULT_SUCCESS;
 bool hiddenPositionRequested=false,proxyRectMatch=false;
 out<<"B174c: PROXY_PREWARM_STATE window_created="<<(g_xefgWindow?1:0)<<", swapchain_created="<<(g_xefgSwapChain?1:0)<<", visible=0, warmup_presents_hidden=1, reveal_deferred_until_first_live_completion=1\n";out.flush();
 HRESULT prewarmFenceHr=b173n_prepare_stage_fence(out);
 HRESULT firstBackBufferCopyHr=S_OK;bool firstBackBufferReady=true;
 auto enableResult=(inputsOk&&firstBackBufferReady)?xefgSwapChainSetEnabled(g_xefgContext,1):(xefg_swapchain_result_t)-1;
 out<<"B172: stage=XeFGSetEnabledForOnePresent, enabled=1, result="<<(int)enableResult<<"\n";out.flush();
 bool warmupOk=inputsOk&&SUCCEEDED(prewarmFenceHr)&&firstBackBufferReady&&enableResult==XEFG_SWAPCHAIN_RESULT_SUCCESS;
 out<<"B174h: PRELIVE_PRIVATE_PRESENT_SKIPPED phase=warmup, present_id=2, resources_prepared="<<(warmupOk?1:0)<<", swapchain_enabled="<<(enableResult==XEFG_SWAPCHAIN_RESULT_SUCCESS?1:0)<<", private_present_called=0, gpu_wait_ms=0, proxy_visible=0\n";out.flush();
 constants.resetHistory=0;std::memcpy(constants.viewMatrix,w->secondView,sizeof(constants.viewMatrix));std::memcpy(constants.projectionMatrix,w->secondProjection,sizeof(constants.projectionMatrix));
 xefg_swapchain_d3d12_resource_data_t motionSecond=motion;motionSecond.pResource=g_b160MotionResource.Get();xefg_swapchain_d3d12_resource_data_t depthSecond=depth;depthSecond.pResource=g_b163DepthResource.Get();
 auto constants3=warmupOk?xefgSwapChainTagFrameConstants(g_xefgContext,3,&constants):(xefg_swapchain_result_t)-1;
 auto motion3=warmupOk?xefgSwapChainD3D12TagFrameResource(g_xefgContext,nullptr,3,&motionSecond):(xefg_swapchain_result_t)-1;
 auto depth3=warmupOk?xefgSwapChainD3D12TagFrameResource(g_xefgContext,nullptr,3,&depthSecond):(xefg_swapchain_result_t)-1;
 xefg_swapchain_d3d12_resource_data_t colorSecond=color;colorSecond.pResource=g_b171SecondColorResource.Get();
 bool distinctColorReady=g_b171SecondColorReady.load(std::memory_order_acquire)&&g_b171SecondColorResource&&g_b171FirstColorHash!=g_b171SecondColorHash;
 auto color3=distinctColorReady?XEFG_SWAPCHAIN_RESULT_SUCCESS:(xefg_swapchain_result_t)-1;
 out<<"B172: stage=RetagSecondCompleteFrame, present_id=3, reset_history=0, distinct_gta_color="<<(distinctColorReady?1:0)<<", first_color_hash="<<g_b171FirstColorHash<<", second_color_hash="<<g_b171SecondColorHash<<", constants="<<(int)constants3<<", motion="<<(int)motion3<<", depth="<<(int)depth3<<", color="<<(int)color3<<"\n";out.flush();
 bool secondInputsOk=constants3==XEFG_SWAPCHAIN_RESULT_SUCCESS&&motion3==XEFG_SWAPCHAIN_RESULT_SUCCESS&&depth3==XEFG_SWAPCHAIN_RESULT_SUCCESS&&color3==XEFG_SWAPCHAIN_RESULT_SUCCESS;
 HRESULT secondBackBufferCopyHr=S_OK;bool secondBackBufferReady=secondInputsOk;
 const bool hiddenPrewarmComplete=warmupOk&&secondInputsOk&&secondBackBufferReady;
 out<<"B174h: PRELIVE_PRIVATE_PRESENT_SKIPPED phase=x4, present_id=3, resources_prepared="<<(hiddenPrewarmComplete?1:0)<<", private_present_called=0, gpu_wait_ms=0, proxy_visible=0, live_reset_will_start_history=1\n";out.flush();
 if(hiddenPrewarmComplete){
  HRESULT depthMotionPoolHr=b173j_prepare_depth_motion_pool(w->width,w->height,out);
  out<<"B173m: NATIVE_DEPTH_POOL_PREWARM_RESULT fallback_retained=1, hr="<<(long)depthMotionPoolHr<<"\n";out.flush();
 HRESULT workerOn12Hr=SUCCEEDED(depthMotionPoolHr)?b174e_prepare_worker_on12(out):depthMotionPoolHr;
  HRESULT frameRingHr=SUCCEEDED(workerOn12Hr)?b174o_prepare_frame_resource_ring(w->width,w->height,out):workerOn12Hr;
 RECT currentClient{};POINT currentOrigin{};const bool currentClientReady=g_b174GtaWindow&&GetClientRect(g_b174GtaWindow,&currentClient)&&ClientToScreen(g_b174GtaWindow,&currentOrigin)&&currentClient.right>currentClient.left&&currentClient.bottom>currentClient.top;
 if(currentClientReady){OffsetRect(&currentClient,currentOrigin.x,currentOrigin.y);AcquireSRWLockExclusive(&g_b174WindowLock);g_b174GtaClientRect=currentClient;ReleaseSRWLockExclusive(&g_b174WindowLock);}
 out<<"B174c: GTA_CLIENT_RECT_REFRESH ready="<<(currentClientReady?1:0)<<", rect={"<<currentClient.left<<","<<currentClient.top<<","<<(currentClient.right-currentClient.left)<<","<<(currentClient.bottom-currentClient.top)<<"}\n";out.flush();
 const bool prewarmReady=SUCCEEDED(depthMotionPoolHr)&&SUCCEEDED(workerOn12Hr)&&SUCCEEDED(frameRingHr)&&g_b173iColorPoolReady.load(std::memory_order_acquire)&&g_b173nStageFence&&currentClientReady;
 if(prewarmReady&&g_xefgWindow){
  g_b171WindowVisible.store(false,std::memory_order_release);
  // B230: the proxy belongs to GTA's window thread. Marshal the hidden positioning
  // synchronously so readiness cannot be evaluated while it is still the 64x64 creation size.
  DWORD_PTR visibilityResult=0;
  const LRESULT sent=SendMessageTimeoutA(g_xefgWindow,kB171VisibilityMessage,2,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&visibilityResult);
  hiddenPositionRequested=sent!=0;
 }
 RECT positionedRect{};if(g_xefgWindow)GetWindowRect(g_xefgWindow,&positionedRect);const RECT targetProxyRect=b174m_proxy_rect();proxyRectMatch=EqualRect(&positionedRect,&targetProxyRect)!=FALSE;
 out<<"B175r: REPLACEMENT_SURFACE_POSITIONED_HIDDEN requested="<<(hiddenPositionRequested?1:0)<<", visible="<<(g_b171WindowVisible.load(std::memory_order_acquire)?1:0)<<", gta_hwnd="<<(g_b174GtaWindow?1:0)<<", gta_client_rect={"<<g_b174GtaClientRect.left<<","<<g_b174GtaClientRect.top<<","<<(g_b174GtaClientRect.right-g_b174GtaClientRect.left)<<","<<(g_b174GtaClientRect.bottom-g_b174GtaClientRect.top)<<"}, replacement_rect={"<<positionedRect.left<<","<<positionedRect.top<<","<<(positionedRect.right-positionedRect.left)<<","<<(positionedRect.bottom-positionedRect.top)<<"}, rect_match="<<(proxyRectMatch?1:0)<<", inset_pixels=0, geometry_target=B316_full_monitor_host, reveal_deferred=1, borderless=1, topmost=1, no_activate=1\n";out.flush();
 const bool pipelineReady=prewarmReady&&hiddenPositionRequested&&!g_b171WindowVisible.load(std::memory_order_acquire)&&proxyRectMatch;
 out<<"B231: PREWARM_GATE depth_motion="<<(SUCCEEDED(depthMotionPoolHr)?1:0)<<", worker_on12="<<(SUCCEEDED(workerOn12Hr)?1:0)<<", frame_ring="<<(SUCCEEDED(frameRingHr)?1:0)<<", color_pool="<<(g_b173iColorPoolReady.load(std::memory_order_acquire)?1:0)<<", stage_fence="<<(g_b173nStageFence?1:0)<<", client_ready="<<(currentClientReady?1:0)<<", hidden_position_sync="<<(hiddenPositionRequested?1:0)<<", proxy_hidden="<<(!g_b171WindowVisible.load(std::memory_order_acquire)?1:0)<<", proxy_rect_match="<<(proxyRectMatch?1:0)<<"\n";out.flush();
 g_b175PipelineFailed.store(!pipelineReady,std::memory_order_release);g_b175PipelineReady.store(pipelineReady,std::memory_order_release);
 out<<"B175: ASYNC_PIPELINE_PREWARM_COMPLETE ready="<<(pipelineReady?1:0)<<", failed="<<(pipelineReady?0:1)<<", d3d12_bridge=1, d3d11on12_bridge=1, xefg=1, xell=1, shaders=1, proxy_configured_output=1, command_objects=1, color_slots=3, depth_motion_slots=3, frame_ring_slots=3, activation_pending=1\n";out.flush();
 while(pipelineReady&&!g_b174ShutdownRequested.load(std::memory_order_acquire)){
 while(pipelineReady&&!g_b174ShutdownRequested.load(std::memory_order_acquire)&&!g_b175ActivationRequested.exchange(false,std::memory_order_acq_rel)){b174_retire_completed_frames(out);Sleep(10);}
 if(g_b174ShutdownRequested.load(std::memory_order_acquire))break;
 const auto sessionEnable=xefgSwapChainSetEnabled(g_xefgContext,1);
 if(sessionEnable!=XEFG_SWAPCHAIN_RESULT_SUCCESS){out<<"B174y: FAILURE_FALLBACK reason=xefg_enable_failed, result="<<(int)sessionEnable<<", normal_gta_presentation_visible=1\n";out.flush();b174_request_stop(4);break;}
 if(hiddenPositionRequested&&!g_b171WindowVisible.load(std::memory_order_acquire)&&proxyRectMatch){
  g_b171LiveConstants=constants;g_b171LiveConstants.resetHistory=0;g_b171LiveWidth=w->width;g_b171LiveHeight=w->height;
  b174_retire_completed_frames(out);g_b171LiveSubmitted.store(0,std::memory_order_release);g_b174Accepted.store(0,std::memory_order_release);g_b174PendingCount.store(0,std::memory_order_release);g_b174Dropped.store(0,std::memory_order_release);g_b174Replaced.store(0,std::memory_order_release);g_b174OutputFrames.store(0,std::memory_order_release);g_b174GeneratedFrames.store(0,std::memory_order_release);g_b174oObservedSourcePresents.store(0,std::memory_order_release);g_b174oFirstSourcePresent.store(0,std::memory_order_release);g_b174oLastSourcePresent.store(0,std::memory_order_release);g_b174LastAcceptedSource.store(0,std::memory_order_release);g_b174LastPresentedSource.store(0,std::memory_order_release);g_b174LastCaptureStart.store(0,std::memory_order_release);g_b270LastSuccessfulOutputTick.store(0,std::memory_order_release);g_b403ConfirmedStallLatched.store(false,std::memory_order_release);g_b174StopReason.store(0,std::memory_order_release);g_b174VisibilityLost.store(false,std::memory_order_release);g_b174ProxyRevealed.store(false,std::memory_order_release);g_b174SessionEverRevealed.store(false,std::memory_order_release);g_b174StartupBackbuffersSeeded.store(false,std::memory_order_release);g_b174PreviousPrivatePresentStart={};
  g_b174oPendingHead=0;g_b174oPendingTail=0;g_b174oPendingSize=0;for(auto&pending:g_b174oPendingJobs)pending=B173hLiveJob{};g_b173hJobPending.store(false,std::memory_order_release);g_b173hJobProcessing.store(false,std::memory_order_release);
  AcquireSRWLockExclusive(&g_b175DecisionLock);std::memset(g_b175DecisionIds,0,sizeof(g_b175DecisionIds));g_b175DecisionCount=0;g_b175DecisionCursor=0;ReleaseSRWLockExclusive(&g_b175DecisionLock);g_b175InvariantFailures.store(0,std::memory_order_release);g_b175HaveConsumedViewProjection=false;g_b172PreviousTime={};g_b245PreviousObservedPresentQpc.store(0,std::memory_order_release);g_b245ObservedFrameIntervalUs.store(16667,std::memory_order_release);
  g_b174TestStart=GetTickCount64();g_b175rRevealTick=0;g_b316PersistentMonitorCover.store(true,std::memory_order_release);g_b174LastWindowPoll.store(g_b174TestStart,std::memory_order_release);g_b174StartupDeadline.store(g_b174TestStart+5000,std::memory_order_release);b174_rebaseline_rate_window(g_b174TestStart);g_b174HiddenCompleted.store(0,std::memory_order_release);g_b352InitialValidX4Seen.store(false,std::memory_order_release);g_b174RevealFenceMarker.store(0,std::memory_order_release);g_b174ResumeNeedsFresh.store(true,std::memory_order_release);g_b174TestActive.store(true,std::memory_order_release);g_b171LiveReady.store(true,std::memory_order_release);g_b174ToggleOnCount.fetch_add(1,std::memory_order_acq_rel);
  out<<"B175r: TOGGLE_ON continuous_live_replacement=1, startup_timeout_ms=5000, retained_pipeline=1, discovery_repeated=0, replacement_visible=0, reveal_policy=B352_initial_one_valid_x4_latched_plus_1500ms_quiet_then_existing_recovery_gate, present_id_policy=process_monotonic, color_size="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<", motion_size="<<w->width<<"x"<<w->height<<", motion_scale={"<<constants.motionVectorScaleX<<","<<constants.motionVectorScaleY<<"}, capture_policy=ordered_frame_queue, xell_sleep_per_present=1, xell_required_markers=6, frame_resource_slots=8, gta_backbuffer_read_only=1, cpu_wait=0, xess_upscaling=1\n";out.flush();
  out<<"B242: THROUGHPUT_FIX frame_resource_slots=8, pending_queue_capacity=7, per_frame_disk_logging=throttled, accepted_frame_processing=ordered, xefg_interpolations=3, target_effective_multiplier=4, presentation_architecture_changed=0\n";out.flush();
  out<<"B246: PIPELINE_FAST_PATH retirement_logging=sampled, retirement_flush_per_frame=0, source_cadence_logging=sampled, queue_capacity_unchanged=7, resource_slots_unchanged=8, serial_xefg_present_preserved=1, frozen_proxy_architecture=1\n";out.flush();
 out<<"B302: RUNTIME_TIMER_POLICY hud_runtime=1, format=HH:MM:SS, timer_origin=F9_activation, runtime_log=GTAXeSS-Runtime.log, periodic_log_seconds=60, b301_fast_bypass_unchanged=1, b276_x4_unchanged=1\n";out<<"B304: PERFORMANCE_HUD_POLICY cpu_process_normalized=1, gpu_process_max_engine=1, vram_local_process_usage=1, sample_interval_ms=1000, performance_log=GTAXeSS-Performance.log, direct_camera_reacquire_disabled=1, xell_contract_sleep_preserved=1\nB305: PERFORMANCE_TELEMETRY_FIX vram_adapter_reacquire_by_luid=1, render_thread_cpu=1, instantaneous_fps_log=1, hud_render_thread_cpu=1\n";out.flush();
 out<<"B300: GHOSTING_PATH motion_source=direct_camera_cb_reconstructed_depth_plus_native_motion_validation, native_motion_candidate_probe=1, native_gta_motion_vectors_runtime=0, motion_resolution="<<g_b171LiveWidth<<"x"<<g_b171LiveHeight<<", output_resolution="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<", motion_scale={"<<((float)g_b232OutputWidth/g_b171LiveWidth)<<","<<((float)g_b232OutputHeight/g_b171LiveHeight)<<"}, jitter_offsets=forced_zero, pixel_space_mv_scale={1,1}, prior_b235_scale_was_output_over_motion, presentation_architecture_changed=0\n";out.flush();
  out<<"B175n: WINDOW_STATE_POLL_ACTIVE identity=B175n PERSISTENT XESS-XEFG 2026-09-20-CHECK-170, poll_origin=worker, interval_ms=10, present_final_gate=1, win32_queries_only=1\n";const BOOL b267PriorityHeld=SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);out<<"B267: WORKER_PRIORITY_REGRESSION_FIX startup_priority=THREAD_PRIORITY_HIGHEST, activation_priority=THREAD_PRIORITY_HIGHEST, priority_set="<<(b267PriorityHeld?1:0)<<", prior_activation_priority=THREAD_PRIORITY_ABOVE_NORMAL, xell_low_latency=1, serial_xefg_present=1, interpolation_count=3, queue_capacity="<<kB174oPendingCapacity<<", resource_slots="<<kB173jDepthMotionSlotCount<<", goal=remove_activation_time_worker_priority_downgrade_without_changing_xell_or_xefg_semantics\n";out<<"CURRENT_BUILD=B352; CORE_RENDER_FG_LOCKED=1; B276_AUTO_240_LOCK_REDISCOVERY_FROZEN xell_sleep_every_frame_id=1, same_xell_xefg_present_id=1, simulation_phase_nonzero=1, simulation_render_submit_overlap=1, pacing_lock_frames=90, rediscovery_cooldown_ms=30000, existing_b216_b252_path=1, xefg_toggle=0, unsafe_sleep_skip=0, b306_temporary_backlog_no_wait_mode=1, present_sync_interval=0, allow_tearing=1, f9_latch_preserved=1, queue_capacity="<<kB174oPendingCapacity<<", resource_slots="<<kB173jDepthMotionSlotCount<<", selected_interpolations=3\n";out<<"B297: TRANSIENT_DISCOVERY_RETRY_POLICY initial_live_discovery_retryable=1, retry_delay_ms=250, max_attempts=6, clean_cross_frame_pair_required=1, b296_camera_rescue_unchanged=1, b276_xefg_x4_frozen=1\n";out<<"B300: CAMERA_AND_PERF_POLICY direct_camera_cb_tracking=1, draw_hooks_release_after_3_stable_frames=1, map_unmap_copy_only_direct_mode=1, direct_view_max_delta=500, direct_projection_max_delta=2, direct_vp_max_delta=500, final_vp_submit_limit=1500, native_motion_validation=1, native_motion_runtime_switch=1, safe_fallback=b299_reconstructed_motion, xefg_interpolations=3, xefg_multiplier=4, pacing_semantics_unchanged=1\n";out<<"B301: MAP_UNMAP_FAST_BYPASS_POLICY selected_camera_resource_pointer_gate=1, non_selected_map_direct_forward=1, non_selected_unmap_direct_forward=1, generic_known_cb_lookup_after_lock=0, legacy_constant_write_observation_after_lock=0, selected_camera_copy_bytes=256, b300_camera_selection_unchanged=1, native_motion_runtime_switch=1, xefg_interpolations=3, xefg_multiplier=4, b276_frozen=1\n";out<<"B296: CAMERA_RUNTIME_WIRING camera_source=B291_direct_invariant, camera_selection=depth_owning_family_cluster_plus_cross_family_continuity_rescue_plausibility_gated, reconstructed_motion_runtime_enabled=1, cross_family_rescue_max_delta=500, persistent_cut_frames=8, persistent_cut_max_delta=1500, native_gta_motion_vectors=1, depth_path_unchanged=1, jitter_unchanged=1, motion_scale_unchanged=1, pacing_unchanged=1, xefg_x4_unchanged=1, b276_frozen=1\n";
  if(g_xefgWindow){HMONITOR mon=MonitorFromWindow(g_xefgWindow,MONITOR_DEFAULTTONEAREST);MONITORINFOEXA mi{};mi.cbSize=sizeof(mi);DEVMODEA dm{};dm.dmSize=sizeof(dm);if(GetMonitorInfoA(mon,&mi)&&EnumDisplaySettingsA(mi.szDevice,ENUM_CURRENT_SETTINGS,&dm)){out<<"DISPLAY_TIMING: device="<<mi.szDevice<<", width="<<dm.dmPelsWidth<<", height="<<dm.dmPelsHeight<<", refresh_hz="<<dm.dmDisplayFrequency<<", present_sync_interval=0, allow_tearing=1\n";}}out<<"B256: SAME_QUEUE_PREFILL_WAIT_REMOVAL cpu_wait_removed=1, sampled_120=1, worker_priority=HIGHEST_PRESERVED_BY_B267, worker_priority_set="<<(b267PriorityHeld?1:0)<<", serial_xefg_present=1, interpolation_count=3, queue_capacity="<<kB174oPendingCapacity<<", runtime_semantics_unchanged=1\n";out.flush();
 }
 while(g_b171LiveReady.load(std::memory_order_acquire)&&!g_b174ShutdownRequested.load(std::memory_order_acquire)){
  static bool b316HostVerified=false;
  if(!b316HostVerified&&g_b174SessionEverRevealed.load(std::memory_order_acquire)&&g_xefgWindow&&!g_b174SurfaceSuspended.load(std::memory_order_acquire)){
   RECT gtaRect{};AcquireSRWLockShared(&g_b174WindowLock);gtaRect=g_b174GtaClientRect;ReleaseSRWLockShared(&g_b174WindowLock);
   RECT monitorRect{};const bool monitorOk=b315_monitor_rect(monitorRect);RECT proxyRect{};GetWindowRect(g_xefgWindow,&proxyRect);
   const bool monitorMatch=monitorOk&&EqualRect(&proxyRect,&monitorRect)!=FALSE;
   out<<"B316: PERSISTENT_MONITOR_COVER_ACTIVE gta_client_rect={"<<gtaRect.left<<","<<gtaRect.top<<","<<(gtaRect.right-gtaRect.left)<<","<<(gtaRect.bottom-gtaRect.top)<<"}, monitor_rect={"<<monitorRect.left<<","<<monitorRect.top<<","<<(monitorRect.right-monitorRect.left)<<","<<(monitorRect.bottom-monitorRect.top)<<"}, proxy_rect={"<<proxyRect.left<<","<<proxyRect.top<<","<<(proxyRect.right-proxyRect.left)<<","<<(proxyRect.bottom-proxyRect.top)<<"}, monitor_cover_match="<<(monitorMatch?1:0)<<", private_swapchain_size=1920x1080, content_layout=monitor_stretch_retained_for_single_change, xefg_x4_unchanged=1\n";out.flush();
   b316HostVerified=true;
  }
  if(g_b174SurfaceSuspended.load(std::memory_order_acquire)){b174_discard_suspended_pending(out);b174_retire_completed_frames(out);Sleep(1);continue;}
  if(!g_b175pHiddenOnly&&!g_b174SurfaceSuspended.load(std::memory_order_acquire)&&!g_b174ProxyRevealed.load(std::memory_order_acquire)&&GetTickCount64()>=g_b174StartupDeadline.load(std::memory_order_acquire)){const ULONGLONG retryNow=GetTickCount64();g_b174StartupDeadline.store(retryNow+1500,std::memory_order_release);g_b174ResumeNeedsFresh.store(true,std::memory_order_release);out<<"B206: RECOVERY_WAIT reason=no_verified_fresh_live_output persistent_session=1 toggle_off=0 proxy_remained_hidden=1 retry_deadline_ms=1500 elapsed_from_activation_ms="<<(g_b174ActivationStart?retryNow-g_b174ActivationStart:0)<<"\n";out.flush();}
  // B209: the XeFG proxy must never cover GTA with a stale gameplay frame.  GTA menus
  // (notably ESC/pause) can stop the internal live-color capture while the source window
  // remains focused.  Hide the replacement surface promptly so GTA's own UI/input is
  // visible; the existing fresh-x4 reveal gate will restore it automatically afterwards.
  if(g_b174ProxyRevealed.load(std::memory_order_acquire)){
   const ULONGLONG lastCapture=g_b174LastCaptureStart.load(std::memory_order_acquire);
   const ULONGLONG watchdogNow=GetTickCount64();
   const ULONGLONG lastOutput=g_b270LastSuccessfulOutputTick.load(std::memory_order_acquire);
   const ULONGLONG captureAge=(lastCapture&&watchdogNow>lastCapture)?watchdogNow-lastCapture:0;
   const ULONGLONG outputAge=(lastOutput&&watchdogNow>lastOutput)?watchdogNow-lastOutput:0;
   const unsigned pending=g_b174PendingCount.load(std::memory_order_acquire);
   // B270: capture age alone is not proof of a dead F9 path. Queue pressure can suppress new capture
   // while already accepted frames are still producing valid 4/3 output. Keep the proxy visible while
   // output is advancing or queued work exists. Only hide after both capture and generated output have
   // genuinely stopped and the queue has drained; the persistent F9/rediscovery path remains armed.
   // B405: 500 ms was too aggressive for ordinary GTA-side cadence interruptions.
   // Preserve source binding, XeSS/XeFG contexts and temporal history through short gaps.
   // Recovery is permitted only after both capture and successful output have been continuously
   // absent for 1500 ms with no queued work. Genuine device/swapchain replacement handling is unchanged.
   const bool b403ConfirmedStall=lastCapture&&captureAge>1500&&lastOutput&&outputAge>1500&&pending==0;
   if(b403ConfirmedStall){
    // B403: recover on the transition into a confirmed stall, not on every ~1 ms worker pass.
    // B402 owns stable source binding, so repeatedly consuming/rearming B388 only creates a
    // recovery/reset/log storm. One edge gets one history reset + one recovery request.
    if(!g_b403ConfirmedStallLatched.exchange(true,std::memory_order_acq_rel)){
     b174_hide_proxy("confirmed_live_pipeline_stalled");
     g_b174ResumeNeedsFresh.store(true,std::memory_order_release);
     g_b175bXessReset.store(true,std::memory_order_release);
     const bool b388NewRequest=!g_b388StallRecoveryRequested.exchange(true,std::memory_order_acq_rel);
     out<<"B405: SUSTAINED_STALL_EDGE action=one_shot_recovery, capture_stale_ms="<<captureAge<<", output_stale_ms="<<outputAge<<", pending="<<pending<<", xess_reset_once=1, b388_request_once="<<(b388NewRequest?1:0)<<", fullscreen_visibility_unchanged=1, stable_source_binding=1\n";
     out<<"B270: PERSISTENT_F9_WATCHDOG capture_timer_only_hide=0, require_output_stalled=1, require_queue_drained=1, persistent_f9=1, performance_policy=FROZEN\n";out.flush();
    }
   }else if(g_b403ConfirmedStallLatched.exchange(false,std::memory_order_acq_rel)){
    out<<"B405: SUSTAINED_STALL_CLEARED action=rearm_edge_detector, capture_age_ms="<<captureAge<<", output_age_ms="<<outputAge<<", pending="<<pending<<"\n";out.flush();
   }
  }
  B173hLiveJob job{};
  if(b173h_take_live_job(job)){HRESULT liveHr=b173h_present_live_job(job,out);g_b173hJobProcessing.store(false,std::memory_order_release);if(FAILED(liveHr)){if(job.slotIndex<kB173iColorSlotCount){AcquireSRWLockExclusive(&g_b173hJobLock);B174ReplacedPending retired{};retired.nativeFenceValue=job.nativeFenceValue;retired.slotIndex=job.slotIndex;retired.sourceFrame=job.sourceFrame;g_b174ReplacedPending.push_back(retired);g_b174ColorSlotState[job.slotIndex].store(4,std::memory_order_release);g_b174DepthSlotState[job.slotIndex].store(4,std::memory_order_release);ReleaseSRWLockExclusive(&g_b173hJobLock);}break;}}
  else{b174_retire_completed_frames(out);Sleep(1);}
  const ULONGLONG now=GetTickCount64(),lastTelemetry=g_b174LastTelemetry.load(std::memory_order_acquire);if(g_b174ActivationStart&&now>=g_b174ActivationStart){const ULONGLONG runtimeMs=now-g_b174ActivationStart;const unsigned long long runtimeMinute=runtimeMs/60000ULL;const auto lastRuntimeMinute=g_b302LastRuntimeLoggedMinute.load(std::memory_order_acquire);if(runtimeMinute>=1&&runtimeMinute>lastRuntimeMinute){g_b302LastRuntimeLoggedMinute.store(runtimeMinute,std::memory_order_release);b302_runtime_log_event("XeSS/XeFG running",runtimeMs);}}if(now>=lastTelemetry&&now-lastTelemetry>=1000){b304_sample_performance();const auto rateWindow=b174_rate_window_snapshot();const ULONGLONG elapsed=rateWindow.start&&now>=rateWindow.start?now-rateWindow.start:0;const double seconds=elapsed?elapsed/1000.0:0.0;const unsigned observed=g_b174oObservedSourcePresents.load(std::memory_order_acquire),output=g_b174OutputFrames.load(std::memory_order_acquire);const unsigned windowObserved=observed>=rateWindow.observedBase?observed-rateWindow.observedBase:0,windowOutput=output>=rateWindow.outputBase?output-rateWindow.outputBase:0;RECT proxy{},target=b174m_proxy_rect();if(g_xefgWindow)GetWindowRect(g_xefgWindow,&proxy);const bool aligned=EqualRect(&proxy,&target)!=FALSE;const double sourceFps=seconds?windowObserved/seconds:0.0,fgFps=seconds?windowOutput/seconds:0.0;const ULONGLONG b304RuntimeMs=(g_b174ActivationStart&&now>=g_b174ActivationStart)?now-g_b174ActivationStart:elapsed;b304_log_performance(sourceFps,fgFps,g_b191SourceFps100.load(std::memory_order_acquire)/100.0,g_b191FgFps100.load(std::memory_order_acquire)/100.0,b304RuntimeMs);out<<"B214: FPS_DISPLAY rolling_window_ms=250, valid="<<(g_b207FpsValid.load()?1:0)<<", display_source_fps="<<(g_b191SourceFps100.load()/100.0)<<", display_xefg_fps="<<(g_b191FgFps100.load()/100.0)<<", lifetime_source_fps="<<sourceFps<<", lifetime_xefg_fps="<<fgFps<<"\n";out<<"B177: FPS_OVERLAY source_fps="<<sourceFps<<", xefg_presented_fps="<<fgFps<<", actual_multiplier="<<(sourceFps>0.01?fgFps/sourceFps:0.0)<<", measured_not_multiplied=1\n";out<<"B175n: TELEMETRY elapsed_ms="<<elapsed<<", source_fps="<<(seconds?windowObserved/seconds:0.0)<<", successful_xess_frames="<<g_b175bXessFrames.load()<<", displayed_effective_fps="<<(seconds?windowOutput/seconds:0.0)<<", accepted="<<g_b174Accepted.load()<<", generated="<<g_b174GeneratedFrames.load()<<", displayed="<<output<<", dropped="<<g_b174Dropped.load()<<", replaced="<<g_b174Replaced.load()<<", xess_errors="<<g_b175bXessErrors.load()<<", xefg_errors="<<(g_b174PresentErrors.load()+g_b174TagErrors.load()+g_b174StatusErrors.load())<<", device_errors="<<g_b175bDeviceErrors.load()<<", mailbox_depth="<<g_b174PendingCount.load()<<", visible="<<(g_b171WindowVisible.load()?1:0)<<", focused="<<(GetForegroundWindow()==g_b174GtaWindow?1:0)<<", foreground_hwnd="<<(uintptr_t)GetForegroundWindow()<<", gta_hwnd="<<(uintptr_t)g_b174GtaWindow<<", proxy_hwnd="<<(uintptr_t)g_xefgWindow<<", fps_overlay_hwnd="<<(uintptr_t)g_b187FpsOverlay<<", aligned="<<(aligned?1:0)<<"\n";
const unsigned b264Xess=g_b175bXessFrames.load(std::memory_order_relaxed),b264Accepted=g_b174Accepted.load(std::memory_order_relaxed),b264Pending=g_b174PendingCount.load(std::memory_order_relaxed);const long long b264Residual=(long long)b264Xess-(long long)b264Accepted-(long long)b264Pending;const unsigned b264Classified=g_b264DropResourceOwned.load()+g_b264DropQueueFull.load()+g_b264DropSurfaceSuspended.load()+g_b264DropFastSnapshot.load()+g_b264DropCaptureFailed.load()+g_b264DropSlotRace.load()+g_b264DropFastOther.load()+g_b264DropNonMonotonic.load()+g_b264DropWorkerSuspend.load()+g_b264DropToggleDrain.load()+g_b264DropOther.load();out<<"B264: RESIDUAL_FRAME_ACCOUNTING observed="<<observed<<", successful_xess="<<b264Xess<<", accepted="<<b264Accepted<<", pending="<<b264Pending<<", xess_minus_accepted_pending="<<b264Residual<<", dropped_total="<<g_b174Dropped.load()<<", classified_drops="<<b264Classified<<", resource_owned="<<g_b264DropResourceOwned.load()<<", queue_full="<<g_b264DropQueueFull.load()<<", surface_suspended="<<g_b264DropSurfaceSuspended.load()<<", fast_snapshot="<<g_b264DropFastSnapshot.load()<<", capture_failed="<<g_b264DropCaptureFailed.load()<<", slot_race="<<g_b264DropSlotRace.load()<<", fast_other="<<g_b264DropFastOther.load()<<", non_monotonic="<<g_b264DropNonMonotonic.load()<<", worker_suspend="<<g_b264DropWorkerSuspend.load()<<", toggle_drain="<<g_b264DropToggleDrain.load()<<", other="<<g_b264DropOther.load()<<", accepted_disposition="<<g_b264AcceptedDisposition.load()<<", diagnostic_only=1, runtime_policy_changed=0\n";out.flush();out<<"B266: QUEUE_FULL_CORRELATION_SUMMARY queue_full_bursts="<<g_b266QueueFullBursts.load()<<", queue_full_drops="<<g_b266QueueFullDrops.load()<<", burst_active="<<(g_b266QueueFullBurstActive.load()?1:0)<<", last_xell_sleep_ms="<<(g_b266LastXellSleepUs.load()/1000.0)<<", last_private_present_ms="<<(g_b266LastPrivatePresentUs.load()/1000.0)<<", last_worker_total_ms="<<(g_b266LastWorkerTotalUs.load()/1000.0)<<", last_worker_pending="<<g_b266LastWorkerPending.load()<<", diagnostic_only=1, runtime_policy_changed=0\n";out.flush();ULONGLONG expectedTelemetry=lastTelemetry;g_b174LastTelemetry.compare_exchange_strong(expectedTelemetry,now,std::memory_order_acq_rel);}
 }
 g_b171LiveReady.store(false,std::memory_order_release);
 if(g_b174ProxyRevealed.load(std::memory_order_acquire))PostMessageA(g_xefgWindow,kB171VisibilityMessage,0,0);
 while(g_b171LiveBusy.load(std::memory_order_acquire)||g_b173hJobProcessing.load(std::memory_order_acquire))Sleep(1);
 AcquireSRWLockExclusive(&g_b173hJobLock);while(g_b174oPendingSize){auto&pending=g_b174oPendingJobs[g_b174oPendingHead];if(pending.slotIndex<kB173iColorSlotCount){B174ReplacedPending retired{};retired.nativeFenceValue=pending.nativeFenceValue;retired.slotIndex=pending.slotIndex;retired.sourceFrame=pending.sourceFrame;g_b174ReplacedPending.push_back(retired);g_b174ColorSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174DepthSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(pending.sourceFrame,"dropped_toggle_drain","capture_stopped_before_worker_accept",pending.slotIndex);}pending=B173hLiveJob{};g_b174oPendingHead=(g_b174oPendingHead+1)%kB174oPendingCapacity;--g_b174oPendingSize;}g_b174oPendingHead=0;g_b174oPendingTail=0;g_b173hJobPending.store(false,std::memory_order_release);g_b174PendingCount.store(0,std::memory_order_release);ReleaseSRWLockExclusive(&g_b173hJobLock);
 for(unsigned polls=0;(!g_b174InFlightFrames.empty()||!g_b174ReplacedPending.empty())&&polls<5000;++polls){b174_retire_completed_frames(out);if(!g_b174InFlightFrames.empty()||!g_b174ReplacedPending.empty())Sleep(1);}
 const bool gpuDrainComplete=g_b174InFlightFrames.empty()&&g_b174ReplacedPending.empty();if(!gpuDrainComplete){g_b175bDeviceErrors.fetch_add(1,std::memory_order_acq_rel);g_b175PipelineReady.store(false,std::memory_order_release);g_b175PipelineFailed.store(true,std::memory_order_release);out<<"B175n: ASYNC_STOP_DRAIN_FAILED pending="<<g_b174InFlightFrames.size()+g_b174ReplacedPending.size()<<", fail_closed=1\n";out.flush();}
 unsigned liveSubmitted=g_b171LiveSubmitted.load(std::memory_order_acquire);
 const bool proxyWasRevealed=g_b174SessionEverRevealed.load(std::memory_order_acquire);
 if(g_xefgWindow){PostMessageA(g_xefgWindow,kB171VisibilityMessage,0,0);for(unsigned i=0;i<50&&g_b171WindowVisible.load(std::memory_order_acquire);++i)Sleep(10);}
 bool hiddenConfirmed=!g_b171WindowVisible.load(std::memory_order_acquire);
 g_b174TestActive.store(false,std::memory_order_release);g_b174ToggleOffCount.fetch_add(1,std::memory_order_acq_rel);b174_retire_completed_frames(out);
 const ULONGLONG elapsedMs=g_b174TestStart?(GetTickCount64()-g_b174TestStart):0;const ULONGLONG b302RuntimeMs=(g_b174ActivationStart&&GetTickCount64()>=g_b174ActivationStart)?GetTickCount64()-g_b174ActivationStart:elapsedMs;b302_runtime_log_event("XeSS/XeFG disabled",b302RuntimeMs);const auto rateWindow=b174_rate_window_snapshot();const ULONGLONG rateElapsedMs=rateWindow.start?GetTickCount64()-rateWindow.start:0;const double rateElapsedSeconds=rateElapsedMs?rateElapsedMs/1000.0:0.0;
 const unsigned accepted=g_b174Accepted.load(std::memory_order_acquire),dropped=g_b174Dropped.load(std::memory_order_acquire),replaced=g_b174Replaced.load(std::memory_order_acquire),outputFrames=g_b174OutputFrames.load(std::memory_order_acquire),generatedFrames=g_b174GeneratedFrames.load(std::memory_order_acquire),stopReason=g_b174StopReason.load(std::memory_order_acquire);
 const unsigned observedSourcePresents=g_b174oObservedSourcePresents.load(std::memory_order_acquire);
 const char*stopLabel=stopReason==2?"f9":stopReason==3?"escape":stopReason==4?"error":stopReason==5?"startup_timeout":stopReason==6?"generated_rate_below_source":stopReason==8?"source_swapchain_replaced":stopReason==9?"legacy_bounded_stop":"shutdown";
 const bool continuouslyVisible=proxyWasRevealed&&!g_b174VisibilityLost.load(std::memory_order_acquire);
 const unsigned rateObserved=observedSourcePresents>=rateWindow.observedBase?observedSourcePresents-rateWindow.observedBase:0,rateOutput=outputFrames>=rateWindow.outputBase?outputFrames-rateWindow.outputBase:0;
 out<<"B174y: TOGGLE_OFF reason="<<stopLabel<<", elapsed_ms="<<elapsedMs<<", rate_window_elapsed_ms="<<rateElapsedMs<<", gta_source_presents="<<observedSourcePresents<<", accepted="<<accepted<<", completed="<<liveSubmitted<<", pending="<<g_b174PendingCount.load(std::memory_order_acquire)<<", dropped="<<dropped<<", replaced="<<replaced<<", source_fps="<<(rateElapsedSeconds?rateObserved/rateElapsedSeconds:0.0)<<", displayed_effective_fps="<<(rateElapsedSeconds?rateOutput/rateElapsedSeconds:0.0)<<", generated="<<generatedFrames<<", present_errors="<<g_b174PresentErrors.load()<<", tag_errors="<<g_b174TagErrors.load()<<", status_errors="<<g_b174StatusErrors.load()<<", proxy_was_revealed="<<(proxyWasRevealed?1:0)<<", proxy_hidden="<<(hiddenConfirmed?1:0)<<", pending_fence_retirements="<<g_b174InFlightFrames.size()+g_b174ReplacedPending.size()<<", reusable_pipeline="<<((stopReason==2||stopReason==3||stopReason==8||stopReason==9)?1:0)<<"\n";out.flush();
 auto disableAfterResult=xefgSwapChainSetEnabled(g_xefgContext,0);
 out<<"B172: stage=XeFGDisableAfterSecondPresent, enabled=0, result="<<(int)disableAfterResult<<"\n";
 const bool cleanStop=(stopReason==2||stopReason==3||stopReason==8||stopReason==9)&&gpuDrainComplete&&disableAfterResult==XEFG_SWAPCHAIN_RESULT_SUCCESS&&hiddenConfirmed;
 if(!cleanStop&&!g_b174ShutdownRequested.load(std::memory_order_acquire)){g_b175PipelineReady.store(false,std::memory_order_release);g_b175PipelineFailed.store(true,std::memory_order_release);out<<"B174y: FAILURE_FALLBACK reason="<<stopLabel<<", proxy_hidden="<<(hiddenConfirmed?1:0)<<", xefg_disabled="<<(disableAfterResult==XEFG_SWAPCHAIN_RESULT_SUCCESS?1:0)<<", normal_gta_presentation_visible=1\n";out.flush();break;}
 }
 }
 delete w;return g_b175PipelineFailed.load(std::memory_order_acquire)?2:0;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B164SubmitFrameInputs(const void*motionPixels,UINT motionPitch,const void*firstDepthPixels,UINT firstDepthPitch,const void*secondDepthPixels,UINT secondDepthPitch,const float*firstView,const float*firstProjection,const float*secondView,const float*secondProjection,UINT width,UINT height,const char*path){
 if(!motionPixels||!motionPitch||!firstDepthPixels||!firstDepthPitch||!secondDepthPixels||!secondDepthPitch||!firstView||!firstProjection||!secondView||!secondProjection||!width||!height||!path)return E_INVALIDARG;if(g_b171LiveBusy.load(std::memory_order_acquire)){log_line("B174o: CPU_UPLOAD_LIVE_PATH_REJECTED reason=all_live_d3d12_resources_must_be_prewarmed, gpu_frame_resource_ring_required=1");return E_UNEXPECTED;}
 if(g_b166FrameAttempted.exchange(true)){log_line("B172: additional F9 frame submission ignored; one two-frame private generation proof is permitted per process.");return S_FALSE;}auto*w=new B160MotionWork{};w->width=width;w->height=height;w->motionPitch=motionPitch;w->firstDepthPitch=firstDepthPitch;w->secondDepthPitch=secondDepthPitch;w->path=path;w->motionBytes.resize((size_t)motionPitch*height);w->firstDepthBytes.resize((size_t)firstDepthPitch*height);w->secondDepthBytes.resize((size_t)secondDepthPitch*height);std::memcpy(w->motionBytes.data(),motionPixels,w->motionBytes.size());std::memcpy(w->firstDepthBytes.data(),firstDepthPixels,w->firstDepthBytes.size());std::memcpy(w->secondDepthBytes.data(),secondDepthPixels,w->secondDepthBytes.size());std::memcpy(w->firstView,firstView,sizeof(w->firstView));std::memcpy(w->firstProjection,firstProjection,sizeof(w->firstProjection));std::memcpy(w->secondView,secondView,sizeof(w->secondView));std::memcpy(w->secondProjection,secondProjection,sizeof(w->secondProjection));HANDLE t=CreateThread(nullptr,0,b160_motion_worker,w,0,nullptr);if(!t){delete w;return HRESULT_FROM_WIN32(GetLastError());}CloseHandle(t);return S_OK;}
static unsigned bytes_per_pixel(DXGI_FORMAT f)
{
    if(f==DXGI_FORMAT_R16G16B16A16_FLOAT)return 8;
    if(f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||
       f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)return 4;
    return 0;
}
static unsigned long long hash_mapped(const D3D11_MAPPED_SUBRESOURCE& m,unsigned width,unsigned height,unsigned bpp)
{
    unsigned long long h=1469598103934665603ull;const size_t rowBytes=(size_t)width*bpp;
    for(unsigned y=0;y<height;++y){const auto* p=(const unsigned char*)m.pData+(size_t)y*m.RowPitch;
        for(size_t x=0;x<rowBytes;++x){h^=p[x];h*=1099511628211ull;}}
    return h;
}
static bool b173i_color_desc_matches(const D3D11_TEXTURE2D_DESC&a,const D3D11_TEXTURE2D_DESC&b)
{
 return a.Width==b.Width&&a.Height==b.Height&&a.Format==b.Format&&a.SampleDesc.Count==b.SampleDesc.Count&&a.SampleDesc.Quality==b.SampleDesc.Quality;
}
static HRESULT b173i_prepare_color_slot(B173iColorSlot&slot,ID3D11Device*nativeDevice,const D3D11_TEXTURE2D_DESC&source)
{
 if(!nativeDevice||!g_on12Device||!g_on12||!g_device12)return E_INVALIDARG;
 if(source.Width<640||source.Height<360||source.Width>3840||source.Height>2160||(source.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT&&source.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&source.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)||source.SampleDesc.Count!=1)return E_INVALIDARG;
 if(slot.ready&&b173i_color_desc_matches(slot.sourceDesc,source))return S_FALSE;
 slot=B173iColorSlot{};
 D3D11_TEXTURE2D_DESC inputDesc=source;inputDesc.MipLevels=1;inputDesc.ArraySize=1;inputDesc.Usage=D3D11_USAGE_DEFAULT;inputDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;inputDesc.CPUAccessFlags=0;inputDesc.MiscFlags=0;
 HRESULT hr=S_OK;
 hr=nativeDevice->CreateTexture2D(&inputDesc,nullptr,&slot.xessInput);if(FAILED(hr))return hr;
 D3D11_TEXTURE2D_DESC outputDesc=inputDesc;outputDesc.Width=g_b232OutputWidth;outputDesc.Height=g_b232OutputHeight;outputDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;outputDesc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
 hr=nativeDevice->CreateTexture2D(&outputDesc,nullptr,&slot.xessOutput);if(FAILED(hr))return hr;
 hr=nativeDevice->CreateShaderResourceView(slot.xessOutput.Get(),nullptr,&slot.xessOutputSrv);if(FAILED(hr))return hr;
 D3D11_TEXTURE2D_DESC sharedDesc=outputDesc;sharedDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
 sharedDesc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;sharedDesc.MiscFlags=D3D11_RESOURCE_MISC_SHARED;
 hr=nativeDevice->CreateTexture2D(&sharedDesc,nullptr,&slot.shared);if(FAILED(hr)||!slot.shared)return FAILED(hr)?hr:E_FAIL;
 hr=nativeDevice->CreateRenderTargetView(slot.shared.Get(),nullptr,&slot.sharedRtv);if(FAILED(hr))return hr;
 hr=nativeDevice->CreateDeferredContext(0,&slot.conversionContext);if(FAILED(hr))return hr;
 D3D11_VIEWPORT viewport{};viewport.Width=(float)g_b232OutputWidth;viewport.Height=(float)g_b232OutputHeight;viewport.MaxDepth=1.0f;
 slot.conversionContext->RSSetViewports(1,&viewport);slot.conversionContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 slot.conversionContext->VSSetShader(g_b175bColorConvertVs.Get(),nullptr,0);slot.conversionContext->PSSetShader(g_b175bColorConvertPs.Get(),nullptr,0);slot.conversionContext->RSSetState(g_b175bColorConvertRs.Get());
 ID3D11ShaderResourceView*srv=slot.xessOutputSrv.Get();slot.conversionContext->PSSetShaderResources(0,1,&srv);ID3D11RenderTargetView*rtv=slot.sharedRtv.Get();slot.conversionContext->OMSetRenderTargets(1,&rtv,nullptr);slot.conversionContext->Draw(3,0);
 srv=nullptr;rtv=nullptr;slot.conversionContext->PSSetShaderResources(0,1,&srv);slot.conversionContext->OMSetRenderTargets(1,&rtv,nullptr);
 hr=slot.conversionContext->FinishCommandList(FALSE,&slot.conversionCommands);if(FAILED(hr))return hr;
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC colorDesc{};colorDesc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;colorDesc.Width=g_b232OutputWidth;colorDesc.Height=g_b232OutputHeight;colorDesc.DepthOrArraySize=1;colorDesc.MipLevels=1;colorDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;colorDesc.SampleDesc.Count=1;colorDesc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
 hr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&colorDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&slot.output));if(FAILED(hr)||!slot.output)return FAILED(hr)?hr:E_FAIL;
 slot.sourceDesc=source;slot.ready=true;return S_OK;
}
static HRESULT b173i_prepare_color_pool(ID3D11Device*nativeDevice,const D3D11_TEXTURE2D_DESC&source,std::ofstream&out)
{
 LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);HRESULT hr=S_OK;unsigned prepared=0,reused=0;
 for(unsigned i=0;i<kB173iColorSlotCount;++i){HRESULT slotHr=b173i_prepare_color_slot(g_b173iColorSlots[i],nativeDevice,source);if(slotHr==S_FALSE)++reused;else if(SUCCEEDED(slotHr))++prepared;else{hr=slotHr;break;}}
 const bool ready=SUCCEEDED(hr);g_b173iColorPoolReady.store(ready,std::memory_order_release);if(ready){g_b173iNextColorSlot=0;for(auto&state:g_b174ColorSlotState)state.store(0,std::memory_order_release);}QueryPerformanceCounter(&end);
 out<<"B175n: COLOR_XESS_POOL_PREWARM slots="<<kB173iColorSlotCount<<", prepared="<<prepared<<", reused="<<reused<<", internal_source_size="<<source.Width<<"x"<<source.Height<<", xess_output_size="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<"_RGBA16F, xefg_input_size="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<"_BGRA8, presentation_conversion=final_sdr_passthrough, live_allocation=0, source_format="<<(unsigned)source.Format<<", bridge_format="<<(unsigned)DXGI_FORMAT_B8G8R8A8_UNORM<<", cpu_ms="<<b173g_elapsed_ms(begin,end)<<", ready="<<(ready?1:0)<<", hr="<<(long)hr<<"\n";out.flush();return hr;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B175PrepareDiscoveredInternalColor(ID3D11Texture2D*source,const char*path)
{
 if(!source||!path||!g_device12)return E_INVALIDARG;D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);ComPtr<ID3D11Device>nativeDevice;source->GetDevice(&nativeDevice);
 if(!nativeDevice||desc.Width<640||desc.Height<360||desc.Width>3840||desc.Height>2160||desc.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT||desc.SampleDesc.Count!=1||(desc.BindFlags&D3D11_BIND_SHADER_RESOURCE)==0)return E_INVALIDARG;
 std::ofstream out(path,std::ios::app);HRESULT hr=b175b_prepare_xess(nativeDevice.Get(),desc.Format,desc.Width,desc.Height,out);if(SUCCEEDED(hr)||hr==S_FALSE)hr=b175b_prepare_color_conversion(nativeDevice.Get(),out);if(SUCCEEDED(hr)){
  D3D11_TEXTURE2D_DESC finalDesc=desc;finalDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;finalDesc.BindFlags=D3D11_BIND_RENDER_TARGET;finalDesc.MiscFlags=0;
  hr=b173i_prepare_color_pool(nativeDevice.Get(),finalDesc,out);
  out<<"B241: FINAL_COMPOSITED_COLOR_POOL source="<<finalDesc.Width<<"x"<<finalDesc.Height<<"_BGRA8, trigger_internal="<<desc.Width<<"x"<<desc.Height<<"_RGBA16F, phone_hud_expected_in_source=1, frame_scaling_dependency=0\n";out.flush();
}
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC bridgeColor{};bridgeColor.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;bridgeColor.Width=g_b232OutputWidth;bridgeColor.Height=g_b232OutputHeight;bridgeColor.DepthOrArraySize=1;bridgeColor.MipLevels=1;bridgeColor.Format=DXGI_FORMAT_B8G8R8A8_UNORM;bridgeColor.SampleDesc.Count=1;bridgeColor.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
 g_b147ColorResource.Reset();if(SUCCEEDED(hr))hr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&bridgeColor,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&g_b147ColorResource));
 out<<"B175n: DEPENDENT_BRIDGE_COLOR_CREATE after_discovery=1, created="<<(g_b147ColorResource?1:0)<<", size="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<", format=BGRA8, hr="<<(long)hr<<"\n";out.flush();
 D3D11_RESOURCE_FLAGS wrappedFlags{};ComPtr<ID3D11Resource>wrappedColor;if(SUCCEEDED(hr))hr=g_on12->CreateWrappedResource(g_b147ColorResource.Get(),&wrappedFlags,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,IID_PPV_ARGS(&wrappedColor));
 if(SUCCEEDED(hr)&&wrappedColor){ID3D11Resource*resources[]{wrappedColor.Get()};g_on12->AcquireWrappedResources(resources,1);g_on12->ReleaseWrappedResources(resources,1);g_on12Context->Flush();}
 out<<"B175n: DEPENDENT_WRAPPED_COLOR_CREATE after_discovery=1, created="<<(wrappedColor?1:0)<<", outgoing_state=NON_PIXEL_SHADER_RESOURCE, hr="<<(long)hr<<"\n";out.flush();
 D3D12_RESOURCE_DESC color{};color.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;color.Width=g_b232OutputWidth;color.Height=g_b232OutputHeight;color.DepthOrArraySize=1;color.MipLevels=1;color.Format=DXGI_FORMAT_B8G8R8A8_UNORM;color.SampleDesc.Count=1;color.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
 g_b171SecondColorResource.Reset();if(SUCCEEDED(hr))hr=g_device12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&color,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&g_b171SecondColorResource));
 const bool ready=SUCCEEDED(hr)&&g_b171SecondColorResource&&g_b173iColorPoolReady.load(std::memory_order_acquire);g_b175DiscoveryColorDesc=desc;g_b171SecondColorHash=2;g_b171SecondColorReady.store(ready,std::memory_order_release);
 if(ready){HRESULT windowHr=b225_create_proxy_on_gta_thread(path);if(FAILED(windowHr))hr=windowHr;else hr=start_b146_worker(g_b232OutputWidth,g_b232OutputHeight,DXGI_FORMAT_B8G8R8A8_UNORM,path);}
 out<<"B175n: DISCOVERED_INTERNAL_COLOR_PREPARE ready="<<(ready&&SUCCEEDED(hr)?1:0)<<", source="<<desc.Width<<"x"<<desc.Height<<"_RGBA16F, xess_output="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<"_RGBA16F, xefg_transport="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<"_BGRA8, presentation_conversion=final_sdr_passthrough, retained_identity=1, live_color_source=final_composited_swapchain_backbuffer, gta_ui_preserved=1, dependent_xefg_dispatched_after_discovery="<<(ready?1:0)<<", worker_thread=1, hr="<<(long)hr<<"\n";out.flush();return ready&&SUCCEEDED(hr)?S_OK:FAILED(hr)?hr:E_FAIL;
}
static bool b240_log_sample(unsigned long long frameId)
{
 static std::atomic<unsigned long long> n{0};const auto v=n.fetch_add(1,std::memory_order_relaxed)+1;return v<=8||(v%120)==0;
}
static HRESULT b173i_capture_live_color(ID3D11DeviceContext*context,ID3D11Texture2D*internalColor,const D3D11_TEXTURE2D_DESC&source,std::ofstream&out)
{
 if(!context||!internalColor||!g_b173iColorPoolReady.load(std::memory_order_acquire)||source.Width<640||source.Height<360||source.Width>3840||source.Height>2160||(source.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT&&source.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&source.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)||source.SampleDesc.Count!=1)return E_INVALIDARG;
 ComPtr<ID3D11Device>nativeDevice;context->GetDevice(&nativeDevice);
 const unsigned slotIndex=g_b174CaptureSlot;if(slotIndex>=kB173iColorSlotCount)return E_FAIL;auto&slot=g_b173iColorSlots[slotIndex];if(!slot.ready||!b173i_color_desc_matches(slot.sourceDesc,source))return E_FAIL;
 auto&depthSlot=g_b173jDepthMotionSlots[slotIndex];if(!g_b175bXessReady.load(std::memory_order_acquire)||!depthSlot.nativeMotion||!depthSlot.privateDepth)return E_FAIL;
 LARGE_INTEGER begin{},afterCopy{},end{};QueryPerformanceCounter(&begin);context->CopyResource(slot.xessInput.Get(),internalColor);QueryPerformanceCounter(&afterCopy);
 xess_d3d11_execute_params_t execute{};execute.pColorTexture=slot.xessInput.Get();execute.pVelocityTexture=depthSlot.nativeMotion.Get();execute.pDepthTexture=depthSlot.privateDepth.Get();execute.pOutputTexture=slot.xessOutput.Get();execute.exposureScale=1.0f;execute.resetHistory=g_b175bXessReset.exchange(false,std::memory_order_acq_rel)?1u:0u;execute.inputWidth=source.Width;execute.inputHeight=source.Height;
 const xess_result_t xessResult=xessD3D11Execute(g_b175bXess,&execute);if(xessResult!=XESS_RESULT_SUCCESS){g_b175bXessErrors.fetch_add(1,std::memory_order_acq_rel);const HRESULT removed=nativeDevice?nativeDevice->GetDeviceRemovedReason():E_NOINTERFACE;if(xessResult==XESS_RESULT_ERROR_DEVICE||xessResult==XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY||FAILED(removed))g_b175bDeviceErrors.fetch_add(1,std::memory_order_acq_rel);g_b175bXessReset.store(true,std::memory_order_release);out<<"B175n: XESS_EXECUTE_FAILED source_frame_id="<<g_b172GtaFrame<<", slot="<<slotIndex<<", result="<<(int)xessResult<<", device_removed_reason="<<(long)removed<<", fail_closed=1\n";out.flush();return E_FAIL;}
 if(!slot.conversionCommands)return E_FAIL;context->ExecuteCommandList(slot.conversionCommands.Get(),TRUE);/* B213: no FPS/UI drawing in the XeSS transport surface. */QueryPerformanceCounter(&end);AcquireSRWLockExclusive(&g_b175MotionHistoryLock);std::memcpy(g_b175bXessPreviousViewProjection,g_b174CaptureJob.currentViewProjection,64);g_b175bHaveXessPreviousViewProjection=true;ReleaseSRWLockExclusive(&g_b175MotionHistoryLock);g_b175bXessFrames.fetch_add(1,std::memory_order_acq_rel);
 g_b171SecondColorResource=slot.output;g_b171SecondColorHash=(unsigned long long)g_b172GtaFrame+1ull;g_b171SecondColorReady.store(true,std::memory_order_release);
 if(b240_log_sample(g_b172GtaFrame))out<<"B175n: XESS_LIVE_EXECUTE source_frame_id="<<g_b172GtaFrame<<", slot="<<slotIndex<<", input_copy_ms="<<b173g_elapsed_ms(begin,afterCopy)<<", execute_and_conversion_submit_ms="<<b173g_elapsed_ms(afterCopy,end)<<", source_size="<<source.Width<<"x"<<source.Height<<", source_format="<<(source.Format==DXGI_FORMAT_B8G8R8A8_UNORM?"BGRA8":source.Format==DXGI_FORMAT_R8G8B8A8_UNORM?"RGBA8":"RGBA16F")<<", final_composited_backbuffer="<<(source.Format==DXGI_FORMAT_B8G8R8A8_UNORM?1:0)<<", xess_output="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<"_RGBA16F, xefg_transport="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<"_BGRA8, presentation_conversion=final_sdr_passthrough, reset_history="<<execute.resetHistory<<", source_read_only=1, gpu_only=1, allocation_ms=0, result="<<(int)xessResult<<"\n";out.flush();return S_OK;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B175CaptureSecondColorAtPresent(ID3D11DeviceContext*context,IDXGISwapChain*swap,const char*path)
{
 if(!context||!swap||!path)return E_INVALIDARG;log_line("B175n: PRESENT_BACKBUFFER_COLOR_CAPTURE_REJECTED discovered_internal_RGBA16F_required=1, fail_closed=1");return E_NOTIMPL;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B175PrepareCapturedSecondColor(const char*path)
{
 if(!path)return E_INVALIDARG;return E_NOTIMPL;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B171CaptureSecondBackBuffer(ID3D11DeviceContext* context,IDXGISwapChain* swap,const char* path)
{
    if(!context||!swap||!path)return E_INVALIDARG;
    std::ofstream out(path,std::ios::app);ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr=swap->GetBuffer(0,IID_PPV_ARGS(&backBuffer));if(FAILED(hr)||!backBuffer)return FAILED(hr)?hr:E_FAIL;
    D3D11_TEXTURE2D_DESC source{};backBuffer->GetDesc(&source);const unsigned bpp=bytes_per_pixel(source.Format);
    if(!bpp||source.SampleDesc.Count!=1)return E_NOTIMPL;
    ComPtr<ID3D11Device> nativeDevice;context->GetDevice(&nativeDevice);
    log_line("B175n: BACKBUFFER_TO_XESS_PATH_DISABLED discovered_internal_RGBA16F_required=1, fail_closed=1");return E_NOTIMPL;
}
extern "C" __declspec(dllexport) HRESULT WINAPI B171PresentLiveFrame(ID3D11DeviceContext*context,IDXGISwapChain*swap,ID3D11Texture2D*internalColor,const char*path)
{
 BOOL b208Fullscreen=FALSE;if(swap)swap->GetFullscreenState(&b208Fullscreen,nullptr);g_b208ExclusiveFullscreen.store(b208Fullscreen!=FALSE,std::memory_order_release);
 LARGE_INTEGER timingBegin{},timingAfterGetBuffer{},timingAfterCapture{},timingBeforeFlush{},timingAfterFlush{},timingQueued{},timingQueueLockBegin{},timingAfterQueue{};QueryPerformanceCounter(&timingBegin);const long long b317PriorEntry=g_b317LastSourcePipelineEntryQpc.exchange(timingBegin.QuadPart,std::memory_order_acq_rel);LARGE_INTEGER b317Freq{};QueryPerformanceFrequency(&b317Freq);const double b317EntryGapMs=(b317PriorEntry>0&&b317Freq.QuadPart>0)?((double)(timingBegin.QuadPart-b317PriorEntry)*1000.0/(double)b317Freq.QuadPart):0.0;
 if(!context||!swap||!internalColor||!path)return E_INVALIDARG;
 b392_start_watchdog(path);g_b393LastSourceEntryQpc.store(timingBegin.QuadPart,std::memory_order_release);g_b393LastSourceFrame.store(g_b172GtaFrame,std::memory_order_release);g_b393SourceEntrySeq.fetch_add(1,std::memory_order_acq_rel);b392_set_stage(g_b392GtaStage,1,g_b172GtaFrame,0);
 if(!g_b171LiveBusy.load(std::memory_order_acquire)){b392_clear_stage(g_b392GtaStage);return S_FALSE;}
 static thread_local std::ofstream out; static thread_local std::string outPath; if(!out.is_open()||outPath!=path){if(out.is_open())out.close();out.clear();out.open(path,std::ios::app);outPath=path;} // B396: no per-frame log file open/close on GTA render thread.
 const unsigned captureSlot=g_b174CaptureSlot;auto releaseCapture=[&](){b174c_release_capture_slot(captureSlot);g_b174CaptureSlot=~0u;g_b171LiveBusy.store(false,std::memory_order_release);b392_clear_stage(g_b392GtaStage);};
 unsigned ordinal=g_b174Accepted.load(std::memory_order_acquire);
 if(!g_b172Fresh){b174_request_stop(4);log_line("B174y: FAILURE_FALLBACK reason=fresh_native_inputs_unavailable, stale_fallback=0");releaseCapture();return E_FAIL;}
 // B387 freeze-only: reject excess source work BEFORE backbuffer capture, XeSS execution, fence submission, or XeFG tagging.
 // B386 made Present nonblocking, but an 11-frame producer backlog could still build behind a slow compositor.
 // Six pending frames preserves x4 feed headroom while bounding latency/resource ownership and prevents the GTA render path from feeding a saturated worker.
 const unsigned b387PendingBeforeCapture=g_b174PendingCount.load(std::memory_order_acquire);
 if(!b385_load_protected_modules()){b174_request_stop(4);releaseCapture();return E_FAIL;}
 const unsigned b423PendingLimit=g_b423FreezePendingLimit();
 if(b387PendingBeforeCapture>=b423PendingLimit){
  g_b266QueueFullDrops.fetch_add(1,std::memory_order_relaxed);g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);
  b175_log_disposition(g_b172GtaFrame,"dropped_pre_capture_backpressure","pending_ge_locked_freeze_dll_limit_before_gpu_submission",captureSlot);
  out<<"B387: PRE_CAPTURE_BACKPRESSURE_DROP source_frame_id="<<g_b172GtaFrame<<", pending="<<b387PendingBeforeCapture<<", threshold="<<b423PendingLimit<<", before_color_capture=1, before_xess=1, before_native_fence=1, before_xefg=1, private_present_do_not_wait_retained=1, ghosting_unchanged=1\n";out.flush();
  g_b172Fresh=false;releaseCapture();return E_PENDING;
 }
 b392_set_stage(g_b392GtaStage,2,g_b172GtaFrame,0);ComPtr<ID3D11Texture2D> finalBackBuffer;HRESULT finalBufferHr=swap->GetBuffer(0,IID_PPV_ARGS(&finalBackBuffer));
 D3D11_TEXTURE2D_DESC internalDesc{};if(SUCCEEDED(finalBufferHr)&&finalBackBuffer)finalBackBuffer->GetDesc(&internalDesc);QueryPerformanceCounter(&timingAfterGetBuffer);
 if(SUCCEEDED(finalBufferHr)&&finalBackBuffer)b333_poll_forensic_capture(context,finalBackBuffer.Get(),g_b172GtaFrame,out);
 b392_set_stage(g_b392GtaStage,3,g_b172GtaFrame,0);HRESULT captureHr=(SUCCEEDED(finalBufferHr)&&finalBackBuffer)?b173i_capture_live_color(context,finalBackBuffer.Get(),internalDesc,out):finalBufferHr;QueryPerformanceCounter(&timingAfterCapture);
 if(captureHr!=S_OK||captureSlot>=kB173iColorSlotCount){b174_request_stop(4);releaseCapture();return FAILED(captureHr)?captureHr:E_FAIL;}
 b392_set_stage(g_b392GtaStage,4,g_b172GtaFrame,0);UINT64 nativeFenceValue=++g_b173fFenceValue;HRESULT signalHr=g_b173fNativeContext4?g_b173fNativeContext4->Signal(g_b173fNativeFence.Get(),nativeFenceValue):E_NOINTERFACE;
 if(SUCCEEDED(signalHr)){QueryPerformanceCounter(&timingBeforeFlush);context->Flush();QueryPerformanceCounter(&timingAfterFlush);signalHr=g_queue12->Wait(g_b173fSharedFence12.Get(),nativeFenceValue);}QueryPerformanceCounter(&timingQueued);
 if(FAILED(signalHr)){b174_request_stop(4);releaseCapture();return signalHr;}
 if(!g_b171LiveReady.load(std::memory_order_acquire)){releaseCapture();return S_FALSE;}
 B173hLiveJob job=g_b174CaptureJob;
 const unsigned b486DepthCameraSourceFrame=job.sourceFrame;
 auto b486Fp64=[](const float*m)->unsigned long long{const unsigned char*p=(const unsigned char*)m;unsigned long long h=1469598103934665603ull;for(unsigned i=0;i<64;++i){h^=p[i];h*=1099511628211ull;}return h;};
 job.b486DepthCameraSourceFrame=b486DepthCameraSourceFrame;job.b486ColorSourceFrame=g_b172GtaFrame;job.b486CameraVpFingerprint=b486Fp64(job.currentViewProjection);job.b486DepthCameraCaptureQpc=job.captureTime.QuadPart;job.b486ColorCaptureQpc=timingAfterCapture.QuadPart;
 B489AuthoritativeHandoffSnapshot b489Handoff{};if(B489GetAuthoritativeHandoff(&b489Handoff)){job.b489PublishedSerial=b489Handoff.publishedSerial;job.b489PublishedSourceFrame=b489Handoff.publishedSourceFrame;job.b489ConsumedSerial=b489Handoff.consumedSerial;job.b489ConsumedSourceFrame=b489Handoff.consumedSourceFrame;auto b489Fp=[](const unsigned char*b){unsigned long long h=1469598103934665603ull;for(unsigned i=0;i<256;++i){h^=(unsigned long long)b[i];h*=1099511628211ull;}return h;};if(b489Handoff.publishedValid)job.b489PublishedFp=b489Fp(b489Handoff.publishedBytes);if(b489Handoff.consumedValid)job.b489ConsumedFp=b489Fp(b489Handoff.consumedBytes);}
 {static unsigned long long b489Trace=0;const unsigned long long n=++b489Trace;if(n<=24||(n%60)==0){out<<"B489: AUTHORITATIVE_HANDOFF_PUBLISH_CONSUME source_frame_id="<<job.sourceFrame<<", published_source_frame="<<job.b489PublishedSourceFrame<<", published_serial="<<job.b489PublishedSerial<<", published_fingerprint="<<job.b489PublishedFp<<", consumed_source_frame="<<job.b489ConsumedSourceFrame<<", consumed_serial="<<job.b489ConsumedSerial<<", consumed_fingerprint="<<job.b489ConsumedFp<<", publish_consume_serial_match="<<((job.b489PublishedSerial!=0&&job.b489PublishedSerial==job.b489ConsumedSerial)?1:0)<<", publish_consume_bytes_match="<<((job.b489PublishedFp!=0&&job.b489PublishedFp==job.b489ConsumedFp)?1:0)<<", capture_matches_consumed_source_frame="<<((job.b489ConsumedSourceFrame!=0&&job.sourceFrame==job.b489ConsumedSourceFrame)?1:0)<<", depth_camera_capture_qpc="<<job.b486DepthCameraCaptureQpc<<", color_capture_qpc="<<job.b486ColorCaptureQpc<<", b488_rolling_recorder_retired=1, exact_b445_b480_object_only=1, hashing_at_capture_boundary_only=1, diagnostic_only=1, runtime_behavior_changed=0, camera_selection_unchanged=1, b474_history_policy_unchanged=1, b423_freezing_unchanged=1, xefg_x4_unchanged=1\n";}}
 b392_set_stage(g_b392GtaStage,5,g_b172GtaFrame,0);QueryPerformanceCounter(&timingQueueLockBegin);AcquireSRWLockExclusive(&g_b173hJobLock);
 const bool fresh=job.sourceFrame>g_b174LastAcceptedSource.load(std::memory_order_acquire);
 bool queued=false;unsigned b339Collapsed=0,b339FirstDropped=0,b339LastDropped=0;
 const long long b339LastPresentUs=g_b266LastPrivatePresentUs.load(std::memory_order_relaxed);
 if(fresh&&b385_load_protected_modules()&&g_b423FreezeCollapseSourceGap(g_b174oPendingSize,b317EntryGapMs)){
  while(g_b174oPendingSize){
   auto&pending=g_b174oPendingJobs[g_b174oPendingHead];
   if(pending.slotIndex<kB173iColorSlotCount){
    if(!b339Collapsed)b339FirstDropped=pending.sourceFrame;b339LastDropped=pending.sourceFrame;++b339Collapsed;
    B174ReplacedPending retired{};retired.nativeFenceValue=pending.nativeFenceValue;retired.slotIndex=pending.slotIndex;retired.sourceFrame=pending.sourceFrame;g_b174ReplacedPending.push_back(retired);
    g_b174ColorSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174DepthSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);
   }
   pending=B173hLiveJob{};g_b174oPendingHead=(g_b174oPendingHead+1)%kB174oPendingCapacity;--g_b174oPendingSize;
  }
  g_b174oPendingHead=0;g_b174oPendingTail=0;g_b174PendingCount.store(0,std::memory_order_release);job.constants.resetHistory=1;
 }
 if(fresh&&g_b174oPendingSize<kB174oPendingCapacity){
  g_b174oPendingJobs[g_b174oPendingTail]=job;g_b174oPendingTail=(g_b174oPendingTail+1)%kB174oPendingCapacity;++g_b174oPendingSize;queued=true;g_b173hJobPending.store(true,std::memory_order_release);g_b174PendingCount.store(g_b174oPendingSize,std::memory_order_release);g_b174ColorSlotState[captureSlot].store(2,std::memory_order_release);g_b174DepthSlotState[captureSlot].store(2,std::memory_order_release);g_b172Fresh=false;g_b174LastAcceptedSource.store(job.sourceFrame,std::memory_order_release);
 }
 ReleaseSRWLockExclusive(&g_b173hJobLock);QueryPerformanceCounter(&timingAfterQueue);b392_clear_stage(g_b392GtaStage);
 if(b339Collapsed){out<<"B395: STALE_PENDING_COLLAPSED_ON_SOURCE_GAP source_entry_gap_ms="<<b317EntryGapMs<<", last_private_present_ms="<<(b339LastPresentUs/1000.0)<<", dropped_pending="<<b339Collapsed<<", dropped_source_range={"<<b339FirstDropped<<","<<b339LastDropped<<"}, newest_source_frame="<<job.sourceFrame<<", queue_after="<<g_b174PendingCount.load()<<", worker_owned_untouched=1, fence_retirement_used=1, newest_reset_history="<<(job.constants.resetHistory?1:0)<<", xefg_x4_semantics_unchanged=1\n";out.flush();}
 if(queued)g_b266QueueFullBurstActive.store(false,std::memory_order_release);
 g_b174CaptureSlot=~0u;g_b171LiveBusy.store(false,std::memory_order_release);
 const unsigned long long b317Ordinal=g_b317SourceProfileOrdinal.fetch_add(1,std::memory_order_relaxed)+1;const double b317ColorXessMs=b173g_elapsed_ms(timingBegin,timingAfterCapture);const double b317FenceMs=b173g_elapsed_ms(timingAfterCapture,timingQueued);const double b317QueueLockMs=b173g_elapsed_ms(timingQueueLockBegin,timingAfterQueue);const double b317SourceTotalMs=job.nativeDepthMs+b173g_elapsed_ms(timingBegin,timingAfterQueue);const bool b317Sample=(b317Ordinal<=12)||((b317Ordinal%30)==0)||b317SourceTotalMs>=4.0;if(b317Sample){out<<"B317: SOURCE_FRAME_PIPELINE_PROFILE ordinal="<<b317Ordinal<<", source_frame_id="<<job.sourceFrame<<", source_entry_gap_ms="<<b317EntryGapMs<<", depth_total_ms="<<job.nativeDepthMs<<", depth_pack_submit_ms="<<job.depthPackSubmitMs<<", motion_reconstruct_submit_ms="<<job.motionReconstructSubmitMs<<", xess_color_capture_ms="<<b317ColorXessMs<<", fence_submit_ms="<<b317FenceMs<<", queue_lock_submit_ms="<<b317QueueLockMs<<", source_pipeline_total_ms="<<b317SourceTotalMs<<", queue_depth_after="<<g_b174PendingCount.load()<<", queue_full_drops="<<g_b266QueueFullDrops.load()<<", last_private_present_ms="<<(g_b266LastPrivatePresentUs.load()/1000.0)<<", last_worker_total_ms="<<(g_b266LastWorkerTotalUs.load()/1000.0)<<", render_thread_only=1, diagnostic_only=1, runtime_behavior_changed=0, xefg_x4_unchanged=1\n";out.flush();}
if(b317Sample){out<<"B465: GTA_THREAD_STAGE_SPLIT source_frame_id="<<job.sourceFrame<<", getbuffer_desc_ms="<<b173g_elapsed_ms(timingBegin,timingAfterGetBuffer)<<", color_capture_ms="<<b173g_elapsed_ms(timingAfterGetBuffer,timingAfterCapture)<<", fence_signal_setup_ms="<<b173g_elapsed_ms(timingAfterCapture,timingBeforeFlush)<<", d3d11_flush_cpu_ms="<<b173g_elapsed_ms(timingBeforeFlush,timingAfterFlush)<<", d3d12_queue_wait_submit_ms="<<b173g_elapsed_ms(timingAfterFlush,timingQueued)<<", queue_lock_ms="<<b317QueueLockMs<<", depth_total_ms="<<job.nativeDepthMs<<", helper_total_ms="<<b317SourceTotalMs<<", source_entry_gap_ms="<<b317EntryGapMs<<", pending="<<g_b174PendingCount.load()<<", diagnostic_only=1, protected_freezing_unchanged=1, protected_ghosting_unchanged=1\n";out.flush();}
  const bool b254Sample=b240_log_sample(job.sourceFrame);if(b254Sample){out<<"B174c: GTA_THREAD_NATIVE_SUBMISSION ordinal="<<ordinal+1<<", source_frame_id="<<job.sourceFrame<<", slot="<<captureSlot<<", depth_capture_ms="<<job.nativeDepthMs<<", color_and_fence_ms="<<b173g_elapsed_ms(timingBegin,timingQueued)<<", gta_thread_total_ms="<<job.gtaThreadMs<<", target_below_2ms="<<(job.gtaThreadMs<2.0?1:0)<<", native_fence="<<nativeFenceValue<<", cpu_wait=0\n";out<<"B173h: LIVE_FRAME_QUEUED ordinal="<<ordinal+1<<", source_frame_id="<<job.sourceFrame<<", present_id=worker_assigned, fresh="<<(fresh?1:0)<<", capture_hr="<<(long)captureHr<<", color_capture_ms="<<b173g_elapsed_ms(timingBegin,timingAfterCapture)<<", gta_thread_total_ms="<<job.gtaThreadMs<<", worker_pending="<<(queued?1:0)<<", gta_thread_private_present=0, ready="<<(queued?1:0)<<"\n";}
 if(!fresh||!queued){if(fresh){g_b266QueueFullDrops.fetch_add(1,std::memory_order_relaxed);const bool wasFull=g_b266QueueFullBurstActive.exchange(true,std::memory_order_acq_rel);if(!wasFull){const unsigned burst=g_b266QueueFullBursts.fetch_add(1,std::memory_order_relaxed)+1;out<<"B266: QUEUE_FULL_TRANSITION burst="<<burst<<", source_frame_id="<<job.sourceFrame<<", queue_depth="<<g_b174PendingCount.load()<<", capacity="<<kB174oPendingCapacity<<", last_xell_sleep_ms="<<(g_b266LastXellSleepUs.load()/1000.0)<<", last_private_present_ms="<<(g_b266LastPrivatePresentUs.load()/1000.0)<<", last_worker_total_ms="<<(g_b266LastWorkerTotalUs.load()/1000.0)<<", last_worker_pending="<<g_b266LastWorkerPending.load()<<", xell_low_latency=1, behavior_change=0\n";out.flush();}}b174c_release_capture_slot(captureSlot);g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(job.sourceFrame,fresh?"dropped_queue_full":"dropped_non_monotonic",fresh?"ordered_queue_backpressure":"source_id_not_newer",captureSlot);return fresh?E_PENDING:E_FAIL;}
 if(b254Sample){out<<"B176: ORDERED_FRAME_QUEUE source_frame_id="<<job.sourceFrame<<", slot="<<captureSlot<<", pending_count="<<g_b174PendingCount.load()<<", capacity="<<kB174oPendingCapacity<<", replaced_previous=0, worker_owned_untouched=1, fence_retiring_untouched=1\n";out.flush();}
 return S_OK;
}
static HRESULT b175_async_prewarm(ID3D11DeviceContext* context,IDXGISwapChain* swap,const char* path)
{
    if(!context||!swap||!path)return E_INVALIDARG;
    g_b174ActivationStart=GetTickCount64();
    HRESULT hr=B76LogSelectedDraws(context,0,0,path);if(FAILED(hr))return hr;
    std::ofstream out(path,std::ios::app);if(!out)return E_FAIL;
    DXGI_SWAP_CHAIN_DESC swapDesc{};HRESULT swapDescHr=swap->GetDesc(&swapDesc);RECT client{};POINT origin{};
    const bool clientReady=SUCCEEDED(swapDescHr)&&swapDesc.OutputWindow&&GetClientRect(swapDesc.OutputWindow,&client)&&ClientToScreen(swapDesc.OutputWindow,&origin)&&client.right>client.left&&client.bottom>client.top;
    if(clientReady){OffsetRect(&client,origin.x,origin.y);g_b174GtaWindow=swapDesc.OutputWindow;AcquireSRWLockExclusive(&g_b174WindowLock);g_b174GtaClientRect=client;ReleaseSRWLockExclusive(&g_b174WindowLock);}
    out<<"B174c: GTA_CLIENT_RECT hwnd="<<(swapDesc.OutputWindow?1:0)<<", get_desc_hr="<<(long)swapDescHr<<", ready="<<(clientReady?1:0)<<", rect={"<<client.left<<","<<client.top<<","<<(client.right-client.left)<<","<<(client.bottom-client.top)<<"}, activation_start_recorded=1\n";out.flush();
    const bool exactClient=clientReady&&(client.right-client.left)==1920&&(client.bottom-client.top)==1080;
    if(!exactClient){out<<"B175n: PREWARM_REJECTED reason=gta_client_not_1920x1080, observed="<<(client.right-client.left)<<"x"<<(client.bottom-client.top)<<", fail_closed=1\n";out.flush();return E_INVALIDARG;}
    ComPtr<ID3D11Device> nativeDevice;context->GetDevice(&nativeDevice);
    const UINT nativeFlags=nativeDevice?nativeDevice->GetCreationFlags():D3D11_CREATE_DEVICE_SINGLETHREADED;
    if(nativeFlags&D3D11_CREATE_DEVICE_SINGLETHREADED){out<<"B175n: PREWARM_REJECTED reason=native_d3d11_device_is_single_threaded, asynchronous_xess_initialization_unsafe=1, fail_closed=1\n";out.flush();return E_FAIL;}
    out<<"B175n: D3D11_THREADING_GATE single_threaded=0, async_device_resource_creation=1, live_immediate_context_calls_present_thread_only=1\n";out.flush();
    ComPtr<ID3D11Texture2D> backBuffer;hr=swap->GetBuffer(0,IID_PPV_ARGS(&backBuffer));
    out<<"B143: stage=GetRealBackBuffer, obtained="<<(backBuffer?1:0)<<", hr="<<(long)hr<<"\n";out.flush();
    if(FAILED(hr)||!backBuffer)return FAILED(hr)?hr:E_FAIL;
    D3D11_TEXTURE2D_DESC source{};backBuffer->GetDesc(&source);const unsigned bpp=bytes_per_pixel(source.Format);
    out<<"B143: real_backbuffer_desc="<<source.Width<<"x"<<source.Height<<", format="<<(unsigned)source.Format<<", samples="<<source.SampleDesc.Count<<", bpp="<<bpp<<"\n";out.flush();
    const bool requiredNativeSize=source.Width==1920&&source.Height==1080;
    out<<"B175n: GTA_SWAPCHAIN_BACKBUFFER_GATE observed="<<source.Width<<"x"<<source.Height<<", required=1920x1080, internal_xess_source_required=dynamic_RGBA16F_frame_scaling_independent, exact_match="<<(requiredNativeSize?1:0)<<", prewarm_enabled="<<(requiredNativeSize?1:0)<<"\n";out.flush();
    if(!requiredNativeSize){out<<"B175n: PREWARM_REJECTED reason=gta_swapchain_backbuffer_dimension_mismatch, observed="<<source.Width<<"x"<<source.Height<<", required=1920x1080, xess_initialization_started=0, xefg_initialization_started=0, proxy_created=0\n";out.flush();return E_INVALIDARG;}
    if(!bpp||source.SampleDesc.Count!=1)return E_NOTIMPL;
    out<<"B238: XESS_INIT_DEFERRED_UNTIL_NATIVE_SOURCE_DISCOVERY frame_scaling_dependency=0, hardcoded_960x540_removed=1\n";out.flush();
    out<<"B175n: INTERNAL_COLOR_PREWARM_DEFERRED allocation_after_two_frame_identity_discovery=1, present_thread_allocation=0, backbuffer_copy=0\n";out.flush();
    g_b171FirstColorHash=1;
    out<<"B175n: CORE_PREPARATION_READY_FOR_DISCOVERY xess_ready=0, xess_init_deferred=1, bridge_core_ready=1, xefg_initialization_started=0, private_proxy_created=0, color_pool_created=0, frame_ring_created=0, dependent_prewarm_deferred=1\n";out.flush();
    return S_OK;
}
static std::atomic<bool> g_b175AsyncPrewarmStarted{false};
static std::atomic<unsigned> g_b175bCorePreparationState{0};
extern "C" __declspec(dllexport) unsigned WINAPI B175nCorePreparationState(){return g_b175bCorePreparationState.load(std::memory_order_acquire);}
extern "C" __declspec(dllexport) HRESULT WINAPI B143CaptureBackBuffer(ID3D11DeviceContext* context,IDXGISwapChain* swap,const char* path)
{
    if(!context||!swap||!path)return E_INVALIDARG;
    ComPtr<ID3D11Texture2D> backBuffer;HRESULT hr=swap->GetBuffer(0,IID_PPV_ARGS(&backBuffer));if(FAILED(hr)||!backBuffer)return FAILED(hr)?hr:E_FAIL;
    D3D11_TEXTURE2D_DESC source{};backBuffer->GetDesc(&source);
    if(source.Width!=1920||source.Height!=1080){std::ofstream out(path,std::ios::app);out<<"B175n: PREWARM_REJECTED reason=gta_swapchain_backbuffer_dimension_mismatch, observed="<<source.Width<<"x"<<source.Height<<", required=1920x1080, xess_initialization_started=0, xefg_initialization_started=0, proxy_created=0\n";out.flush();return E_INVALIDARG;}
    if(g_b175AsyncPrewarmStarted.exchange(true,std::memory_order_acq_rel))return S_FALSE;
    ComPtr<ID3D11DeviceContext> retainedContext=context;ComPtr<IDXGISwapChain> retainedSwap=swap;std::string retainedPath=path;
    std::thread([retainedContext,retainedSwap,retainedPath](){
        const HRESULT result=b175_async_prewarm(retainedContext.Get(),retainedSwap.Get(),retainedPath.c_str());
        g_b175bCorePreparationState.store(SUCCEEDED(result)?1u:2u,std::memory_order_release);
        if(FAILED(result)){g_b175PipelineReady.store(false,std::memory_order_release);g_b175PipelineFailed.store(true,std::memory_order_release);std::ofstream out(retainedPath,std::ios::app);out<<"B175n: ASYNC_PIPELINE_PREWARM_FAILED hr="<<(long)result<<"\n";out.flush();}
    }).detach();
    std::ofstream out(path,std::ios::app);out<<"B175n: ASYNC_PIPELINE_PREWARM_DISPATCHED present_thread_blocking_initialization=0, gta_swapchain=1920x1080, internal_source_discovery=dynamic_RGBA16F_frame_scaling_independent, xess_output="<<g_b232OutputWidth<<"x"<<g_b232OutputHeight<<"_RGBA16F\n";out.flush();
    return S_OK;
}
extern "C" __declspec(dllexport) BOOL WINAPI B203GetMeasuredFps(float* sourceFps,float* xefgFps){if(!sourceFps||!xefgFps)return FALSE;*sourceFps=g_b191SourceFps100.load(std::memory_order_acquire)/100.0f;*xefgFps=g_b191FgFps100.load(std::memory_order_acquire)/100.0f;return (*sourceFps>0.0f||*xefgFps>0.0f)?TRUE:FALSE;}
extern "C" __declspec(dllexport) unsigned WINAPI GTAXeSSGetCaptureDurationMs(){return 0u;}
extern "C" __declspec(dllexport) BOOL WINAPI GTAXeSSUseDirectPresentValidation(){return FALSE;}
extern "C" __declspec(dllexport) BOOL WINAPI GTAXeSSUseLeanHook(){return FALSE;}
extern "C" __declspec(dllexport) BOOL WINAPI B126UseLeanHook(){return TRUE;}
