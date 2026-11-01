#pragma once
#include <algorithm>
#include <cmath>
// GTA uses row vectors: clip * inverseCurrentVP * previousVP.
// Measure motion after homogeneous division; raw VP coefficients depend on
// world origin and matrix scale and are not a screen-space cut metric.
inline bool ReprojectionCut(const float* previous,const float* inverse){
 double transform[16]{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)for(unsigned k=0;k<4;++k)transform[r*4+c]+=(double)inverse[r*4+k]*previous[k*4+c];
 const double points[5][2]={{0,0},{-.6,-.6},{.6,-.6},{-.6,.6},{.6,.6}};double movement[15]{};unsigned count=0;
 // Reversed depth: these samples represent roughly 150m, 30m and 7.5m
 // with GTA near=0.15. Near-plane samples mislabeled ordinary translation as cuts.
 for(double z:{0.001,0.005,0.02})for(const auto&pt:points){double v[4]{pt[0],pt[1],z,1},q[4]{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)q[c]+=v[r]*transform[r*4+c];
  if(!std::isfinite(q[3])||q[3]<=1e-6)return true;
  const double dx=q[0]/q[3]-pt[0],dy=q[1]/q[3]-pt[1];if(!std::isfinite(dx)||!std::isfinite(dy))return true;movement[count++]=std::sqrt(dx*dx+dy*dy);
 }
 std::sort(movement,movement+15);return movement[7]>1.0; // median exceeds half a screen in NDC
}
