//
//  he_bitmap.h
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 11/06/24.
//

#ifndef he_bitmap_h
#define he_bitmap_h

#include "pd_api.h"

typedef struct HEBitmap {
    void *prv;
    int width;
    int height;
} HEBitmap;

typedef struct HEBitmapTable {
    void *prv;
    unsigned int length;
} HEBitmapTable;

//
// Bitmap
//
HEBitmap* HEBitmap_load(const char *filename);
HEBitmap* HEBitmap_fromLCDBitmap(LCDBitmap *lcd_bitmap);
HEBitmap* HEBitmap_loadHEB(const char *filename);
void HEBitmap_draw(HEBitmap *bitmap, int x, int y);
LCDColor HEBitmap_colorAt(HEBitmap *bitmap, int x, int y);
void HEBitmap_getData(HEBitmap *bitmap, uint8_t **data, uint8_t **mask, int *rowbytes, int *bx, int *by, int *bw, int *bh);
void HEBitmap_free(HEBitmap *bitmap);

//
// Bitmap table
//
HEBitmapTable* HEBitmapTable_load(const char *filename);
HEBitmapTable* HEBitmapTable_fromLCDBitmapTable(LCDBitmapTable *lcd_bitmapTable);
HEBitmapTable* HEBitmapTable_loadHEBT(const char *filename);
HEBitmap* HEBitmap_atIndex(HEBitmapTable *bitmapTable, unsigned int index);
void HEBitmapTable_free(HEBitmapTable *bitmapTable);

#endif /* he_bitmap_h */
