from pathlib import Path
p=Path('ScriptHookProbe/SourcePacer.h');s=p.read_text().replace('return double(GetTickCount64()*0);','QueryPerformanceCounter(&now);return double(now.QuadPart-begin)*1000/frequency;');p.write_text(s)
p=Path('ScriptHookProbe/main.cpp');s=p.read_text().replace('#include "PreparationPolicy.h"','#include "PreparationPolicy.h"\n#include "SourcePacer.h"');needle=' // B404: B237 raw-Present cadence recorder retired from the GTA render hot path.'
insert=''' // B550: cap real source cadence before input capture, without worker waits
 // or extra GPU submissions. F9 and F11 polling above stay responsive.
 {
  static SourcePacer pacer;
  static const unsigned fps=SourceCadence::fps((int)GetPrivateProfileIntA("Pacing","SourceFPS",30,(module_directory()+"GTAXeSS.ini").c_str()));
  HMODULE pacingHelper=GetModuleHandleA("B25DepthHelper.dll");
  auto testActive=pacingHelper?(b174_active_fn)GetProcAddress(pacingHelper,"B174IsTestActive"):nullptr;
  auto active=[&](){return g_b206UserEnabled.load(std::memory_order_acquire)&&testActive&&testActive()&&GetForegroundWindow()==g_b175RealHwnd;};
  const bool running=active();static bool wasRunning=false;
  if(running&&!wasRunning)log("B550: SOURCE_PACING source_fps_cap="+std::to_string(fps)+", nominal_x4_output_cap="+std::to_string(fps*4)+", wait=high_resolution_timer, no_catchup=1, worker_unchanged=1");
  wasRunning=running;const double waited=pacer.pace(fps,active);
  if(running&&sourceId%120==0)log("B550: SOURCE_PACING_WAIT source_frame_id="+std::to_string(sourceId)+", wait_ms="+std::to_string(waited)+", source_fps_cap="+std::to_string(fps));
 }
'''
assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
p=Path('GTAXeSS.ini');s=p.read_text();s+='\n[Pacing]\n; Source cap while enabled and focused. 30 targets 120 FPS at x4; 0 disables.\n; Restart GTA after changes.\nSourceFPS=30\n';p.write_text(s)
p=Path('Fast-Build.ps1');s=p.read_text().replace('B549','B550');needle='$proof=Join-Path';s=s.replace(needle,'if((Get-Content $installedIni -Raw) -notmatch \'(?im)^\\[Pacing\\]\'){Add-Content -LiteralPath $installedIni -Value "`r`n[Pacing]`r`nSourceFPS=30"}\n'+needle);p.write_text(s)
