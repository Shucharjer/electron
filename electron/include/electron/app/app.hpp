#pragma once
#include <exec/static_thread_pool.hpp>
#include <neutron/ecs.hpp>
#include <neutron/execution.hpp>
#include "electron/app/config.hpp"
#include "electron/resources/VulkanContext.hpp"
#include "electron/systems/render.hpp"

namespace electron {

class App {
    class Impl;
    static Impl* _create_impl();
    static VulkanContext* _init_impl(Impl*, const wnd_config&);
    static bool _poll_events(Impl*);
    static bool _is_stopped(Impl*);
    static void _render_begin(Impl*);
    static void _render_end(Impl*);
    static void _destroy_impl(Impl*);

public:
    using config_type = std::tuple<wnd_config>;

    static App Create() { return {}; }

    template <auto OriginalWorld>
    static consteval auto Mixin() noexcept {
        using namespace neutron;
        using namespace systems;
        using enum stage;

        auto renders = add_system<startup, &startup_render> |
                       add_system<render, &render_system> |
                       add_system<shutdown, &shutdown_render>;

        return OriginalWorld | renders;
    }

    template <auto World>
    void run(auto&& tup) {
        using namespace neutron;
        using namespace neutron::execution;
        using enum stage;
        auto& [config]    = tup;
        auto* const pimpl = _create_impl();

        auto* pVkContext = _init_impl(pimpl, config);
        if (pVkContext == nullptr) {
            return;
        }

        auto thread_pool   = exec::static_thread_pool{};
        scheduler auto sch = thread_pool.get_scheduler();

        const auto concurrency = thread_pool.available_parallelism();
        std::vector<command_buffer<>> cmdbufs(concurrency);

        constexpr auto descriptor = Mixin<World>();
        neutron::world auto world  = make_world<descriptor>();

        auto [vkContext] = res<VulkanContext&>(world);
        vkContext        = *pVkContext;

        call_startup(sch, cmdbufs, world);

        while (true) {
            if (!_poll_events(pimpl)) [[unlikely]] {
                continue;
            }
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
