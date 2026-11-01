#include "../B25DepthHelper/PacingGap.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(){try{
 HMODULE module=LoadLibraryW(L"ProtectedFixes/x64/Release/GTAXeSS.FreezingFix.dll");check(module!=nullptr,"load unchanged safety DLL");using Gap=BOOL(__stdcall*)(unsigned,double);auto collapse=(Gap)GetProcAddress(module,"GTAXeSSFreezeCollapsePendingOnSourceGap");check(collapse!=nullptr,"load safety entry point");
 for(int fps:{30,40,45,50,60,120}){double period=1000.0/fps;check(!collapse(1,UnexpectedSourceGapMs(period+.5,fps)),"normal paced frame is not a stall");check(!collapse(1,UnexpectedSourceGapMs(period+19.9,fps)),"below stall budget preserves frame");check(collapse(1,UnexpectedSourceGapMs(period+20.1,fps)),"genuine excess delay preserves safety reset");check(!collapse(0,UnexpectedSourceGapMs(period+100,fps)),"empty queue remains empty");}
 check(collapse(1,UnexpectedSourceGapMs(20,0))&& !collapse(1,UnexpectedSourceGapMs(19.9,0)),"uncapped policy unchanged");check(UnexpectedSourceGapMs(std::numeric_limits<double>::quiet_NaN(),50)==0&&UnexpectedSourceGapMs(-1,50)==0,"invalid intervals do not request collapse");
 std::ifstream recorded("tests/b553-source-gaps.txt");check(recorded.good(),"recorded gaps available");unsigned total=0,oldDrops=0,newDrops=0;double gap;while(recorded>>gap){++total;oldDrops+=collapse(1,gap)!=FALSE;newDrops+=collapse(1,UnexpectedSourceGapMs(gap,50))!=FALSE;}
 check(total>100&&oldDrops==total&&newDrops<total/10,"recorded normal50FPS gaps no longer trigger reset storm");
 std::cout<<"PASS: unchanged safety DLL, all caps, real stalls/empty/uncapped; recorded collapses="<<oldDrops<<" corrected="<<newDrops<<"\n";FreeLibrary(module);return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
