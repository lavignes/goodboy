#ifndef GB_APU_H
#define GB_APU_H

#include <goodboy/abi.h>

typedef struct Bus Bus;
typedef struct Apu Apu;

typedef struct {
    Bool enabled;
    U16  freq;
    UInt period;
    UInt timer;
    U8   duty;
} Ch1;

struct Apu {
    Ch1 ch1;
};

#endif // GB_APU_H
