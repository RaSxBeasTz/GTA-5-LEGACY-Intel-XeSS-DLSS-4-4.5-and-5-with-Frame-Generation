from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();needle='static std::atomic<unsigned> g_b264DropResourceOwned{0}'
insert='''// B551: split the historical combined resource-owned drop reason without
// changing admission, slot lifetime, or temporal policy. Render path atomics
// only; the serial worker writes the once-per-second ownership snapshot.
static std::atomic<unsigned> g_b551BusyDrops{0},g_b551PoolDrops{0},g_b551DropSource{0};
static std::atomic<unsigned long long> g_b551DropColorMask{0},g_b551DropDepthMask{0};
'''
assert needle in s;s=s.replace(needle,insert+needle)
old='if(g_b171LiveBusy.load(std::memory_order_acquire)||!b174c_has_free_capture_slot()){g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_no_safe_slot","frame_resource_ring_owned",~0u);return FALSE;}'
new='''const bool b551Busy=g_b171LiveBusy.load(std::memory_order_acquire);
 if(b551Busy||!b174c_has_free_capture_slot()){
  (b551Busy?g_b551BusyDrops:g_b551PoolDrops).fetch_add(1,std::memory_order_relaxed);
  unsigned long long colorMask=0,depthMask=0;
  for(unsigned i=0;i<kB173iColorSlotCount;++i){colorMask|=(unsigned long long)(g_b174ColorSlotState[i].load(std::memory_order_acquire)&15)<<(i*4);depthMask|=(unsigned long long)(g_b174DepthSlotState[i].load(std::memory_order_acquire)&15)<<(i*4);}
  g_b551DropColorMask.store(colorMask,std::memory_order_relaxed);g_b551DropDepthMask.store(depthMask,std::memory_order_relaxed);g_b551DropSource.store(gtaFrame,std::memory_order_release);
  g_b174Dropped.fetch_add(1,std::memory_order_acq_rel);g_b174TotalDropped.fetch_add(1,std::memory_order_acq_rel);b175_log_disposition(gtaFrame,"dropped_no_safe_slot",b551Busy?"unfinished_capture_owned":"frame_resource_ring_owned",~0u);return FALSE;
 }'''
assert old in s;s=s.replace(old,new)
# Keep existing aggregate counter classifications intact while adding split counters.
s=s.replace('if(reason&&strcmp(reason,"frame_resource_ring_owned")==0)','if(reason&&(strcmp(reason,"frame_resource_ring_owned")==0||strcmp(reason,"unfinished_capture_owned")==0))')
needle='  B173hLiveJob job{};\n  if(b173h_take_live_job(job))'
new='''  {static ULONGLONG lastOwnershipTick=0;static unsigned lastDropSource=0;const ULONGLONG tick=GetTickCount64();
   const unsigned droppedSource=g_b551DropSource.load(std::memory_order_acquire);
   if(droppedSource!=lastDropSource&&tick-lastOwnershipTick>=1000){lastOwnershipTick=tick;lastDropSource=droppedSource;
    unsigned counts[5]{};unsigned long long currentColor=0,currentDepth=0;
    for(unsigned i=0;i<kB173iColorSlotCount;++i){const unsigned c=g_b174ColorSlotState[i].load(std::memory_order_acquire),d=g_b174DepthSlotState[i].load(std::memory_order_acquire);currentColor|=(unsigned long long)(c&15)<<(i*4);currentDepth|=(unsigned long long)(d&15)<<(i*4);if(c<5)++counts[c];}
    out<<"B551: RESOURCE_OWNERSHIP last_drop_source="<<droppedSource<<", unfinished_capture_drops="<<g_b551BusyDrops.load()<<", exhausted_pool_drops="<<g_b551PoolDrops.load()<<", drop_color_mask="<<g_b551DropColorMask.load()<<", drop_depth_mask="<<g_b551DropDepthMask.load()<<", current_color_mask="<<currentColor<<", current_depth_mask="<<currentDepth<<", slot_counts_free_capture_queued_worker_retiring={"<<counts[0]<<","<<counts[1]<<","<<counts[2]<<","<<counts[3]<<","<<counts[4]<<"}, in_flight="<<g_b174InFlightFrames.size()<<", oldest_retained_marker="<<(g_b174InFlightFrames.empty()?0:g_b174InFlightFrames.front().marker)<<", stage_completed="<<(g_b173nStageFence?g_b173nStageFence->GetCompletedValue():0)<<", native_completed="<<(g_b173fSharedFence12?g_b173fSharedFence12->GetCompletedValue():0)<<", replaced_pending="<<g_b174ReplacedPending.size()<<", capture_busy="<<(g_b171LiveBusy.load()?1:0)<<", admission_unchanged=1\\n";
   }
  }
  B173hLiveJob job{};
  if(b173h_take_live_job(job))'''
assert needle in s;s=s.replace(needle,new);p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B550','B551'))
