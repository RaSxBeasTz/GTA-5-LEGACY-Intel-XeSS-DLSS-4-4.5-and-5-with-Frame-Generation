from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text().replace('#include "ArtifactDiagnostics.h"','#include "ArtifactDiagnostics.h"\n#include "PacingGap.h"');needle=' const long long b339LastPresentUs=g_b266LastPrivatePresentUs.load(std::memory_order_relaxed);'
insert=''' static const int b553SourceFps=[](){wchar_t file[32768]{};GetModuleFileNameW(GetModuleHandleW(nullptr),file,32768);std::wstring path(file);path.resize(path.find_last_of(L"\\\\/")+1);return (int)GetPrivateProfileIntW(L"Pacing",L"SourceFPS",30,(path+L"GTAXeSS.ini").c_str());}();
 const double b553UnexpectedGapMs=UnexpectedSourceGapMs(b317EntryGapMs,b553SourceFps);
'''
assert needle in s;s=s.replace(needle,needle+'\n'+insert);s=s.replace('g_b423FreezeCollapseSourceGap(g_b174oPendingSize,b317EntryGapMs)','g_b423FreezeCollapseSourceGap(g_b174oPendingSize,b553UnexpectedGapMs)');s=s.replace('<<b317EntryGapMs<<", last_private_present_ms="','<<b317EntryGapMs<<", unexpected_source_gap_ms="<<b553UnexpectedGapMs<<", configured_source_fps="<<b553SourceFps<<", last_private_present_ms="');p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B552','B553'))
