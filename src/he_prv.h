//
//  he_api_prv.h
//  HE Playdate Library
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#ifndef he_prv_h
#define he_prv_h

#include "he_api.h"
#include "he_foundation.h"

typedef struct {
    HERect _clipRect;
    HERect clipRect;
} HEGraphicsContext;

extern HEGraphicsContext *he_graphics_context;

static const HERect he_gfx_screenRect = {
    .x = 0,
    .y = 0,
    .width = LCD_COLUMNS,
    .height = LCD_ROWS
};

HERect he_rect_intersection(HERect clipRect, HERect rect);

void he_bitmap_clip_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, HERect clipRect);

static inline int he_min(const int a, const int b)
{
    return a < b ? a : b;
}

static inline int he_max(const int a, const int b)
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

#endif /* he_prv_h */
