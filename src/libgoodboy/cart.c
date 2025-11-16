#include <goodboy/bus.h>
#include <goodboy/cart.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static View const ERRS[] = {
    [CART_OK]             = VIEW("no error"),
    [CART_ERR_IO]         = VIEW("i/o error"),
    [CART_ERR_BAD_HEADER] = VIEW("malformed ROM header"),
};

static struct {
    U8   type;
    View view;
} const TYPES[] = {
    {CART_TYPE_MBC0, VIEW("MBC0")},
    {CART_TYPE_MBC1, VIEW("MBC1")},
    {CART_TYPE_MBC1_RAM, VIEW("MBC1 + RAM")},
    {CART_TYPE_MBC1_RAM_BATT, VIEW("MBC1 + RAM + Battery")},
    {CART_TYPE_MBC2, VIEW("MBC2")},
    {CART_TYPE_MBC2_BATT, VIEW("MBC2 + Battery")},
    {CART_TYPE_MBC3_RTC_BATT, VIEW("MBC3 + RTC + Battery")},
    {CART_TYPE_MBC3_RTC_RAM_BATT, VIEW("MBC3 + RTC + RAM + Battery")},
    {CART_TYPE_MBC3, VIEW("MBC3")},
    {CART_TYPE_MBC3_RAM, VIEW("MBC3 + RAM")},
    {CART_TYPE_MBC3_RAM_BATT, VIEW("MBC3 + RAM + Battery")},
    {CART_TYPE_MBC5, VIEW("MBC5")},
    {CART_TYPE_MBC5_RAM, VIEW("MBC5 + RAM")},
    {CART_TYPE_MBC5_RAM_BATT, VIEW("MBC5 + RAM + Battery")},
    {CART_TYPE_MBC5_RUMBLE, VIEW("MBC5 + Rumble")},
    {CART_TYPE_MBC5_RUMBLE_RAM, VIEW("MBC5 + Rumble + RAM")},
    {CART_TYPE_MBC5_RUMBLE_RAM_BATT, VIEW("MBC5 + Rumble + RAM + Battery")},
};

enum {
    OFFSET_TITLE    = 0x0134,
    OFFSET_TYPE     = 0x0147,
    OFFSET_ROM_SIZE = 0x0148,
    OFFSET_RAM_SIZE = 0x0149,
};

static struct {
    U8   romsize;
    UInt size;
} const ROM_SIZES[] = {
    {ROM_SIZE_32KB, 32 * 1024},      {ROM_SIZE_64KB, 64 * 1024},
    {ROM_SIZE_128KB, 128 * 1024},    {ROM_SIZE_256KB, 256 * 1024},
    {ROM_SIZE_512KB, 512 * 1024},    {ROM_SIZE_1MB, 1 * 1024 * 1024},
    {ROM_SIZE_2MB, 2 * 1024 * 1024}, {ROM_SIZE_4MB, 4 * 1024 * 1024},
    {ROM_SIZE_8MB, 8 * 1024 * 1024},
};

static struct {
    U8   ramsize;
    UInt size;
} const RAM_SIZES[] = {
    {RAM_SIZE_NONE, 0 * 1024},  {RAM_SIZE_8KB, 8 * 1024},
    {RAM_SIZE_32KB, 32 * 1024}, {RAM_SIZE_128KB, 128 * 1024},
    {RAM_SIZE_64KB, 64 * 1024},
};

static INLINE View const* typeName(U8 type) {
    for (UInt i = 0; i < sizeof(TYPES) / sizeof(TYPES[0]); ++i) {
        if (TYPES[i].type == type) {
            return &TYPES[i].view;
        }
    }
    return NULL;
}

static INLINE UInt const* romSize(U8 romsize) {
    for (UInt i = 0; i < sizeof(ROM_SIZES) / sizeof(ROM_SIZES[0]); ++i) {
        if (ROM_SIZES[i].romsize == romsize) {
            return &ROM_SIZES[i].size;
        }
    }
    return NULL;
}

static INLINE UInt const* ramSize(U8 ramsize) {
    for (UInt i = 0; i < sizeof(RAM_SIZES) / sizeof(RAM_SIZES[0]); ++i) {
        if (RAM_SIZES[i].ramsize == ramsize) {
            return &RAM_SIZES[i].size;
        }
    }
    return NULL;
}

static void mbc0Fini(U8 rom[]) { free(rom); }
static U8   mbc0Read(U8 rom[], U16 addr) { return rom[addr]; }

