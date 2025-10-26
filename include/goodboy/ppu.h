#ifndef GB_PPU_H
#define GB_PPU_H

#include <goodboy/abi.h>

typedef struct Bus Bus;
typedef struct Ppu Ppu;

struct Ppu {
    U8 vram[0x2000][2];
    U8 objs[40 * 4];

    U8 vbk;
};

void ppuReset(Ppu *ppu, Bus *bus);
UInt ppuTick(Ppu *ppu, Bus *bus);

#endif // GB_PPU_H
