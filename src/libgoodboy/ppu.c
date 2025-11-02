#include <goodboy/bus.h>

#include <string.h>

enum {
    MODE2_DOT   = 0,
    MODE3_DOT   = MODE2_DOT + 80,
    MODE0_DOT   = MODE3_DOT + 172,
    DOTS_COUNT  = 456,
    MODE1_LINES = 10,
};

void ppuReset(Ppu* ppu) { memset(ppu, 0, sizeof(Ppu)); }

static INLINE void drawLine(Ppu* ppu) {
    U32* line  = ppu->pixels[ppu->ly];
    U8*  zline = ppu->zbuf[ppu->ly];
    memset(zline, 0, sizeof(ppu->zbuf[0]));
    U16       bgaddr   = (ppu->lcdc & LCDC_BG_MAP)
                             ? (VRAM_BG_MAP0_ADDR - VRAM_START_ADDR)
                             : (VRAM_BG_MAP1_ADDR - VRAM_START_ADDR);
    U8 const* chridxs  = &ppu->vram[0][bgaddr];
    U8 const* chrattrs = &ppu->vram[1][bgaddr];
    UInt      y        = (((UInt)ppu->ly) + ((UInt)ppu->scy)) % 256;
    UInt      chryoff =
        2 * (y % 8); // Pixel y-offset into tile data (2 bytes per pixel)
    for (UInt dot = 0; dot < SCREEN_WIDTH; ++dot) {
        UInt x       = (((UInt)dot) + ((UInt)ppu->scx)) % 256;
        UInt bgidx   = (x / 8) + ((y / 8) * 32);
        U8   chridx  = chridxs[bgidx];
        U8   chrattr = chrattrs[bgidx];
        UInt chroff  = (ppu->lcdc & LCDC_TILES)
                           ? (chridx * 16)
                           : ((UInt)(0x1000 + (((Int)((I8)chridx)) * 16)));
        UInt chrx    = x % 8;
        U8   lo      = ppu->vram[0][chroff + chryoff];
        U8   hi      = ppu->vram[0][chroff + chryoff + 1];
        U8   bitlo   = (lo & (0x80 >> chrx)) != 0;
        U8   bithi   = (hi & (0x80 >> chrx)) != 0;
        U8   bits    = (bithi << 1) | bitlo;
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
    if (ppu->ly < SCREEN_HEIGHT) {
        if (ppu->dot == MODE2_DOT) {
            // oam scan start (mode 2)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x02;
            if (ppu->stat & STAT_MODE2_INT) {
                bus->iflags |= IFLAG_LCDSTAT;
            }
        } else if (ppu->dot == MODE3_DOT) {
            // drawing start (mode 3)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x03;
            drawLine(ppu);
        } else if (ppu->dot == MODE0_DOT) {
            // hblank (mode 0)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x00;
            if (ppu->stat & STAT_MODE0_INT) {
                bus->iflags |= IFLAG_LCDSTAT;
            }
        }
        ++ppu->dot;
        if (ppu->dot == DOTS_COUNT) {
            ppu->dot = 0;
            ++ppu->ly;
        }
        return false;
    }
    ++ppu->dot;
    if (ppu->dot == DOTS_COUNT) {
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
