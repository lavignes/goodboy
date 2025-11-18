#include <goodboy/bus.h>

void busReset(Bus* bus) {
    cpuReset(&bus->cpu);
    apuReset(&bus->apu);
    ppuReset(&bus->ppu);
    bus->mbc       = DEV_BOOT;
    bus->mbc.state = &bus->cart.mbc;
    bus->mbc.reset(bus->mbc.state);
    bus->input.reset(bus->input.state);
    bus->serial.reset(bus->serial.state);
}

UInt busTick(Bus* bus) {
    UInt cycles = cpuTick(&bus->cpu, bus);
    for (UInt i = 0; i < cycles; ++i) {
        ++bus->divcnt;
        if (apuTick(&bus->apu, bus) && bus->apucb) {
            bus->apucb(bus);
        }
        if (ppuTick(&bus->ppu, bus) && bus->ppucb) {
            bus->ppucb(bus);
        }
        bus->mbc.tick(bus->mbc.state);
        bus->input.tick(bus->input.state);
        bus->serial.tick(bus->serial.state);
    }
    // TODO: CGB timer speed doubling
    if (bus->divcnt >= (CPU_FREQ_NORMAL / DIV_FREQ)) {
        bus->divcnt -= (CPU_FREQ_NORMAL / DIV_FREQ);
        ++bus->div;
    }
    if (bus->tac & TAC_ENABLE) {
        bus->timacnt += cycles;
        UInt tacfreq = CPU_FREQ_NORMAL / TAC_FREQS[bus->tac & TAC_FREQ_MASK];
        while (bus->timacnt >= tacfreq) {
            bus->timacnt -= tacfreq;
            ++bus->tima;
            if (bus->tima == 0) {
                bus->tima = bus->tma;
                bus->iflags |= INT_TIMER;
            }
        }
    }
    return cycles;
}
