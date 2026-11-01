from pathlib import Path
p=Path('redesign/B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();needle='static bool b173h_take_live_job(B173hLiveJob&job)';assert needle in s
pop=next(line for line in s.splitlines() if line.startswith(' if(available){job=g_b174oPendingJobs'))
fn='''static bool take_latest_completed_job(B173hLiveJob&job){
 if(!g_b173fSharedFence12)return false;
 const UINT64 complete=g_b173fSharedFence12->GetCompletedValue();
 if(complete==UINT64_MAX){b174_request_stop(4);return false;}
 unsigned droppedFrames[kB174oPendingCapacity]{},droppedSlots[kB174oPendingCapacity]{},dropped=0;
 AcquireSRWLockExclusive(&g_b173hJobLock);
 std::uint64_t fences[kB174oPendingCapacity]{};
 for(unsigned i=0;i<g_b174oPendingSize;++i)fences[i]=g_b174oPendingJobs[(g_b174oPendingHead+i)%kB174oPendingCapacity].nativeFenceValue;
 auto selected=gta::LatestCompletedFrame(fences,g_b174oPendingSize,complete);
 if(selected==gta::NoReadyFrame){ReleaseSRWLockExclusive(&g_b173hJobLock);return false;}
 while(selected--){
  auto& stale=g_b174oPendingJobs[g_b174oPendingHead];
  if(stale.slotIndex<kB173iColorSlotCount){
   droppedFrames[dropped]=stale.sourceFrame;droppedSlots[dropped]=stale.slotIndex;++dropped;
   B174ReplacedPending retired{};retired.nativeFenceValue=stale.nativeFenceValue;retired.slotIndex=stale.slotIndex;retired.sourceFrame=stale.sourceFrame;g_b174ReplacedPending.push_back(retired);
   g_b174ColorSlotState[retired.slotIndex].store(4,std::memory_order_release);g_b174DepthSlotState[retired.slotIndex].store(4,std::memory_order_release);
   g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);
  }
  stale=B173hLiveJob{};g_b174oPendingHead=(g_b174oPendingHead+1)%kB174oPendingCapacity;--g_b174oPendingSize;
 }
 const bool available=true;
'''+pop+'''
 ReleaseSRWLockExclusive(&g_b173hJobLock);
 for(unsigned i=0;i<dropped;++i)b175_log_disposition(droppedFrames[i],"dropped_completed_superseded","newer_completed_frame_ready",droppedSlots[i]);
 b175_log_disposition(job.sourceFrame,"accepted_current_worker","newest_gpu_completed_frame",job.slotIndex);
 return true;
}
'''
s=s.replace(needle,fn+needle);s=s.replace(needle+'\n{',needle+'\n{\n if(completed_scheduler_enabled())return take_latest_completed_job(job);')
# Avoid submitting expensive copies/depth packing if producer cannot admit a packet.
needle=' if(g_b171LiveBusy.load(std::memory_order_acquire)||!b174c_has_free_capture_slot())'
assert needle in s
s=s.replace(needle,''' if(completed_scheduler_enabled()&&g_b174PendingCount.load(std::memory_order_acquire)>=5){
  g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);
  b175_log_disposition(gtaFrame,"dropped_before_capture","bounded_completed_frame_admission",~0u);return FALSE;
 }
'''+needle,1)
p.write_text(s)
p=Path('redesign/GTAXeSS.ini');text=p.read_text();import re;text=re.sub(r'(?m)^SourceFPS=\d+','SourceFPS=0',text);text+='\n[Redesign]\nDirectSharedColor=1\nSingleQueue=1\nLatestCompleted=1\n\n[NativeMotion]\n; Optional raw candidate capture only. Unvalidated vectors remain disabled.\nCapture=0\n';p.write_text(text)
p=Path('redesign/Fast-Build.ps1');p.write_text(p.read_text().replace('B560','R600'))
