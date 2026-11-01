from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();base=Path('backups/pre-B559-source/B140aDirectPresentSharedOpen.cpp').read_text()
block=''' // B559: the native fence covers both current colour and depth capture.
 if(!job.nativeFenceValue||!g_b173fSharedFence12||!g_b174iPreparationQueue)return fail("ValidateNativeCaptureFence",E_UNEXPECTED);
 const HRESULT captureWait=g_b174iPreparationQueue->Wait(g_b173fSharedFence12.Get(),job.nativeFenceValue);
 if(FAILED(captureWait))return fail("QueueNativeCaptureWait",captureWait);
'''
s=s.replace(block,'').replace(' // B559: capture dependency is enqueued before colour reads, above.',' if(job.nativeFenceValue&&g_b173fSharedFence12)g_b174iPreparationQueue->Wait(g_b173fSharedFence12.Get(),job.nativeFenceValue);').replace(' job.nativeFenceValue=nativeFenceValue; // B559: publish the capture completion dependency.\n','')
assert s==base,'Unexpected non-synchronization change'
print('PASS: exact B558 source outside capture-fence handoff/order changes')
