from pathlib import Path
import csv
rows=[]
for a in csv.DictReader(open('G:/SteamLibrary/steamapps/common/Grand Theft Auto V/GTAXeSS-ArtifactFrames.csv')):
 if a.get('camera_valid')=='1' and a.get('inverse_vp15'):
  rows.append(' '.join(a[k+str(i)] for k in ['current_vp','inverse_vp'] for i in range(16)))
Path('tests/b547-camera-fixture.txt').write_text('\n'.join(rows)+'\n')
p=Path('tests/ResetRegression.cpp');s=p.read_text();needle=' const char* shaders[]='
insert=''' // Replay actual B546 cameras through the compiled B547 capture and worker policy.
 GhostingClearConsumed();GhostingInvalidateXess();GhostingTakeXessReset();
 std::ifstream fixture("tests/b547-camera-fixture.txt");check(fixture.good(),"recorded camera fixture available");
 unsigned replayed=0,rebases=0,workerResets=0,nonzero=0;
 while(fixture>>view[0]){for(unsigned i=1;i<16;++i)fixture>>view[i];for(float&v:inverse)fixture>>v;check(!fixture.fail(),"complete camera fixture row");
  GhostingTemporalFrame f{};f.valid=true;f.transition=replayed==0;std::memcpy(f.validated,view,64);f.effectiveInverse=inverse;f.effectiveView=identity;f.effectiveProjection=identity;
  GhostingPrepareMotion(1000+replayed,view,"reset-regression.log",f);rebases+=f.rebase;
  GhostingCameraPair pair{};GhostingFinalizePair(1000+replayed,view,"reset-regression.log",f,pair);GhostingAlignWorkerPair(pair);
  workerResets+=GhostingWorkerReset(pair,false,1000+replayed,log)!=0;
  nonzero+=std::memcmp(pair.previousViewProjection,pair.currentViewProjection,64)!=0;
  GhostingCommitXess(view);GhostingCommitPresented(view,pair.cameraEpoch);++replayed;
 }
 check(replayed>4000&&rebases<20&&workerResets<20&&nonzero>4000,"recorded normal movement preserves nonzero motion without reset storm");
 std::cout<<"Replay rows="<<replayed<<" capture rebases="<<rebases<<" worker resets="<<workerResets<<" nonzero motion="<<nonzero<<"\\n";
'''
assert needle in s;s=s.replace(needle,insert+needle);p.write_text(s)
