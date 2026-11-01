import csv,math
r=list(csv.DictReader(open('G:/SteamLibrary/steamapps/common/Grand Theft Auto V/GTAXeSS-ArtifactFrames.csv')))
counts=[0,0]; last=None; n=0
for a in r:
 if a['camera_valid']!='1' or not a.get('inverse_vp15'): continue
 cur=[float(a['current_vp'+str(i)]) for i in range(16)]; inv=[float(a['inverse_vp'+str(i)]) for i in range(16)]
 if last:
  n+=1; d=[cur[i]-last[i] for i in range(16)]; counts[0]+=max(map(abs,d))>=2 or math.sqrt(sum(x*x for x in d)/16)>=.5
  t=[sum(inv[4*i+k]*last[4*k+j] for k in range(4)) for i in range(4) for j in range(4)]; moves=[]
  for z in [.001,.005,.02]:
   for x,y in [(0,0),(-.6,-.6),(.6,-.6),(-.6,.6),(.6,.6)]:
    v=[x,y,z,1]; q=[sum(v[k]*t[k*4+j] for k in range(4)) for j in range(4)]; moves.append(1e9 if q[3]<=1e-6 else math.hypot(q[0]/q[3]-x,q[1]/q[3]-y))
  counts[1]+=sorted(moves)[7]>1
 last=cur
print('valid adjacent pairs',n,'raw resets',counts[0],'reversed-depth screen cuts',counts[1])

