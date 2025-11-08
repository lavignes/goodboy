#include <goodboy/bus.h>

#include <string.h>

enum {
    MODE2_START = 0,
    MODE3_START = MODE2_START + 80,
    MODE0_START = MODE3_START + 172,

    DOTS_PER_LINE = 456,
    MODE1_LINES   = 10,
};

enum {
    TILE_WIDTH  = 8,
    TILE_HEIGHT = 8,
};

enum {
    OBJ_SIZE_SMALL = 8,
    OBJ_SIZE_LARGE = 16,

    OBJ_ORIGIN_X = 8,
    OBJ_ORIGIN_Y = 16,

    OBJ_ATTR_DMG_PALETTE = 1 << 4,
    OBJ_ATTR_X_FLIP      = 1 << 5,
    OBJ_ATTR_Y_FLIP      = 1 << 6,
    OBJ_ATTR_PRIORITY    = 1 << 7,
};

static U8 const XFLIP_TABLE[] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
    0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4,
    0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
    0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA,
    0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
    0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1,
    0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
    0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
    0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
    0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7,
    0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
    0x3F, 0xBF, 0x7F, 0xFF};

static U32 const DMG_PALETTE[] = {
    0xFFFFFFFF,
    0xAAAAAAFF,
    0x555555FF,
    0x000000FF,
};

void ppuReset(Ppu* ppu) { memset(ppu, 0, sizeof(Ppu)); }

static INLINE void drawLine(Ppu* ppu) {
    U32* line  = ppu->pixels[ppu->ly];
    U8*  zline = ppu->zbuf[ppu->ly];
    memset(zline, 0, sizeof(*ppu->zbuf));
    U16       mapaddr = (ppu->lcdc & LCDC_BG_MAP)
                            ? (VRAM_BG_MAP1_ADDR - VRAM_START_ADDR)
                            : (VRAM_BG_MAP0_ADDR - VRAM_START_ADDR);
    U8 const* idxs    = &ppu->vram[0][mapaddr];
    // U8 const* attrs   = &ppu->vram[1][mapaddr];
    U8        y       = ppu->ly + ppu->scy;
    // Pixel y-offset into tile data (2 bytes per pixel)
    U8        tiley   = 2 * (y % TILE_HEIGHT);
    for (U16 dot = 0; dot < SCREEN_WIDTH; ++dot) {
        U8  x       = dot + ppu->scx;
        U16 mapidx  = (((U16)x) / TILE_WIDTH) + ((((U16)y) / TILE_HEIGHT) * 32);
        U8  idx     = idxs[mapidx];
        // U8   attr   = attrs[mapidx];
        U16 tileoff = (ppu->lcdc & LCDC_TILES)
                          ? (((U16)idx) * 16)
                          : ((U16)(0x1000 + (((I16)((I8)idx)) * 16)));
        U8  tilex   = x % TILE_WIDTH;
        // TODO: index into U16 view of VRAM instead? if so,^ don't multiply
        // yoff by 2 and idx by 16.. it would be by 8 then.
        U8  patlo   = ppu->vram[0][tileoff + tiley + 0];
        U8  pathi   = ppu->vram[0][tileoff + tiley + 1];
        U8  bitlo   = (patlo & (0x80 >> tilex)) != 0;
        U8  bithi   = (pathi & (0x80 >> tilex)) != 0;
        U8  bits    = (bithi << 1) | bitlo;
        line[dot]   = DMG_PALETTE[(ppu->bgp >> (bits * 2)) & 0x03];
    }
    // sprites
    if (ppu->lcdc & LCDC_OBJ_ENABLE) {
        U8 objsize =
            (ppu->lcdc & LCDC_OBJ_SIZE) ? OBJ_SIZE_LARGE : OBJ_SIZE_SMALL;
        // TODO: do real OAM search algo here
        for (UInt obj = 0; obj < OAM_SIZE; obj += 4) {
            U8 y = ppu->objs[obj + 0] - OBJ_ORIGIN_Y;
            if ((ppu->ly < y) || (ppu->ly >= (y + objsize))) {
                continue;
            }
            U8  idx     = ppu->objs[obj + 2];
            U8  attr    = ppu->objs[obj + 3];
            U8  lly     = ppu->ly - y; // TODO compute this into y ?
            U8  tiley   = (attr & OBJ_ATTR_Y_FLIP)
                              ? (2 * (objsize - (lly % OBJ_ORIGIN_Y) - 1))
                              : (2 * (lly % OBJ_ORIGIN_Y));
            U16 tileoff = ((U16)idx) * 16;
            U8  patlo   = ppu->vram[0][tileoff + tiley + 0];
            U8  pathi   = ppu->vram[0][tileoff + tiley + 1];
            if (attr & OBJ_ATTR_X_FLIP) {
                patlo = XFLIP_TABLE[patlo];
                pathi = XFLIP_TABLE[pathi];
            }
            U8 x = ppu->objs[obj + 1] - OBJ_ORIGIN_X;
            for (U8 tilex = 0; tilex < TILE_WIDTH; ++tilex) {
                U8 dot = x + tilex;
                if (dot >= SCREEN_WIDTH) {
                    continue;
                }
                U8 bitlo = (patlo & (0x80 >> tilex)) != 0;
                U8 bithi = (pathi & (0x80 >> tilex)) != 0;
                U8 bits  = (bithi << 1) | bitlo;
                if (bits == 0) {
                    continue;
                }
                line[dot] =
                    DMG_PALETTE[(attr & OBJ_ATTR_DMG_PALETTE)
                                    ? ((ppu->obp1 >> (bits * 2)) & 0x03)
                                    : ((ppu->obp0 >> (bits * 2)) & 0x03)];
            }
        }
    }
}

