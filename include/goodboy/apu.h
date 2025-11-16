#ifndef GB_APU_H
#define GB_APU_H

#include <goodboy/abi.h>

typedef struct Bus Bus;
typedef struct Apu Apu;

typedef struct {
    F32 amp;
} Ch;

struct Apu {
    Ch ch1;
    Ch ch2;
    Ch ch3;
    Ch ch4;

    F32  buf[1024];
    UInt bufpos;
};

void apuReset(Apu* apu);
void apuTick(Apu* apu, Bus* bus);

#endif // GB_APU_H
