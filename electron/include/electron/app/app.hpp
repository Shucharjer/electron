#pragma once
#include <exec/static_thread_pool.hpp>
#include <neutron/execution.hpp>
#include <proton/command_buffer.hpp>
#include <proton/registry.hpp>
#include <proton/stage.hpp>
#include <proton/world.hpp>
#include "electron/app/config.hpp"

namespace electron {

class app {
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

    static app create() { return {}; }

    template <auto... Worlds>
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
        std::vector<command_buffer<>> cmdbufs{ concurrency };

        auto worlds = make_worlds<Worlds...>();

        call_startup(sch, cmdbufs, worlds);

        while (true) {
            _poll_events(pimpl);
            if (_is_stopped(pimpl)) [[unlikely]] {
                break;
            }
            call_update(sch, cmdbufs, worlds);

            _render_begin(pimpl);
            call<render>(sch, cmdbufs, worlds);
            _render_end(pimpl);
        }

        call<shutdown>(sch, cmdbufs, worlds);
        _destroy_impl(pimpl);
    }
};

} // namespace electron
