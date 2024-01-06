//
//  hebitmap.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#include "hebitmap.h"

#define HEBITMAP_MASK
#define HEBITMAP_CROP

#define HE_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define HE_MIN(x, y) (((x) < (y)) ? (x) : (y))

static PlaydateAPI *playdate;

typedef struct {
    uint8_t *data;
    uint8_t *mask;
    int rowbytes;
    int bx;
    int by;
    int bw;
    int bh;
} _HEBitmap;

static void buffer8_align32(uint8_t *dst, uint8_t *src, int dst_cols, int src_cols, int x, int y, int width, int height, uint8_t fill_value);
static void get_bounds(uint8_t *data, uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh);
static void adjust_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top);
static inline uint32_t bswap32(uint32_t n);

void HEBitmapSetPlaydateAPI(PlaydateAPI *pd)
{
    playdate = pd;
}

HEBitmap* HEBitmapNew(LCDBitmap *lcd_bitmap)
{
    HEBitmap *bitmap = playdate->system->realloc(NULL, sizeof(HEBitmap));
    
    _HEBitmap *prv = playdate->system->realloc(NULL, sizeof(_HEBitmap));
    bitmap->prv = prv;
    
    int width, height, rowbytes;
    uint8_t *mask, *data;
    playdate->graphics->getBitmapData(lcd_bitmap, &width, &height, &rowbytes, &mask, &data);
    
    int bx, by, bw, bh;
    get_bounds(data, mask, rowbytes, width, height, &bx, &by, &bw, &bh);
    
    prv->bx = bx;
    prv->by = by;
    prv->bw = bw;
    prv->bh = bh;

    bitmap->width = width;
    bitmap->height = height;
    
    int rowbytes_aligned = ((bw + 31) / 32) * 4;
    
    prv->rowbytes = rowbytes_aligned;
    
    size_t data_size = rowbytes_aligned * bh;

    prv->data = playdate->system->realloc(NULL, data_size);
    buffer8_align32(prv->data, data, rowbytes_aligned, rowbytes, bx, by, bw, bh, 0x00);

    prv->mask = NULL;
    
    if(mask)
    {
        prv->mask = playdate->system->realloc(NULL, data_size);
        buffer8_align32(prv->mask, mask, rowbytes_aligned, rowbytes, bx, by, bw, bh, 0x00);
    }
    
    return bitmap;
}

