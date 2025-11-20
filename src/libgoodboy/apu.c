#include <goodboy/bus.h>

#include <string.h>

static const U8 PULSE_DUTY_TABLE[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
    {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
    {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
    {0, 1, 1, 1, 1, 1, 1, 0}  // 75%
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
    if ((!pulse->active) || (!pulse->dacen)) {
        return 0.0f;
    }
    U8 wav = PULSE_DUTY_TABLE[pulse->duty][pulse->dutycnt] * pulse->vol;
    return (((F32)wav) / 7.5f) - 1.0f;
}

static INLINE F32 sampleWav() { return 0.0f; }

static INLINE F32 sampleNoise() { return 0.0f; }

void apuReset(Apu* apu) { memset(apu, 0, sizeof(*apu)); }

Bool apuTick(Apu* apu, Bus* bus) {
    tickPulse(&apu->ch1.pulse);
    tickPulse(&apu->ch2.pulse);
    tickCh3(apu, bus);
    tickCh4(apu, bus);
    if ((bus->divcnt % (CPU_FREQ_NORMAL / APU_FREQ_FRAME)) == 0) {
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
    if ((bus->divcnt % (CPU_FREQ_NORMAL / APU_FREQ_SAMPLE)) == 0) {
        F32 ch1   = samplePulse(&apu->ch1.pulse);
        F32 ch2   = samplePulse(&apu->ch2.pulse);
        F32 ch3   = sampleWav();
        F32 ch4   = sampleNoise();
        F32 left  = 0.0;
        F32 right = 0.0;
        if (apu->nr51 & NR51_CH1_LEFT) {
            left += ch1;
        }
        if (apu->nr51 & NR51_CH2_LEFT) {
            left += ch2;
        }
        if (apu->nr51 & NR51_CH3_LEFT) {
            left += ch3;
        }
        if (apu->nr51 & NR51_CH4_LEFT) {
            left += ch4;
        }
        if (apu->nr51 & NR51_CH1_RIGHT) {
            right += ch1;
        }
        if (apu->nr51 & NR51_CH2_RIGHT) {
            right += ch2;
        }
        if (apu->nr51 & NR51_CH3_RIGHT) {
            right += ch3;
        }
        if (apu->nr51 & NR51_CH4_RIGHT) {
            right += ch4;
        }
        apu->buf[apu->bufpos++] = left / 4.0f;
        apu->buf[apu->bufpos++] = right / 4.0f;
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
        return 0xFF;
    case PORT_NR11:
        return (apu->ch1.pulse.duty << 6) | 0x3F;
    case PORT_NR12:
    case PORT_NR13:
        return 0xFF;
    case PORT_NR14:
        return (apu->ch1.pulse.lenen << 6) | 0xBF;
    case PORT_NR21:
        return (apu->ch2.pulse.duty << 6) | 0x3F;
    case PORT_NR22:
    case PORT_NR23:
    case PORT_NR24:
        return (apu->ch2.pulse.lenen << 6) | 0xBF;
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
        return 0xFF;
    case PORT_NR51:
        return apu->nr51;
    case PORT_NR52:
        return NR52_UNUSED_MASK | (apu->enabled ? NR52_APU_ENABLE : 0x00) |
               (apu->ch1.pulse.active << NR52_BIT_CH1_ACTIVE) |
               (apu->ch2.pulse.active << NR52_BIT_CH2_ACTIVE) |
               (apu->ch3.active << NR52_BIT_CH3_ACTIVE) |
               (apu->ch4.active << NR52_BIT_CH4_ACTIVE);
    default:
        UNREACHABLE();
    }
}

void apuWrite(Apu* apu, U16 addr, U8 val) {
    if ((!apu->enabled) && (addr != PORT_NR52)) {
        return;
    }
    switch (addr) {
    case PORT_NR10:
        return;
    case PORT_NR11:
        apu->ch1.pulse.duty   = val >> 6;
        apu->ch1.pulse.lencnt = 64 - (val & 0x3F);
        return;
    case PORT_NR12:
        apu->ch1.pulse.envdir  = (val & 0x08) != 0;
        apu->ch1.pulse.envpace = val & 0x07;
        apu->ch1.pulse.volset  = val >> 4;
        apu->ch1.pulse.dacen   = ((val >> 3) & 0x1F) != 0;
        if (!apu->ch1.pulse.dacen) {
            apu->ch1.pulse.active = FALSE;
        }
        return;
    case PORT_NR13:
        apu->ch1.pulse.freq = (apu->ch1.pulse.freq & 0x0700) | ((U16)val);
        return;
    case PORT_NR14:
        apu->ch1.pulse.freq =
            (apu->ch1.pulse.freq & 0x00FF) | (((U16)(val & 0x07)) << 8);
        apu->ch1.pulse.lenen = (val & 0x40) != 0;
        if ((apu->ch1.pulse.dacen) && (val & 0x80)) {
            apu->ch1.pulse.active  = TRUE;
            apu->ch1.pulse.freqcnt = apu->ch1.pulse.freq;
            apu->ch1.pulse.vol     = apu->ch1.pulse.volset;
        }
        return;
    case PORT_NR21:
        apu->ch2.pulse.duty   = val >> 6;
        apu->ch2.pulse.lencnt = 64 - (val & 0x3F);
        return;
    case PORT_NR22:
        apu->ch2.pulse.envdir  = (val & 0x08) != 0;
        apu->ch2.pulse.envpace = val & 0x07;
        apu->ch2.pulse.volset  = val >> 4;
        apu->ch2.pulse.dacen   = ((val >> 3) & 0x1F) != 0;
        if (!apu->ch2.pulse.dacen) {
            apu->ch2.pulse.active = FALSE;
        }
        return;
    case PORT_NR23:
        apu->ch2.pulse.freq = (apu->ch2.pulse.freq & 0x0700) | ((U16)val);
        return;
    case PORT_NR24:
        apu->ch2.pulse.freq =
            (apu->ch1.pulse.freq & 0x00FF) | (((U16)(val & 0x07)) << 8);
        apu->ch2.pulse.lenen = (val & 0x40) != 0;
        if ((apu->ch2.pulse.dacen) && (val & 0x80)) {
            apu->ch2.pulse.active  = TRUE;
            apu->ch2.pulse.freqcnt = apu->ch2.pulse.freq;
            apu->ch2.pulse.vol     = apu->ch2.pulse.volset;
        }
        return;
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
        return;
    case PORT_NR51:
        apu->nr51 = val;
        return;
    case PORT_NR52: {
        Bool enable = (val & NR52_APU_ENABLE) != 0;
        if (apu->enabled) {
            apu->enabled = enable;
            if (!enable) {
                apuReset(apu);
            }
            return;
        }
        if (enable) {
            apu->enabled           = enable;
            apu->fscnt             = 0;
            apu->ch1.pulse.dutycnt = 0;
            apu->ch2.pulse.dutycnt = 0;
            apu->ch3.wavcnt        = 0;
        }
        return;
    }
    default:
        UNREACHABLE();
    }
}