typedef struct {
    Buf  rom;
    Buf  ram;
    U8   loaddr;
    U8   hiaddr;
    U32  rom0base;
    U32  romXbase;
    U32  rommask;
    U16  rambase;
    U16  rammask;
    Bool ramenabled;
    U8   mode;
} Mbc1;

static void mbc1Fini(Mbc1* mbc1) {
    bufFini(&mbc1->rom);
    bufFini(&mbc1->ram);
    free(mbc1);
}

static INLINE void mbc1Logic(Mbc1* mbc1) {
    if (mbc1->loaddr == 0) {
        mbc1->loaddr = 1;
    }
    mbc1->romXbase =
        ((mbc1->hiaddr << 19) | (mbc1->loaddr << 14)) - ROM_BANKX_START_ADDR;
    if (mbc1->mode == 0) {
        mbc1->rom0base = 0;
        mbc1->rambase  = 0;
        return;
    }
    mbc1->rom0base = mbc1->hiaddr << 19;
    mbc1->rambase  = mbc1->hiaddr << 13;
}

static void mbc1Reset(Mbc1* mbc1) {
    mbc1->loaddr     = 0;
    mbc1->hiaddr     = 0;
    mbc1->rom0base   = 0;
    mbc1->romXbase   = 0;
    mbc1->rambase    = 0;
    mbc1->ramenabled = FALSE;
    mbc1->mode       = 0;
    mbc1Logic(mbc1);
}

static U8 mbc1Read(Mbc1* mbc1, U16 addr) {
    switch (addr) {
    case ROM_BANK0_START_ADDR ... ROM_BANK0_END_ADDR:
        return mbc1->rom.view.bytes[(mbc1->rom0base + addr) & mbc1->rommask];
    case ROM_BANKX_START_ADDR ... ROM_BANKX_END_ADDR:
        return mbc1->rom.view.bytes[(mbc1->romXbase + addr) & mbc1->rommask];
    case CRAM_START_ADDR ... CRAM_END_ADDR:
        if (!mbc1->ramenabled) {
            return 0xFF;
        }
        return mbc1->ram.view
            .bytes[(mbc1->rambase + addr - CRAM_START_ADDR) & mbc1->rammask];
    default:
        UNREACHABLE();
    }
}

static void mbc1Write(Mbc1* mbc1, U16 addr, U8 val) {
    switch (addr) {
    case 0x0000 ... 0x1FFF:
        mbc1->ramenabled = (val & 0x0F) == 0x0A;
        return;
    case 0x2000 ... 0x3FFF:
        mbc1->loaddr = val & 0x1F;
        mbc1Logic(mbc1);
        return;
    case 0x4000 ... 0x5FFF:
        mbc1->hiaddr = val & 0x03;
        mbc1Logic(mbc1);
        return;
    case 0x6000 ... 0x7FFF:
        mbc1->mode = val & 0x01;
        mbc1Logic(mbc1);
        return;
    case CRAM_START_ADDR ... CRAM_END_ADDR:
        if (!mbc1->ramenabled) {
            return;
        }
        mbc1->ram.view
            .bytes[(mbc1->rambase + addr - CRAM_START_ADDR) & mbc1->rammask] =
            val;
        return;
    default:
        UNREACHABLE();
    }
}