#ifdef HEBITMAP_MASK
static void HEBitmapDrawMask(HEBitmap *bitmap, int x, int y)
#else
static void HEBitmapDrawOpaque(HEBitmap *bitmap, int x, int y)
#endif
{
    _HEBitmap *prv = bitmap->prv;
    
    x += prv->bx;
    y += prv->by;
    
    if(x >= LCD_COLUMNS || (x + prv->bw) <= 0 || y >= LCD_ROWS || (y + prv->bh) <= 0)
    {
        return;
    }
    
    unsigned int x1, y1, x2, y2, offset_left, offset_top;
    adjust_bounds(bitmap, x, y, &x1, &y1, &x2, &y2, &offset_left, &offset_top);
    
    uint8_t *frame_start = playdate->graphics->getFrame() + y1 * LCD_ROWSIZE + (x1 / 32) * 4;
    
    if(x >= 0)
    {
        int shift = (unsigned int)x % 32;
        uint32_t shift_mask = (shift > 0) ? (0xFFFFFFFF << (32 - shift)) : 0x00000000;
        
#ifndef HEBITMAP_MASK
        int max_len = x2 - (x1 + bitmap->width) - shift + 32;
#endif
        int data_offset = offset_top * prv->rowbytes;

        uint8_t *data_start = prv->data + data_offset;
#ifdef HEBITMAP_MASK
        uint8_t *mask_start = prv->mask + data_offset;
#endif
        for(int row = y1; row < y2; row++)
        {
            uint32_t *frame_ptr = (uint32_t*)frame_start;
            uint32_t *data_ptr = (uint32_t*)data_start;
#ifdef HEBITMAP_MASK
            uint32_t *mask_ptr = (uint32_t*)mask_start;
#endif
            uint32_t data_left = bswap32(*frame_ptr);
#ifdef HEBITMAP_MASK
            uint32_t mask_left = 0x00000000;
#endif
            int len = x2 - x1;
            
            while(len > 0)
            {
                uint32_t data_right = *data_ptr >> shift;
#ifdef HEBITMAP_MASK
                uint32_t mask_right = *mask_ptr >> shift;
#endif
                uint32_t data = (data_left & shift_mask) | (data_right & ~shift_mask);
                
#ifndef HEBITMAP_MASK
                int ignored_len = max_len - len;
                if(ignored_len > 0)
                {
                    uint32_t ignored_mask = 0xFFFFFFFF << ignored_len;
                    data = (data & ignored_mask) | (bswap32(*frame_ptr) & ~ignored_mask);
                }
#endif
#ifdef HEBITMAP_MASK
                uint32_t mask = (mask_left & shift_mask) | (mask_right & ~shift_mask);
                data = (bswap32(*frame_ptr) & ~mask) | (data & mask);
#endif
                *frame_ptr++ = bswap32(data);
                
                // Fetch data for next iteration
                data_left = *data_ptr << (32 - shift);
#ifdef HEBITMAP_MASK
                // Fetch mask for next iteration
                mask_left = *mask_ptr << (32 - shift);
#endif
                data_ptr++;
#ifdef HEBITMAP_MASK
                mask_ptr++;
#endif
                len -= 32;
            }
            
            int remainder = len + shift;
            if(remainder > 0)
            {
                uint32_t remainder_mask = 0xFFFFFFFF << (32 - remainder);
                uint32_t data = (data_left & remainder_mask) | (bswap32(*frame_ptr) & ~remainder_mask);
#ifdef HEBITMAP_MASK
                data = (bswap32(*frame_ptr) & ~mask_left) | (data & mask_left);
#endif
                *frame_ptr = bswap32(data);
            }
            
            frame_start += LCD_ROWSIZE;
            
            data_start += prv->rowbytes;
#ifdef HEBITMAP_MASK
            mask_start += prv->rowbytes;
#endif
        }
    }
    else
    {
        int shift = (unsigned int)-x % 32;
        uint32_t shift_mask = (shift > 0) ? (0xFFFFFFFF << shift) : 0xFFFFFFFF;
        int data_offset = offset_top * prv->rowbytes + offset_left / 32 * 4;
                
        uint8_t *data_start = prv->data + data_offset;
#ifdef HEBITMAP_MASK
        uint8_t *mask_start = prv->mask + data_offset;
#endif
        for(int row = y1; row < y2; row++)
        {
            uint32_t *frame_ptr = (uint32_t*)frame_start;
            uint32_t *data_ptr = (uint32_t*)data_start;
#ifdef HEBITMAP_MASK
            uint32_t *mask_ptr = (uint32_t*)mask_start;
#endif
            uint32_t data_left = *data_ptr << shift;
#ifdef HEBITMAP_MASK
            uint32_t mask_left = *mask_ptr << shift;
#endif
            int len = x2;
            
            while(len > 0)
            {
                uint32_t data_right = ((len + shift) > 32) ? *++data_ptr >> (32 - shift) : bswap32(*frame_ptr);
#ifdef HEBITMAP_MASK
                uint32_t mask_right = ((len + shift) > 32) ? *++mask_ptr >> (32 - shift) : 0x00000000;
#endif
                uint32_t data = (data_left & shift_mask) | (data_right & ~shift_mask);
#ifndef HEBITMAP_MASK
                if(len < 32)
                {
                    uint32_t ignored_mask = 0xFFFFFFFF << (32 - len);
                    data = (data & ignored_mask) | (bswap32(*frame_ptr) & ~ignored_mask);
                }
#endif
#ifdef HEBITMAP_MASK
                uint32_t mask = (mask_left & shift_mask) | (mask_right & ~shift_mask);
                data = (bswap32(*frame_ptr) & ~mask) | (data & mask);
#endif
                *frame_ptr++ = bswap32(data);
                
                // Fetch data for next iteration
                data_left = *data_ptr << shift;
#ifdef HEBITMAP_MASK
                // Fetch mask for next iteration
                mask_left = *mask_ptr << shift;
#endif
                len -= 32;
            }
            
            frame_start += LCD_ROWSIZE;
            
            data_start += prv->rowbytes;
#ifdef HEBITMAP_MASK
            mask_start += prv->rowbytes;
#endif
        }
    }
    
    playdate->graphics->markUpdatedRows(y1, y2 - 1);
}

