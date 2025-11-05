#include <goodboy/bus.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

void mbcReset(U8 rom[]) { (void)rom; }

U8 mbcRead(U8 rom[], U16 addr) { return rom[addr]; }

void mbcWrite(U8 rom[], U16 addr, U8 val) { rom[addr] = val; }

typedef struct {
    UInt counter;
    U8   p1;
    Bool quit;
} InputState;

void inputReset(InputState* state) { (void)state; }

void inputTick(InputState* state) {
    ++state->counter;
    if (state->counter > (4194304 / 60)) {
        state->counter = 0;
        SDL_PumpEvents();
        U8 const* keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_ESCAPE]) {
            state->quit = true;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                state->quit = true;
            }
        }
    }
}

U8 inputRead(InputState* state, U16 addr) {
    (void)addr;
    return state->p1;
}

void inputWrite(InputState* state, U16 addr, U8 val) {
    (void)state;
    (void)addr;
    (void)val;
    TODO();
}

int main(int argc, char* argv[]) {
    (void)argc;
    int exitcode = EXIT_FAILURE;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "Failed to initialize subsystems: %s\n",
                SDL_GetError());
        goto cleanupSDL;
    }
    SDL_Window* win = SDL_CreateWindow(
        "goodboy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * 4, SCREEN_HEIGHT * 4, SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        goto cleanupSDL;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        goto cleanupWindow;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!texture) {
        fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
        goto cleanupRenderer;
    }
    U8    rom[0x8000] = {0};
    FILE* file        = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "Failed to open ROM file: %s\n", strerror(errno));
        goto cleanupRenderer;
    }
    if (fread(rom, 1, sizeof(rom), file) == 0) {
        int err = ferror(file);
        if (err) {
            fprintf(stderr, "Failed to read ROM file: %s\n", strerror(err));
            goto cleanupRenderer;
        }
    }
    InputState istate = {0};
    ;
    Bus bus = {
        .cart =
            {
                .state = &rom,
                .reset = (DevResetFn)mbcReset,
                .tick  = DEV_NULL.tick,
                .read  = (DevReadFn)mbcRead,
                .write = (DevWriteFn)mbcWrite,
            },
        .input =
            {
                .state = &istate,
                .reset = (DevResetFn)inputReset,
                .tick  = (DevTickFn)inputTick,
                .read  = (DevReadFn)inputRead,
                .write = (DevWriteFn)inputWrite,
            },
        .serial = DEV_NULL,
    };
    busReset(&bus);

    UInt frames = 0;
    U64  last   = SDL_GetTicks64();
    while (!istate.quit) {
        if (busTick(&bus)) {
            void* pixels;
            int   pitch;
            SDL_LockTexture(texture, NULL, &pixels, &pitch);
            memcpy(pixels, bus.ppu.pixels, sizeof(bus.ppu.pixels));
            SDL_UnlockTexture(texture);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            ++frames;
        }
        U64 now = SDL_GetTicks64();
        if ((now - last) >= 1000) {
            char title[64];
            snprintf(title, sizeof(title), "goodboy - %" UINT_FMT " fps",
                     frames);
            SDL_SetWindowTitle(win, title);
            frames = 0;
            last   = now;
        }
    }

    exitcode = EXIT_SUCCESS;
    SDL_DestroyTexture(texture);
cleanupRenderer:
    SDL_DestroyRenderer(renderer);
cleanupWindow:
    SDL_DestroyWindow(win);
cleanupSDL:
    SDL_Quit();
    return exitcode;
}
