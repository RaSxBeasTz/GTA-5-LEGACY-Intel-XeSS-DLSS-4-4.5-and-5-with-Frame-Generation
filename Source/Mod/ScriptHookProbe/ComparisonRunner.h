#pragma once
#include "../B25DepthHelper/ComparisonCounters.h"
// Included after on_key: all orchestration runs on real GTA Present callbacks.
static void comparison_toggle_f9(){g_comparisonInternal=true;g_b277F9PollingDispatch.store(true);on_key(VK_F9,0,0,FALSE,FALSE,FALSE,TRUE);g_b277F9PollingDispatch.store(false);g_comparisonInternal=false;}
static void comparison_tick(unsigned sourceId){
 using mode_fn=void(WINAPI*)(BOOL);using counters_fn=void(WINAPI*)(ComparisonCounters*);
 static int phase=-1;static ULONGLONG requested{},ready{},sampleStart{};static unsigned sampleSource{};static ComparisonCounters base{};static std::ofstream report;static std::string reportPath;
 static bool held=false;const bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;const bool pressed=held&&!down;held=down;
 if(phase<0&&!pressed)return;
 HMODULE h=GetModuleHandleA("B25DepthHelper.dll");auto mode=h?(mode_fn)GetProcAddress(h,"B606SetCaptureOnly"):nullptr;auto counters=h?(counters_fn)GetProcAddress(h,"B606GetComparisonCounters"):nullptr;auto active=h?(b174_active_fn)GetProcAddress(h,"B174IsTestActive"):nullptr;
 auto profiling=h?(mode_fn)GetProcAddress(h,"B606SetComparisonActive"):nullptr;
 auto proxy=h?(mode_fn)GetProcAddress(h,"B608SetProxyOnly"):nullptr;
 auto visible=h?(b174_active_fn)GetProcAddress(h,"B608ProxyVisible"):nullptr;
 using set_count_fn=void(WINAPI*)(unsigned);using get_count_fn=unsigned(WINAPI*)();
 auto setCount=h?(set_count_fn)GetProcAddress(h,"B609SetInterpolations"):nullptr;auto getCount=h?(get_count_fn)GetProcAddress(h,"B609GetInterpolations"):nullptr;
 auto setReduced=h?(mode_fn)GetProcAddress(h,"B612SetReducedMotion"):nullptr;auto getReduced=h?(b174_active_fn)GetProcAddress(h,"B612GetReducedMotion"):nullptr;
 using latency_fn=BOOL(WINAPI*)(unsigned);auto setLatency=h?(latency_fn)GetProcAddress(h,"B614SetNativeQueue"):nullptr;auto getLatency=h?(get_count_fn)GetProcAddress(h,"B614GetNativeQueue"):nullptr;
 auto restoreDefault=h?(b174_active_fn)GetProcAddress(h,"B615RestoreNativeDefault"):nullptr;
 auto clear=[&](const char*why,bool restore){native_present_experiment_set(false);const bool restored=g_b206UserEnabled.load()?(restoreDefault&&restoreDefault()):(!setLatency||setLatency(0));if(!restored){log("R613: NATIVE_RENDER_AHEAD_RESTORE_FAILED");why="native_render_ahead_restore_failed";}if(setReduced)setReduced(FALSE);if(setCount)setCount(3);if(profiling)profiling(FALSE);if(proxy)proxy(FALSE);if(mode)mode(FALSE);if(report.is_open()){report<<"# "<<why<<"\n";report.close();}phase=-1;log(std::string("R606: COMPARISON_END reason=")+why+", report="+reportPath);if(restore&&!g_b206UserEnabled.load())comparison_toggle_f9();};
 if(g_comparisonCancel.exchange(false)&&phase>=0){clear("cancelled_by_f9",false);return;}
 if(pressed){
  if(phase>=0){clear("cancelled_by_f8",true);return;}
  if(!restoreDefault||!setLatency||!getLatency||!setReduced||!getReduced||!setCount||!getCount||!mode||!proxy||!visible||!profiling||!counters||!active||!active()||!g_b206UserEnabled.load()){log("R606: COMPARISON_NOT_STARTED reason=activate_with_f9_first");return;}
  reportPath=module_directory()+"GTAXeSS-Comparison-"+std::to_string(GetTickCount64())+".csv";report.open(reportPath,std::ios::trunc);if(!report){log("R606: COMPARISON_NOT_STARTED reason=report_open_failed");return;}
  report<<"phase,seconds,source_frames,source_fps,output_frames,output_fps,depth_gpu_ms,depth_samples,colour_gpu_ms,colour_samples,fg_queue_elapsed_ms,fg_queue_samples,reset_frames,generated_frames,valid_interpolation_sample,first_source_frame,last_source_frame\n";
  phase=0;requested=GetTickCount64();ready=sampleStart=0;profiling(TRUE);proxy(FALSE);mode(FALSE);setCount(3);setReduced(FALSE);native_present_experiment_set(false);if(!setLatency(1)){clear("native_render_ahead_change_failed",true);return;}log("R606: COMPARISON_BEGIN phases=x4_native_normal_x4_native_test_only_x4_native_normal_repeat, settle_seconds=3, sample_seconds=12, report="+reportPath);
 }
 if(phase<0)return;
 const auto now=GetTickCount64();
 if(GetForegroundWindow()!=g_b175RealHwnd){clear("focus_lost",true);return;}
 const bool running=active&&active();
 if(phase==1&&!native_present_experiment_enabled()){clear("native_present_experiment_fell_back",true);return;}
 const bool desired=running&&visible&&visible()&&getCount&&getCount()==3&&getReduced&&getReduced()==FALSE&&getLatency&&getLatency()==1&&native_present_experiment_enabled()==(phase==1);
 if(!desired){if(now-requested>20000)clear("pipeline_transition_timeout",true);return;}
 if(!ready){ready=now;sampleStart=0;log("R606: COMPARISON_PHASE phase="+std::to_string(phase)+", settling_ms=3000");}
 if(now-ready<3000)return;
 if(!sampleStart){sampleStart=now;sampleSource=sourceId;counters(&base);return;}
 if(now-sampleStart<12000)return;
 ComparisonCounters end{};counters(&end);const double seconds=(now-sampleStart)/1000.0;const unsigned frames=sourceId-sampleSource;const char*label=phase==0?"x4_native_normal":phase==1?"x4_native_test_only":"x4_native_normal_repeat";
 auto diff=[](std::uint64_t a,std::uint64_t b){return a>=b?a-b:0;};auto mean=[&](std::uint64_t ns,std::uint64_t count){return count?double(ns)/double(count)/1.0e6:0.0;};
 auto output=diff(end.outputFrames,base.outputFrames),dn=diff(end.depthSamples,base.depthSamples),cn=diff(end.colourSamples,base.colourSamples),fn=diff(end.fgQueueSamples,base.fgQueueSamples);
 report<<label<<','<<seconds<<','<<frames<<','<<frames/seconds<<','<<output<<','<<output/seconds<<','<<mean(diff(end.depthNs,base.depthNs),dn)<<','<<dn<<','<<mean(diff(end.colourNs,base.colourNs),cn)<<','<<cn<<','<<mean(diff(end.fgQueueNs,base.fgQueueNs),fn)<<','<<fn<<','<<diff(end.resetFrames,base.resetFrames)<<','<<diff(end.generatedFrames,base.generatedFrames)<<','<<(diff(end.generatedFrames,base.generatedFrames)>=fn*3*9/10&&fn>0?1:0)<<','<<sampleSource<<','<<sourceId<<'\n';report.flush();
 log(std::string("R606: COMPARISON_RESULT phase=")+label+", source_fps="+std::to_string(frames/seconds)+", output_fps="+std::to_string(output/seconds)+", fg_queue_samples="+std::to_string(fn));
 if(!fn||diff(end.generatedFrames,base.generatedFrames)<fn*3*9/10){clear("invalid_sample_history_resets_or_missing_generation",true);return;}
 if(phase==2){clear("completed",false);return;}
 ++phase;requested=now;ready=sampleStart=0;
 if(!native_present_experiment_set(phase==1)){clear("native_present_experiment_not_supported",true);return;}
}