void HEBitmapDraw(HEBitmap *bitmap, int x, int y)
{
    if(((_HEBitmap*)bitmap->prv)->mask)
    {
#ifdef HEBITMAP_MASK
        HEBitmapDrawMask(bitmap, x, y);
#endif
    }
    else
    {
#ifndef HEBITMAP_MASK
        HEBitmapDrawOpaque(bitmap, x, y);
#endif
    }
}
    
void HEBitmapFree(HEBitmap *bitmap)
{
    _HEBitmap *prv = bitmap->prv;
    
    playdate->system->realloc(prv->data, 0);
    if(prv->mask)
    {
        playdate->system->realloc(prv->mask, 0);
    }
    playdate->system->realloc(prv, 0);
    playdate->system->realloc(bitmap, 0);
}

static void get_bounds(uint8_t *data, uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh)
{
    int min_y = 0; int min_x = 0; int max_x = width; int max_y = height;

#ifdef HEBITMAP_CROP
    if(mask)
    {
        int min_set = 0;

        max_x = 0;
        max_y = 0;
        
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                uint8_t bitmask = 1 << (7 - (unsigned int)x % 8);
                if((mask[y * rowbytes + (unsigned int)x / 8] & bitmask))
                {
                    if(!min_set)
                    {
                        min_x = x;
                        min_y = y;
                        min_set = 1;
                    }
                    
                    min_y = HE_MIN(min_y, y);
                    max_y = HE_MAX(max_y, y + 1);
                    
                    min_x = HE_MIN(min_x, x);
                    max_x = HE_MAX(max_x, x + 1);
                }
            }
        }
    }
#endif
    
    *bx = min_x; *by = min_y; *bw = max_x - min_x; *bh = max_y - min_y;
}
    
static void buffer8_align32(uint8_t *dst, uint8_t *src, int dst_rowbytes, int src_rowbytes, int x, int y, int width, int height, uint8_t fill_value)
{
    int src_offset = y * src_rowbytes;
    int dst_offset = 0;
    int aligned_width = dst_rowbytes * 8;
    
    for(int dst_y = 0; dst_y < height; dst_y++)
    {
        uint32_t *dst_ptr32 = (uint32_t*)(dst + dst_offset);
        int byte = 0;
        
        for(int dst_x = 0; dst_x < aligned_width; dst_x++)
        {
            uint8_t value = fill_value;
            if(dst_x < width)
            {
                int src_x = x + dst_x;
                uint8_t src_bitmask = 1 << (7 - (unsigned int)src_x % 8);
                value = (src[src_offset + (unsigned int)src_x / 8] & src_bitmask);
            }
            
            uint8_t dst_bitmask = 1 << (7 - (unsigned int)dst_x % 8);
            uint8_t *dst_ptr = dst + dst_offset + (unsigned int)dst_x / 8;
            
            if(value)
            {
                *dst_ptr |= dst_bitmask;
            }
            else
            {
                *dst_ptr &= ~dst_bitmask;
            }
            
            byte++;
            if(byte == 32)
            {
                *dst_ptr32 = bswap32(*dst_ptr32);
                dst_ptr32++;
                byte = 0;
            }
        }
        
        src_offset += src_rowbytes;
        dst_offset += dst_rowbytes;
    }
}
    
static void adjust_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top)
{
    _HEBitmap *prv = bitmap->prv;
    
    *offset_left = 0;
    *offset_top = 0;
    
    if(x >= 0)
    {
        *x1 = x;
    }
    else
    {
        *x1 = 0;
        *offset_left = -x;
    }
    
    if(y >= 0)
    {
        *y1 = y;
    }
    else
    {
        *y1 = 0;
        *offset_top = -y;
    }
    
    *x2 = HE_MIN(*x1 - *offset_left + prv->bw, LCD_COLUMNS);
    *y2 = HE_MIN(*y1 - *offset_top + prv->bh, LCD_ROWS);
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
