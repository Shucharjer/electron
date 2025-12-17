#pragma once
#include <exec/static_thread_pool.hpp>
#include <neutron/execution.hpp>
#include <proton/command_buffer.hpp>
#include <proton/registry.hpp>
#include <proton/stage.hpp>
#include <proton/world.hpp>
#include "electron/app/config.hpp"
#include "electron/resources/VulkanContext.hpp"
#include "electron/systems/render.hpp"

namespace electron {

class App {
    class impl;
    static impl* _create_impl();
    static bool _init_impl(impl*, const wnd_config&);
    static void _poll_events(impl*);
    static bool _is_stopped(impl*);
    static void _render_begin(impl*);
    static void _render_end(impl*);
    static void _destroy_impl(impl*);

public:
    using config_type = std::tuple<wnd_config>;

    static App create() { return {}; }

    template <auto OriginalWorld>
    static consteval auto mixin() noexcept {
        using namespace proton;
        using namespace systems;
        using enum stage;

        auto renders = add_system<startup, &startup_render> |
                       add_system<render, &render_system> |
                       add_system<shutdown, &shutdown_render>;

        return OriginalWorld | renders;
    }

    template <auto World>
    void run(auto&& tup) {
        using namespace neutron::execution;
        using namespace proton;
        using enum stage;
        auto& [config]    = tup;
        auto* const pimpl = _create_impl();

        if (!_init_impl(pimpl, config)) {
            return;
        }

        auto thread_pool   = exec::static_thread_pool{};
        scheduler auto sch = thread_pool.get_scheduler();

        const auto concurrency = thread_pool.available_parallelism();
        std::vector<command_buffer<>> cmdbufs(concurrency);

        constexpr auto descriptor = mixin<World>();
        proton::world auto world  = make_world<descriptor>();

        auto [vkContext] = res<VulkanContext&>(world);

        call_startup(sch, cmdbufs, world);

        while (true) {
            _poll_events(pimpl);
            if (_is_stopped(pimpl)) [[unlikely]] {
                break;
            }
            call_update(sch, cmdbufs, world);

            _render_begin(pimpl);
            call<render>(sch, cmdbufs, world);
            _render_end(pimpl);
        }

        call<shutdown>(sch, cmdbufs, world);
        _destroy_impl(pimpl);
    }
};

} // namespace electron
