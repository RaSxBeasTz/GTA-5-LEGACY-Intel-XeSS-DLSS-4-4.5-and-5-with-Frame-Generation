#pragma once
#include <windows.h>
#include <cstdint>

// Source cadence only. No worker/queue or temporal history ownership.
struct SourceCadence {
 long long next{};
 static unsigned fps(int value){return value<=0?0u:(unsigned)(value<20?20:(value>120?120:value));}
 void reset(){next=0;}
 long long remaining(long long now)const{return next>now?next-now:0;}
 void advance(long long actual,long long period){next=actual+period;} // Never catch up after a stall.
};

class SourcePacer {
 SourceCadence cadence;
 HANDLE timer{};
 long long frequency{};
public:
 SourcePacer(){LARGE_INTEGER f{};QueryPerformanceFrequency(&f);frequency=f.QuadPart;
  timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
  if(!timer)timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
 }
 ~SourcePacer(){if(timer)CloseHandle(timer);}
 template<class Active> double pace(unsigned fps,Active active){
  if(!fps||!frequency||!active()){cadence.reset();return 0;}
  LARGE_INTEGER now{};QueryPerformanceCounter(&now);const auto begin=now.QuadPart;
  while(cadence.remaining(now.QuadPart)>0){
   if(!active()){cadence.reset();return double(now.QuadPart-begin)*1000/frequency;}
   const auto remaining=cadence.remaining(now.QuadPart);
   LARGE_INTEGER due{};due.QuadPart=-((remaining*10000000+frequency-1)/frequency);
   if(timer&&SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)){
    // Recheck focus/F9 every 5 ms; do not busy-spin or hold resource locks.
    while(WaitForSingleObject(timer,5)==WAIT_TIMEOUT){if(!active()){CancelWaitableTimer(timer);cadence.reset();QueryPerformanceCounter(&now);return double(now.QuadPart-begin)*1000/frequency;}}
   }else Sleep((DWORD)((remaining*1000+frequency-1)/frequency));
   QueryPerformanceCounter(&now);
  }
  cadence.advance(now.QuadPart,(frequency+fps/2)/fps);
  return double(now.QuadPart-begin)*1000/frequency;
 }
};
