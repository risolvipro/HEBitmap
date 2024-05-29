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

typedef struct HEBitmapTable {
    void *prv;
    unsigned int length;
} HEBitmapTable;

void HEBitmapInit(PlaydateAPI *pd, int enableLua);

HEBitmap* HEBitmapLoad(const char *filename);
HEBitmap* HEBitmapFromLCDBitmap(LCDBitmap *lcd_bitmap);
HEBitmap* HEBitmapLoadHEB(const char *filename);

void HEBitmapDraw(HEBitmap *bitmap, int x, int y);
LCDColor HEBitmapColorAt(HEBitmap *bitmap, int x, int y);
void HEBitmapGetData(HEBitmap *bitmap, uint8_t **data, uint8_t **mask, int *rowbytes, int *bx, int *by, int *bw, int *bh);
void HEBitmapSetClipRect(int x, int y, int width, int height);
void HEBitmapClearClipRect(void);
void HEBitmapFree(HEBitmap *bitmap);

HEBitmapTable* HEBitmapTableLoad(const char *filename);
HEBitmapTable* HEBitmapTableFromLCDBitmapTable(LCDBitmapTable *lcd_bitmapTable);
HEBitmapTable* HEBitmapTableLoadHEBT(const char *filename);

HEBitmap* HEBitmapAtIndex(HEBitmapTable *bitmapTable, unsigned int index);
void HEBitmapTableFree(HEBitmapTable *bitmapTable);

#endif /* hebitmap_h */
