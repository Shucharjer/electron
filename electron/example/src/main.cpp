#include <type_traits>
#include <imgui.h>
#include <neutron/ecs.hpp>
#include "electron/app/app.hpp"
#include "electron/app/config.hpp"
#include "fps.hpp"
#include "hello_world.hpp"

using namespace neutron;
using namespace electron;
using enum stage;

int main() {
    constexpr auto world = world_desc | add_system<render, &renderHello> |
                           add_system<pre_update, &printFps>;
    using desc_t      = std::remove_cvref_t<decltype(world)>;
    using resources   = typename descriptor_traits<desc_t>::resources;
    const auto config = wnd_config{ .name   = "example",
                                    .width  = 1280,
                                    .height = 960,
                                    .flags  = window_flags::none };
    App::Create() | run_worlds<world>(config);

    return 0;
}
