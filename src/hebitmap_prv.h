//
//  hebitmap_prv.h
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#ifndef hebitmap_prv_h
#define hebitmap_prv_h

#include "hebitmap.h"

typedef struct _HEBitmap {
    uint8_t *data;
    uint8_t *mask;
    int rowbytes;
    int bx;
    int by;
    int bw;
    int bh;
    uint8_t *buffer;
    int isOwner;
    HEBitmapTable *bitmapTable;
    LuaUDObject *luaRef;
    LuaUDObject *luaTableRef;
} _HEBitmap;

typedef struct _HEBitmapTable {
    HEBitmap **bitmaps;
    uint8_t *buffer;
    int luaRefCount;
    LuaUDObject *luaRef;
} _HEBitmapTable;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} _HEBitmapRect;

extern _HEBitmapRect _clipRect;
extern _HEBitmapRect clipRect;

void clip_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, _HEBitmapRect clipRect);

static inline int hebitmap_min(const int a, const int b)
{
    return a < b ? a : b;
}

static inline int hebitmap_max(const int a, const int b)
{
    return a > b ? a : b;
}

static inline uint32_t bswap32(uint32_t n)
{
#if TARGET_PLAYDATE
    uint32_t r;
    __asm("rev %0, %1" : "=r"(r) : "r"(n));
    return r;
#else
    return (n >> 24) | ((n << 8) & 0xFF0000U) | (n << 24) | ((n >> 8) & 0x00FF00U);
#endif
}

#endif /* hebitmap_prv_h */
