//
//  hebitmap_prv.c
//  HE Playdate Library
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#include "he_prv.h"

static PlaydateAPI *playdate;

HEGraphicsContext *he_graphics_context;

//
// Rect
//
HERect he_rect_intersection(HERect clipRect, HERect rect)
{
    clipRect.width = he_max(clipRect.width, 0);
    clipRect.height = he_max(clipRect.height, 0);
    
    rect.width = he_max(rect.width, 0);
    rect.height = he_max(rect.height, 0);
    
    int clipRight = clipRect.x + clipRect.width;
    int clipBottom = clipRect.y + clipRect.height;
    
    if(rect.x < clipRect.x)
    {
        int new_width = rect.width - (clipRect.x - rect.x);
        rect.width = he_max(new_width, 0);
        rect.x = clipRect.x;
    }
    if(rect.y < clipRect.y)
    {
        int new_height = rect.height - (clipRect.y - rect.y);
        rect.height = he_max(new_height, 0);
        rect.y = clipRect.y;
    }
    if((rect.x + rect.width) > clipRight)
    {
        rect.width = clipRight - rect.x;
    }
    if((rect.y + rect.height) > clipBottom)
    {
        rect.height = clipBottom - rect.y;
    }
    
    return rect;
}

//
// Utils
//
void he_bitmap_clip_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, HERect clipRect)
{
    _HEBitmap *prv = bitmap->prv;
    
    *x1 = x;
    *offset_left = 0;

    if(x < clipRect.x)
    {
        *x1 = clipRect.x;
        *offset_left = clipRect.x - x;
    }
    
    *y1 = y;
    *offset_top = 0;
    
    if(y < clipRect.y)
    {
        *y1 = clipRect.y;
        *offset_top = clipRect.y - y;
    }
    
    int bitmap_x2 = x + prv->bw;
    int bitmap_y2 = y + prv->bh;
    
    int clip_x2 = clipRect.x + clipRect.width;
    int clip_y2 = clipRect.y + clipRect.height;

    *x2 = he_min(bitmap_x2, clip_x2);
    *y2 = he_min(bitmap_y2, clip_y2);
}

void he_prv_init(PlaydateAPI *pd)
{
    playdate = pd;
}
