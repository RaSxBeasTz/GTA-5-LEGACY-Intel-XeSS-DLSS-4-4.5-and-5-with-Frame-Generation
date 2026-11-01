#include <cstdint>

extern "C" __declspec(noinline) std::uint64_t GTAXeSS_B1_DormantLayoutProbe(std::uint64_t value)
{
    value ^= 0x9E3779B97F4A7C15ull;
    value *= 0xBF58476D1CE4E5B9ull;
    value ^= value >> 31;
    return value;
}

#pragma comment(linker, "/include:GTAXeSS_B1_DormantLayoutProbe")
