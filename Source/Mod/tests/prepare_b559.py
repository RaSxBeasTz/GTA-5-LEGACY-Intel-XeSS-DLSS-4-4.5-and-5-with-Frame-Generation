from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();d=Path('backups/pre-B559-source');d.mkdir(exist_ok=True);(d/p.name).write_text(s)
old=' if(job.nativeFenceValue&&g_b173fSharedFence12)g_b174iPreparationQueue->Wait(g_b173fSharedFence12.Get(),job.nativeFenceValue);';assert old in s;s=s.replace(old,' // B559: capture dependency is enqueued before colour reads, above.')
needle=' job.color=colorSlot.output;';assert s.count(needle)==1;s=s.replace(needle,''' // B559: the native fence covers both current colour and depth capture.
 if(!job.nativeFenceValue||!g_b173fSharedFence12||!g_b174iPreparationQueue)return fail("ValidateNativeCaptureFence",E_UNEXPECTED);
 const HRESULT captureWait=g_b174iPreparationQueue->Wait(g_b173fSharedFence12.Get(),job.nativeFenceValue);
 if(FAILED(captureWait))return fail("QueueNativeCaptureWait",captureWait);
'''+needle)
needle=' B173hLiveJob job=g_b174CaptureJob;';assert s.count(needle)==1;s=s.replace(needle,needle+'\n job.nativeFenceValue=nativeFenceValue; // B559: publish the capture completion dependency.');p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B558','B559'))
print('Synchronization-only delta: carry native fence and wait before worker colour; existing producer wait and flushes retained')
