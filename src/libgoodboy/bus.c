#include <goodboy/bus.h>

#ifdef GB_DOCTOR_DEBUG
#include <stdio.h>
#endif // GB_DOCTOR_DEBUG

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
#ifdef GB_DOCTOR_DEBUG
    U8 a = busRead(bus, bus->cpu.pc + 0);
    U8 b = busRead(bus, bus->cpu.pc + 1);
    U8 c = busRead(bus, bus->cpu.pc + 2);
    U8 d = busRead(bus, bus->cpu.pc + 3);
    fprintf(stderr,
            "A:%02" U8_FMTX " F:%02" U8_FMTX " B:%02" U8_FMTX " C:%02" U8_FMTX
            " D:%02" U8_FMTX " E:%02" U8_FMTX " H:%02" U8_FMTX " L:%02" U8_FMTX
            " SP:%04" U16_FMTX " PC:%04" U16_FMTX " PCMEM:%02" U8_FMTX
            ",%02" U8_FMTX ",%02" U8_FMTX ",%02" U8_FMTX "\n",
            bus->cpu.af.h, bus->cpu.af.l, bus->cpu.bc.h, bus->cpu.bc.l,
            bus->cpu.de.h, bus->cpu.de.l, bus->cpu.hl.h, bus->cpu.hl.l,
            bus->cpu.sp, bus->cpu.pc, a, b, c, d);
#endif // GB_DOCTOR_DEBUG

    UInt cycles   = cpuTick(&bus->cpu, bus);
    bus->vblanked = FALSE;
    for (UInt i = 0; i < cycles; ++i) {
        bus->vblanked |= ppuTick(&bus->ppu, bus);
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
