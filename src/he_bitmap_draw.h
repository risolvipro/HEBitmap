//
//  hebitmap_draw.h
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#include "pd_api.h"
#include "he_api.h"
#include "he_prv.h"

#ifdef HE_BITMAP_MASK
void HEBitmap_drawMask(PlaydateAPI *playdate, HEBitmap *bitmap, int x, int y)
#else
void HEBitmap_drawOpaque(PlaydateAPI *playdate, HEBitmap *bitmap, int x, int y)
#endif
{
    _HEBitmap *prv = &bitmap->prv;
    
    x += prv->bx;
    y += prv->by;
    
    HERect clipRect = he_graphics_context->clipRect;
    
    if((x + prv->bw) <= clipRect.x || x >= (clipRect.x + clipRect.width) || (y + prv->bh) <= clipRect.y || y >= (clipRect.y + clipRect.height))
    {
        //
        // Bitmap is not visible
        //
        return;
    }
    
    unsigned int x1, y1, x2, y2, offset_left, offset_top;
    he_bitmap_clip_bounds(bitmap, x, y, &x1, &y1, &x2, &y2, &offset_left, &offset_top, clipRect);
    
    uint8_t *frame_start = playdate->graphics->getFrame() + y1 * LCD_ROWSIZE + x1 / 32 * 4;
    
    if((int)(x1 / 32 * 32) <= x)
    {
        //     start <= x <= x1
        //
        //   start           x      x1
        //     ----------------------------------
        //     |    shift    *******image*******|
        //     0--------------------------------32
        //     |                    ++++clip++++|
        //     0--------------------------------32
        
        int shift = (unsigned int)x % 32;
        uint32_t og_shift_mask = ~(0xFFFFFFFF >> shift);
        
        int data_offset = offset_top * prv->rowbytes;
        
        uint8_t *data_start = prv->data + data_offset;
#ifdef HE_BITMAP_MASK
        uint8_t *mask_start = prv->mask + data_offset;
#endif
        for(int row = y1; row < y2; row++)
        {
            uint32_t *frame_ptr = (uint32_t*)frame_start;
            uint32_t *data_ptr = (uint32_t*)data_start;
            uint32_t data_left = bswap32(*frame_ptr);
#ifdef HE_BITMAP_MASK
            uint32_t *mask_ptr = (uint32_t*)mask_start;
            uint32_t mask_left = 0x00000000;
#endif
            uint32_t shift_mask = ~(0xFFFFFFFF >> (x1 % 32));
            int len = x2 - x1 / 32 * 32;
            
            while(len > 0)
            {
                uint32_t data_right = bswap32(*data_ptr) >> shift;
                uint32_t data = (data_left & shift_mask) | (data_right & ~shift_mask);
#ifdef HE_BITMAP_MASK
                uint32_t mask_right = bswap32(*mask_ptr) >> shift;
                uint32_t mask = (mask_left & shift_mask) | (mask_right & ~shift_mask);
                data = (bswap32(*frame_ptr) & ~mask) | (data & mask);
#endif
                if(len < 32)
                {
                    uint32_t clip_mask = 0xFFFFFFFF << (32 - len);
                    data = (data & clip_mask) | (bswap32(*frame_ptr) & ~clip_mask);
                }
                
                *frame_ptr++ = bswap32(data);
                
                // Fetch data for next iteration
                data_left = bswap32(*data_ptr++) << (32 - shift);
#ifdef HE_BITMAP_MASK
                // Fetch mask for next iteration
                mask_left = bswap32(*mask_ptr++) << (32 - shift);
#endif
                shift_mask = og_shift_mask;
                len -= 32;
            }
            
            frame_start += LCD_ROWSIZE;
            data_start += prv->rowbytes;
#ifdef HE_BITMAP_MASK
            mask_start += prv->rowbytes;
#endif
        }
    }
    else
    {
        //     x < start <= x1
        //
        //                 x                  start   x1
        //     ---------------------------------------------------
        //     |           ********image********|*****************
        //     0--------------------------------32----------------
        //     |                                |     ++++clip++++
        //     0--------------------------------32----------------
        //                 ------offset_32------
        
        int shift = (unsigned int)abs(x) % 32;
        
        //
        //     |            ********image********|
        //     0-----------12-------------------32
        //     ---shift----
        //
        //     |           ********image********|
        //   -32 ------- -20 -------------------0
        //                 --------shift--------
        
        if(x >= 0 && shift > 0)
        {
            shift = 32 - shift;
        }
        uint32_t shift_mask = 0xFFFFFFFF << shift;

        unsigned int offset_32 = x1 / 32 * 32 - x;
        int data_offset = offset_top * prv->rowbytes + offset_32 / 32 * 4;
        
        uint8_t *data_start = prv->data + data_offset;
#ifdef HE_BITMAP_MASK
        uint8_t *mask_start = prv->mask + data_offset;
#endif
        for(int row = y1; row < y2; row++)
        {
            uint32_t *frame_ptr = (uint32_t*)frame_start;
            uint32_t *data_ptr = (uint32_t*)data_start;
            uint32_t data_left = bswap32(*data_ptr) << shift;
#ifdef HE_BITMAP_MASK
            uint32_t *mask_ptr = (uint32_t*)mask_start;
            uint32_t mask_left = bswap32(*mask_ptr) << shift;
#endif
            uint32_t clip_left_mask = ~(0xFFFFFFFF >> (x1 % 32));
            int len = x2 - x1 / 32 * 32;
            
            while(len > 0)
            {
                uint32_t data_right = ((len + shift) > 32) ? (bswap32(*++data_ptr) >> (32 - shift)) : bswap32(*frame_ptr);
                uint32_t data = (data_left & shift_mask) | (data_right & ~shift_mask);
#ifdef HE_BITMAP_MASK
                uint32_t mask_right = ((len + shift) > 32) ? (bswap32(*++mask_ptr) >> (32 - shift)) : 0x00000000;
                uint32_t mask = (mask_left & shift_mask) | (mask_right & ~shift_mask);
                data = (bswap32(*frame_ptr) & ~mask) | (data & mask);
#endif
                if(clip_left_mask != 0x00000000)
                {
                    data = (bswap32(*frame_ptr) & clip_left_mask) | (data & ~clip_left_mask);
                    clip_left_mask = 0x00000000;
                }
                
                if(len < 32)
                {
                    uint32_t clip_right_mask = 0xFFFFFFFF << (32 - len);
                    data = (data & clip_right_mask) | (bswap32(*frame_ptr) & ~clip_right_mask);
                }
                
                *frame_ptr++ = bswap32(data);
                
                // Fetch data for next iteration
                data_left = bswap32(*data_ptr) << shift;
#ifdef HE_BITMAP_MASK
                // Fetch mask for next iteration
                mask_left = bswap32(*mask_ptr) << shift;
#endif
                len -= 32;
            }

            frame_start += LCD_ROWSIZE;
            data_start += prv->rowbytes;
#ifdef HE_BITMAP_MASK
            mask_start += prv->rowbytes;
#endif
        }
    }
    
    playdate->graphics->markUpdatedRows(y1, y2 - 1);
}
