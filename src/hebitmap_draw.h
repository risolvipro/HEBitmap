//
//  hebitmap_draw.h
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#include "pd_api.h"
#include "hebitmap.h"
#include "hebitmap_prv.h"

#ifdef HEBITMAP_MASK
void HEBitmapDrawMask(PlaydateAPI *playdate, HEBitmap *bitmap, int x, int y)
#else
void HEBitmapDrawOpaque(PlaydateAPI *playdate, HEBitmap *bitmap, int x, int y)
#endif
{
    _HEBitmap *prv = bitmap->prv;
    
    x += prv->bx;
    y += prv->by;
    
    if(x >= LCD_COLUMNS || (x + prv->bw) <= 0 || y >= LCD_ROWS || (y + prv->bh) <= 0)
    {
        // Bitmap is not visible
        return;
    }
    
    unsigned int x1, y1, x2, y2, offset_left, offset_top;
    adjust_bounds(bitmap, x, y, &x1, &y1, &x2, &y2, &offset_left, &offset_top, LCD_COLUMNS, LCD_ROWS);
    
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
                uint32_t mask_right = *mask_ptr >> shift;
                uint32_t mask = (mask_left & shift_mask) | (mask_right & ~shift_mask);
                data = (bswap32(*frame_ptr) & ~mask) | (data & mask);
#endif
                *frame_ptr++ = bswap32(data);
                
                // Fetch data for next iteration
                data_left = *data_ptr++ << (32 - shift);
#ifdef HEBITMAP_MASK
                // Fetch mask for next iteration
                mask_left = *mask_ptr++ << (32 - shift);
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
                uint32_t data = (data_left & shift_mask) | (data_right & ~shift_mask);
                
#ifndef HEBITMAP_MASK
                if(len < 32)
                {
                    uint32_t ignored_mask = 0xFFFFFFFF << (32 - len);
                    data = (data & ignored_mask) | (bswap32(*frame_ptr) & ~ignored_mask);
                }
#endif
#ifdef HEBITMAP_MASK
                uint32_t mask_right = ((len + shift) > 32) ? *++mask_ptr >> (32 - shift) : 0x00000000;
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
