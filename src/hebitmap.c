//
//  hebitmap.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#include "hebitmap.h"
#include "hebitmap_prv.h"

#define HEBITMAP_CROP // Enable automatic cropping

#include "hebitmap_draw.h" // Opaque
#define HEBITMAP_MASK
#include "hebitmap_draw.h" // Mask
#undef HEBITMAP_MASK

static void buffer8_align32(uint8_t *dst, uint8_t *src, int dst_cols, int src_cols, int x, int y, int width, int height, uint8_t fill_value);
static void get_bounds(uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh);
static inline uint32_t bswap32(uint32_t n);

static PlaydateAPI *playdate;

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
    get_bounds(mask, rowbytes, width, height, &bx, &by, &bw, &bh);
    
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

void HEBitmapDraw(HEBitmap *bitmap, int x, int y)
{
    if(((_HEBitmap*)bitmap->prv)->mask)
    {
        HEBitmapDrawMask(playdate, bitmap, x, y);
    }
    else
    {
        HEBitmapDrawOpaque(playdate, bitmap, x, y);
    }
}

LCDColor HEBitmapColorAt(HEBitmap *bitmap, int x, int y)
{
    _HEBitmap *prv = bitmap->prv;
    
    if(x >= prv->bx && x <= (prv->bx + prv->bw) && y >= prv->by && y <= (prv->by + prv->bh))
    {
        int src_x = x - prv->bx;
        int src_y = y - prv->by;

        int i = src_y * prv->rowbytes + ((unsigned int)src_x / 32) * 4 + 3 - ((unsigned int)src_x % 32) / 8;
        uint8_t bitmask = (1 << (7 - (unsigned int)src_x % 8));
        
        if(prv->mask && !(prv->mask[i] & bitmask))
        {
            return kColorClear;
        }
        else if(prv->data[i] & bitmask)
        {
            return kColorWhite;
        }
        else
        {
            return kColorBlack;
        }
    }
    
    return kColorClear;
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

static void get_bounds(uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh)
{
    int min_y = 0; int min_x = 0; int max_x = width; int max_y = height;

#ifdef HEBITMAP_CROP
    if(mask)
    {
        int min_set = 0;

        max_x = 0;
        max_y = 0;
        
        int src_offset = 0;
        
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                uint8_t bitmask = 1 << (7 - (unsigned int)x % 8);
                if(mask[src_offset + (unsigned int)x / 8] & bitmask)
                {
                    if(!min_set)
                    {
                        min_x = x;
                        min_y = y;
                        min_set = 1;
                    }
                    
                    min_y = HEBITMAP_MIN(min_y, y);
                    int y2 = y + 1;
                    max_y = HEBITMAP_MAX(max_y, y2);
                    
                    min_x = HEBITMAP_MIN(min_x, x);
                    int x2 = x + 1;
                    max_x = HEBITMAP_MAX(max_x, x2);
                }
            }
            
            src_offset += rowbytes;
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
