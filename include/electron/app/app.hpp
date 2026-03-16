#pragma once
#include <memory>
#include <type_traits>
#include <vector>
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

        using descriptor_type = std::remove_cvref_t<decltype(OriginalWorld)>;

        if constexpr (render_info<descriptor_type>::is_enabled) {
            auto renders = add_systems<startup, &startup_render> |
                           add_systems<render, &render_system> |
                           add_systems<shutdown, &shutdown_render>;

            return OriginalWorld | renders;
        } else {
            return OriginalWorld;
        }
    }

    template <auto... Worlds>
    void run(auto&& tup) {
        using namespace neutron;
        using namespace neutron::execution;

        if constexpr (sizeof...(Worlds) == 0) {
            return;
        }

        auto& [config] = tup;
        std::unique_ptr<Impl, decltype(&App::_destroy_impl)> pimpl(
            _create_impl(), &App::_destroy_impl);

        auto* const p_vk_context = _init_impl(pimpl.get(), config);
        if (p_vk_context == nullptr) {
            return;
        }

        auto thread_pool   = exec::static_thread_pool{};
        scheduler auto sch = thread_pool.get_scheduler();

        const auto concurrency = thread_pool.available_parallelism();
        std::vector<command_buffer<>> cmdbufs(concurrency);
        auto worlds = make_worlds<Mixin<Worlds>()...>();
        scoped_global_binding<VulkanContext> vk_context_binding{
            *p_vk_context
        };

        struct _app_hooks {
            Impl* pimpl;

            bool poll_events() const { return App::_poll_events(pimpl); }
            bool is_stopped() const { return App::_is_stopped(pimpl); }
            void render_begin() const { App::_render_begin(pimpl); }
            void render_end() const { App::_render_end(pimpl); }
        };

        auto schedule = make_world_schedule(
            _app_hooks{ pimpl.get() }, sch, cmdbufs, worlds);
        schedule.run();
    }
};

} // namespace electron
