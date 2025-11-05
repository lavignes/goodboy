#include <goodboy/bus.h>

#include <string.h>

enum {
    MODE2_START   = 0,
    MODE3_START   = MODE2_START + 80,
    MODE0_START   = MODE3_START + 172,

    DOTS_PER_LINE = 456,
    MODE1_LINES   = 10,
};

enum {
    TILE_WIDTH  = 8,
    TILE_HEIGHT = 8,
    TILE_STRIDE = 32,
};

void ppuReset(Ppu* ppu) { memset(ppu, 0, sizeof(Ppu)); }

static INLINE U32 bgColor(Ppu* ppu, U8 bits) {
    U8 shade = (ppu->bgp >> (bits * 2)) & 0x03;
    switch (shade) {
    case 0:
        return 0xFFFFFFFF;
    case 1:
        return 0xAAAAAAFF;
    case 2:
        return 0x555555FF;
    case 3:
        return 0x000000FF;
    default:
        UNREACHABLE();
    }
}

static INLINE void drawLine(Ppu* ppu) {
    U32* line  = ppu->pixels[ppu->ly];
    U8*  zline = ppu->zbuf[ppu->ly];
    memset(zline, 0, sizeof(*ppu->zbuf));
    U16       mapaddr = (ppu->lcdc & LCDC_BG_MAP)
                            ? (VRAM_BG_MAP0_ADDR - VRAM_START_ADDR)
                            : (VRAM_BG_MAP1_ADDR - VRAM_START_ADDR);
    U8 const* idxs    = &ppu->vram[0][mapaddr];
    // U8 const* attrs   = &ppu->vram[1][mapaddr];
    U16       y       = (((U16)ppu->ly) + ((U16)ppu->scy)) % 256;
    // Pixel y-offset into tile data (2 bytes per pixel)
    U16       tiley   = 2 * (y % TILE_HEIGHT);
    for (U16 dot = 0; dot < SCREEN_WIDTH; ++dot) {
        U16 x       = (((U16)dot) + ((U16)ppu->scx)) % 256;
        U16 mapidx  = (x / TILE_WIDTH) + ((y / TILE_HEIGHT) * 32);
        U8  idx     = idxs[mapidx];
        // U8   attr   = attrs[mapidx];
        U16 tileoff = (ppu->lcdc & LCDC_TILES)
                          ? (((U16)idx) * 16)
                          : ((U16)(0x1000 + (((U16)((I8)idx)) * 16)));
        U16 tilex   = x % TILE_WIDTH;
        // TODO: index into U16 view of VRAM instead? if so,^ don't multiply
        // yoff by 2 and idx by 16.. it would be by 8 then.
        U8  lo      = ppu->vram[0][tileoff + tiley];
        U8  hi      = ppu->vram[0][tileoff + tiley + 1];
        U8  bitlo   = (lo & (0x80 >> tilex)) != 0;
        U8  bithi   = (hi & (0x80 >> tilex)) != 0;
        U8  bits    = (bithi << 1) | bitlo;
        line[dot]   = bgColor(ppu, bits);
    }
}

Bool ppuTick(Ppu* ppu, Bus* bus) {
    // dma active?
    if (ppu->dmacnt > 0) {
        // TODO: emulate CGB bus conflicts for WRAM/ROM
        U16 addr                 = (((U16)ppu->dma) << 8) | ppu->dmacnt;
        ppu->objs[--ppu->dmacnt] = busRead(bus, addr);
        return false;
    }
    if (!(ppu->lcdc & LCDC_PPU_ENABLE)) {
        // TODO: blank screen when disabled
        ppu->stat &= ~STAT_MODE_MASK;
        ppu->ly  = 0;
        ppu->dot = 0;
        return false;
    }
    if (ppu->dot == 0) {
        if (ppu->ly == ppu->lyc) {
            ppu->stat |= STAT_LYC;
            if (ppu->stat & STAT_LYC_INT) {
                bus->iflags |= IFLAG_LCDSTAT;
            }
        }
    }
    // before vblank
    if (ppu->ly < SCREEN_HEIGHT) {
        if (ppu->dot == MODE2_START) {
            // oam scan start (mode 2)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x02;
            if (ppu->stat & STAT_MODE2_INT) {
                bus->iflags |= IFLAG_LCDSTAT;
            }
        } else if (ppu->dot == MODE3_START) {
            // drawing start (mode 3)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x03;
            drawLine(ppu);
        } else if (ppu->dot == MODE0_START) {
            // hblank (mode 0)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x00;
            if (ppu->stat & STAT_MODE0_INT) {
                bus->iflags |= IFLAG_LCDSTAT;
            }
        }
        ++ppu->dot;
        if (ppu->dot == DOTS_PER_LINE) {
            ppu->dot = 0;
            ++ppu->ly;
        }
        return false;
    }
    ++ppu->dot;
    if (ppu->dot == DOTS_PER_LINE) {
        ppu->dot = 0;
        ++ppu->ly;
        if (ppu->ly == (SCREEN_HEIGHT + MODE1_LINES)) {
            ppu->ly = 0;
        }
    }
    // vblank (mode 1)
    if ((ppu->dot == 1) && (ppu->ly == SCREEN_HEIGHT)) {
        ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x01;
        if (ppu->stat & STAT_MODE1_INT) {
            bus->iflags |= IFLAG_LCDSTAT;
        }
        return true;
    }
    return false;
}
