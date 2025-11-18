#include <goodboy/bus.h>

#include <SDL2/SDL.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    UInt      ticks;
    U8        p1;
    Bool      quit;
    U8 const* keys;
} InputState;

static void inputReset(InputState* state) {
    memset(state, 0, sizeof(*state));
    state->p1   = P1_MASK;
    state->keys = SDL_GetKeyboardState(NULL);
}

static void inputTick(InputState* state) {
    ++state->ticks;
    if (state->ticks > (CPU_FREQ_NORMAL / 60)) {
        state->ticks = 0;
        SDL_PumpEvents();
        if (state->keys[SDL_SCANCODE_ESCAPE]) {
            state->quit = TRUE;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                state->quit = TRUE;
                break;
            default:
                break;
            }
        }
    }
}

static U8 inputRead(InputState* state, U16 addr) {
    switch (addr) {
    case PORT_P1:
        return state->p1;
    default:
        UNREACHABLE();
    }
}

static void inputWrite(InputState* state, U16 addr, U8 val) {
    switch (addr) {
    case PORT_P1:
        if (!(val & P1_CTRL_DPAD)) {
            state->p1 |= P1_DPAD_MASK;
            if (state->keys[SDL_SCANCODE_RIGHT]) {
                state->p1 &= ~P1_RIGHT;
            }
            if (state->keys[SDL_SCANCODE_LEFT]) {
                state->p1 &= ~P1_LEFT;
            }
            if (state->keys[SDL_SCANCODE_UP]) {
                state->p1 &= ~P1_UP;
            }
            if (state->keys[SDL_SCANCODE_DOWN]) {
                state->p1 &= ~P1_DOWN;
            }
            return;
        }
        if (!(val & P1_CTRL_BUTTONS)) {
            state->p1 |= P1_BUTTONS_MASK;
            if (state->keys[SDL_SCANCODE_Z]) {
                state->p1 &= ~P1_B;
            }
            if (state->keys[SDL_SCANCODE_X]) {
                state->p1 &= ~P1_A;
            }
            if (state->keys[SDL_SCANCODE_RSHIFT]) {
                state->p1 &= ~P1_SELECT;
            }
            if (state->keys[SDL_SCANCODE_RETURN]) {
                state->p1 &= ~P1_START;
            }
            return;
        }
        state->p1 |= P1_MASK;
        return;
    default:
        UNREACHABLE();
    }
}

static void serialWrite(void* state, U16 addr, U8 val) {
    (void)state;
    switch (addr) {
    case PORT_SB:
        fprintf(stdout, "%c", val);
        return;
    case PORT_SC:
        return;
    default:
        UNREACHABLE();
    }
}

static void help(char const* name) {
    fprintf(stderr,
            "usage: %s [OPTIONS] <ROM>\n"
            "\n"
            "arguments:\n"
            "  <ROM>  gameboy ROM file to load\n"
            "\n"
            "options:\n"
            "  -s, --scale N      scale the window by N (default: 4)\n"
            "  -f, --fast-boot    skip the boot ROM (Nintendo logo)\n"
            "  -h, --help         show this help message and exit\n",
            name);
}

SDL_Renderer*     renderer;
SDL_Texture*      texture;
SDL_AudioDeviceID audev;
UInt              fps;

static void ppuCallback(Bus* bus, void* state) {
    (void)state;
    void* pixels;
    int   pitch;
    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    memcpy(pixels, bus->ppu.pixels, sizeof(bus->ppu.pixels));
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    ++fps;
}

