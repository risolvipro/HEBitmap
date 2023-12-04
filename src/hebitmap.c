//
//  hebitmap.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#include "hebitmap.h"

#define HE_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define HE_MIN(x, y) (((x) < (y)) ? (x) : (y))

#define HEBITMAP_MASK

static PlaydateAPI *playdate;

typedef struct {
    uint8_t *data;
    uint8_t *mask;
    int rowbytes;
} _HEBitmap;

static void buffer8_align32(uint8_t *dst, uint8_t *src, int dst_cols, int src_cols, int rows, uint32_t fill_value);
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
    
    bitmap->width = width;
    bitmap->height = height;
    
    int rowbytes_aligned = ((rowbytes * 8 + 31) / 32) * 4;
    
    prv->rowbytes = rowbytes_aligned;
    
    size_t data_size = rowbytes_aligned * height;

    prv->data = playdate->system->realloc(NULL, data_size);
    buffer8_align32(prv->data, data, rowbytes_aligned, rowbytes, height, 0x00000000);

    prv->mask = NULL;
    
    if(mask)
    {
        prv->mask = playdate->system->realloc(NULL, data_size);
        buffer8_align32(prv->mask, mask, rowbytes_aligned, rowbytes, height, 0x00000000);
    }
    
    return bitmap;
}

#ifdef HEBITMAP_MASK
void HEBitmapDrawMask(HEBitmap *bitmap, int x, int y)
{
#else
void HEBitmapDrawOpaque(HEBitmap *bitmap, int x, int y)
{
#endif
    if(x >= LCD_COLUMNS || (x + bitmap->width) <= 0 || y >= LCD_ROWS || (y + bitmap->height) <= 0)
    {
        return;
    }
    
    unsigned int x1, y1, x2, y2, offset_left, offset_top;
    adjust_bounds(bitmap, x, y, &x1, &y1, &x2, &y2, &offset_left, &offset_top);
    
    _HEBitmap *prv = bitmap->prv;

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

static void buffer8_align32(uint8_t *dst, uint8_t *src, int dst_cols, int src_cols, int rows, uint32_t fill_value)
{
    uint8_t *src_start = src;
    uint8_t *dst_start = dst;

    int src_offset = 0;
    int dst_offset = 0;
    
    int unaligned_cols = src_cols % 4;
    int aligned_cols = src_cols - unaligned_cols;

    for(int row = 0; row < rows; row++)
    {
        uint32_t *src_ptr = (uint32_t*)(src_start + src_offset);
        uint32_t *dst_ptr = (uint32_t*)(dst_start + dst_offset);
        uint32_t *dst_end = (uint32_t*)(dst_start + dst_offset + aligned_cols);
        
        // Copy 32-bit aligned data
        
        while(dst_ptr < dst_end)
        {
            *dst_ptr++ = bswap32(*src_ptr++);
        }
        
        // Copy unaligned data
        
        if(unaligned_cols > 0)
        {
            uint8_t *u_ptr = src_start + src_offset + aligned_cols;
            uint32_t value32 = fill_value;
            
            switch(unaligned_cols)
            {
                case 1:
                    value32 = u_ptr[0] << 24 | fill_value << 16 | fill_value << 8 | fill_value;
                    break;
                case 2:
                    value32 = u_ptr[0] << 24 | u_ptr[1] << 16 | fill_value << 8 | fill_value;
                    break;
                case 3:
                    value32 = u_ptr[0] << 24 | u_ptr[1] << 16 | u_ptr[2] << 8 | fill_value;
                    break;
            }
            
            *dst_end = value32;
        }
        
        src_offset += src_cols;
        dst_offset += dst_cols;
    }
}

    
void adjust_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top)
{
    
    *offset_left = 0;
    *offset_top = 0;
    
    if(x > 0)
    {
        *x1 = x;
        
    }
    else {
        *x1 = 0;
        *offset_left = -x;
    }
    
    if(y > 0)
    {
        *y1 = y;
    }
    else {
        *y1 = 0;
        *offset_top = -y;
    }
    
    *x2 = HE_MIN(*x1 - *offset_left + bitmap->width, LCD_COLUMNS);
    *y2 = HE_MIN(*y1 - *offset_top + bitmap->height, LCD_ROWS);
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
