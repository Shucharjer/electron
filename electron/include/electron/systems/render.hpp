#pragma once
#include <proton/args/common/commands.hpp>
#include <proton/args/common/query.hpp>
#include <proton/args/system/local.hpp>
#include <proton/args/system/res.hpp>
#include <proton/proton.hpp>
#include "electron/resources/VulkanContext.hpp"

namespace electron::systems {

using namespace proton;

inline void startup_render(res<VulkanContext&> res) {
    //
}

inline void render_system(res<VulkanContext&> res) {
    //
}

inline void shutdown_render(res<VulkanContext&> res) {
    //
}

} // namespace electron::systems
