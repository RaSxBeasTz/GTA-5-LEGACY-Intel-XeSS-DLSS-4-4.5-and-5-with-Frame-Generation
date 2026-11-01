#pragma once
// Discovery requires an initialized bridge and an explicit enabled session.
constexpr unsigned SourceChangePreparation(unsigned current,bool enabled,bool bridgeReady){
 (void)enabled;return bridgeReady?10u:current;
}
constexpr bool CanStartLiveDiscovery(bool enabled,bool bridgeReady){return enabled&&bridgeReady;}
constexpr bool CanArmPreparation(unsigned current){return current==0u||current==5u;}
