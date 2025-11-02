#include <stdlib.h>

#include <SDL2/SDL.h>

#include <goodboy/buf.h>
#include <goodboy/bus.h>

typedef struct {
    View rom;
    View sram;
} MbcState;

void mbcReset(MbcState* state) { (void)state; }

U8 mbcRead(MbcState* state, U16 addr) {
    (void)state;
    (void)addr;
    return 0;
}

void mbcWrite(MbcState* state, U16 addr, U8 val) {
    (void)state;
    (void)addr;
    (void)val;
}

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

int main() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "Failed to initialize SDL subsystems: %s\n",
                SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_Window*   win;
    SDL_Renderer* renderer;
    if (SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0, &win,
                                    &renderer)) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    MbcState   mbcstate = {0};
    InputState istate   = {0};

    ;

    Bus bus = {
        .mbc =
            {
                .state = &mbcstate,
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

    while (!istate.quit) {
        busTick(&bus);
    }

    SDL_Quit();
    return EXIT_SUCCESS;
}
