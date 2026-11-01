#pragma once
#include <windows.h>
#include <string>
#include <atomic>
struct RuntimePreferences {
 bool hud=true,fps=true,frameTimes=false,usage=true,status=true;
 static RuntimePreferences get(){
  static std::atomic<ULONGLONG> next{0};static std::atomic<unsigned> flags{27};auto now=GetTickCount64();auto due=next.load(std::memory_order_relaxed);
  if(now>=due&&next.compare_exchange_strong(due,now+250)){
  wchar_t file[32768]{};GetModuleFileNameW(nullptr,file,32768);std::wstring path(file);path=path.substr(0,path.find_last_of(L"\\/")+1)+L"GTAXeSS.ini";
  auto read=[&](const wchar_t*key,int fallback){return GetPrivateProfileIntW(L"HUD",key,fallback,path.c_str())!=0;};
  unsigned value=(read(L"Enabled",1)?1:0)|(read(L"ShowFPS",1)?2:0)|(read(L"ShowFrameTimes",0)?4:0)|(read(L"ShowUsage",1)?8:0)|(read(L"ShowStatus",1)?16:0);flags.store(value,std::memory_order_release);
  }
  auto bits=flags.load(std::memory_order_acquire);return {(bits&1)!=0,(bits&2)!=0,(bits&4)!=0,(bits&8)!=0,(bits&16)!=0};
 }
};
