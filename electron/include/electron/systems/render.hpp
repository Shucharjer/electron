#pragma once
#include <neutron/ecs.hpp>
#include "electron/resources/VulkanContext.hpp"

namespace electron::systems {

namespace _render {

using namespace neutron;

inline void startup_render(res<VulkanContext&> res) {
    //
}

inline void render_system(res<VulkanContext&> res) {
    //
}

inline void shutdown_render(res<VulkanContext&> res) {
    //
}

} // namespace _render

using _render::startup_render;
using _render::render_system;
using _render::shutdown_render;

} // namespace electron::systems
