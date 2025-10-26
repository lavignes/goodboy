#ifndef GB_BUS_H
#define GB_BUS_H

#include <goodboy/abi.h>
#include <goodboy/cpu.h>
#include <goodboy/ppu.h>

#include <assert.h>

enum {
    IFLAG_VBLANK  = 1 << 0,
    IFLAG_LCDSTAT = 1 << 1,
    IFLAG_TIMER   = 1 << 2,
    IFLAG_SERIAL  = 1 << 3,
    IFLAG_JOYPAD  = 1 << 4,
};

enum {
    PORT_P1    = 0xFF00,
    PORT_SB    = 0xFF01,
    PORT_SC    = 0xFF02,

    PORT_DIV   = 0xFF04,
    PORT_TIMA  = 0xFF05,
    PORT_TMA   = 0xFF06,
    PORT_TAC   = 0xFF07,

    PORT_IF    = 0xFF0F,

    PORT_NR10  = 0xFF10,
    PORT_NR11  = 0xFF11,
    PORT_NR12  = 0xFF12,
    PORT_NR13  = 0xFF13,
    PORT_NR14  = 0xFF14,

    PORT_NR21  = 0xFF16,
    PORT_NR22  = 0xFF17,
    PORT_NR23  = 0xFF18,
    PORT_NR24  = 0xFF19,

    PORT_LCDC  = 0xFF40,
    PORT_STAT  = 0xFF41,
    PORT_SCY   = 0xFF42,
    PORT_SCX   = 0xFF43,
    PORT_LY    = 0xFF44,
    PORT_LYC   = 0xFF45,
    PORT_DMA   = 0xFF46,
    PORT_BGP   = 0xFF47,
    PORT_OBP0  = 0xFF48,
    PORT_OBP1  = 0xFF49,
    PORT_WY    = 0xFF4A,
    PORT_WX    = 0xFF4B,

    PORT_KEY1  = 0xFF4D,
    PORT_VBK   = 0xFF4F,
    PORT_BOOT  = 0xFF50,

    PORT_HDMA1 = 0xFF51,
    PORT_HDMA2 = 0xFF52,
    PORT_HDMA3 = 0xFF53,
    PORT_HDMA4 = 0xFF54,
    PORT_HDMA5 = 0xFF55,

    PORT_BCPS  = 0xFF68,
    PORT_BCPD  = 0xFF69,
    PORT_OCPS  = 0xFF6A,
    PORT_OCPD  = 0xFF6B,
    PORT_WBK   = 0xFF70,

    PORT_IE    = 0xFFFF,
};

typedef struct {
    void *state;
    void (*reset)(void *state);
    U8 (*read)(void *state, U16 addr);
    void (*write)(void *state, U16 addr, U8 val);
} Dev;

struct Bus {
    Cpu *cpu;
    Ppu *ppu;
    U8   wram[0x2000][8];

    Dev mbc;
    Dev input;
    Dev serial;

    U8 wbk;
    U8 iflags;
    U8 ieflags;
};

static inline U8   busRead(Bus *bus, U16 addr);
static inline void busWrite(Bus *bus, U16 addr, U8 val);

#ifndef GB_BUS_MOCK
static inline U8 busRead(Bus *bus, U16 addr) {
    switch (addr) {
    case 0x0000 ... 0x7FFF:
        return bus->mbc.read(bus->mbc.state, addr);
    case 0x8000 ... 0x9FFF:
        return bus->ppu->vram[addr - 0x8000][bus->ppu->vbk];
    case 0xA000 ... 0xBFFF:
        return bus->mbc.read(bus->mbc.state, addr);
    case 0xC000 ... 0xCFFF:
        return bus->wram[0][addr - 0xC000];
    case 0xD000 ... 0xDFFF:
        return bus->wram[bus->wbk][addr - 0xD000];
    case 0xE000 ... 0xEFFF:
        return bus->wram[0][addr - 0xE000];
    case 0xF000 ... 0xFDFF:
        return bus->wram[bus->wbk][addr - 0xF000];
    case 0xFE00 ... 0xFE9F:
        return bus->ppu->objs[addr - 0xFE00];
    case 0xFEA0 ... 0xFEFF:
        return 0xFF;
    case PORT_P1:
        return bus->input.read(bus->input.state, addr);
    default:
        assert(false);
    }
}
#endif // GB_BUS_MOCK

#ifndef GB_BUS_MOCK
static inline void busWrite(Bus *bus, U16 addr, U8 val) {
    switch (addr) {
    case 0x0000 ... 0x7FFF:
        bus->mbc.write(bus->mbc.state, addr, val);
        return;
    case 0x8000 ... 0x9FFF:
        bus->ppu->vram[addr - 0x8000][bus->ppu->vbk] = val;
        return;
    case 0xA000 ... 0xBFFF:
        bus->mbc.write(bus->mbc.state, addr, val);
        return;
    case 0xC000 ... 0xCFFF:
        bus->wram[0][addr - 0xC000] = val;
        return;
    case 0xD000 ... 0xDFFF:
        bus->wram[bus->wbk][addr - 0xD000] = val;
        return;
    case 0xE000 ... 0xEFFF:
        bus->wram[0][addr - 0xE000] = val;
        return;
    case 0xF000 ... 0xFDFF:
        bus->wram[bus->wbk][addr - 0xF000] = val;
        return;
    case 0xFE00 ... 0xFE9F:
        bus->ppu->objs[addr - 0xFE00] = val;
        return;
    case 0xFEA0 ... 0xFEFF:
        return;
    case PORT_P1:
        bus->input.write(bus->input.state, addr, val);
        return;
    default:
        assert(false);
    }
}
#endif // GB_BUS_MOCK

#endif // GB_BUS_H
