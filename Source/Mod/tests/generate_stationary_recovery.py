from pathlib import Path
old=Path('backups/pre-B555-source/CameraRuntime.cpp').read_text();new=Path('ProtectedFixes/CameraRuntime.cpp').read_text()
start=old.index('    if(!b429StationaryRelockUsed&&b343FalseStaticFrames>=120){');end=old.index('\n   }\n   else if(b343SelectedActuallyMoving)',start)
oldblock=old[start:end]
start=new.index('    // B555: a validated camera');end=new.index('\n   }\n   else if(b343SelectedActuallyMoving)',start);newblock=new[start:end]
start=new.index(' if(g_b430ResponsiveReacquireActive.load');end=new.index(' // B300: lock a stable camera',start);fallback=new[start:end]
base=r'''#include <windows.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
struct ID3D11Resource{};static ID3D11Resource resource;
static std::atomic<bool>g_b300DirectMode{true},g_b430ResponsiveReacquireActive{false};
static std::atomic<ID3D11Resource*>g_b301DirectCbResource{&resource},g_b430RejectedStaticCameraResource{nullptr};
static std::atomic<int>g_b300DirectCbIndex{1};
static SRWLOCK g_b300DirectWriteLock=SRWLOCK_INIT,g_b489AuthoritativeLock=SRWLOCK_INIT;
static unsigned g_b300DirectWriteHead=0,g_b300DirectWriteCount=0;
struct Snapshot{int dummy{};};static Snapshot g_b445LatestSelectedSnapshot,g_b489Authoritative;
static ID3D11Resource*g_b445LatestSelectedResource=nullptr;static unsigned long long g_b445LatestSelectedSerial=0;
static unsigned long long b441ReacquireStartSource=0;static ID3D11Resource*b441FallbackResource=nullptr;static int b441FallbackCbIndex=-1;
static unsigned restored=0;static void restore(void*,void*){++restored;}
struct Transport{void(*drawTransport1)(void*,void*)=restore;void(*drawTransport3)(void*,void*)=restore;};static Transport cameraTransport;
static void reset(){g_b300DirectMode=true;g_b301DirectCbResource=&resource;g_b430ResponsiveReacquireActive=false;g_b300DirectCbIndex=1;g_b430RejectedStaticCameraResource=nullptr;b441FallbackResource=nullptr;b441ReacquireStartSource=0;restored=0;}
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
'''
def method(name,body,locals):return f'void {name}(unsigned sourceId,unsigned b343FalseStaticFrames){{const char*path="stationary-regression.log";void*context=nullptr;{locals}\n{body}\n}}\n'
s=base+method('oldStationary',oldblock,'static bool b429StationaryRelockUsed=false;bool b344UnsafeCameraActive=false,b288HistoryReset=false;')+method('newStationary',newblock,'')+method('fallback',fallback,'')+r'''
int main(){try{reset();for(unsigned n=1;n<=120;++n)oldStationary(n,n);check(!g_b300DirectMode&&g_b301DirectCbResource==nullptr,"old branch reproduces stationary lock loss");
 reset();for(unsigned n=1;n<=10000;++n)newStationary(n,n);check(g_b300DirectMode&&g_b301DirectCbResource==&resource&&!g_b430ResponsiveReacquireActive&&restored==0,"validated stationary lock remains selected");
 g_b300DirectMode=false;g_b301DirectCbResource=nullptr;g_b430ResponsiveReacquireActive=true;g_b430RejectedStaticCameraResource=&resource;b441FallbackResource=&resource;b441FallbackCbIndex=1;b441ReacquireStartSource=100;
 fallback(105,0);check(!g_b300DirectMode&&restored==0,"recovery search is not prematurely restored");fallback(106,0);check(g_b300DirectMode&&g_b301DirectCbResource==&resource&&!g_b430ResponsiveReacquireActive&&restored==1,"search ends at six frames and restores original resource");fallback(107,0);check(restored==1,"fallback restoration is one shot");
 std::cout<<"PASS: old stationary lock loss reproduced;10000 stationary frames retain lock; six-frame recovery restores once\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
'''
Path('tests/StationaryRecovery.generated.cpp').write_text(s)
p=Path('tests/SourcePacer.vcxproj').read_text().replace('SourcePacer','StationaryRecovery').replace('25C000000550','25C000000555').replace('Include="StationaryRecovery.cpp"','Include="StationaryRecovery.generated.cpp"');Path('tests/StationaryRecovery.vcxproj').write_text(p)
