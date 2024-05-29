//
//  hebitmap_prv.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#include "hebitmap_prv.h"

_HEBitmapRect _clipRect = (_HEBitmapRect){
    .x = 0,
    .y = 0,
    .width = LCD_COLUMNS,
    .height = LCD_ROWS
};

_HEBitmapRect clipRect = (_HEBitmapRect){
    .x = 0,
    .y = 0,
    .width = LCD_COLUMNS,
    .height = LCD_ROWS
};

void clip_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, _HEBitmapRect clipRect)
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

    *x2 = hebitmap_min(bitmap_x2, clip_x2);
    *y2 = hebitmap_min(bitmap_y2, clip_y2);
}
