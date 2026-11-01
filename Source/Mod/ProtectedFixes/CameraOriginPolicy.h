#pragma once
#include <cmath>
// A same-frame translation-free pass is auxiliary only when a translated twin
// independently agrees on orientation and projection. Never reject origin alone.
inline bool CameraOriginTwin(const double* origin,const double* translated,const double* projection,const double* otherProjection){
 for(unsigned i=0;i<16;++i)if(!std::isfinite(origin[i])||!std::isfinite(translated[i])||!std::isfinite(projection[i])||!std::isfinite(otherProjection[i]))return false;
 if(std::fabs(origin[15]-1)>1e-5||std::fabs(translated[15]-1)>1e-5)return false;
 double originPosition=0,otherPosition=0;
 for(unsigned i=12;i<15;++i){originPosition+=origin[i]*origin[i];otherPosition+=translated[i]*translated[i];}
 if(originPosition>1e-10||otherPosition<1.0)return false;
 for(unsigned i=0;i<12;++i)if(std::fabs(origin[i]-translated[i])>1e-5)return false;
 for(unsigned i=0;i<16;++i)if(std::fabs(projection[i]-otherProjection[i])>1e-4*(1+std::fabs(projection[i])))return false;
 return true;
}
