from pathlib import Path
p=Path('B25DepthHelper/B140aDirectPresentSharedOpen.cpp');s=p.read_text();Path('backups/pre-B557-source').mkdir(exist_ok=True);Path('backups/pre-B557-source/B140aDirectPresentSharedOpen.cpp').write_text(s)
s=s.replace('#include "ArtifactDiagnostics.h"','#include "ArtifactDiagnostics.h"\n#include "FrameTransportPolicy.h"')
start=s.index(' slot->deferredContext->UpdateSubresource(slot->nativeMotionConstants.Get()');end=s.index(' QueryPerformanceCounter(&afterMotionSubmit);',start)
block=s[start:end];s=s[:start]+''' D3D11_TEXTURE2D_DESC captureColorTransport{};g_b173iColorSlots[slotIndex].shared->GetDesc(&captureColorTransport);
 const bool needsCaptureMotion=NeedsCaptureMotion(g_b173iColorSlots[slotIndex].sourceDesc,captureColorTransport);
 if(needsCaptureMotion){
'''+block+''' }
'''+s[end:]
needle=' job.color=colorSlot.output;';assert s.count(needle)==1;s=s.replace(needle,''' // B557: every preparation GPU read, including colour, follows capture completion.
 if(!job.nativeFenceValue||!g_b173fSharedFence12||!g_b174iPreparationQueue)return fail("ValidateNativeCaptureFence",E_UNEXPECTED);
 HRESULT nativeInputWait=g_b174iPreparationQueue->Wait(g_b173fSharedFence12.Get(),job.nativeFenceValue);
 if(FAILED(nativeInputWait))return fail("QueueNativeCaptureWait",nativeInputWait);
'''+needle)
s=s.replace('g_b174eWorkerOn12->ReleaseWrappedResources(colorResources,1);g_b174eWorkerOn12Context->Flush();QueryPerformanceCounter(&afterColor);','g_b174eWorkerOn12->ReleaseWrappedResources(colorResources,1);QueryPerformanceCounter(&afterColor);')
old=' if(job.nativeFenceValue&&g_b173fSharedFence12)g_b174iPreparationQueue->Wait(g_b173fSharedFence12.Get(),job.nativeFenceValue);';assert old in s;s=s.replace(old,' // Native input wait is enqueued before colour and motion, above.');old='signalHr=g_queue12->Wait(g_b173fSharedFence12.Get(),nativeFenceValue);';assert old in s;s=s.replace(old,'/* B557: consumer queues wait only for inputs they actually consume. */')
old='source.Width==transport.Width&&source.Height==transport.Height&&source.Format==transport.Format&&source.MipLevels==transport.MipLevels&&source.ArraySize==transport.ArraySize&&source.SampleDesc.Count==transport.SampleDesc.Count&&source.SampleDesc.Quality==transport.SampleDesc.Quality';assert old in s;s=s.replace(old,'NativeColorCopyCompatible(source,transport)')
s=s.replace('B555: CAMERA_CAPTURE_CPU_MIRROR fallback_256B=1, valid_stationary_lock_preserved=1, bounded_recovery=6, native_color=B548, full_resolution_dilation=B549, queue_policy=confirmed_B550','B557: ORDERED_FRAME_TRANSPORT native_capture_to_preparation_to_generation=1, wait_before_color=1, producer_generation_waits=0, unused_native_color_motion_dispatch=0, combined_preparation_flush=1, camera_policy=B555, dilation=B549')
p.write_text(s)
p=Path('Fast-Build.ps1');p.write_text(p.read_text().replace('B556','B557'))
