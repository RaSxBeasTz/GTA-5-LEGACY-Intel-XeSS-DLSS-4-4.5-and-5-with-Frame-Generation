from pathlib import Path
import shutil,sys,os,ctypes,subprocess
r=Path(__file__).resolve().parent.parent
if len(sys.argv)>1:
 case=Path(sys.argv[1]).resolve();expected=sys.argv[2]=='accept'
 ctypes.windll.kernel32.SetErrorMode(0x0001|0x0002|0x8000)
 handles=[os.add_dll_directory(str(case)),os.add_dll_directory(str(Path(os.environ['LOCALAPPDATA'])/'GTAXeSS/xess/bin'))]
 try:
  dll=ctypes.WinDLL(str(case/'B25DepthHelper.dll'))
  fn=dll.B63bGetBuildIdentity;fn.restype=ctypes.c_char_p
  accepted=fn() is not None
 except OSError as e:
  accepted=False;print('loader status:',e.winerror)
 print(case.name, 'accepted' if accepted else 'rejected')
 sys.exit(0 if accepted==expected else 1)
for name,expect in [('matching','accept'),('wrong-hash','reject'),('old-api','reject'),('missing','reject')]:
 case=r/'tests/integrity'/name;case.mkdir(parents=True,exist_ok=True)
 shutil.copyfile(r/'B25DepthHelper/x64/Release/B25DepthHelper.dll',case/'B25DepthHelper.dll')
 if name!='missing':
  src=(r/'tests/fixtures/GhostingFix-v1.dll') if name=='old-api' else r/'ProtectedFixes/x64/Release/GTAXeSS.GhostingFix.dll'
  shutil.copyfile(src,case/'GTAXeSS.GhostingFix.dll')
  if name=='wrong-hash':
   with (case/'GTAXeSS.GhostingFix.dll').open('ab') as f:f.write(b'INCOMPATIBLE_BUILD_TEST')
 result=subprocess.run([sys.executable,__file__,str(case),expect],cwd=case)
 if result.returncode:raise SystemExit(result.returncode)
print('PASS: matching DLL accepted; changed hash, old API and missing DLL rejected')

