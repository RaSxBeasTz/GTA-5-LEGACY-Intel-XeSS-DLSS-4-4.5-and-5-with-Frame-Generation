#include <windows.h>
#include <dxgi.h>
// GTAXeSS B423 LOCKED FREEZING POLICY MODULE.
// This file owns the accepted anti-freeze/stutter policy. The main XeFG runtime only
// performs generic queue/presentation mechanics and queries this DLL for policy decisions.
// LOCKED after B422 acceptance: do not edit while working on ghosting/upscaling/other features.
extern "C" __declspec(dllexport) unsigned __stdcall GTAXeSSFreezingApiVersion(){return 3;}
extern "C" __declspec(dllexport) unsigned __stdcall GTAXeSSFreezeBufferCount(){return 3;}
extern "C" __declspec(dllexport) unsigned __stdcall GTAXeSSFreezeMaximumFrameLatency(){return 3;}
extern "C" __declspec(dllexport) unsigned __stdcall GTAXeSSFreezePresentFlags(BOOL allowTearing){return (allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0) | DXGI_PRESENT_DO_NOT_WAIT;}
extern "C" __declspec(dllexport) unsigned __stdcall GTAXeSSFreezePendingAdmissionLimit(){return 5;}
extern "C" __declspec(dllexport) unsigned __stdcall GTAXeSSFreezeMaximumQueuedAgeMs(){return 25;}
extern "C" __declspec(dllexport) BOOL __stdcall GTAXeSSFreezeCollapseQueuedWorkerJobs(unsigned pendingCount, double oldestAgeMs){return pendingCount>1 && oldestAgeMs>=25.0;}
extern "C" __declspec(dllexport) BOOL __stdcall GTAXeSSFreezeCollapsePendingOnSourceGap(unsigned pendingCount, double sourceEntryGapMs){return pendingCount>0 && sourceEntryGapMs>=20.0;}
extern "C" __declspec(dllexport) BOOL __stdcall GTAXeSSFreezeInitialRevealAllowed(BOOL validX4Latched, unsigned long long elapsedMs, BOOL result3){
    (void)result3;
    return (elapsedMs>=1500 && validX4Latched) ? TRUE : FALSE;
}
