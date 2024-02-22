//
//  hebitmap_prv.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 12/02/24.
//

#include "hebitmap_prv.h"

void adjust_bounds(HEBitmap *bitmap, int x, int y, unsigned int *x1, unsigned int *y1, unsigned int *x2, unsigned int *y2, unsigned int *offset_left, unsigned int *offset_top, int target_width, int target_height)
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
    
    unsigned int p_x2 = *x1 - *offset_left + prv->bw;
    unsigned int p_y2 = *y1 - *offset_top + prv->bh;
    
    *x2 = HEBITMAP_MIN(p_x2, target_width);
    *y2 = HEBITMAP_MIN(p_y2, target_height);
}
