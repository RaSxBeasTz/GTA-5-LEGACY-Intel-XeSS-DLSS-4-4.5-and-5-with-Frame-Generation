#pragma once
#include <cmath>
inline bool CameraMoving(double vd,double pd,double vpd){return vd>0.001&&vd<=500.0&&pd<=2.0&&vpd<=500.0;}
inline bool PreferCoherentCamera(bool have,double oldVd,double oldPd,double oldVpd,double oldScore,unsigned long long oldSerial,double vd,double pd,double vpd,double score,unsigned long long serial){
 if(!have)return true;
 const bool moving=CameraMoving(vd,pd,vpd),oldMoving=CameraMoving(oldVd,oldPd,oldVpd);
 if(moving!=oldMoving)return moving;
 return score<oldScore||(std::fabs(score-oldScore)<1.0e-9&&serial>oldSerial);
}
