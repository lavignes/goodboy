#include <goodboy/bus.h>

#include <string.h>

static const F32 PULSE_DUTY_TABLE[4][8] = {
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}, // 12.5%
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}, // 25%
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}, // 50%
    {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f}  // 75%
};

static INLINE void tickPulse(Pulse* pulse) {
    if (pulse->freqcnt == 0) {
        pulse->freqcnt = (2048 - pulse->freq) * 4;
        pulse->dutycnt = (pulse->dutycnt + 1) & 0x07;
    }
    --pulse->freqcnt;
}

static INLINE void tickCh3(Apu* apu, Bus* bus) {
    (void)apu;
    (void)bus;
}
static INLINE void tickCh4(Apu* apu, Bus* bus) {
    (void)apu;
    (void)bus;
}

static INLINE void tickLen(Bool* lenen, U8* lencnt) {
    if (*lenen && *lencnt > 0) {
        --(*lencnt);
        if (*lencnt == 0) {
            *lenen = FALSE;
        }
    }
}

static INLINE void tickPulseEnv(Pulse* pulse) { (void)pulse; }

static INLINE F32 samplePulse(Pulse* pulse) {
    if ((!pulse->enabled) || (!pulse->dacen)) {
        return 0.0f;
    }
    return PULSE_DUTY_TABLE[pulse->duty][pulse->dutycnt] ? 1.0f : -1.0f;
}

void apuReset(Apu* apu) { memset(apu, 0, sizeof(*apu)); }

Bool apuTick(Apu* apu, Bus* bus) {
    tickPulse(&apu->ch1.pulse);
    tickPulse(&apu->ch2.pulse);
    tickCh3(apu, bus);
    tickCh4(apu, bus);
    if ((bus->divcnt % (CPU_FREQ_NORMAL / APU_FRAME_FREQ)) == 0) {
        switch (apu->fscnt) {
        case 0:
            tickLen(&apu->ch1.pulse.lenen, &apu->ch1.pulse.lencnt);
            tickLen(&apu->ch2.pulse.lenen, &apu->ch2.pulse.lencnt);
            break;
        case 2:
            tickLen(&apu->ch1.pulse.lenen, &apu->ch1.pulse.lencnt);
            tickLen(&apu->ch2.pulse.lenen, &apu->ch2.pulse.lencnt);
            break;
        case 4:
            tickLen(&apu->ch1.pulse.lenen, &apu->ch1.pulse.lencnt);
            tickLen(&apu->ch2.pulse.lenen, &apu->ch2.pulse.lencnt);
            break;
        case 6:
            tickLen(&apu->ch1.pulse.lenen, &apu->ch1.pulse.lencnt);
            tickLen(&apu->ch2.pulse.lenen, &apu->ch2.pulse.lencnt);
            break;
        case 7:
            tickPulseEnv(&apu->ch1.pulse);
            tickPulseEnv(&apu->ch2.pulse);
            break;
        default:
            break;
        }
        apu->fscnt = (apu->fscnt + 1) & 0x07;
    }

    if ((bus->divcnt % (CPU_FREQ_NORMAL / APU_SAMPLE_FREQ)) == 0) {
        apu->buf[apu->bufpos++] = samplePulse(&apu->ch1.pulse) * 0.5f +
                                  samplePulse(&apu->ch2.pulse) * 0.5f;
        apu->buf[apu->bufpos++] = samplePulse(&apu->ch1.pulse) * 0.5f +
                                  samplePulse(&apu->ch2.pulse) * 0.5f;
        if (apu->bufpos == APU_BUF_SIZE) {
            apu->bufpos = 0;
            return TRUE;
        }
    }
    return FALSE;
}

U8 apuRead(Apu* apu, U16 addr) {
    (void)apu;
    switch (addr) {
    case PORT_NR10:
    case PORT_NR11:
    case PORT_NR12:
    case PORT_NR13:
    case PORT_NR14:
    case PORT_NR21:
    case PORT_NR22:
    case PORT_NR23:
    case PORT_NR24:
    case PORT_NR30:
    case PORT_NR31:
    case PORT_NR32:
    case PORT_NR33:
    case PORT_NR34:
    case PORT_NR41:
    case PORT_NR42:
    case PORT_NR43:
    case PORT_NR44:
    case PORT_NR50:
    case PORT_NR51:
        return 0xFF;
    case PORT_NR52:

    default:
        UNREACHABLE();
    }
}

void apuWrite(Apu* apu, U16 addr, U8 val) {
    switch (addr) {
    case PORT_NR10:
    case PORT_NR11:
    case PORT_NR12:
    case PORT_NR13:
    case PORT_NR14:
    case PORT_NR21:
    case PORT_NR22:
    case PORT_NR23:
    case PORT_NR24:
    case PORT_NR30:
    case PORT_NR31:
    case PORT_NR32:
    case PORT_NR33:
    case PORT_NR34:
    case PORT_NR41:
    case PORT_NR42:
    case PORT_NR43:
    case PORT_NR44:
    case PORT_NR50:
    case PORT_NR51:
        return;
    case PORT_NR52:
        if ((val & NR52_APU_ENABLE) == 0) {
            apuReset(apu);
        }
        // TODO: disbale writes
        return;
    default:
        UNREACHABLE();
    }
}
