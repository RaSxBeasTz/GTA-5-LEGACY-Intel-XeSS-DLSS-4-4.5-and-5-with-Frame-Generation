#include <windows.h>
// ACCEPTED B345/B343 GHOSTING IMPLEMENTATION - DO NOT MODIFY.
// Reopen only with explicit user approval.
extern "C" __declspec(dllexport) unsigned __stdcall GTAXeSSGhostingApiVersion(){return 1;}
extern "C" __declspec(dllexport) BOOL __stdcall GTAXeSSGhostingUnsafeFramePolicy(BOOL unsafeCamera){
    // B345/B343: every unsafe frame gets zero motion + temporal reset; no debounce/one-shot.
    return unsafeCamera ? TRUE : FALSE;
}
