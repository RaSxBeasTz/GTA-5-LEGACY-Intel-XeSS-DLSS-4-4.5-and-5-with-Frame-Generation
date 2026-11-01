from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');save=Path('backups/pre-B558-source');save.mkdir(exist_ok=True);(save/p.name).write_bytes(p.read_bytes())
baseline=Path('backups/pre-B557-source/B140aDirectPresentSharedOpen.cpp').read_text();line=' job.nativeFenceValue=nativeFenceValue; // B556: retain real GPU completion marker through queue/retirement.\n';assert line in baseline;baseline=baseline.replace(line,'');Path('tests/B558-baseline-B555.cpp.txt').write_text(baseline)
s=baseline.replace('#include "ArtifactDiagnostics.h"','#include "ArtifactDiagnostics.h"\n#include "FrameTransportPolicy.h"')
start=s.index(' slot->deferredContext->UpdateSubresource(slot->nativeMotionConstants.Get()');end=s.index(' QueryPerformanceCounter(&afterMotionSubmit);',start);block=s[start:end]
setup=''' // B558: omit only the unused capture-motion pass when native colour bypasses XeSS.
 bool needsCaptureMotion=true;
 if(g_b173iColorPoolReady.load(std::memory_order_acquire)&&g_b173iColorSlots[slotIndex].shared){
  D3D11_TEXTURE2D_DESC captureColorTransport{};g_b173iColorSlots[slotIndex].shared->GetDesc(&captureColorTransport);
  needsCaptureMotion=NeedsCaptureMotion(g_b173iColorSlots[slotIndex].sourceDesc,captureColorTransport);
 }
 if(needsCaptureMotion){
'''
s=s[:start]+setup+block+' }\n'+s[end:];p.write_text(s)
# Proof that the complete source differs only by the new capture-motion guard.
unwrapped=s.replace('#include "FrameTransportPolicy.h"\n','').replace(setup,'').replace(block+' }\n',block)
assert unwrapped==baseline, 'Unexpected changes outside motion guard'
Path('tests/B558-motion-guard.txt').write_text(setup)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B557','B558'))
p=Path('GTAXeSS.ini');import re;s=p.read_text();s=re.sub(r'(?m)^SourceFPS=\d+','SourceFPS=45',s);s=s.replace('; Uncapped source rendering for B557 performance test; 0 disables the source limiter.','; Source cap45 for controlled single-change test; 0 disables.');p.write_text(s)
print('PASS: exact B555 source outside capture-motion guard; no queue/sync changes')
