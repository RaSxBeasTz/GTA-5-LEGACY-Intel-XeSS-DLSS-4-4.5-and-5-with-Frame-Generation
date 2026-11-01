#include <atomic>
#include <cstdint>

namespace
{
    std::atomic<bool> g_b2_install_requested{ false };
    std::atomic<std::uint32_t> g_b2_active_calls{ 0 };
    std::atomic<std::uint32_t> g_b2_completed_presents{ 0 };
    thread_local bool g_b2_reentrancy_guard = false;
}

extern "C" __declspec(noinline) std::uintptr_t GTAXeSS_B2_InactiveStateProbe()
{
    const auto a = g_b2_install_requested.load(std::memory_order_relaxed) ? 1u : 0u;
    const auto b = g_b2_active_calls.load(std::memory_order_relaxed);
    const auto c = g_b2_completed_presents.load(std::memory_order_relaxed);
    const auto d = g_b2_reentrancy_guard ? 1u : 0u;
    return static_cast<std::uintptr_t>(a + b + c + d);
}

#pragma comment(linker, "/include:GTAXeSS_B2_InactiveStateProbe")
