#include <stdlib.h>

#include <SDL2/SDL.h>

int main() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Failed to initialize SDL video subsystem: %s\n",
                SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Quit();
    return EXIT_SUCCESS;
}
