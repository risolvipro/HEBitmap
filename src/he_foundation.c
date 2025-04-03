//
//  he_foundation.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 11/06/24.
//

#include "he_foundation.h"

HERect he_rect_new(int x, int y, int width, int height)
{
    return (HERect){
        .x = x,
        .y = y,
        .width = width,
        .height = height
    };
}

HERect he_rect_zero(void)
{
    return he_rect_new(0, 0, 0, 0);
}
