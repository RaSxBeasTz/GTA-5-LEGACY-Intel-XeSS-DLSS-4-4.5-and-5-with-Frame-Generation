from pathlib import Path
import hashlib,json,re
backup=Path('backups/pre-B555-source');backup.mkdir(exist_ok=True)
for name in ['ProtectedFixes/CameraRuntime.cpp','ProtectedFixes/RuntimeContract.cpp','ProtectedFixes/B535-source-lock.json','B25DepthHelper/GhostingExpected.h','B25DepthHelper/B140aDirectPresentSharedOpen.cpp','B25DepthHelper/B25DepthHelper.cpp']:
 (backup/Path(name).name).write_bytes(Path(name).read_bytes())
p=Path('ProtectedFixes/CameraRuntime.cpp');s=p.read_text();start=s.index('    // B429: B428 runtime proved');end=s.index('\n   }\n   else if(b343SelectedActuallyMoving)',start)
s=s[:start]+'''    // B555: a validated camera with fresh writes can legitimately stand still.
    // Do not quarantine it solely for 120 zero-delta frames. That used to
    // clear the lock and leave the expensive classifier running indefinitely.
    if(b343FalseStaticFrames==120){std::ofstream stable(path,std::ios::app);stable<<"B555: VALID_STATIONARY_LOCK_RETAINED source_frame_id="<<sourceId<<", stationary_frames=120, direct_camera_preserved=1, reacquire_armed=0\\n";}
'''+s[end:]
s=s.replace('if(false&&g_b430ResponsiveReacquireActive.load','if(g_b430ResponsiveReacquireActive.load');p.write_text(s)
p=Path('B25DepthHelper/B25DepthHelper.cpp');s=p.read_text();needle='if(bytes){t_pending.resource=r;t_pending.data=m->pData;';replacement='if(bytes){if(bytes==256&&sub==0){void* mirror=t_cameraMirror.begin(m->pData,r,c);if(mirror)m->pData=mirror;}t_pending.resource=r;t_pending.data=m->pData;';assert s.count(needle)==1;s=s.replace(needle,replacement);p.write_text(s)
# Rebuild on confirmed native-colour/temporal/pacing baseline, not B552-B554 queue experiments.
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=Path('backups/B548-confirmed-ghosting-free/B140aDirectPresentSharedOpen.cpp').read_text();needle=' const char*source=GhostingMotionShader1();';assert needle in s;s=s.replace(needle,needle+'\n out<<"B555: CAMERA_CAPTURE_CPU_MIRROR fallback_256B=1, valid_stationary_lock_preserved=1, bounded_recovery=6, native_color=B548, full_resolution_dilation=B549, queue_policy=confirmed_B550\\n";');p.write_text(s)
# Camera and temporal source both contribute to this policy contract.
h=hashlib.sha256(Path('ProtectedFixes/CameraRuntime.cpp').read_bytes()+Path('ProtectedFixes/TemporalRuntime.cpp').read_bytes()).hexdigest()
p=Path('ProtectedFixes/RuntimeContract.cpp');s=p.read_text().replace('return 549','return 555');s=re.sub(r'return "[0-9a-f]{64}"',f'return "{h}"',s);p.write_text(s)
p=Path('B25DepthHelper/GhostingExpected.h');s=p.read_text().replace('549u','555u');s=re.sub(r'GHOSTING_EXPECTED_CONTRACT "[^"]+"',f'GHOSTING_EXPECTED_CONTRACT "{h}"',s);p.write_text(s)
p=Path('ProtectedFixes/B535-source-lock.json');lock=json.loads(p.read_text());
for name in ['ProtectedFixes/CameraRuntime.cpp','ProtectedFixes/RuntimeContract.cpp']:lock[name]=hashlib.sha256(Path(name).read_bytes()).hexdigest()
p.write_text(json.dumps(lock,indent=2)+'\n')
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B554','B555'))
