from pathlib import Path
import re,hashlib
from cpp_extract import mask,units
root=Path(__file__).resolve().parent.parent
base=root/'tests/baseline'
def functions(s):
 out={}
 for a,b,t,k in units(s):
  if k!='func':continue
  ns=re.findall(r'\b(\w+)\s*\(',mask(t).split('{')[0])
  if ns:out[ns[-1]]=t[t.index('{')+1:t.rfind('}')]
 return out
def norm(s):return re.sub(r'\s+','',mask(s))
a=(base/'B25DepthHelper.cpp').read_text();b=(root/'ProtectedFixes/CameraRuntime.cpp').read_text();af=functions(a);bf=functions(b)
shared=set(af)&set(bf)
for name in shared:
 assert norm(af[name])==norm(bf[name]),f'Changed camera function: {name}'
# Verify the complete original frame decision block, allowing only output bindings and D3D callback transport.
x=a[a.index(' B160M4 currentVp=b160matrix(&current,32,true)'):a.index(' HRESULT hr=valid?B173SubmitGpuFrame')]
callbacks=[]
pat=re.compile(r'auto\*\*\*\w+=reinterpret_cast<void\*\*\*>\(context\);if\(')
while (m:=pat.search(x)):
 start=m.start();brace=x.index('{',m.end());masked=mask(x);depth=1;i=brace+1
 while depth:
  if masked[i]=='{':depth+=1
  elif masked[i]=='}':depth-=1
  i+=1
 code=x[start:i];name=f'drawTransport{len(callbacks)}';callbacks.append((name,code));x=x[:start]+f'cameraTransport.{name}(context, '+('selectedResource' if 'selectedResource' in code else 'nullptr')+');'+x[i:]
x=x.replace(' bool valid=b160inverse',' bool& valid=result.valid; valid=b160inverse',1).replace(' float currentVpF[16]{},currentInverseVpF[16]{},currentViewF[16]{},currentProjectionF[16]{};',' auto& currentVpF=result.vp;auto& currentInverseVpF=result.inverseVp;auto& currentViewF=result.view;auto& currentProjectionF=result.projection;',1).replace('QueryPerformanceCounter(&b306AfterCamera);','QueryPerformanceCounter(&result.afterCamera);')
assert norm(x)==norm(bf['GhostingSelectCamera']),'Camera frame decisions differ from original'
helper=(root/'B25DepthHelper/B25DepthHelper.cpp').read_text();hf=functions(helper)
for name,code in callbacks:assert norm(hf[name])==norm(code),f'Changed transport callback {name}'
# Raw shader bodies are compared byte-for-byte, not reconstructed from their description.
orig=(base/'B140aDirectPresentSharedOpen.cpp').read_text();new=(root/'ProtectedFixes/TemporalRuntime.cpp').read_text()
raw=re.compile(r'R"([^ ()\\\t\r\n]{0,16})\((.*?)\)\1"',re.S)
oldsh=[m[2] for m in raw.finditer(orig) if 'currentInverseViewProjection' in m[2] or 'Texture2D<float2> nativeMotion' in m[2]]
newsh=[m[2] for m in raw.finditer(new)]
assert sorted(oldsh)==sorted(newsh) and len(newsh)==4,'Motion shader source differs'
assert hashlib.sha256((root/'ProtectedFixes/FreezingFix.cpp').read_bytes()).hexdigest().upper()=='528D8F37E0A46770BC689924BFBC2A10D2E17C01B3998678F8B6CB8E3C362448','B423 source changed'
print(f'PASS: {len(shared)} camera functions, full camera frame decisions, {len(callbacks)} D3D callbacks, four exact shaders, B423 hash')