Bool ppuTick(Ppu* ppu, Bus* bus) {
    // dma active?
    if (ppu->dmacnt > 0) {
        --ppu->dmacnt;
        // TODO: emulate CGB bus conflicts for WRAM/ROM
        U16 addr               = (((U16)ppu->dma) << 8) | ppu->dmacnt;
        ppu->objs[ppu->dmacnt] = busRead(bus, addr);
        return FALSE;
    }
    if (!(ppu->lcdc & LCDC_PPU_ENABLE)) {
        // TODO: blank screen when disabled
        ppu->stat &= ~STAT_MODE_MASK;
        ppu->ly  = 0;
        ppu->dot = 0;
        return FALSE;
    }
    if (ppu->dot == 0) {
        if (ppu->ly == ppu->lyc) {
            ppu->stat |= STAT_LYC;
            if (ppu->stat & STAT_INT_LYC) {
                bus->iflags |= INT_LCDSTAT;
            }
        }
    }
    // before vblank
    if (ppu->ly < SCREEN_HEIGHT) {
        if (ppu->dot == MODE2_START) {
            // oam scan start (mode 2)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x02;
            if (ppu->stat & STAT_INT_MODE2) {
                bus->iflags |= INT_LCDSTAT;
            }
        } else if (ppu->dot == MODE3_START) {
            // drawing start (mode 3)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x03;
            drawLine(ppu);
        } else if (ppu->dot == MODE0_START) {
            // hblank (mode 0)
            ppu->stat = (ppu->stat & ~STAT_MODE_MASK) | 0x00;
            if (ppu->stat & STAT_INT_MODE0) {
                bus->iflags |= INT_LCDSTAT;
            }
        }
        ++ppu->dot;
        if (ppu->dot == DOTS_PER_LINE) {
            ppu->dot = 0;
            ++ppu->ly;
        }
        return FALSE;
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
        bus->iflags |= INT_VBLANK;
        if (ppu->stat & STAT_INT_MODE1) {
            bus->iflags |= INT_LCDSTAT;
        }
        return TRUE;
    }
    return FALSE;
}
