from pathlib import Path
s=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp').read_text()
retire=s[s.index('static void b174_retire_completed_frames(std::ofstream&out)'):s.index('static D3D12_RESOURCE_STATES colour_input_state(')]
start=s.index(' B173hLiveJob job=g_b174CaptureJob;');end=s.index(' const unsigned b486DepthCameraSourceFrame',start)
handoff=s[start:end]
source=r'''#include <windows.h>
#include "../B25DepthHelper/FrameLifetime.h"
#include <atomic>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
static constexpr unsigned kB173iColorSlotCount=12;
struct Fence{UINT64 value{};UINT64 GetCompletedValue(){return value;}};
static Fence nativeFence,stageFence;
static Fence* g_b173fSharedFence12=&nativeFence,*g_b173nStageFence=&stageFence;
static std::atomic<unsigned> g_b174ColorSlotState[12],g_b174DepthSlotState[12];
struct B174InFlightFrame{UINT64 marker{};unsigned slotIndex{},sourceFrame{},presentId{};};
struct B174ReplacedPending{UINT64 nativeFenceValue{};unsigned slotIndex{},sourceFrame{};};
static std::vector<B174InFlightFrame> g_b174InFlightFrames;
static std::vector<B174ReplacedPending> g_b174ReplacedPending;
static SRWLOCK g_b173hJobLock=SRWLOCK_INIT;
static bool b240_log_sample(unsigned){return false;}
struct B173hLiveJob{UINT64 nativeFenceValue{};unsigned slotIndex{};};
static B173hLiveJob g_b174CaptureJob;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
'''+retire+'\nstatic B173hLiveJob handoff(UINT64 nativeFenceValue){\n'+handoff+' return job;\n}\n'+r'''
int main(){try{std::ofstream log("retirement-regression.log");
 // Negative control: missing handoff strands nine slots despite completed GPU.
 for(unsigned i=0;i<9;++i){g_b174ColorSlotState[i]=g_b174DepthSlotState[i]=4;B173hLiveJob broken=g_b174CaptureJob;g_b174ReplacedPending.push_back({broken.nativeFenceValue,i,i});}
 nativeFence.value=100;b174_retire_completed_frames(log);check(g_b174ReplacedPending.size()==9,"old zero-fence handoff reproduces exhausted pool");g_b174ReplacedPending.clear();
 // Actual production handoff and retirement: nine dropped jobs, incomplete,
 // partial and full completion. Never release before the real fence.
 for(unsigned i=0;i<9;++i){auto job=handoff(i+1);g_b174ReplacedPending.push_back({job.nativeFenceValue,i,i});}
 nativeFence.value=0;b174_retire_completed_frames(log);check(g_b174ReplacedPending.size()==9,"GPU still owns incomplete captures");
 nativeFence.value=8;b174_retire_completed_frames(log);check(g_b174ReplacedPending.size()==1,"only completed native jobs retire");check(g_b174ColorSlotState[8]==4&&g_b174DepthSlotState[8]==4,"unfinished slot retained");
 nativeFence.value=9;b174_retire_completed_frames(log);check(g_b174ReplacedPending.empty(),"all nine stranded slots recover");
 for(unsigned i=0;i<9;++i)check(g_b174ColorSlotState[i]==0&&g_b174DepthSlotState[i]==0,"paired resources released");
 for(unsigned n=10;n<10010;++n){unsigned slot=n%12;g_b174ColorSlotState[slot]=g_b174DepthSlotState[slot]=4;auto job=handoff(n);g_b174ReplacedPending.push_back({job.nativeFenceValue,slot,n});nativeFence.value=n-1;b174_retire_completed_frames(log);check(g_b174ColorSlotState[slot]==4,"no early recycling");nativeFence.value=n;b174_retire_completed_frames(log);check(g_b174ReplacedPending.empty()&&g_b174ColorSlotState[slot]==0&&g_b174DepthSlotState[slot]==0,"bounded ring survives repeated drops");}
 g_b174ColorSlotState[0]=g_b174DepthSlotState[0]=3;g_b174InFlightFrames.push_back({55,0,1,1});stageFence.value=54;b174_retire_completed_frames(log);check(g_b174ColorSlotState[0]==3,"presented frame retains worker GPU ownership");stageFence.value=55;b174_retire_completed_frames(log);check(g_b174ColorSlotState[0]==0&&g_b174InFlightFrames.empty(),"presented completion path unchanged");
 // R603: a later seed copy must extend ownership past Present completion.
 g_b174ColorSlotState[1]=g_b174DepthSlotState[1]=3;g_b174InFlightFrames.push_back({gta::LastStageUse(55,70),1,2,2});
 stageFence.value=55;b174_retire_completed_frames(log);check(g_b174ColorSlotState[1]==3,"Present completion cannot retire later seed-copy input");
 stageFence.value=69;b174_retire_completed_frames(log);check(g_b174ColorSlotState[1]==3,"seed still owns colour before final-use fence");
 stageFence.value=UINT64_MAX;b174_retire_completed_frames(log);check(g_b174ColorSlotState[1]==3,"device removed is not successful retirement");
 stageFence.value=70;b174_retire_completed_frames(log);check(g_b174ColorSlotState[1]==0,"final seed completion releases slot");
 check(!gta::StageUseCompleted(69,70)&&gta::StageUseCompleted(70,70),"seed allocator reuse gate");
 std::cout<<"PASS: production handoff/retirement, old zero-fence negative control, 9-slot recovery, incomplete fences retained, 10000 reuse cycles, presented path\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
'''
Path('tests/FrameRetirement.generated.cpp').write_text(source)
p=Path('tests/SourcePacer.vcxproj').read_text().replace('SourcePacer','FrameRetirement').replace('25C000000550','25C000000552').replace('Include="FrameRetirement.cpp"','Include="FrameRetirement.generated.cpp"');Path('tests/FrameRetirement.vcxproj').write_text(p)
