#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cout << "SDL_Init Error: " << SDL_GetError() << '\n';
        return 1;
    }
    std::cout << "SDL init success!\n";

    SDL_Window* win = SDL_CreateWindow("Test", 1024, 1024, 0);
    if (win == nullptr) {
        std::cout << "Window Error: " << SDL_GetError() << '\n';
    } else {
        SDL_Delay(3000); // NOLINT
        SDL_DestroyWindow(win);
    }

    SDL_Quit();
    return 0;
}
