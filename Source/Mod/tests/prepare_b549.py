from pathlib import Path
import hashlib,json,re
p=Path('ProtectedFixes/TemporalRuntime.cpp');s=p.read_text();start=s.index('GHOST_API const char* GhostingMotionShader1()');end=s.index('GHOST_API const char* GhostingMotionShader2()');shader=s[start:end];needle=' depthOutput[id.xy]=z;\n float2 motion='
insert=''' depthOutput[id.xy]=z; // Keep actual per-pixel depth; dilate only motion.
 // B549: XeFG full-resolution motion requires foremost-surface dilation.
 // GTA reversed Z: the largest depth is the closest surface. Start at the
 // centre and use strict greater-than so equal-depth surfaces do not wander.
 uint2 motionPixel=id.xy;
 for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox){
  int2 samplePixel=clamp(int2(id.xy)+int2(ox,oy),int2(0,0),int2(frameSize)-1);
  uint3 bytes=(uint3)round(saturate(depthInput.Load(int3(samplePixel,0)).rgb)*255.0);
  float candidate=(float(bytes.x)+float(bytes.y)*256.0+float(bytes.z)*65536.0)/16777215.0;
  if(candidate>z){z=candidate;motionPixel=uint2(samplePixel);}
 }
 float2 motion='''
assert needle in shader;shader=shader.replace(needle,insert).replace('((id.x+0.5)/frameSize.x)','((motionPixel.x+0.5)/frameSize.x)').replace('((id.y+0.5)/frameSize.y)','((motionPixel.y+0.5)/frameSize.y)');s=s[:start]+shader+s[end:];p.write_text(s)
h=hashlib.sha256(p.read_bytes()).hexdigest()
p=Path('ProtectedFixes/RuntimeContract.cpp');s=p.read_text().replace('return 547','return 549');s=re.sub(r'return "[0-9a-f]{64}"',f'return "{h}"',s);p.write_text(s)
p=Path('B25DepthHelper/GhostingExpected.h');s=p.read_text().replace('547u','549u');s=re.sub(r'GHOSTING_EXPECTED_CONTRACT "[^"]+"',f'GHOSTING_EXPECTED_CONTRACT "{h}"',s);p.write_text(s)
p=Path('ProtectedFixes/B535-source-lock.json');lock=json.loads(p.read_text())
for name in ['ProtectedFixes/TemporalRuntime.cpp','ProtectedFixes/RuntimeContract.cpp']:lock[name]=hashlib.sha256(Path(name).read_bytes()).hexdigest()
p.write_text(json.dumps(lock,indent=2)+'\n')
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text().replace('WORKER_ON12_CONTEXT_PREWARM device=','WORKER_ON12_CONTEXT_PREWARM device=');needle=' const char*source=GhostingMotionShader1();';assert needle in s;s=s.replace(needle,needle+'\n out<<"B549: XEFG_FULL_RES_MOTION_DILATION kernel=3x3, nearest_surface=reversed_depth_max, original_depth_preserved=1, native_color=B548, pacing_unchanged=1\\n";');p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B548','B549'))
