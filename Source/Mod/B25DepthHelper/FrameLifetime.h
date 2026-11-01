#pragma once
#include <cstdint>
namespace gta {
inline std::uint64_t LastStageUse(std::uint64_t present,std::uint64_t laterCopy){return present>laterCopy?present:laterCopy;}
inline bool StageUseCompleted(std::uint64_t completed,std::uint64_t required){return required&&completed!=UINT64_MAX&&completed>=required;}
}
