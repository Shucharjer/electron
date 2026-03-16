#pragma once
#include <neutron/ecs.hpp>
#include "electron/resources/VulkanContext.hpp"

namespace electron::systems {

namespace _render {

using namespace neutron;

inline void startup_render(global<VulkanContext&> context) {
    [[maybe_unused]] auto& [vk_context] = context;
}

inline void render_system(global<VulkanContext&> context) {
    [[maybe_unused]] auto& [vk_context] = context;
}

inline void shutdown_render(global<VulkanContext&> context) {
    [[maybe_unused]] auto& [vk_context] = context;
}

} // namespace _render

using _render::startup_render;
using _render::render_system;
using _render::shutdown_render;

} // namespace electron::systems
