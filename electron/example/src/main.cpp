#include <proton/proton.hpp>
#include <proton/run.hpp>
#include <proton/system.hpp>
#include <proton/world.hpp>
#include "electron/app/app.hpp"
#include "electron/app/config.hpp"

using namespace proton;
using namespace electron;
using enum stage;

void print() {}

int main() {
    constexpr auto world = world_desc | add_system<update, &print>;
    const auto config    = wnd_config{ .name   = "example",
                                       .width  = 1280,
                                       .height = 960,
                                       .flags  = window_flags::none };
    App::create() | run_worlds<world>(config);

    return 0;
}
