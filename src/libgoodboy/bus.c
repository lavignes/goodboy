#include <goodboy/bus.h>

#include <stdlib.h>

static void devNullReset(void* state) { (void)state; }

static void devNullTick(void* state) { (void)state; }

static U8 devNullRead(void* state, U16 addr) {
    (void)state;
    (void)addr;
    return 0;
}

static void devNullWrite(void* state, U16 addr, U8 val) {
    (void)state;
    (void)addr;
    (void)val;
}

Dev const DEV_NULL = {
    .state = NULL,
    .reset = devNullReset,
    .tick  = devNullTick,
    .read  = devNullRead,
    .write = devNullWrite,
};
