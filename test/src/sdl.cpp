#include <iostream>

#ifdef ELECTRON_USES_SDL3_VULKAN

    #include <SDL3/SDL.h>
    #include <SDL3/SDL_video.h>

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << '\n';
        return 1;
    }
    std::cout << "SDL init success!\n";

    const auto* const driver = SDL_GetCurrentVideoDriver();
    std::cout << "Current SDL video driver: "
              << (driver == nullptr ? "null" : driver) << '\n';

    SDL_Window* win = SDL_CreateWindow("Test", 1024, 1024, SDL_WINDOW_VULKAN);
    if (win == nullptr) {
        std::cout << "Window Error: " << SDL_GetError() << '\n';
    } else {
        SDL_Delay(3000); // NOLINT
        SDL_DestroyWindow(win);
    }

    SDL_Quit();
    return 0;
}

#else

    #include <SDL2/SDL.h>

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << '\n';
        return 1;
    }
    std::cout << "SDL init success!\n";

    const auto* const driver = SDL_GetCurrentVideoDriver();
    std::cout << "Current SDL video driver: "
              << (driver == nullptr ? "null" : driver) << '\n';

    SDL_Window* win = SDL_CreateWindow(
        "Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 1024,
        SDL_WINDOW_VULKAN);
    if (win == nullptr) {
        std::cout << "Window Error: " << SDL_GetError() << '\n';
    } else {
        SDL_Delay(3000); // NOLINT
        SDL_DestroyWindow(win);
    }

    SDL_Quit();
    return 0;
}

#endif
