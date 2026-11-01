#include "../ScriptHookProbe/SourcePacer.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static double now(){LARGE_INTEGER q{},f{};QueryPerformanceCounter(&q);QueryPerformanceFrequency(&f);return double(q.QuadPart)*1000/f.QuadPart;}
int main(){try{
 check(SourceCadence::fps(0)==0&&SourceCadence::fps(-5)==0,"disabled cap");check(SourceCadence::fps(1)==20&&SourceCadence::fps(30)==30&&SourceCadence::fps(1000)==120,"cap bounds");
 SourceCadence c;c.advance(100,33);check(c.remaining(110)==23&&c.remaining(150)==0,"deadline arithmetic");c.advance(300,33);check(c.next==333,"late frames do not catch up");c.reset();check(c.next==0,"idle clears deadline");
 SourcePacer p;auto active=[](){return true;};std::vector<double> gaps;double last=now();p.pace(30,active);last=now();for(int i=0;i<15;++i){Sleep(i%3);p.pace(30,active);double t=now();gaps.push_back(t-last);last=t;}
 auto range=std::minmax_element(gaps.begin(),gaps.end());check(*range.first>=31.0&&*range.second<120,"actual source intervals respect 30FPS cap");
 double t=now();p.pace(0,active);check(now()-t<50,"uncapped does not wait");p.pace(30,active);t=now();p.pace(30,[](){return false;});check(now()-t<50,"disabled or unfocused does not wait");
 t=now();p.pace(30,active);check(now()-t<20,"resume starts fresh without stale deadline");
 t=now();p.pace(30,[&](){return now()-t<8;});check(now()-t<30,"focus loss interrupts timer wait");
 p.pace(30,active);Sleep(80);t=now();p.pace(30,active);check(now()-t<20,"slow render does not add another full delay");t=now();p.pace(30,active);check(now()-t>=31,"no burst after slow render");
 std::cout<<"PASS: deadline, cap bounds, disabled/focus reset, cancellation, no catchup; actual 30FPS intervals ms min="<<*range.first<<" max="<<*range.second<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
