#include <chrono>
#include <concepts>
#include <cstdint>
#include <neutron/ecs.hpp>
#include <neutron/print.hpp>

using duration = std::chrono::microseconds;
using ticks_t  = uint32_t;

constexpr auto b = std::default_initializable<
    std::chrono::time_point<std::chrono::system_clock>>;

void printFps(
    neutron::local<
        std::chrono::time_point<std::chrono::system_clock>, duration, ticks_t>
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
