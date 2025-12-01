#ifndef GB_UI_H
#define GB_UI_H

#include <goodboy/abi.h>

typedef struct {
    I32 x;
    I32 y;
} UiPoint;

typedef struct {
    I32 w;
    I32 h;
} UiSize;

typedef struct {
    UiPoint point;
    UiSize  size;
} UiRect;

typedef void (*UiDamageFn)();
typedef UiSize (*UiMeasureFn)(UiSize size);
typedef void (*UiLayoutFn)();
typedef void (*UiDrawFn)(U32** target);
typedef Bool (*UiHitTest)(UiPoint point);

typedef struct UiView UiView;

struct UiView {
    UiDamageFn  dmgfn;
    UiMeasureFn measurefn;
    UiLayoutFn  layoutfn;
    UiDrawFn    drawfn;

    UiRect rect;

    UiView* parent;
    UiView* sibling;
    UiView* children;
};

typedef struct {
    UiView root;
} Ui;

UiSize uiMeasure(Ui* ui, UiSize size);

#endif // GB_UI_H
