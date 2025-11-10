#ifndef GB_DEV_H
#define GB_DEV_H

#include <goodboy/abi.h>

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
extern Dev const DEV_BOOT;

#endif // GB_DEV_H
