#define GHOSTING_RUNTIME_TEST 1
#include "../ProtectedFixes/TemporalRuntime.h"
#include "../ProtectedFixes/CameraCapturePolicy.h"
#include "../ProtectedFixes/CameraOriginPolicy.h"
#include "../B25DepthHelper/WriteDiscardMirror.h"
#include <iostream>
#include <stdexcept>
static void check(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
int main(){try{
 unsigned char gpu[256],pattern[256];for(unsigned i=0;i<256;++i){gpu[i]=0xCD;pattern[i]=(unsigned char)i;}
 int resource=0,context=0,other=0;WriteDiscardMirror mirror;void* cpu=mirror.begin(gpu,&resource,&context);check(cpu&&cpu!=gpu,"CPU mirror is separate");
 std::memcpy(cpu,pattern,256);for(auto v:gpu)check(v==0xCD,"no mapped-memory access before Unmap");
 check(!mirror.commit(&other,&context,0),"unrelated Unmap does not flush");check(!mirror.commit(&resource,&context,1),"wrong subresource does not flush");check(!mirror.begin(gpu,&resource,&context),"nested map does not overwrite pending upload");
 check(mirror.commit(&resource,&context,0),"matching Unmap flushes");check(std::memcmp(gpu,pattern,256)==0,"all game bytes preserved");check(!mirror.commit(&resource,&context,0),"upload occurs once");check(mirror.begin(gpu,&resource,&context)!=nullptr,"next Map reuses mirror");mirror.commit(&resource,&context,0);
 auto*r=reinterpret_cast<ID3D11Resource*>(0x1000);PendingMap pending{};pending.target=true;pending.resource=r;pending.data=pattern;pending.byte_count=256;
 for(unsigned n=0;n<1200;++n){pattern[0]=(unsigned char)n;check(GhostingAdmitSelectedWrite(r,pending),"later camera writes must not be discarded");}

 // The full capture path keeps late camera phases and coalesces repeated
 // object writes under one view so they cannot evict all earlier view phases.
 g_b301DirectCbResource.store(r);g_b300DirectWriteCount=0;g_b300DirectWriteHead=0;
 alignas(64) unsigned char raw[256]{};pending.data=raw;
 for(unsigned n=1;n<=1200;++n){std::memcpy(raw,&n,sizeof(n));check(GhostingAdmitSelectedWrite(r,pending),"all selected updates admitted");b309_capture_direct_camera_write_if_needed(r,raw,256);}
 check(g_b300DirectWriteCount==1,"same-view object writes coalesce");
 raw[192]=1;b309_capture_direct_camera_write_if_needed(r,raw,256);
 check(g_b300DirectWriteCount==2,"late view phase preserved");
 check(std::memcmp(g_b445LatestSelectedSnapshot.bytes,raw,256)==0,"latest full tuple handed off");
 pending.byte_count=64;check(!GhostingAdmitSelectedWrite(r,pending),"short snapshots rejected");
 check(PreferCoherentCamera(true,0,0,0,0,1,0.2,0.01,0.3,0.22,900),"moving camera beats stale zero-score candidate");
 check(!PreferCoherentCamera(true,0.2,0.01,0.3,0.22,900,0,0,0,0,901),"static phase cannot replace moving camera");
 check(!PreferCoherentCamera(true,0,0,0,0,1,0.2,200,0.3,20,900),"incompatible projection does not gain motion preference");
 double origin[16]{},world[16]{},projection[16]{};for(unsigned i=0;i<16;i+=5)origin[i]=world[i]=projection[i]=1;world[12]=249;world[13]=-423;world[14]=485;
 check(CameraOriginTwin(origin,world,projection,projection),"same-frame zero-origin twin filtered");
 check(!CameraOriginTwin(world,origin,projection,projection),"world-positioned camera retained");
 check(!CameraOriginTwin(origin,origin,projection,projection),"real origin camera without translated twin retained");
 world[0]=0.5;check(!CameraOriginTwin(origin,world,projection,projection),"different orientation cannot be classified as twin");world[0]=1;
 double otherProjection[16];std::memcpy(otherProjection,projection,sizeof(projection));otherProjection[0]=2;
 check(!CameraOriginTwin(origin,world,projection,otherProjection),"different projection retained for existing camera validators");
 origin[12]=0.1;check(!CameraOriginTwin(origin,world,projection,projection),"near-origin real camera not rejected");origin[12]=0;
 world[12]=NAN;check(!CameraOriginTwin(origin,world,projection,projection),"invalid twin cannot remove candidate");
 float identity[16]{};for(unsigned i=0;i<16;i+=5)identity[i]=1;
 GhostingTemporalFrame f{};GhostingSetCaptureCameraSafe(false);GhostingValidate(1,identity,identity,identity,identity,"capture-regression.log",f);check(!f.valid,"unsafe held matrix cannot pass math-only validation");
 GhostingSetCaptureCameraSafe(true);GhostingValidate(2,identity,identity,identity,identity,"capture-regression.log",f);check(f.valid&&f.transition,"fresh camera recovery transitions to valid");
 std::cout<<"PASS: write-only GPU upload, exact byte preservation, later camera phases, moving candidate priority, unsafe-camera propagation\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
