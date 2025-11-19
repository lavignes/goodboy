#ifndef GB_APU_H
#define GB_APU_H

#include <goodboy/abi.h>

typedef struct Bus Bus;
typedef struct Apu Apu;

enum {
    APU_FREQ_SAMPLE = 44100,
    APU_BUF_SIZE    = 1024,

    APU_FREQ_FRAME = 512,
};

enum {
    NR51_CH1_RIGHT = 1 << 0,
    NR51_CH2_RIGHT = 1 << 1,
    NR51_CH3_RIGHT = 1 << 2,
    NR51_CH4_RIGHT = 1 << 3,
    NR51_CH1_LEFT  = 1 << 4,
    NR51_CH2_LEFT  = 1 << 5,
    NR51_CH3_LEFT  = 1 << 6,
    NR51_CH4_LEFT  = 1 << 7,

    NR52_BIT_CH1_ACTIVE = 0,
    NR52_BIT_CH2_ACTIVE = 1,
    NR52_BIT_CH3_ACTIVE = 2,
    NR52_BIT_CH4_ACTIVE = 3,
    NR52_APU_ENABLE     = 1 << 7,
    NR52_UNUSED_MASK    = 0x70,
};

typedef struct {
    Bool active;
    Bool dacen;
    Bool lenen;
    U8   lencnt;
    U16  freq;
    U16  freqcnt;
    U8   duty;
    U8   dutycnt;
    U8   vol;
    U8   volset;
    U8   envdir;
    U8   envpace;
    U8   envcnt;
} Pulse;

typedef struct {
    Pulse pulse;
} Ch1;

typedef struct {
    Pulse pulse;
} Ch2;

typedef struct {
    Bool active;
    U8   wavcnt;
} Ch3;

typedef struct {
    Bool active;
} Ch4;

struct Apu {
    F32  buf[APU_BUF_SIZE];
    UInt bufpos;

    Bool enabled;
    U8   fscnt;

    Ch1 ch1;
    Ch2 ch2;
    Ch3 ch3;
    Ch4 ch4;

    U8 nr51;
};

void apuReset(Apu* apu);
Bool apuTick(Apu* apu, Bus* bus);
U8   apuRead(Apu* apu, U16 addr);
void apuWrite(Apu* apu, U16 addr, U8 val);

#endif // GB_APU_H