Int cartInit(Cart* cart, char const* rompath) {
    Int   err     = CART_OK;
    FILE* romfile = fopen(rompath, "rb");
    if (!romfile) {
        debug("failed to open ROM file: %s\n", strerror(errno));
        return CART_ERR_IO;
    }
    if (fseek(romfile, 0, SEEK_END)) {
        debug("failed to seek ROM file: %s\n", strerror(errno));
        err = CART_ERR_IO;
        goto closeFile;
    }
    long filesize = ftell(romfile);
    if (filesize < 0) {
        debug("failed to tell ROM file size: %s\n", strerror(errno));
        err = CART_ERR_IO;
        goto closeFile;
    }
    if (filesize < 0x014F) {
        debug("ROM file too small to contain valid header\n");
        err = CART_ERR_BAD_HEADER;
        goto closeFile;
    }
    if (filesize > 0x800000) {
        debug("ROM file too large (max 8MiB)\n");
        err = CART_ERR_BAD_HEADER;
        goto closeFile;
    }
    if (fseek(romfile, 0, SEEK_SET)) {
        debug("failed to seek ROM file: %s\n", strerror(errno));
        err = CART_ERR_IO;
        goto closeFile;
    }
    U8* rom = malloc(filesize);
    if (!rom) {
        fatal("out of memory\n");
    }
    if (fread(rom, 1, filesize, romfile) != (size_t)filesize) {
        debug("failed to read ROM file: %s\n", strerror(errno));
        free(rom);
        err = CART_ERR_IO;
        goto closeFile;
    }
    memcpy(cart->title, rom + OFFSET_TITLE, sizeof(cart->title));
    for (UInt i = 0; i < sizeof(cart->title); ++i) {
        if (cart->title[i] == 0) {
            cart->title[i] = ' ';
        }
    }
    debug("title: %" VIEW_FMT "\n", VIEW_FMT_ARG(VIEW(cart->title)));

    cart->type       = rom[OFFSET_TYPE];
    View const* type = typeName(cart->type);
    if (!type) {
        debug("unsupported cartridge type: %02" U8_FMTX "\n", cart->type);
        free(rom);
        err = CART_ERR_BAD_HEADER;
        goto closeFile;
    }
    debug("type: %" VIEW_FMT "\n", VIEW_FMT_ARG(*type));

    UInt const* romsize = romSize(rom[OFFSET_ROM_SIZE]);
    if (!romsize) {
        debug("invalid ROM size: %02" U8_FMTX "\n", rom[OFFSET_ROM_SIZE]);
        free(rom);
        err = CART_ERR_BAD_HEADER;
        goto closeFile;
    }
    debug("rom size: %" UINT_FMT " bytes\n", *romsize);

    UInt const* ramsize = ramSize(rom[OFFSET_RAM_SIZE]);
    if (!ramsize) {
        debug("invalid RAM size: %02" U8_FMTX "\n", rom[OFFSET_RAM_SIZE]);
        free(rom);
        err = CART_ERR_BAD_HEADER;
        goto closeFile;
    }
    debug("ram size: %" UINT_FMT " bytes\n", *ramsize);
    U8* ram = NULL;
    if (*ramsize > 0) {
        ram = malloc(*ramsize);
        if (!ram) {
            fatal("out of memory\n");
        }
        FILE* ramfile = NULL;
        char* rampath = malloc(strlen(rompath) + strlen(".sav") + 1);
        if (!rampath) {
            fatal("out of memory\n");
        }
        sprintf(rampath, "%s.sav", rompath);
        ramfile = fopen(rampath, "rb");
        if (ramfile) {
            if (fread(ram, 1, *ramsize, ramfile) != *ramsize) {
                debug("failed to read SAV file: %s\n", strerror(errno));
            }
            if (fclose(ramfile) == EOF) {
                fatal("failed to close SAV file: %s\n", strerror(errno));
            }
        }
        free(rampath);
    }

    switch (cart->type) {
    case CART_TYPE_MBC0:
        cart->mbc = (Dev){
            .state = rom,
            .reset = DEV_NULL.reset,
            .tick  = DEV_NULL.tick,
            .read  = (DevReadFn)mbc0Read,
            .write = DEV_NULL.write,
        };
        break;
    case CART_TYPE_MBC1:
    case CART_TYPE_MBC1_RAM:
    case CART_TYPE_MBC1_RAM_BATT: {
        Mbc1* mbc1 = malloc(sizeof(Mbc1));
        if (!mbc1) {
            fatal("out of memory\n");
        }
        mbc1->rom.view.bytes = rom;
        mbc1->rom.view.len   = *romsize;
        mbc1->rom.cap        = *romsize;
        mbc1->rommask        = *romsize - 1;
        mbc1->ram.view.bytes = ram;
        mbc1->ram.view.len   = *ramsize;
        mbc1->ram.cap        = *ramsize;
        mbc1->rammask        = *ramsize - 1;

        cart->mbc = (Dev){
            .state = mbc1,
            .reset = (DevResetFn)mbc1Reset,
            .tick  = DEV_NULL.tick,
            .read  = (DevReadFn)mbc1Read,
            .write = (DevWriteFn)mbc1Write,
        };
        break;
    }
    default:
        TODO();
    }

closeFile:
    if (fclose(romfile) == EOF) {
        fatal("failed to close ROM file: %s\n", strerror(errno));
    }
    return err;
}

void cartFini(Cart* cart) {
    switch (cart->type) {
    case CART_TYPE_MBC0:
        mbc0Fini(cart->mbc.state);
        break;
    case CART_TYPE_MBC1:
    case CART_TYPE_MBC1_RAM:
    case CART_TYPE_MBC1_RAM_BATT:
        mbc1Fini(cart->mbc.state);
        break;
    default:
        TODO();
    }
    memset(cart, 0, sizeof(*cart));
}

View cartErr(Int err) {
    if (err < 0 || (size_t)err >= sizeof(ERRS) / sizeof(ERRS[0])) {
        return VIEW("unknown error");
    }
    return ERRS[err];
}
