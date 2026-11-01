from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();d=Path('backups/pre-B560-source');d.mkdir(exist_ok=True);(d/p.name).write_text(s)
needle='signalHr=g_queue12->Wait(g_b173fSharedFence12.Get(),nativeFenceValue);';assert s.count(needle)==1
replacement='/* B560: worker preparation waits on capture; generation waits on preparation. */'
s=s.replace(needle,replacement);p.write_text(s)
assert s.replace(replacement,needle)==(d/p.name).read_text()
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B559','B560'))
print('PASS: sole runtime delta removes producer generation-queue wait')
