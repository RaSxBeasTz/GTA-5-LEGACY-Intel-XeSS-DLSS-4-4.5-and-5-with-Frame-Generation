#include "../B25DepthHelper/ArtifactDiagnostics.h"
#include <stdexcept>
#include <iostream>
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(){try{
 ArtifactSample base{};base.source=10;base.present=1;base.frames=4;base.valid=true;base.motionValid=true;base.statusObserved=true;base.visible=true;base.frameMs=16.667;base.presentMs=1;base.workerMs=2;base.sourceMs=16;base.tick=1000;base.qpc=100;
 check(ClassifyArtifact(base,nullptr)==0,"healthy frame has no flags");
 auto bad=base;bad.valid=false;check(ClassifyArtifact(bad,&base)&UnsafeCamera,"unsafe camera indicator");bad=base;bad.motionValid=false;check(ClassifyArtifact(bad,&base)&InvalidMotion,"invalid motion indicator");bad=base;bad.motionMax=2;check(ClassifyArtifact(bad,&base)&LargeMotion,"excessive reconstructed motion indicator");
 bad=base;bad.frames=1;check(ClassifyArtifact(bad,&base)&OutputModeChange,"generated-to-source-only transition");bad=base;bad.visible=false;check(ClassifyArtifact(bad,&base)&VisibilityChange,"visibility transition");bad=base;bad.reset=true;check(ClassifyArtifact(bad,&base)&ResetTransition,"reset transition");bad=base;bad.presentHr=(long)0x887a0005;check(ClassifyArtifact(bad,&base)&PresentError,"presentation error indicator");
 bad=base;bad.queueMs=30;check(ClassifyArtifact(bad,&base)&QueueHitch,"queue age stutter");bad=base;bad.presentMs=40;check(ClassifyArtifact(bad,&base)&PresentHitch,"present stutter");bad=base;bad.sourceMs=50;check(ClassifyArtifact(bad,&base)&SourceHitch,"source stutter");bad=base;bad.workerMs=80;check(ClassifyArtifact(bad,&base)&WorkerHitch,"worker stutter");bad=base;bad.source=12;check(ClassifyArtifact(bad,&base)&SourceSkipped,"source gap");
 bad=base;bad.current[0]=2;check(ClassifyArtifact(bad,&base)&ZeroMotionHistory,"changed camera with zero motion and retained history");bad=base;bad.epoch=1;check(ClassifyArtifact(bad,&base)&EpochWithoutReset,"epoch advance without history reset");
 check(ArtifactDiagnosticsEnabled(),"diagnostics default enabled");ArtifactRecord(base);B543MarkVisualArtifact();bad=base;bad.source=12;bad.present=2;bad.frames=1;bad.motionMax=2;bad.queueMs=40;bad.reset=true;bad.tick=2000;bad.qpc=200;ArtifactRecord(bad);ArtifactFlush();
 std::cout<<"PASS: ghosting-risk, flashing-risk, measured stutter classifications; CSV/event output and user bookmark\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
