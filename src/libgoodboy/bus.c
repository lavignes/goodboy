#include <goodboy/bus.h>

void busReset(Bus* bus) {
    cpuReset(&bus->cpu);
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
        if (apuTick(&bus->apu, bus)) {
            if (bus->apuCb) {
                bus->apuCb(bus, bus->apuCbState);
            }
        }
        if (ppuTick(&bus->ppu, bus)) {
            if (bus->ppuCb) {
                bus->ppuCb(bus, bus->ppuCbState);
            }
        }
        bus->mbc.tick(bus->mbc.state);
        bus->input.tick(bus->input.state);
        bus->serial.tick(bus->serial.state);
    }
    bus->divcnt += cycles;
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
