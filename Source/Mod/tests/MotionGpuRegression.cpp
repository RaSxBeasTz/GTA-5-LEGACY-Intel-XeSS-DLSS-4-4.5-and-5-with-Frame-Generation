#include "../ProtectedFixes/TemporalRuntime.h"
#include "../ProtectedFixes/TemporalContinuity.h"
#include "../B25DepthHelper/ReducedMotionInputs.h"
#include "../B25DepthHelper/RenderAheadScope.h"
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <iostream>
#include <stdexcept>
#include <vector>
static void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
static void hr(HRESULT h,const char*s){check(SUCCEEDED(h),s);}
static float half(unsigned short x){unsigned e=(x>>10)&31,m=x&1023;return (x&32768?-1.f:1.f)*(e?std::ldexp(1.f+m/1024.f,(int)e-15):std::ldexp((float)m,-24));}
static void projection(float*p){std::memset(p,0,64);p[0]=p[5]=1;p[10]=100.f/99.9f;p[11]=1;p[14]=-10.f/99.9f;}
static void inverse(const float*m,float*out){double d[16]{},z[16]{};for(unsigned i=0;i<16;++i)d[i]=m[i];check(b287_inverse4(d,z),"invert perspective camera");for(unsigned i=0;i<16;++i)out[i]=(float)z[i];}
static void multiply(const float*a,const float*b,float*out){double d[16]{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)for(unsigned k=0;k<4;++k)d[r*4+c]+=(double)a[r*4+k]*b[k*4+c];for(unsigned i=0;i<16;++i)out[i]=(float)d[i];}
int main(){try{
 ComPtr<IDXGIFactory1> factory;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");ComPtr<IDXGIAdapter1> arc;
 for(unsigned i=0;;++i){ComPtr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x8086&&std::wcsstr(d.Description,L"Arc")){arc=a;std::wcout<<L"Adapter: "<<d.Description<<L"\n";break;}}
 ComPtr<ID3D11Device>dev;ComPtr<ID3D11DeviceContext>ctx;hr(D3D11CreateDevice(arc.Get(),arc?D3D_DRIVER_TYPE_UNKNOWN:D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,nullptr,&ctx),"device");
 {ComPtr<IDXGIDevice1>nativeDxgi;hr(dev.As(&nativeDxgi),"native DXGI latency interface");UINT original{},actual{};hr(nativeDxgi->GetMaximumFrameLatency(&original),"read original native render ahead");RenderAheadScope<IDXGIDevice1>scope;hr(scope.apply(nativeDxgi.Get(),true),"apply native queue one");hr(nativeDxgi->GetMaximumFrameLatency(&actual),"read applied native render ahead");check(actual==1,"driver acknowledges one native queued frame");hr(scope.apply(nativeDxgi.Get(),true,2),"apply native queue two");hr(nativeDxgi->GetMaximumFrameLatency(&actual),"read two-frame queue");check(actual==2&&scope.original==original,"two-frame driver change retains original");hr(scope.apply(nativeDxgi.Get(),true,1),"repeat native queue one");hr(scope.apply(nativeDxgi.Get(),false),"restore native render ahead");hr(nativeDxgi->GetMaximumFrameLatency(&actual),"verify restored native render ahead");check(actual==original,"native queue original restored exactly");std::cout<<"PASS: real native DXGI render-ahead scope original="<<original<<" tested=1/2/1 restored="<<actual<<"\n";}
 // Validate the production D3D12/On12 half-size allocation, including odd dimensions.
 {ComPtr<ID3D12Device>d12;hr(D3D12CreateDevice(arc.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d12)),"reduced input D3D12 device");
  D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue>queue;hr(d12->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"reduced input queue");
  IUnknown*queues[]{queue.Get()};ComPtr<ID3D11Device>wrappedDevice;ComPtr<ID3D11DeviceContext>wrappedContext;
  hr(D3D11On12CreateDevice(d12.Get(),0,nullptr,0,queues,1,0,&wrappedDevice,&wrappedContext,nullptr),"reduced input On12 device");ComPtr<ID3D11On12Device>on12;hr(wrappedDevice.As(&on12),"On12 interface");
  gta::ReducedMotionInputs inputs;hr(inputs.create(d12.Get(),on12.Get(),wrappedDevice.Get(),13,9),"production reduced input allocation");
  check(inputs.width==7&&inputs.height==5&&inputs.motion->GetDesc().Width==7&&inputs.depth->GetDesc().Height==5,"odd dimensions round up and motion/depth agree");
 }
 // B548 native SDR transport: preserve two distinct full frames exactly,
 // including alpha and alternating edges, across the shared texture path.
 {
  D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=8;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;d.MiscFlags=D3D11_RESOURCE_MISC_SHARED;
  ComPtr<ID3D11Texture2D> source,shared,readback;hr(dev->CreateTexture2D(&d,nullptr,&shared),"shared SDR transport");d.MiscFlags=0;hr(dev->CreateTexture2D(&d,nullptr,&source),"native SDR source");d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;hr(dev->CreateTexture2D(&d,nullptr,&readback),"SDR readback");
  for(unsigned frame=0;frame<2;++frame){unsigned pixels[64];for(unsigned i=0;i<64;++i)pixels[i]=(frame?0x1f0055aa:0xe0ffaa55)^(i*0x01010101u);ctx->UpdateSubresource(source.Get(),0,nullptr,pixels,32,0);ctx->CopyResource(shared.Get(),source.Get());ctx->CopyResource(readback.Get(),shared.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(ctx->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"map SDR copy");bool exact=true;for(unsigned y=0;y<8;++y)exact&=std::memcmp((char*)mapped.pData+y*mapped.RowPitch,pixels+y*8,32)==0;ctx->Unmap(readback.Get(),0);check(exact,"native SDR colour must be bit exact without temporal trails");}
  std::cout<<"PASS: B548 shared SDR transport preserves both frames bit exactly\n";
 }
 float p[16]{},prevView[16]{},currentView[16]{},prevVp[16]{},curVp[16]{};projection(p);for(unsigned i=0;i<16;i+=5)prevView[i]=currentView[i]=1;
 prevView[12]=-1000;currentView[12]=-1000.125f;multiply(prevView,p,prevVp);multiply(currentView,p,curVp);
 B173GpuConstants constants{};std::memcpy(constants.previousViewProjection,prevVp,64);inverse(curVp,constants.currentInverseViewProjection);constants.width=constants.height=8;check(GhostingBuildMotionTransform(constants,curVp),"double precision motion transform");
 // B547 cut probes are reversed-depth only; forward-depth shader math remains tested below.
 // Noncommuting row-vector view and perspective projection expose B541's error.
 float rotation[16]{},yawView[16]{},yawVp[16]{},yawInverse[16]{};for(unsigned i=0;i<16;i+=5)rotation[i]=1;rotation[0]=rotation[10]=std::cos(.001f);rotation[2]=-std::sin(.001f);rotation[8]=std::sin(.001f);
 multiply(prevView,rotation,yawView);multiply(yawView,p,yawVp);inverse(yawVp,yawInverse);// Check continuity with the actual reversed-depth projection below.
 D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(constants);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA cbd{&constants,0,0};ComPtr<ID3D11Buffer> buffer;hr(dev->CreateBuffer(&cb,&cbd,&buffer),"constants");
 D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=8;desc.MipLevels=desc.ArraySize=1;desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R16G16_FLOAT;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;ComPtr<ID3D11Texture2D>motion,readback;hr(dev->CreateTexture2D(&desc,nullptr,&motion),"motion");ComPtr<ID3D11UnorderedAccessView>mu;hr(dev->CreateUnorderedAccessView(motion.Get(),nullptr,&mu),"motion UAV");desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;hr(dev->CreateTexture2D(&desc,nullptr,&readback),"readback");
 desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;desc.Format=DXGI_FORMAT_R32_FLOAT;ComPtr<ID3D11Texture2D>depthOut;ComPtr<ID3D11UnorderedAccessView>du;hr(dev->CreateTexture2D(&desc,nullptr,&depthOut),"depth output");hr(dev->CreateUnorderedAccessView(depthOut.Get(),nullptr,&du),"depth UAV");
 float depth[64]{};unsigned packed[64]{};const float z=p[10]+p[14]/10.f;const unsigned z24=(unsigned)std::round(z*16777215.f);for(unsigned i=0;i<64;++i){depth[i]=z;packed[i]=z24|0xff000000u;}
 auto run=[&](const char*source,bool packedInput,float expectedX=.05f,const float* expectedPixels=nullptr,bool fullSizeInput=false){ComPtr<ID3DBlob>code,error;hr(D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error),"compile actual shader");ComPtr<ID3D11ComputeShader>shader;hr(dev->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader),"shader");
  D3D11_TEXTURE2D_DESC input=desc;input.BindFlags=D3D11_BIND_SHADER_RESOURCE;input.Format=packedInput?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_R32_FLOAT;unsigned expanded[256]{};if(fullSizeInput){input.Width=input.Height=16;for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)expanded[y*16+x]=(x%2&&y%2)?packed[(y/2)*8+x/2]:0;}
  D3D11_SUBRESOURCE_DATA init{fullSizeInput?(void*)expanded:packedInput?(void*)packed:(void*)depth,fullSizeInput?64u:32u,0};ComPtr<ID3D11Texture2D>texture;hr(dev->CreateTexture2D(&input,&init,&texture),"depth input");ComPtr<ID3D11ShaderResourceView>srv;hr(dev->CreateShaderResourceView(texture.Get(),nullptr,&srv),"depth SRV");
  ID3D11ShaderResourceView*srvs[]{srv.Get()};ID3D11UnorderedAccessView*uavs[]{mu.Get(),du.Get()};ID3D11Buffer*cbs[]{buffer.Get()};ctx->CSSetShader(shader.Get(),nullptr,0);ctx->CSSetConstantBuffers(0,1,cbs);ctx->CSSetShaderResources(0,1,srvs);ctx->CSSetUnorderedAccessViews(0,2,uavs,nullptr);ctx->Dispatch(1,1,1);ID3D11UnorderedAccessView*nullUav[2]{};ctx->CSSetUnorderedAccessViews(0,2,nullUav,nullptr);ID3D11ShaderResourceView*nullSrv{};ctx->CSSetShaderResources(0,1,&nullSrv);ctx->CopyResource(readback.Get(),motion.Get());D3D11_MAPPED_SUBRESOURCE map{};hr(ctx->Map(readback.Get(),0,D3D11_MAP_READ,0,&map),"read GPU output");unsigned correct=0;float minX=1e9f,maxX=-1e9f,maxY=0;for(unsigned y=0;y<8;++y){auto*row=(unsigned short*)((unsigned char*)map.pData+y*map.RowPitch);for(unsigned x=0;x<8;++x){float mx=half(row[x*2]),my=half(row[x*2+1]);minX=(std::min)(minX,mx);maxX=(std::max)(maxX,mx);maxY=(std::max)(maxY,std::fabs(my));if(std::fabs(mx-(expectedPixels?expectedPixels[y*8+x]:expectedX))<.003f&&std::fabs(my)<.003f)++correct;}}ctx->Unmap(readback.Get(),0);std::cout<<"GPU samples packed="<<packedInput<<", x={"<<minX<<","<<maxX<<"}, max_abs_y="<<maxY<<", correct="<<correct<<"\n";return correct;
 };
 check(run(GhostingMotionShader1(),true)==64,"packed motion shader1: all pixels match analytic +0.05px");check(run(GhostingMotionShader2(),true)==64,"packed motion shader2: all pixels match analytic +0.05px");check(run(GhostingMotionShader3(),false)==64,"native depth shader3: all pixels match analytic +0.05px");
 check(run(gta::ReducedMotionShader,true,.05f,nullptr,true)==64,"half-resolution shader: analytic input-grid motion from 16x16 depth");
 std::string old=GhostingMotionShader3();auto replace=[&](const char*a,const char*b){auto pos=old.find(a);check(pos!=std::string::npos,"negative control source");old.replace(pos,std::strlen(a),b);};replace("mul(world,clipToPrevious)","mul(previousViewProjection,mul(currentInverseViewProjection,clip))");check(run(old.c_str(),false)<64,"old shader must fail the same GPU regression");

 // Verify stationary and reversed motion with the same production GPU shaders.
 std::memcpy(constants.previousViewProjection,curVp,64);check(GhostingBuildMotionTransform(constants,curVp),"stationary transform");ctx->UpdateSubresource(buffer.Get(),0,nullptr,&constants,0,0);
 check(run(GhostingMotionShader1(),true,0)==64&&run(GhostingMotionShader2(),true,0)==64&&run(GhostingMotionShader3(),false,0)==64,"stationary camera produces zero motion");
 prevView[12]=-1000.25f;multiply(prevView,p,prevVp);std::memcpy(constants.previousViewProjection,prevVp,64);check(GhostingBuildMotionTransform(constants,curVp),"reverse transform");ctx->UpdateSubresource(buffer.Get(),0,nullptr,&constants,0,0);
 check(run(gta::ReducedMotionShader,true,-.05f,nullptr,true)==64,"half resolution reverse motion sign");
 check(run(GhostingMotionShader1(),true,-.05f)==64&&run(GhostingMotionShader2(),true,-.05f)==64&&run(GhostingMotionShader3(),false,-.05f)==64,"reverse motion has correct sign");

 // GTA log matrices use reversed depth: A < 0 and B > 0. Exercise this
 // actual convention, not just the forward-depth perspective test above.
 p[10]=-.1f/99.9f;p[14]=10.f/99.9f;prevView[12]=-1000;currentView[12]=-1000.125f;multiply(prevView,p,prevVp);multiply(currentView,p,curVp);std::memcpy(constants.previousViewProjection,prevVp,64);inverse(curVp,constants.currentInverseViewProjection);check(GhostingBuildMotionTransform(constants,curVp),"reversed-depth transform");check(!ReprojectionCut(prevVp,constants.currentInverseViewProjection),"reversed-depth small pan is continuous");ctx->UpdateSubresource(buffer.Get(),0,nullptr,&constants,0,0);
 multiply(prevView,rotation,yawView);multiply(yawView,p,yawVp);inverse(yawVp,yawInverse);check(!ReprojectionCut(prevVp,yawInverse),"reversed-depth small yaw preserves history");
 const float reversedZ=p[10]+p[14]/10.f;const unsigned reversed24=(unsigned)std::round(reversedZ*16777215.f);for(unsigned i=0;i<64;++i){depth[i]=reversedZ;packed[i]=reversed24|0xff000000u;}
 check(run(GhostingMotionShader1(),true)==64&&run(GhostingMotionShader2(),true)==64&&run(GhostingMotionShader3(),false)==64,"GTA reversed-depth inputs preserve correct motion");
 // Depth discontinuity: foreground occupies one pixel. XeFG requires its
 // velocity in the surrounding 3x3, including clamped boundaries, while the
 // original depth resource must remain undilated.
 for(unsigned foreground:{0u,27u,63u}){
  const float nearZ=p[10]+p[14]/5.f;const unsigned near24=(unsigned)std::round(nearZ*16777215.f);
  for(unsigned i=0;i<64;++i)packed[i]=reversed24|0xff000000u;packed[foreground]=near24|0xff000000u;
  float expected[64];for(unsigned i=0;i<64;++i){int dx=int(i%8)-int(foreground%8),dy=int(i/8)-int(foreground/8);expected[i]=(std::abs(dx)<=1&&std::abs(dy)<=1)?.1f:.05f;}
  check(run(GhostingMotionShader1(),true,.05f,expected)==64,"XeFG foreground 3x3 dilation including image boundaries");
  check(run(GhostingMotionShader2(),true,.05f,expected)<64,"undilated shader negative control at depth edge");
  run(GhostingMotionShader1(),true,.05f,expected);
  D3D11_TEXTURE2D_DESC dd{};depthOut->GetDesc(&dd);dd.Usage=D3D11_USAGE_STAGING;dd.BindFlags=0;dd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> originalDepth;hr(dev->CreateTexture2D(&dd,nullptr,&originalDepth),"depth check staging");ctx->CopyResource(originalDepth.Get(),depthOut.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(ctx->Map(originalDepth.Get(),0,D3D11_MAP_READ,0,&mapped),"depth check read");bool exact=true;for(unsigned y=0;y<8;++y){auto row=(float*)((char*)mapped.pData+y*mapped.RowPitch);for(unsigned x=0;x<8;++x)exact&=std::fabs(row[x]-float(packed[y*8+x]&0xffffff)/16777215.f)<1e-7f;}ctx->Unmap(originalDepth.Get(),0);check(exact,"motion dilation must preserve original depth");
 }
 // Low-resolution path samples raw depth once, without pre-dilation.
 float lowExpected[64];for(unsigned i=0;i<64;++i)lowExpected[i]=(i==63)?.1f:.05f;
 check(run(gta::ReducedMotionShader,true,.05f,lowExpected,true)==64,"half input foreground stays undilated before SDK upsampling");
 // Dilation must never invent motion with a stationary camera at depth edges.
 std::memcpy(constants.previousViewProjection,curVp,64);check(GhostingBuildMotionTransform(constants,curVp),"stationary edge transform");ctx->UpdateSubresource(buffer.Get(),0,nullptr,&constants,0,0);check(run(GhostingMotionShader1(),true,0)==64,"stationary depth edges remain zero motion");
 check(run(gta::ReducedMotionShader,true,0,nullptr,true)==64,"half input stationary edges remain zero motion");
 std::cout<<"PASS: B549 nearest-foreground motion, border clamping, undilated depth, stationary edge, old-path negative control\n";
 std::cout<<"PASS: GTA reversed depth, large-origin perspective motion, small yaw continuity, all three GPU shader outputs, old shader negative control\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
