#ifndef GB_BUS_H
#define GB_BUS_H

#include <goodboy/cpu.h>
#include <goodboy/fatal.h>
#include <goodboy/ppu.h>

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
    PORT_NR30  = 0xFF1A,
    PORT_NR31  = 0xFF1B,
    PORT_NR32  = 0xFF1C,
    PORT_NR33  = 0xFF1D,
    PORT_NR34  = 0xFF1E,
    PORT_NR41  = 0xFF20,
    PORT_NR42  = 0xFF21,
    PORT_NR43  = 0xFF22,
    PORT_NR44  = 0xFF23,
    PORT_NR50  = 0xFF24,
    PORT_NR51  = 0xFF25,
    PORT_NR52  = 0xFF26,

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

    PORT_SYS   = 0xFF4D,
    PORT_SPD   = 0xFF4D,
    PORT_VBK   = 0xFF4F,
    PORT_BOOT  = 0xFF50,

    PORT_HDMA1 = 0xFF51,
    PORT_HDMA2 = 0xFF52,
    PORT_HDMA3 = 0xFF53,
    PORT_HDMA4 = 0xFF54,
    PORT_HDMA5 = 0xFF55,

    PORT_RP    = 0xFF56,

    PORT_BCPS  = 0xFF68,
    PORT_BCPD  = 0xFF69,
    PORT_OCPS  = 0xFF6A,
    PORT_OCPD  = 0xFF6B,
    PORT_OPRI  = 0xFF6C,

    PORT_WBK   = 0xFF70,

    PORT_PCM12 = 0xFF76,
    PORT_PCM34 = 0xFF77,

    PORT_IE    = 0xFFFF,
};

enum {
    ROM_BANK_SIZE         = 0x4000,
    ROM_BANK0_START_ADDR  = 0x0000,
    ROM_BANK0_END_ADDR    = ROM_BANK0_START_ADDR + ROM_BANK_SIZE - 1,
    ROM_BANKX_START_ADDR  = 0x4000,
    ROM_BANKX_END_ADDR    = ROM_BANKX_START_ADDR + ROM_BANK_SIZE - 1,

    SRAM_SIZE             = 0x2000,
    SRAM_START_ADDR       = 0xA000,
    SRAM_END_ADDR         = SRAM_START_ADDR + SRAM_SIZE - 1,

    WRAM_BANK_SIZE        = 0x1000,
    WRAM_BANK0_START_ADDR = 0xC000,
    WRAM_BANK0_END_ADDR   = WRAM_BANK0_START_ADDR + WRAM_BANK_SIZE - 1,
    WRAM_BANKX_START_ADDR = 0xD000,
    WRAM_BANKX_END_ADDR   = WRAM_BANKX_START_ADDR + WRAM_BANK_SIZE - 1,

    ECHO_BANK0_SIZE       = 0x1000,
    ECHO_BANK0_START_ADDR = 0xE000,
    ECHO_BANK0_END_ADDR   = ECHO_BANK0_START_ADDR + ECHO_BANK0_SIZE - 1,
    ECHO_BANKX_SIZE       = 0x0E00,
    ECHO_BANKX_START_ADDR = 0xF000,
    ECHO_BANKX_END_ADDR   = ECHO_BANKX_START_ADDR + ECHO_BANKX_SIZE - 1,
};

typedef void (*DevResetFn)(void*);
typedef void (*DevTickFn)(void*);
typedef U8 (*DevReadFn)(void*, U16);
typedef void (*DevWriteFn)(void*, U16, U8);

typedef struct {
    void*      state;
    DevResetFn reset;
    DevTickFn  tick;
    DevReadFn  read;
    DevWriteFn write;
} Dev;

extern Dev const DEV_NULL;

struct Bus {
    Cpu cpu;
    Ppu ppu;
    U8  wram[WRAM_BANK_SIZE][8];

    Dev mbc;
    Dev input;
    Dev serial;

    U8 wbk;
    U8 iflags;
    U8 ieflags;
};

static inline void busReset(Bus* bus) {
    cpuReset(&bus->cpu);
    ppuReset(&bus->ppu);
    bus->mbc.reset(bus->mbc.state);
    bus->input.reset(bus->input.state);
    bus->serial.reset(bus->serial.state);
}

static inline Bool busTick(Bus* bus) {
    cpuTick(&bus->cpu, bus);
    Bool vblank = ppuTick(&bus->ppu, bus);
    bus->mbc.tick(bus->mbc.state);
    bus->input.tick(bus->input.state);
    bus->serial.tick(bus->serial.state);
    return vblank;
}

static INLINE U8   busRead(Bus* bus, U16 addr);
static INLINE void busWrite(Bus* bus, U16 addr, U8 val);

