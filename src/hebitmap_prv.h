//
//  hebitmap_prv.h
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#ifndef hebitmap_prv_h
#define hebitmap_prv_h

#include "hebitmap.h"

#define HEBITMAP_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define HEBITMAP_MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef struct {
    uint8_t *data;
    uint8_t *mask;
    uint8_t *fb;
    int rowbytes;
    int bx;
    int by;
    int bw;
    int bh;
} _HEBitmap;

void adjust_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, int target_width, int target_height);

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
