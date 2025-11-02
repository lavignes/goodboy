#ifndef GB_CPU_H
#define GB_CPU_H

#include <goodboy/abi.h>

typedef struct Bus Bus;
typedef struct Cpu Cpu;

enum {
    FLAG_Z = 1 << 7,
    FLAG_N = 1 << 6,
    FLAG_H = 1 << 5,
    FLAG_C = 1 << 4,
};

typedef union {
    U16 hl;
    struct {
        U8 l;
        U8 h;
    };
} Reg;

struct Cpu {
    U16 pc;
    U16 sp;
    Reg af;
    Reg bc;
    Reg de;
    Reg hl;

    Bool ime;
    Bool stopped;
    Bool halted;
};

void cpuReset(Cpu* cpu);
UInt cpuTick(Cpu* cpu, Bus* bus);

#endif // GB_CPU_H