#ifndef GB_BUS_MOCK
static INLINE U8 busRead(Bus* bus, U16 addr) {
    switch (addr) {
    case ROM_BANK0_START_ADDR ... ROM_BANKX_END_ADDR:
        return bus->mbc.read(bus->mbc.state, addr);
    case VRAM_START_ADDR ... VRAM_END_ADDR:
        return bus->ppu.vram[addr - VRAM_START_ADDR][bus->ppu.vbk];
    case SRAM_START_ADDR ... SRAM_END_ADDR:
        return bus->mbc.read(bus->mbc.state, addr);
    case WRAM_BANK0_START_ADDR ... WRAM_BANK0_END_ADDR:
        return bus->wram[0][addr - WRAM_BANK0_START_ADDR];
    case WRAM_BANKX_START_ADDR ... WRAM_BANKX_END_ADDR:
        return bus->wram[bus->wbk][addr - WRAM_BANKX_START_ADDR];
    case ECHO_BANK0_START_ADDR ... ECHO_BANK0_END_ADDR:
        return bus->wram[0][addr - ECHO_BANK0_START_ADDR];
    case ECHO_BANKX_START_ADDR ... ECHO_BANKX_END_ADDR:
        return bus
            ->wram[(bus->wbk < 2) ? 1 : bus->wbk][addr - ECHO_BANK0_START_ADDR];
    case OAM_START_ADDR ... OAM_END_ADDR:
        return bus->ppu.objs[addr - OAM_START_ADDR];
    case 0xFEA0 ... 0xFEFF:
        return 0xFF;
    case PORT_P1:
        return bus->input.read(bus->input.state, addr);
    case PORT_IF:
        return bus->iflags;
    case PORT_LCDC:
        return bus->ppu.lcdc;
    case PORT_STAT:
        return bus->ppu.stat;
    case PORT_SCY:
        return bus->ppu.scy;
    case PORT_SCX:
        return bus->ppu.scx;
    case PORT_LY:
        return bus->ppu.ly;
    case PORT_LYC:
        return bus->ppu.lyc;
    case PORT_DMA:
        return bus->ppu.dma;
    case PORT_BGP:
        return bus->ppu.bgp;
    case PORT_OBP0:
        return bus->ppu.obp0;
    case PORT_OBP1:
        return bus->ppu.obp1;
    case PORT_WY:
        return bus->ppu.wy;
    case PORT_WX:
        return bus->ppu.wx;
    case PORT_VBK:
        return bus->ppu.vbk;
    case PORT_HDMA1:
    case PORT_HDMA2:
    case PORT_HDMA3:
    case PORT_HDMA4:
    case PORT_HDMA5:
    case PORT_BCPS:
    case PORT_BCPD:
    case PORT_OCPS:
    case PORT_OCPD:
        TODO();
    case PORT_IE:
        return bus->ieflags;
    default:
        TODO();
    }
}
#endif // GB_BUS_MOCK

#ifndef GB_BUS_MOCK
static INLINE void busWrite(Bus* bus, U16 addr, U8 val) {
    switch (addr) {
    case ROM_BANK0_START_ADDR ... ROM_BANKX_END_ADDR:
        bus->mbc.write(bus->mbc.state, addr, val);
        return;
    case VRAM_START_ADDR ... VRAM_END_ADDR:
        bus->ppu.vram[addr - VRAM_START_ADDR][bus->ppu.vbk & 1] = val;
        return;
    case SRAM_START_ADDR ... SRAM_END_ADDR:
        bus->mbc.write(bus->mbc.state, addr, val);
        return;
    case WRAM_BANK0_START_ADDR ... WRAM_BANK0_END_ADDR:
        bus->wram[0][addr - WRAM_BANK0_START_ADDR] = val;
        return;
    case WRAM_BANKX_START_ADDR ... WRAM_BANKX_END_ADDR:
        bus->wram[bus->wbk][addr - WRAM_BANKX_START_ADDR] = val;
        return;
    case ECHO_BANK0_START_ADDR ... ECHO_BANK0_END_ADDR:
        bus->wram[0][addr - ECHO_BANK0_START_ADDR] = val;
        return;
    case ECHO_BANKX_START_ADDR ... ECHO_BANKX_END_ADDR:
        bus->wram[(bus->wbk < 2) ? 1 : bus->wbk][addr - ECHO_BANKX_START_ADDR] =
            val;
        return;
    case OAM_START_ADDR ... OAM_END_ADDR:
        bus->ppu.objs[addr - OAM_START_ADDR] = val;
        return;
    case 0xFEA0 ... 0xFEFF:
        return;
    case PORT_P1:
        bus->input.write(bus->input.state, addr, val);
        return;
    case PORT_IF:
        bus->iflags = val;
        return;
    case PORT_LCDC:
        bus->ppu.lcdc = val;
        return;
    case PORT_STAT:
        bus->ppu.lcdc = val & ~(STAT_MODE_MASK | STAT_LYC);
        return;
    case PORT_SCY:
        bus->ppu.scy = val;
        return;
    case PORT_SCX:
        bus->ppu.scx = val;
        return;
    case PORT_LY:
        return;
    case PORT_LYC:
        bus->ppu.lyc = val;
        return;
    case PORT_DMA:
        bus->ppu.dma    = val;
        bus->ppu.dmacnt = OAM_SIZE;
        return;
    case PORT_BGP:
        bus->ppu.bgp = val;
        return;
    case PORT_OBP0:
        bus->ppu.obp0 = val;
        return;
    case PORT_OBP1:
        bus->ppu.obp1 = val;
        return;
    case PORT_WY:
        bus->ppu.wy = val;
        return;
    case PORT_WX:
        bus->ppu.wx = val;
        return;
    case PORT_VBK:
        bus->ppu.vbk = val;
        return;
    case PORT_HDMA1:
    case PORT_HDMA2:
    case PORT_HDMA3:
    case PORT_HDMA4:
    case PORT_HDMA5:
    case PORT_BCPS:
    case PORT_BCPD:
    case PORT_OCPS:
    case PORT_OCPD:
        TODO();
    case PORT_IE:
        bus->ieflags = val;
        return;
    default:
        TODO(": %04" U16_FMTX " = %" U8_FMTX "\n", addr, val);
    }
}
#endif // GB_BUS_MOCK

#endif // GB_BUS_H
