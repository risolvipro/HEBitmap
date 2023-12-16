//
//  hebitmap.h
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 05/11/23.
//

#ifndef hebitmap_h
#define hebitmap_h

#include "pd_api.h"

typedef struct HEBitmap {
    void *prv;
    int width;
    int height;
} HEBitmap;

void HEBitmapSetPlaydateAPI(PlaydateAPI *pd);

HEBitmap* HEBitmapNew(LCDBitmap const *lcd_bitmap);
void HEBitmapDraw(HEBitmap const *bitmap, int x, int y);
void HEBitmapFree(HEBitmap *bitmap);

#endif /* hebitmap_h */
