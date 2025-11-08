#ifndef GB_PPU_H
#define GB_PPU_H

#include <goodboy/abi.h>

typedef struct Bus Bus;
typedef struct Ppu Ppu;

enum {
    SCREEN_WIDTH  = 160,
    SCREEN_HEIGHT = 144,
};

enum {
    LCDC_PRIORITY   = 1 << 0,
    LCDC_OBJ_ENABLE = 1 << 1,
    LCDC_OBJ_SIZE   = 1 << 2,
    LCDC_BG_MAP     = 1 << 3,
    LCDC_TILES      = 1 << 4,
    LCDC_WIN_ENABLE = 1 << 5,
    LCDC_WIN_MAP    = 1 << 6,
    LCDC_PPU_ENABLE = 1 << 7,
};

enum {
    STAT_MODE_MASK = 0x03,
    STAT_LYC       = 1 << 2,
    STAT_INT_MODE0 = 1 << 3, // hblank
    STAT_INT_MODE1 = 1 << 4, // vblank
    STAT_INT_MODE2 = 1 << 5, // oam scan
    STAT_INT_LYC   = 1 << 6, // ly==lyc
};

enum {
    OAM_SIZE       = 40 * 4,
    OAM_START_ADDR = 0xFE00,
    OAM_END_ADDR   = OAM_START_ADDR + OAM_SIZE - 1,
};

enum {
    VRAM_SIZE       = 0x2000,
    VRAM_START_ADDR = 0x8000,
    VRAM_END_ADDR   = VRAM_START_ADDR + VRAM_SIZE - 1,

    VRAM_BG_MAP0_ADDR = 0x9800,
    VRAM_BG_MAP1_ADDR = 0x9C00,
};

struct Ppu {
    U8 vram[2][VRAM_SIZE];
    U8 objs[OAM_SIZE];

    U8  dmacnt;
    U16 dot;

    U32 pixels[SCREEN_HEIGHT][SCREEN_WIDTH];
    U8  zbuf[SCREEN_HEIGHT][SCREEN_WIDTH];

    U8 lcdc;
    U8 stat;
    U8 scy;
    U8 scx;
    U8 ly;
    U8 lyc;
    U8 dma;
    U8 bgp;
    U8 obp0;
    U8 obp1;
    U8 wy;
    U8 wx;
    U8 vbk;
    U8 hdma1;
    U8 hdma2;
    U8 hdma3;
    U8 hdma4;
    U8 hdma5;
    U8 bcps;
    U8 bcpd;
    U8 ocps;
    U8 ocpd;
};

void ppuReset(Ppu* ppu);
Bool ppuTick(Ppu* ppu, Bus* bus);

#endif // GB_PPU_H
