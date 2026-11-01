#pragma once
#include <windows.h>
#include <cstdint>
#include <algorithm>
#include <cmath>
struct ArtifactSample {
 unsigned source{},present{},frames{},pending{},backbufferBefore{},backbufferAfter{};
 unsigned long long dropped{};
 unsigned long long epoch{},qpc{},tick{};
 double sourceMs{},frameMs{},queueMs{},presentMs{},workerMs{},motionMax{};
 long presentHr{},prefillHr{},statusResult{},fgResult{};
 bool valid{},reset{},visible{},motionValid{},statusObserved{};
 float previous[16]{},current[16]{},inverse[16]{},transform[16]{};
};
enum ArtifactRisk:unsigned {UnsafeCamera=1,InvalidMotion=2,LargeMotion=4,PresentError=8,OutputModeChange=16,VisibilityChange=32,ResetTransition=64,SourceHitch=128,QueueHitch=256,PresentHitch=512,WorkerHitch=1024,SourceSkipped=2048,UserMarker=4096,ZeroMotionHistory=8192,EpochWithoutReset=16384};
inline unsigned ClassifyArtifact(const ArtifactSample&s,const ArtifactSample*prior){
 unsigned f=0;if(!s.valid)f|=UnsafeCamera;if(!s.motionValid||!std::isfinite(s.motionMax))f|=InvalidMotion;if(s.motionMax>1.0)f|=LargeMotion;
 if(s.presentHr<0||s.prefillHr<0||(s.statusObserved&&s.statusResult<0))f|=PresentError;
 if(prior){if(s.statusObserved&&prior->statusObserved&&((s.frames>1)!=(prior->frames>1)))f|=OutputModeChange;if(s.visible!=prior->visible)f|=VisibilityChange;if(s.reset!=prior->reset)f|=ResetTransition;if(s.source>prior->source+1)f|=SourceSkipped;if(s.valid&&!s.reset&&s.motionMax<1e-6){bool changed=false;for(unsigned i=0;i<16;++i)changed=changed||std::fabs(s.current[i]-prior->current[i])>1e-5f;if(changed)f|=ZeroMotionHistory;}if(s.epoch!=prior->epoch&&!s.reset)f|=EpochWithoutReset;}
 if(s.sourceMs>33.333)f|=SourceHitch;if(s.queueMs>25)f|=QueueHitch;if(s.presentMs>(std::max)(16.667,s.frameMs*1.5))f|=PresentHitch;if(s.workerMs>(std::max)(33.333,s.frameMs*2))f|=WorkerHitch;return f;
}
bool ArtifactDiagnosticsEnabled();
void ArtifactRecord(const ArtifactSample&);
void ArtifactFlush();
extern "C" __declspec(dllexport) void WINAPI B543MarkVisualArtifact();
