from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();old='''   // The newest job may no longer be temporally adjacent to the last presented source.
   // Reset XeFG history once on this discontinuity; do not alter camera classification.
   g_b174oPendingJobs[g_b174oPendingHead].constants.resetHistory=1;''';new='''   // B554: skipped captures do not advance presented history. The worker
   // rebuilds motion against its last successfully presented camera, so keep
   // the newest job\'s existing reset flag. Epoch/cut/resume and >100ms guards
   // still decide resets after alignment; never replace real motion with zero
   // solely because unpresented queue entries were discarded.''';assert old in s;s=s.replace(old,new);p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B553','B554'))
p=Path('tests/ResetRegression.cpp');s=p.read_text();needle=' const char* shaders[]=';insert=''' // B554: sustained queue collapse skips captures but not presented history.
 // Each newest camera must retain nonzero motion and generate without reset.
 GhostingClearConsumed();GhostingCameraPair newest{};newest.b437MotionCameraValid=true;newest.cameraEpoch=GhostingTemporal().cameraEpoch.load()+1;
 std::memcpy(newest.currentViewProjection,identity,64);std::memcpy(newest.currentInverseViewProjection,identity,64);
 GhostingAlignWorkerPair(newest);check(GhostingWorkerReset(newest,false,20000,log)==1,"initial epoch remains reset");GhostingCommitPresented(identity,newest.cameraEpoch);
 float last[16];std::memcpy(last,identity,64);
 for(unsigned n=1;n<=10000;++n){
  std::memcpy(newest.currentViewProjection,identity,64);std::memcpy(newest.currentInverseViewProjection,identity,64);newest.currentViewProjection[12]=n*.006f;newest.currentInverseViewProjection[12]=-n*.006f;newest.forceReset=false;
  GhostingAlignWorkerPair(newest);same(newest.previousViewProjection,last,"newest queued frame follows last presented frame");check(!newest.forceReset&&GhostingWorkerReset(newest,false,20000+n,log)==0,"ordinary skipped sources preserve interpolation");check(std::memcmp(newest.previousViewProjection,newest.currentViewProjection,64)!=0,"skipped sources retain nonzero motion");
  GhostingCommitPresented(newest.currentViewProjection,newest.cameraEpoch);std::memcpy(last,newest.currentViewProjection,64);
 }
 newest.forceReset=true;GhostingAlignWorkerPair(newest);check(GhostingWorkerReset(newest,false,40001,log)==1,"explicit reset survives collapse");same(newest.previousViewProjection,newest.currentViewProjection,"explicit reset stays zero-motion aligned");newest.forceReset=false;
 ++newest.cameraEpoch;GhostingAlignWorkerPair(newest);check(GhostingWorkerReset(newest,false,40002,log)==1,"dropped recovery epoch still resets newest job");same(newest.previousViewProjection,newest.currentViewProjection,"recovery epoch aligns current camera");
 std::cout<<"PASS: 10000 collapsed source batches keep real presented motion, explicit resets and recovery epochs preserved\\n";
''';assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
