#pragma once
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
        auto& [config]    = tup;
        auto* const pimpl = _create_impl();

        // auto worlds = make_worlds<Worlds...>();
        if (!_init_impl(pimpl, config)) {
            return;
        }
        while (true) {
            _poll_events(pimpl);
            if (_is_stopped(pimpl)) [[unlikely]] {
                break;
            }

            _render_begin(pimpl);
            // render
            _render_end(pimpl);
        }

        _destroy_impl(pimpl);
    }
};

} // namespace electron
