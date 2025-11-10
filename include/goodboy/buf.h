#ifndef GB_BUF_H
#define GB_BUF_H

#include <goodboy/abi.h>

typedef struct {
    U8*  bytes;
    UInt len;
} View;

#define VIEW(cstr) ((View){(U8*)(cstr), (sizeof((cstr)) - 1)})

#define VIEW_FMT           ".*s"
#define VIEW_FMT_ARG(view) ((int)(view).len), ((char*)(view).bytes)

extern View const VIEW_NULL;

Bool viewEquals(View lhs, View rhs);

typedef struct {
    View view;
    UInt cap;
} Buf;

void bufFini(Buf* buf);

#endif // GB_BUF_H
