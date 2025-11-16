#include <goodboy/bus.h>

#include <string.h>

static const U8 WAVE_DUTY_TABLE[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0}, // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0}, // 25%
    {0, 1, 1, 1, 1, 0, 0, 0}, // 50%
    {1, 0, 0, 1, 1, 1, 1, 1}, // 75%
};

void apuReset(Apu* apu) { memset(apu, 0, sizeof(*apu)); }

void apuTick(Apu* apu, Bus* bus) {
    (void)apu;
    (void)bus;
}
