#pragma once
#include <cmath>

// The protected stall policy measures *unexpected* source delay. The source
// pacer intentionally spaces captures by one frame period; that period must
// not consume the policy's 20 ms stall budget. Uncapped mode keeps old units.
inline double UnexpectedSourceGapMs(double actualGapMs,int configuredFps){
 if(!std::isfinite(actualGapMs)||actualGapMs<=0)return 0;
 if(configuredFps<=0)return actualGapMs;
 const int fps=configuredFps<20?20:(configuredFps>120?120:configuredFps);
 const double excess=actualGapMs-1000.0/fps;
 return excess>0?excess:0;
}
