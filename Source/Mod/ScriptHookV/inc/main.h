#pragma once

#include <windows.h>

#define IMPORT __declspec(dllimport)

typedef void(*PresentCallback)(void*);
IMPORT void presentCallbackRegister(PresentCallback cb);
IMPORT void presentCallbackUnregister(PresentCallback cb);

typedef void(*KeyboardHandler)(DWORD, WORD, BYTE, BOOL, BOOL, BOOL, BOOL);
IMPORT void keyboardHandlerRegister(KeyboardHandler handler);
IMPORT void keyboardHandlerUnregister(KeyboardHandler handler);


// Standard ScriptHookV script-thread/native interfaces, exported by the bundled
// import library. Native calls must run on this cooperative game script thread.
IMPORT void scriptRegister(HMODULE module,void(*scriptMain)());
IMPORT void scriptUnregister(HMODULE module);
IMPORT void scriptWait(DWORD milliseconds);
IMPORT void nativeInit(unsigned long long hash);
IMPORT void nativePush64(unsigned long long value);
IMPORT unsigned long long* nativeCall();