static void apuCallback(Bus* bus, void* state) {
    (void)state;
    SDL_QueueAudio(audev, bus->apu.buf, sizeof(bus->apu.buf));
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        help(argv[0]);
        return EXIT_FAILURE;
    }
    char const* rompath  = NULL;
    Bool        fastboot = FALSE;
    UInt        scale    = 4;
    for (int argi = 1; argi < argc; ++argi) {
        if ((strcmp(argv[argi], "-h") == 0) ||
            (strcmp(argv[argi], "--help") == 0)) {
            help(argv[0]);
            return EXIT_SUCCESS;
        }
        if ((strcmp(argv[argi], "-s") == 0) ||
            (strcmp(argv[argi], "--scale") == 0)) {
            ++argi;
            if (argi >= argc) {
                fprintf(stderr, "missing scale factor\n");
                return EXIT_FAILURE;
            }
            scale = (UInt)strtoul(argv[argi], NULL, 10);
            if ((scale == ULONG_MAX) || (scale == 0) || (scale > 64)) {
                fprintf(stderr, "invalid scale factor: %s\n", argv[argi]);
                return EXIT_FAILURE;
            }
            continue;
        }
        if ((strcmp(argv[argi], "-f") == 0) ||
            (strcmp(argv[argi], "--fast-boot") == 0)) {
            fastboot = TRUE;
            continue;
        }
        rompath       = argv[argi];
        FILE* romfile = fopen(argv[argi], "rb");
        if (!romfile) {
            fprintf(stderr, "failed to open ROM file: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        if (fclose(romfile) == EOF) {
            fatal("failed to close ROM file: %s\n", strerror(errno));
        }
    }
    if (!rompath) {
        fprintf(stderr, "no ROM file specified\n");
        return EXIT_FAILURE;
    }
    int exitcode = EXIT_FAILURE;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "failed to initialize subsystems: %s\n",
                SDL_GetError());
        goto cleanupSDL;
    }
    SDL_Window* win = SDL_CreateWindow(
        "goodboy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale, SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        fprintf(stderr, "failed to create window: %s\n", SDL_GetError());
        goto cleanupSDL;
    }
    renderer = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "failed to create renderer: %s\n", SDL_GetError());
        goto cleanupWindow;
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                                SCREEN_HEIGHT);
    if (!texture) {
        fprintf(stderr, "failed to create texture: %s\n", SDL_GetError());
        goto cleanupRenderer;
    }
    SDL_AudioSpec auspec = {
        .freq     = APU_SAMPLE_FREQ,
        .format   = AUDIO_F32SYS,
        .channels = 2,
        .samples  = APU_BUF_SIZE / 2,
    };
    audev = SDL_OpenAudioDevice(NULL, 0, &auspec, NULL, 0);
    if (audev == 0) {
        fprintf(stderr, "failed to open audio device: %s\n", SDL_GetError());
        goto cleanupTexture;
    }
    SDL_PauseAudioDevice(audev, 0);
    InputState istate = {0};
    Bus        bus    = {0};
    Int        err;
    if ((err = cartInit(&bus.cart, rompath))) {
        fprintf(stderr, "failed to init cart: %" VIEW_FMT "\n",
                VIEW_FMT_ARG(cartErr(err)));
        goto cleanupTexture;
    }
    bus.ppuCb = ppuCallback;
    bus.apuCb = apuCallback;
    bus.input = (Dev){
        .state = &istate,
        .reset = (DevResetFn)inputReset,
        .tick  = (DevTickFn)inputTick,
        .read  = (DevReadFn)inputRead,
        .write = (DevWriteFn)inputWrite,
    };
    bus.serial = (Dev){
        .state = NULL,
        .reset = DEV_NULL.reset,
        .tick  = DEV_NULL.tick,
        .read  = DEV_NULL.read,
        .write = serialWrite,
    };
    busReset(&bus);
    if (fastboot) {
        busWrite(&bus, PORT_BOOT, 0x01);
        bus.cpu.af.h = 0x01;
        bus.cpu.af.l = 0xB0;
        bus.cpu.bc.h = 0x00;
        bus.cpu.bc.l = 0x13;
        bus.cpu.de.h = 0x00;
        bus.cpu.de.l = 0xD8;
        bus.cpu.hl.h = 0x01;
        bus.cpu.hl.l = 0x4D;
        bus.cpu.sp   = 0xFFFE;
        bus.cpu.pc   = 0x0100;
    }
    fps         = 0;
    UInt cycles = 0;
    U64  last   = SDL_GetTicks64();
    while (!istate.quit) {
        cycles += busTick(&bus);
        U64 now = SDL_GetTicks64();
        if ((now - last) >= 1000) {
            F64  mhz = ((F64)cycles) / 1000000.0;
            char title[64];
            snprintf(title, sizeof(title),
                     "goodboy :: %" UINT_FMT " fps :: "
                     "%.03" F64_FMT " MHz",
                     fps, mhz);
            SDL_SetWindowTitle(win, title);
            fps    = 0;
            cycles = 0;
            last   = now;
        }
    }
    exitcode = EXIT_SUCCESS;
cleanupTexture:
    SDL_DestroyTexture(texture);
cleanupRenderer:
    SDL_DestroyRenderer(renderer);
cleanupWindow:
    SDL_DestroyWindow(win);
cleanupSDL:
    SDL_Quit();
    return exitcode;
}
