#include "../ScriptHookProbe/PreparationPolicy.h"
// Reproduce the log: idle -> loading-screen backbuffer change -> F9.
static_assert(SourceChangePreparation(0,false,false)==0);
static_assert(CanArmPreparation(SourceChangePreparation(0,false,false)));
static_assert(!CanStartLiveDiscovery(false,false));
static_assert(!CanStartLiveDiscovery(true,false));
static_assert(!CanStartLiveDiscovery(false,true));
static_assert(CanStartLiveDiscovery(true,true));
// Preserve in-progress XeSS and background bridge setup across recreation.
static_assert(SourceChangePreparation(7,true,false)==7);
static_assert(SourceChangePreparation(13,true,false)==13);
static_assert(SourceChangePreparation(8,true,false)==8);
// Retained initialized active sessions still rediscover.
static_assert(SourceChangePreparation(4,true,true)==10);
static_assert(SourceChangePreparation(4,false,true)==10);
// F9 can retry a completed failure, without rearming in-progress workers.
static_assert(CanArmPreparation(5));
static_assert(!CanArmPreparation(12));
static_assert(!CanArmPreparation(13));
int main(){return 0;}
