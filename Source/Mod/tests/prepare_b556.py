from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();needle=' B173hLiveJob job=g_b174CaptureJob;';assert s.count(needle)==1;assert 'job.nativeFenceValue=nativeFenceValue;' not in s;s=s.replace(needle,needle+'\n job.nativeFenceValue=nativeFenceValue; // B556: retain real GPU completion marker through queue/retirement.');p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B555','B556'))
