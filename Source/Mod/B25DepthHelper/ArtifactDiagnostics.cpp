#include "ArtifactDiagnostics.h"
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <array>
#include <atomic>
#include <mutex>
#include <cstring>
static std::filesystem::path root(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return std::filesystem::path(path).parent_path();}
bool ArtifactDiagnosticsEnabled(){static const bool enabled=GetPrivateProfileIntW(L"Diagnostics",L"Enabled",1,(root()/L"GTAXeSS.ini").c_str())!=0;return enabled;}
static std::atomic<unsigned long long> markerQpc{0};
extern "C" __declspec(dllexport) void WINAPI B543MarkVisualArtifact(){static std::atomic<ULONGLONG> lastTick{0};const auto now=GetTickCount64();const auto previous=lastTick.exchange(now);if(previous&&now-previous<150)return;LARGE_INTEGER q{};QueryPerformanceCounter(&q);markerQpc.store(q.QuadPart,std::memory_order_release);}
struct ArtifactLog {
 std::array<char,65536> csvBuffer{},ghostBuffer{},flashBuffer{},stutterBuffer{};
 std::ofstream csv,ghost,flash,stutter;std::mutex mutex;ArtifactSample prior{};bool havePrior=false;unsigned lastFlags=0;unsigned long long lastFlush=0,records=0,ghostCount=0,flashCount=0,stutterCount=0;
 ArtifactLog(){auto r=root();auto open=[&](std::ofstream&f,auto&buffer,const wchar_t*name){f.rdbuf()->pubsetbuf(buffer.data(),buffer.size());f.open(r/name,std::ios::trunc);f<<std::setprecision(9);};
  open(csv,csvBuffer,L"GTAXeSS-ArtifactFrames.csv");open(ghost,ghostBuffer,L"GTAXeSS-Ghosting.log");open(flash,flashBuffer,L"GTAXeSS-Flashing.log");open(stutter,stutterBuffer,L"GTAXeSS-Stutter.log");
  for(auto*f:{&ghost,&flash,&stutter})*f<<"B543 diagnostics: risk indicators are not visual detections. No pixel readback. F11 marks a user-observed artifact.\n";
  LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);SYSTEMTIME utc{};GetSystemTime(&utc);for(auto*f:{&ghost,&flash,&stutter})*f<<"SESSION utc="<<utc.wYear<<'-'<<utc.wMonth<<'-'<<utc.wDay<<'T'<<utc.wHour<<':'<<utc.wMinute<<':'<<utc.wSecond<<", qpc_frequency="<<frequency.QuadPart<<", pid="<<GetCurrentProcessId()<<"\n";
  csv<<"source_frame,present_id,qpc,tick_ms,camera_epoch,camera_valid,reset,visible,status_observed,frames_presented,frame_gen_result,status_result,present_hr,prefill_hr,source_ms,frame_ms,queue_ms,present_ms,worker_ms,pending,dropped_total,motion_max_ndc,backbuffer_before,backbuffer_after,flags,user_marker_qpc";
  for(const char*prefix:{"previous_vp","current_vp","inverse_vp","clip_to_previous"})for(unsigned i=0;i<16;++i)csv<<','<<prefix<<i;csv<<'\n';
 }
 void flush(){csv.flush();ghost.flush();flash.flush();stutter.flush();}
};
// Explicit flushing avoids worker joins or GPU waits during DLL unload.
static ArtifactLog& logger(){static auto*log=new ArtifactLog;return *log;}
void ArtifactFlush(){if(!ArtifactDiagnosticsEnabled())return;auto&l=logger();std::lock_guard<std::mutex>guard(l.mutex);l.flush();}
void ArtifactRecord(const ArtifactSample&s){if(!ArtifactDiagnosticsEnabled())return;auto&l=logger();std::lock_guard<std::mutex>guard(l.mutex);
 unsigned flags=ClassifyArtifact(s,l.havePrior?&l.prior:nullptr);auto marker=markerQpc.exchange(0,std::memory_order_acq_rel);if(marker)flags|=UserMarker;
 auto&c=l.csv;c<<s.source<<','<<s.present<<','<<s.qpc<<','<<s.tick<<','<<s.epoch<<','<<s.valid<<','<<s.reset<<','<<s.visible<<','<<s.statusObserved<<','<<s.frames<<','<<s.fgResult<<','<<s.statusResult<<','<<s.presentHr<<','<<s.prefillHr<<','<<s.sourceMs<<','<<s.frameMs<<','<<s.queueMs<<','<<s.presentMs<<','<<s.workerMs<<','<<s.pending<<','<<s.dropped<<','<<s.motionMax<<','<<s.backbufferBefore<<','<<s.backbufferAfter<<','<<flags<<','<<marker;
 for(const float*matrix:{s.previous,s.current,s.inverse,s.transform})for(unsigned i=0;i<16;++i)c<<','<<matrix[i];c<<'\n';++l.records;
 const unsigned ghostMask=UnsafeCamera|InvalidMotion|LargeMotion|ZeroMotionHistory|EpochWithoutReset,flashMask=PresentError|OutputModeChange|VisibilityChange|ResetTransition|EpochWithoutReset,stutterMask=SourceHitch|QueueHitch|PresentHitch|WorkerHitch|SourceSkipped;
 if(flags&ghostMask)++l.ghostCount;if(flags&flashMask)++l.flashCount;if(flags&stutterMask)++l.stutterCount;
 // Full per-job data is always in CSV; event text is bounded to avoid log storms.
 const bool detail=l.records<=8||flags!=l.lastFlags||l.records%120==0||marker;
 auto event=[&](std::ofstream&out,const char*name){out<<name<<" source_frame="<<s.source<<", present_id="<<s.present<<", qpc="<<s.qpc<<", flags="<<flags<<", camera_epoch="<<s.epoch<<", camera_valid="<<s.valid<<", reset="<<s.reset<<", visible="<<s.visible<<", actual_status="<<s.statusObserved<<", frames_presented="<<s.frames<<", motion_max_ndc="<<s.motionMax<<", source_ms="<<s.sourceMs<<", queue_ms="<<s.queueMs<<", present_ms="<<s.presentMs<<", worker_ms="<<s.workerMs<<", dropped_total="<<s.dropped<<", user_marker_qpc="<<marker<<", visual_detection=0\n";};
 if(detail&&(flags&ghostMask||marker))event(l.ghost,"GHOSTING_RISK");if(detail&&(flags&flashMask||marker))event(l.flash,"FLASHING_RISK");if(detail&&(flags&stutterMask||marker))event(l.stutter,"STUTTER_EVENT");
 if(!l.lastFlush||s.tick-l.lastFlush>=1000||marker){for(auto*f:{&l.ghost,&l.flash,&l.stutter})*f<<"SUMMARY qpc="<<s.qpc<<", records="<<l.records<<", ghosting_risk_frames="<<l.ghostCount<<", flashing_risk_frames="<<l.flashCount<<", stutter_flagged_frames="<<l.stutterCount<<"\n";l.flush();l.lastFlush=s.tick;}
 l.prior=s;l.havePrior=true;l.lastFlags=flags;
}
