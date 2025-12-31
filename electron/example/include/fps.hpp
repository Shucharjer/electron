#include <chrono>
#include <cstdint>
#include <proton/args/system/local.hpp>
#include "neutron/print.hpp"

using duration = std::chrono::microseconds;
using ticks_t  = uint32_t;

void printFps(
    proton::local<
        std::chrono::time_point<std::chrono::steady_clock>, duration, ticks_t>
        local) {
    using namespace std::chrono;
    auto [tp, dur, ticks] = local;

    ++ticks;
    auto now     = high_resolution_clock::now();
    auto elapsed = duration_cast<microseconds>(now - tp);
    dur += elapsed;
    if (dur >= 1s) {
        neutron::println("fps: {}", ticks);
        dur   = 0us;
        ticks = 0;
    }
    tp = now;
}
