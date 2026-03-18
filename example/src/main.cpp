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

void observe_app(neutron::insertion<const App::insertion&> app) {
    [[maybe_unused]] auto& [insertion] = app;
}

int main() {
    constexpr auto world = world_desc | enable_render |
                           add_systems<render, &renderHello> |
                           add_systems<pre_update, &observe_app> |
                           add_systems<pre_update, &printFps>;
    using desc_t      = std::remove_cvref_t<decltype(world)>;
    using resources   = typename descriptor_traits<desc_t>::resources;
    const auto config = wnd_config{ .name   = "example",
                                    .width  = 1280,
                                    .height = 960,
                                    .flags  = window_flags::none };
    App::Create() | run_worlds<world>(config);

    return 0;
}
