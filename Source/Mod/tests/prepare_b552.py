from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();start=s.index('// B551: split the historical');end=s.index('static std::atomic<unsigned> g_b264DropResourceOwned',start);s=s[:start]+s[end:]
s=s.replace('if(reason&&(strcmp(reason,"frame_resource_ring_owned")==0||strcmp(reason,"unfinished_capture_owned")==0))','if(reason&&strcmp(reason,"frame_resource_ring_owned")==0)')
start=s.index('const bool b551Busy=g_b171LiveBusy');end=s.index('\n return TRUE;',start);s=s[:start]+'''if(g_b171LiveBusy.load(std::memory_order_acquire)||!b174c_has_free_capture_slot()){g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_no_safe_slot","frame_resource_ring_owned",~0u);return FALSE;}'''+s[end:]
start=s.index('  {static ULONGLONG lastOwnershipTick=0;');end=s.index('  B173hLiveJob job{};',start);s=s[:start]+s[end:]
needle=' B173hLiveJob job=g_b174CaptureJob;';assert s.count(needle)==1;s=s.replace(needle,needle+'\n job.nativeFenceValue=nativeFenceValue; // B552: queued/collapsed jobs retain their actual native completion fence.');p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B551','B552'))
