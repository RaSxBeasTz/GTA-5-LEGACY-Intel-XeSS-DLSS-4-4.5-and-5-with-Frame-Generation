#pragma once
#include <cstdint>
#include <cstddef>
namespace gta {
constexpr std::size_t NoReadyFrame=static_cast<std::size_t>(-1);
// Native capture signals are monotonic. Pick the latest COMPLETE packet;
// newer incomplete packets remain queued. Never wait on future producer work.
inline std::size_t LatestCompletedFrame(const std::uint64_t*fences,std::size_t count,std::uint64_t completed){
 if(!fences||completed==~std::uint64_t(0))return NoReadyFrame;
 std::size_t ready=NoReadyFrame;
 for(std::size_t i=0;i<count;++i)if(fences[i]&&fences[i]<=completed)ready=i;
 return ready;
}
}
