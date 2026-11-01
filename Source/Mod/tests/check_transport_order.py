from pathlib import Path
s=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp').read_text()
p=s[s.index('static HRESULT b174c_prepare_worker_inputs('):s.index('static bool b173h_take_live_job(')]
assert p.index('g_b174iPreparationQueue->Wait(')<p.index('AcquireWrappedResources(colorResources'), 'native dependency must precede colour acquire/read'
assert p.count('g_b174eWorkerOn12Context->Flush()')==1, 'one preparation flush'
producer=s[s.index('extern "C" __declspec(dllexport) HRESULT WINAPI B171PresentLiveFrame('):]
assert 'g_queue12->Wait(' not in producer, 'render producer must not enqueue generation waits'
assert 'job.nativeFenceValue=nativeFenceValue;' in producer, 'capture completion reaches queue and retirement'
worker=s[s.index('static HRESULT b173h_present_live_job('):s.index('struct B160MotionWork')]
assert worker.index('g_queue12->Wait(')<worker.index('xefgSwapChainTagFrameConstants('), 'generation waits for prepared resources'
assert 'if(needsCaptureMotion){' in s and 'NeedsCaptureMotion(g_b173iColorSlots[slotIndex].sourceDesc,captureColorTransport)' in s, 'native colour motion bypass wired'
assert 'if(NativeColorCopyCompatible(source,transport))' in s, 'colour and motion share compatibility predicate'
print('PASS: production wait-before-colour, one flush, no producer generation wait, retirement fence, FG dependency, guarded native-motion bypass')
