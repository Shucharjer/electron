#pragma once
#include <cstdint>
#include <string>

namespace electron {

enum class window_flags : uint8_t {
    none       = 0,
    fullscreen = 1U,
    resizable  = 1U << 1U,
    borderless = 1U << 2U,
    maximized  = 1U << 3U
};

constexpr window_flags
    operator|(const window_flags& lhs, const window_flags& rhs) noexcept {
    return static_cast<window_flags>(
        static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr bool operator&(window_flags lhs, window_flags rhs) noexcept {
    return (static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs)) != 0;
}

struct wnd_config {
    uint8_t framebufferCount = 2;
    std::string name;
    int width;
    int height;
    window_flags flags;
};

} // namespace electron
