#ifndef GB_APU_H
#define GB_APU_H

#include <goodboy/abi.h>

typedef struct Bus Bus;
typedef struct Apu Apu;

enum {
    APU_SAMPLE_FREQ = 44100,
    APU_BUF_SIZE    = 1024,

    APU_FRAME_FREQ = 512,
};

enum {
    NR52_BIT_CH1_ACTIVE = 0,
    NR52_BIT_CH2_ACTIVE = 1,
    NR52_BIT_CH3_ACTIVE = 2,
    NR52_BIT_CH4_ACTIVE = 3,
    NR52_APU_ENABLE     = 1 << 7,

    NR53_UNUSED_MASK = 0x70,
};

typedef struct {
    Bool enabled;
    Bool dacen;
    Bool lenen;
    U8   lencnt;
    U16  freq;
    U16  freqcnt;
    U8   duty;
    U8   dutycnt;
    U8   vol;
} Pulse;

typedef struct {
    Pulse pulse;
} Ch1;

typedef struct {
    Pulse pulse;
} Ch2;

struct Apu {
    F32  buf[APU_BUF_SIZE];
    UInt bufpos;

    Bool enabled;
    U8   fscnt;

    Ch1 ch1;
    Ch2 ch2;
};

void apuReset(Apu* apu);
Bool apuTick(Apu* apu, Bus* bus);
U8   apuRead(Apu* apu, U16 addr);
void apuWrite(Apu* apu, U16 addr, U8 val);

#endif // GB_APU_H
