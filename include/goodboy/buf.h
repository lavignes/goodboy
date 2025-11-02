#ifndef GB_BUF_H
#define GB_BUF_H

#include <goodboy/abi.h>

typedef struct {
    U8*  bytes;
    UInt len;
} View;

typedef struct {
    View view;
    UInt cap;
} Buf;

#endif // GB_BUF_H
