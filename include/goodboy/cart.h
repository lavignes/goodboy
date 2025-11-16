#ifndef GB_CART_H
#define GB_CART_H

#include <goodboy/buf.h>
#include <goodboy/dev.h>

enum {
    CART_TYPE_MBC0                 = 0x00,
    CART_TYPE_MBC1                 = 0x01,
    CART_TYPE_MBC1_RAM             = 0x02,
    CART_TYPE_MBC1_RAM_BATT        = 0x03,
    CART_TYPE_MBC2                 = 0x05,
    CART_TYPE_MBC2_BATT            = 0x06,
    CART_TYPE_MBC3_RTC_BATT        = 0x0F,
    CART_TYPE_MBC3_RTC_RAM_BATT    = 0x10,
    CART_TYPE_MBC3                 = 0x11,
    CART_TYPE_MBC3_RAM             = 0x12,
    CART_TYPE_MBC3_RAM_BATT        = 0x13,
    CART_TYPE_MBC5                 = 0x19,
    CART_TYPE_MBC5_RAM             = 0x1A,
    CART_TYPE_MBC5_RAM_BATT        = 0x1B,
    CART_TYPE_MBC5_RUMBLE          = 0x1C,
    CART_TYPE_MBC5_RUMBLE_RAM      = 0x1D,
    CART_TYPE_MBC5_RUMBLE_RAM_BATT = 0x1E,
};

enum {
    ROM_SIZE_32KB  = 0x00,
    ROM_SIZE_64KB  = 0x01,
    ROM_SIZE_128KB = 0x02,
    ROM_SIZE_256KB = 0x03,
    ROM_SIZE_512KB = 0x04,
    ROM_SIZE_1MB   = 0x05,
    ROM_SIZE_2MB   = 0x06,
    ROM_SIZE_4MB   = 0x07,
    ROM_SIZE_8MB   = 0x08,
};

enum {
    RAM_SIZE_NONE  = 0x00,
    RAM_SIZE_8KB   = 0x02,
    RAM_SIZE_32KB  = 0x03,
    RAM_SIZE_128KB = 0x04,
    RAM_SIZE_64KB  = 0x05,
};

enum {
    CART_OK = 0,
    CART_ERR_IO,
    CART_ERR_BAD_HEADER,
};

typedef struct {
    U8  title[16];
    U8  type;
    Dev mbc;
} Cart;

Int  cartInit(Cart* cart, char const* rompath);
void cartFini(Cart* cart);
View cartErr(Int err);

#endif // GB_CART_H
