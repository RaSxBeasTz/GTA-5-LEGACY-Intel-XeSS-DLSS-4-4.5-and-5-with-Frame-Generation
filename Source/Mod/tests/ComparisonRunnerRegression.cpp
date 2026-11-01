#include <windows.h>
#include <atomic>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "../B25DepthHelper/ComparisonCounters.h"
#include "../B25DepthHelper/RenderAheadScope.h"
static std::atomic<bool>g_comparisonCancel{},g_b277F9PollingDispatch{},g_b206UserEnabled{};static bool g_comparisonInternal{},activeState{},captureOnly{},proxyOnly{},keyHeld{},focused=true;static unsigned latencyMode=0;static bool failLatency=false;static bool reducedMode=false;static bool suppressGeneration=false;static unsigned interpolationCount=3;static bool delayApply=false;static unsigned requestedCount=3;static ULONGLONG now=1000;static unsigned frame{};static ComparisonCounters totals{};static std::vector<std::string>logs;
using b174_active_fn=BOOL(WINAPI*)();static HWND g_b175RealHwnd=(HWND)1;
static BOOL WINAPI fakeActive(){return activeState;}static void WINAPI fakeMode(BOOL v){captureOnly=v!=FALSE;}static void WINAPI fakeCounters(ComparisonCounters*c){*c=totals;}
static ULONGLONG fakeTick(){return now;}static short fakeKey(int){return keyHeld?short(0x8000):0;}static HWND fakeFocus(){return focused?g_b175RealHwnd:nullptr;}static HMODULE fakeModule(const char*){return (HMODULE)1;}
static void WINAPI fakeProfiling(BOOL){}
static void WINAPI fakeProxy(BOOL v){proxyOnly=v!=FALSE;}
static BOOL WINAPI fakeVisible(){return activeState&&!captureOnly;}
static void WINAPI fakeSetCount(unsigned n){requestedCount=n;if(!delayApply)interpolationCount=n;}
static unsigned WINAPI fakeGetCount(){return requestedCount==interpolationCount?interpolationCount:0;}
static void WINAPI fakeSetReduced(BOOL value){requestedCount=3;if(!delayApply)reducedMode=value!=FALSE;}
static BOOL WINAPI fakeGetReduced(){return reducedMode;}
static BOOL WINAPI fakeSetLatency(unsigned value){if(failLatency&&value==2)return FALSE;latencyMode=value;return TRUE;}
static unsigned WINAPI fakeGetLatency(){return latencyMode;}
static BOOL WINAPI fakeRestoreDefault(){return fakeSetLatency(activeState&&g_b206UserEnabled.load()?1u:0u);}
static bool nativeMode=false;
static bool native_present_experiment_set(bool value){if(value&&failLatency)return false;nativeMode=value;return true;}
static bool native_present_experiment_enabled(){return nativeMode;}
static FARPROC fakeProc(HMODULE,const char*s){if(std::string(s)=="B615RestoreNativeDefault")return (FARPROC)fakeRestoreDefault;if(std::string(s)=="B614SetNativeQueue")return (FARPROC)fakeSetLatency;if(std::string(s)=="B614GetNativeQueue")return (FARPROC)fakeGetLatency;if(std::string(s)=="B612SetReducedMotion")return (FARPROC)fakeSetReduced;if(std::string(s)=="B612GetReducedMotion")return (FARPROC)fakeGetReduced;if(std::string(s)=="B609SetInterpolations")return (FARPROC)fakeSetCount;if(std::string(s)=="B609GetInterpolations")return (FARPROC)fakeGetCount;if(std::string(s)=="B608ProxyVisible")return (FARPROC)fakeVisible;if(std::string(s)=="B608SetProxyOnly")return (FARPROC)fakeProxy;if(std::string(s)=="B606SetComparisonActive")return (FARPROC)fakeProfiling;if(std::string(s)=="B606SetCaptureOnly")return (FARPROC)fakeMode;if(std::string(s)=="B606GetComparisonCounters")return (FARPROC)fakeCounters;return (FARPROC)fakeActive;}
static std::string module_directory(){return "redesign/tests/";}static void log(const std::string&s){logs.push_back(s);}static void on_key(DWORD,WORD,BYTE,BOOL,BOOL,BOOL,BOOL){g_b206UserEnabled=!g_b206UserEnabled;activeState=g_b206UserEnabled;}
#define GetTickCount64 fakeTick
#define GetAsyncKeyState fakeKey
#define GetForegroundWindow fakeFocus
#define GetModuleHandleA fakeModule
#define GetProcAddress fakeProc
#include "../ScriptHookProbe/ComparisonRunner.h"
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}static bool contains(const char*s){for(auto&x:logs)if(x.find(s)!=std::string::npos)return true;return false;}
static void tick(){unsigned dt=!activeState?8:captureOnly?10:proxyOnly?8:20;now+=dt;++frame;if(activeState){++totals.depthSamples;totals.depthNs+=20000;++totals.colourSamples;totals.colourNs+=40000;if(!captureOnly){totals.outputFrames+=proxyOnly?1:interpolationCount+1;totals.generatedFrames+=(proxyOnly||suppressGeneration)?0:interpolationCount;if(suppressGeneration)++totals.resetFrames;totals.fgQueueNs+=proxyOnly?1000000:10000000;++totals.fgQueueSamples;}}comparison_tick(frame);}
static void f8(){keyHeld=true;tick();keyHeld=false;tick();}
struct FakeLatencyDevice{UINT value=3;bool failGet=false,failSet=false;unsigned changes=0;HRESULT GetMaximumFrameLatency(UINT*out){if(failGet)return E_FAIL;*out=value;return S_OK;}HRESULT SetMaximumFrameLatency(UINT n){if(failSet)return E_FAIL;value=n;++changes;return S_OK;}};
int main(){try{
 {FakeLatencyDevice device;RenderAheadScope<FakeLatencyDevice>scope;check(SUCCEEDED(scope.apply(&device,true))&&device.value==1,"native one-frame queue applies");check(scope.original==3,"exact original native queue saved");scope.apply(&device,true,2);check(device.value==2&&scope.original==3,"two-frame queue retains original setting");scope.apply(&device,true,1);check(device.value==1&&scope.original==3,"one-frame repeat retains original setting");scope.apply(&device,true);check(scope.original==3,"duplicate enable preserves original");check(SUCCEEDED(scope.apply(&device,false))&&device.value==3&&!scope.active,"native queue restored exactly");
 device.value=5;scope.apply(&device,true);device.failSet=true;check(FAILED(scope.apply(&device,false))&&scope.active,"failed restore retains state for retry");device.failSet=false;check(SUCCEEDED(scope.apply(&device,false))&&device.value==5,"restore retry preserves nondefault setting");
 device.value=1;scope.apply(&device,true);scope.apply(&device,false);check(device.value==1,"already-one original setting preserved");device.failGet=true;check(FAILED(scope.apply(&device,true))&&!scope.active,"query failure makes no changes");}

 f8();check(contains("activate_with_f9_first"),"reject before activation");logs.clear();g_b206UserEnabled=true;activeState=true;
 delayApply=true;interpolationCount=1;f8();check(activeState&&requestedCount==3,"x4 requested without toggling pipeline");
 for(unsigned n=0;n<500;++n)tick();check(!contains("COMPARISON_PHASE"),"wait for worker acknowledgement before settling");
 delayApply=false;interpolationCount=requestedCount;
 for(unsigned n=0;n<10000&&!contains("reason=completed");++n)tick();
 check(contains("reason=completed")&&activeState&&!captureOnly&&!proxyOnly&&interpolationCount==3,"completion restores x4");
 unsigned results=0;for(auto&s:logs)results+=s.find("COMPARISON_RESULT")!=std::string::npos;check(results==3&&!reducedMode&&latencyMode==1,"exactly three measured phases and full inputs restored");
 check(contains("phase=x4_native_normal")&&contains("phase=x4_native_test_only")&&contains("phase=x4_native_normal_repeat")&&!nativeMode,"all presentation modes measured and normal restored");
 logs.clear();f8();for(unsigned n=0;n<10000&&!nativeMode;++n)tick();check(nativeMode&&latencyMode==1,"test-only phase retains queue one");f8();check(latencyMode==1&&!reducedMode&&!nativeMode,"F8 restores normal presentation and full inputs");check(activeState&&interpolationCount==3&&contains("cancelled_by_f8"),"F8 cancellation restores x4");
 logs.clear();f8();for(unsigned n=0;n<10000&&!nativeMode;++n)tick();check(nativeMode,"fallback phase reached");nativeMode=false;tick();check(contains("native_present_experiment_fell_back")&&!nativeMode&&latencyMode==1,"automatic fallback aborts invalid comparison");
 logs.clear();f8();for(unsigned n=0;n<10000&&!nativeMode;++n)tick();check(nativeMode,"F9 cancellation tested during experiment");g_comparisonCancel=true;g_b206UserEnabled=false;activeState=false;tick();check(!activeState&&requestedCount==3&&!nativeMode&&latencyMode==0&&contains("cancelled_by_f9"),"F9 cancellation restores normal presentation and original native queue");
 logs.clear();g_b206UserEnabled=true;activeState=true;f8();for(unsigned n=0;n<10000&&!nativeMode;++n)tick();check(nativeMode,"focus cancellation tested during experiment");focused=false;tick();focused=true;check(activeState&&interpolationCount==3&&!nativeMode&&latencyMode==1&&contains("focus_lost"),"focus loss restores normal presentation and x4");
 logs.clear();delayApply=true;interpolationCount=1;f8();now+=21000;tick();delayApply=false;check(contains("pipeline_transition_timeout")&&requestedCount==3&&latencyMode==1,"transition timeout restores requested x4");
 logs.clear();interpolationCount=requestedCount=3;suppressGeneration=true;f8();for(unsigned n=0;n<10000&&!contains("invalid_sample_history_resets_or_missing_generation");++n)tick();suppressGeneration=false;check(contains("invalid_sample_history_resets_or_missing_generation")&&requestedCount==3&&latencyMode==1,"reset dominated sample rejected and x4 restored");
 logs.clear();failLatency=true;f8();for(unsigned n=0;n<10000&&!contains("native_present_experiment_not_supported");++n)tick();failLatency=false;check(contains("native_present_experiment_not_supported")&&latencyMode==1&&!nativeMode,"unsupported experiment restores baseline");
 std::cout<<"PASS: native queue scope, normal/test-only/normal comparison, worker acknowledgement, completion, fallback, unsupported mode, F8/F9/focus/timeout restoration\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
