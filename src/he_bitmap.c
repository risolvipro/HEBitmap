//
//  he_bitmap.c
//  HEBitmap
//
//  Created by Matteo D'Ignazio on 11/06/24.
//

#include "he_bitmap.h"
#include "he_api.h"
#include "he_prv.h"

#include "he_bitmap_draw.h" // Opaque
#define HE_BITMAP_MASK
#include "he_bitmap_draw.h" // Mask
#undef HE_BITMAP_MASK

static PlaydateAPI *playdate;

static HEBitmap* HEBitmap_fromBuffer(uint8_t *buffer, int isOwner, int *retainBuffer, _HEBitmapAllocator *allocator, int useAllocator);

static _HEBitmapAllocator HEBitmapAllocator_zero(void);
static void HEBitmapAllocator_alloc_bitmaps(_HEBitmapAllocator *allocator, unsigned int length);
static void HEBitmapAllocator_free(_HEBitmapAllocator *allocator);

static uint8_t read_uint8(uint8_t **buffer_ptr);
static uint32_t read_uint32(uint8_t **buffer_ptr);
static void buffer_align_8_32(uint8_t *dst, uint8_t *src, int dst_cols, int src_cols, int x, int y, int width, int height, uint8_t fill_value);
static void get_bounds(uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh);
static void allocation_failed(void);

//
// Bitmap
//
HEBitmap* HEBitmap_base(_HEBitmapAllocator *allocator)
{
    HEBitmap *bitmap;
    
    if(allocator)
    {
        bitmap = &allocator->bitmaps[allocator->bitmapsCount++];
    }
    else
    {
        bitmap = playdate->system->realloc(NULL, sizeof(HEBitmap));
    }
    
    bitmap->width = 0;
    bitmap->height = 0;
    
    _HEBitmap *prv = &bitmap->prv;
    
    prv->rawBuffer = NULL;
    prv->hasMask = 0;
    prv->isOwner = 0;
    prv->freeData = 0;
    prv->freeSelf = allocator ? 0 : 1;

    prv->bx = 0;
    prv->by = 0;
    prv->bw = 0;
    prv->bh = 0;
    prv->rowbytes = 0;
    
    prv->data = NULL;
    prv->mask = NULL;
    
    return bitmap;
}

HEBitmap* HEBitmap_load(const char *filename)
{
    LCDBitmap *lcd_bitmap = playdate->graphics->loadBitmap(filename, NULL);
    if(lcd_bitmap)
    {
        HEBitmap *bitmap = HEBitmap_fromLCDBitmap(lcd_bitmap);
        playdate->graphics->freeBitmap(lcd_bitmap);
        return bitmap;
    }
    return NULL;
}

HEBitmap* _HEBitmap_fromLCDBitmap(LCDBitmap *lcd_bitmap, int isOwner, _HEBitmapAllocator *allocator)
{
    int width, height, rowbytes;
    uint8_t *lcd_mask, *lcd_data;
    playdate->graphics->getBitmapData(lcd_bitmap, &width, &height, &rowbytes, &lcd_mask, &lcd_data);
    
    int bx, by, bw, bh;
    get_bounds(lcd_mask, rowbytes, width, height, &bx, &by, &bw, &bh);
    
    int rowbytes_aligned = ((bw + 31) / 32) * 4;
    size_t data_size = rowbytes_aligned * bh;
    
    uint8_t *data = playdate->system->realloc(NULL, data_size);
    
    if(!data)
    {
        allocation_failed();
        return NULL;
    }
    
    uint8_t *mask = NULL;
    
    if(lcd_mask)
    {
        mask = playdate->system->realloc(NULL, data_size);
        if(!mask)
        {
            allocation_failed();
            playdate->system->realloc(data, 0);
            return NULL;
        }
    }
    
    HEBitmap *bitmap = HEBitmap_base(allocator);
    
    bitmap->width = width;
    bitmap->height = height;
    
    _HEBitmap *prv = &bitmap->prv;
    
    prv->isOwner = isOwner;
    prv->freeData = 1;
    
    prv->bx = bx;
    prv->by = by;
    prv->bw = bw;
    prv->bh = bh;
    
    prv->rowbytes = rowbytes_aligned;

    prv->data = data;
    buffer_align_8_32(prv->data, lcd_data, rowbytes_aligned, rowbytes, bx, by, bw, bh, 0x00);
        
    prv->mask = mask;
    if(prv->mask)
    {
        buffer_align_8_32(prv->mask, lcd_mask, rowbytes_aligned, rowbytes, bx, by, bw, bh, 0x00);
    }
    
    return bitmap;
}

HEBitmap* HEBitmap_fromLCDBitmap(LCDBitmap *lcd_bitmap)
{
    return _HEBitmap_fromLCDBitmap(lcd_bitmap, 1, NULL);
}

HEBitmap* HEBitmap_loadHEB(const char *filename)
{
    SDFile *file = playdate->file->open(filename, kFileRead);
    if(!file)
    {
        return NULL;
    }
    
    playdate->file->seek(file, 0, SEEK_END);
    unsigned int file_size = playdate->file->tell(file);
    playdate->file->seek(file, 0, SEEK_SET);

    uint8_t *buffer = playdate->system->realloc(NULL, file_size);
    if(buffer)
    {
        playdate->file->read(file, buffer, file_size);
    }
    playdate->file->close(file);
    
    if(!buffer)
    {
        allocation_failed();
        return NULL;
    }
    
    int retainBuffer;
    HEBitmap *bitmap = HEBitmap_fromBuffer(buffer, 1, &retainBuffer, NULL, 0);
    if(!retainBuffer)
    {
        playdate->system->realloc(buffer, 0);
    }
    return bitmap;
}

static void decompress(uint8_t *dst, uint8_t **buffer, size_t len)
{
    size_t i = 0;
    
    while(i < len)
    {
        uint8_t count = **buffer;
        (*buffer)++;
        uint8_t data = **buffer;
        (*buffer)++;
        size_t end = i + count;
        if(end > len)
        {
            end = len;
        }
        while(i < end)
        {
            dst[i++] = data;
        }
    }
}

static HEBitmap* HEBitmap_fromBuffer(uint8_t *buffer, int isOwner, int *retainBuffer, _HEBitmapAllocator *allocator, int useAllocator)
{
    HEBitmap *bitmap = HEBitmap_base(allocator);
    _HEBitmap *prv = &bitmap->prv;
    
    prv->isOwner = isOwner;
    
    uint8_t *buffer_ptr = buffer;
    
    // Read version
    uint32_t version = read_uint32(&buffer_ptr);
    
    bitmap->width = read_uint32(&buffer_ptr);
    bitmap->height = read_uint32(&buffer_ptr);
    prv->bx = read_uint32(&buffer_ptr);
    prv->by = read_uint32(&buffer_ptr);
    prv->bw = read_uint32(&buffer_ptr);
    prv->bh = read_uint32(&buffer_ptr);
    prv->rowbytes = read_uint32(&buffer_ptr);
    prv->hasMask = read_uint8(&buffer_ptr);
    
    int compressed = 0;
    if(version >= 3)
    {
        // Version 3 supports compression
        compressed = read_uint8(&buffer_ptr);
    }
    
    if(version >= 2)
    {
        // Version 2 supports padding
        uint32_t padding_len = read_uint32(&buffer_ptr);
        buffer_ptr += padding_len;
    }
        
    if(compressed)
    {
        if(allocator && allocator->data && useAllocator)
        {
            size_t data_size = prv->rowbytes * prv->bh;
                
            prv->data = allocator->data_ptr;
            decompress(allocator->data_ptr, &buffer_ptr, data_size);
            allocator->data_ptr += data_size;
            
            if(bitmap->prv.hasMask)
            {
                bitmap->prv.mask = allocator->data_ptr;
                decompress(allocator->data_ptr, &buffer_ptr, data_size);
                allocator->data_ptr += data_size;
            }
        }
        else
        {
            prv->freeData = 1;
            
            size_t data_size = prv->rowbytes * prv->bh;
            
            prv->data = playdate->system->realloc(NULL, data_size);
            decompress(prv->data, &buffer_ptr, data_size);
            
            if(prv->hasMask)
            {
                prv->mask = playdate->system->realloc(NULL, data_size);
                decompress(prv->mask, &buffer_ptr, data_size);
            }
        }
        
        *retainBuffer = 0;
    }
    else
    {
        if(isOwner)
        {
            prv->rawBuffer = buffer;
        }
        prv->data = buffer_ptr;
        
        if(prv->hasMask)
        {
            buffer_ptr += prv->rowbytes * prv->bh;
            prv->mask = buffer_ptr;
        }
        
        *retainBuffer = 1;
    }
    
    return bitmap;
}

void HEBitmap_draw(HEBitmap *bitmap, int x, int y)
{
    if(bitmap->prv.mask)
    {
        HEBitmap_drawMask(playdate, bitmap, x, y);
    }
    else
    {
        HEBitmap_drawOpaque(playdate, bitmap, x, y);
    }
}

LCDColor HEBitmap_colorAt(HEBitmap *bitmap, int x, int y)
{
    _HEBitmap *prv = &bitmap->prv;
    
    if(x >= prv->bx && x < (prv->bx + prv->bw) && y >= prv->by && y < (prv->by + prv->bh))
    {
        int src_x = x - prv->bx;
        int src_y = y - prv->by;

        int i = src_y * prv->rowbytes + (unsigned int)src_x / 8;
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

void HEBitmap_getData(HEBitmap *bitmap, uint8_t **data, uint8_t **mask, int *rowbytes, int *bx, int *by, int *bw, int *bh)
{
    _HEBitmap *prv = &bitmap->prv;
    
    if(data)
    {
        *data = prv->data;
    }
    if(mask)
    {
        *mask = prv->mask;
    }
    
    *rowbytes = prv->rowbytes;
    *bx = prv->bx;
    *by = prv->by;
    *bw = prv->bw;
    *bh = prv->bh;
}

void _HEBitmap_free(HEBitmap *bitmap)
{
    _HEBitmap *prv = &bitmap->prv;
    
    if(prv->freeData)
    {
        playdate->system->realloc(prv->data, 0);
        if(prv->mask)
        {
            playdate->system->realloc(prv->mask, 0);
        }
    }
    
    if(prv->rawBuffer)
    {
        playdate->system->realloc(prv->rawBuffer, 0);
    }
    
    if(prv->freeSelf)
    {
        playdate->system->realloc(bitmap, 0);
    }
}

void HEBitmap_free(HEBitmap *bitmap)
{
    _HEBitmap *prv = &bitmap->prv;
    
    if(!prv->isOwner)
    {
        return;
    }
    
    _HEBitmap_free(bitmap);
}

//
// Bitmap table
//
HEBitmapTable* HEBitmapTable_base(void)
{
    HEBitmapTable *bitmapTable = playdate->system->realloc(NULL, sizeof(HEBitmapTable));
    
    bitmapTable->length = 0;
    
    _HEBitmapTable *prv = &bitmapTable->prv;
    
    prv->rawBuffer = NULL;
    prv->allocator = HEBitmapAllocator_zero();
    
    return bitmapTable;
}

HEBitmapTable* HEBitmapTable_load(const char *filename)
{
    LCDBitmapTable *lcd_bitmapTable = playdate->graphics->loadBitmapTable(filename, NULL);
    if(lcd_bitmapTable)
    {
        HEBitmapTable *bitmapTable = HEBitmapTable_fromLCDBitmapTable(lcd_bitmapTable);
        playdate->graphics->freeBitmapTable(lcd_bitmapTable);
        return bitmapTable;
    }
    return NULL;
}

HEBitmapTable* HEBitmapTable_fromLCDBitmapTable(LCDBitmapTable *lcd_bitmapTable)
{
    HEBitmapTable *bitmapTable = HEBitmapTable_base();
    _HEBitmapTable *prv = &bitmapTable->prv;
    
    int length;
    playdate->graphics->getBitmapTableInfo(lcd_bitmapTable, &length, NULL);
    bitmapTable->length = length;
    
    HEBitmapAllocator_alloc_bitmaps(&prv->allocator, length);
    
    int valid = 1;
    
    for(int i = 0; i < length; i++)
    {
        LCDBitmap *lcd_bitmap = playdate->graphics->getTableBitmap(lcd_bitmapTable, i);
        HEBitmap *bitmap = _HEBitmap_fromLCDBitmap(lcd_bitmap, 0, &prv->allocator);
        if(!bitmap)
        {
            valid = 0;
            break;
        }
    }
    
    if(!valid)
    {
        HEBitmapTable_free(bitmapTable);
        return NULL;
    }
    
    return bitmapTable;
}

HEBitmapTable* HEBitmapTable_loadHEBT(const char *filename)
{
    return HEBitmapTable_loadHEBT_options(filename, 1);
}

HEBitmapTable* HEBitmapTable_loadHEBT_options(const char *filename, int useAllocator)
{
    SDFile *file = playdate->file->open(filename, kFileRead);
    if(!file)
    {
        return NULL;
    }
    
    playdate->file->seek(file, 0, SEEK_END);
    unsigned int file_size = playdate->file->tell(file);
    playdate->file->seek(file, 0, SEEK_SET);

    uint8_t *buffer = playdate->system->realloc(NULL, file_size);
    if(buffer)
    {
        playdate->file->read(file, buffer, file_size);
    }
    playdate->file->close(file);
    
    if(!buffer)
    {
        allocation_failed();
        return NULL;
    }
    
    HEBitmapTable *bitmapTable = HEBitmapTable_base();
    _HEBitmapTable *prv = &bitmapTable->prv;
    
    uint8_t *buffer_ptr = buffer;
    
    // Read version
    uint32_t version = read_uint32(&buffer_ptr);
    
    uint32_t length = read_uint32(&buffer_ptr);
    bitmapTable->length = length;
    
    HEBitmapAllocator_alloc_bitmaps(&prv->allocator, length);
    
    int compressed = 0;
    if(version >= 3)
    {
        // Version 3 supports compression
        compressed = read_uint8(&buffer_ptr);
        
        if(version >= 4 && compressed)
        {
            // Version 4 supports allocator
            uint32_t allocator_data_len = read_uint32(&buffer_ptr);
            if(useAllocator)
            {
                prv->allocator.data = playdate->system->realloc(NULL, allocator_data_len);
                if(!prv->allocator.data)
                {
                    allocation_failed();
                    HEBitmapTable_free(bitmapTable);
                    return NULL;
                }
                prv->allocator.data_ptr = prv->allocator.data;
            }
        }
    }
    
    if(version >= 2)
    {
        // Version 2 supports padding
        uint32_t padding_len = read_uint32(&buffer_ptr);
        buffer_ptr += padding_len;
    }
    
    if(compressed && useAllocator && !prv->allocator.data)
    {
        // Compatibility mode
        uint8_t *table_ptr = buffer_ptr;
        size_t allocator_data_len = 0;
        
        for(uint32_t i = 0; i < length; i++)
        {
            uint32_t bitmap_size = read_uint32(&table_ptr);
            uint8_t *bitmap_ptr = table_ptr;
            
            // Skip metadata
            bitmap_ptr += 4; // version
            bitmap_ptr += 4; // width
            bitmap_ptr += 4; // height
            bitmap_ptr += 4; // bx
            bitmap_ptr += 4; // by
            bitmap_ptr += 4; // bw
            
            int bh = read_uint32(&bitmap_ptr);
            int rowbytes = read_uint32(&bitmap_ptr);
            int hasMask = read_uint8(&bitmap_ptr);
            
            allocator_data_len += rowbytes * bh;
            if(hasMask)
            {
                allocator_data_len += rowbytes * bh;
            }
            
            table_ptr += bitmap_size;
        }
        
        prv->allocator.data = playdate->system->realloc(NULL, allocator_data_len);
        if(!prv->allocator.data)
        {
            allocation_failed();
            HEBitmapTable_free(bitmapTable);
            return NULL;
        }
        prv->allocator.data_ptr = prv->allocator.data;
    }
    
    int retainBufferTable = 0;
    
    for(uint32_t i = 0; i < length; i++)
    {
        uint32_t bitmap_size = read_uint32(&buffer_ptr);
        
        int retainBufferBitmap;
        HEBitmap_fromBuffer(buffer_ptr, 0, &retainBufferBitmap, &prv->allocator, useAllocator);
        
        if(retainBufferBitmap){
            retainBufferTable = 1;
        }
        buffer_ptr += bitmap_size;
    }
    
    if(retainBufferTable)
    {
        prv->rawBuffer = buffer;
    }
    else
    {
        playdate->system->realloc(buffer, 0);
    }
    
    return bitmapTable;
}

HEBitmap* HEBitmap_atIndex(HEBitmapTable *bitmapTable, unsigned int index)
{
    _HEBitmapTable *prv = &bitmapTable->prv;
    
    if(index < bitmapTable->length)
    {
        HEBitmap *bitmap = &prv->allocator.bitmaps[index];
        return bitmap;
    }
    
    return NULL;
}

void HEBitmapTable_free(HEBitmapTable *bitmapTable)
{
    _HEBitmapTable *prv = &bitmapTable->prv;
    
    for(unsigned int i = 0; i < prv->allocator.bitmapsCount; i++)
    {
        HEBitmap *bitmap = &prv->allocator.bitmaps[i];
        _HEBitmap_free(bitmap);
    }
    
    if(prv->rawBuffer)
    {
        playdate->system->realloc(prv->rawBuffer, 0);
    }
    
    HEBitmapAllocator_free(&prv->allocator);
    
    playdate->system->realloc(bitmapTable, 0);
}

static uint8_t read_uint8(uint8_t **buffer_ptr)
{
    uint8_t *buffer = *buffer_ptr;
    uint8_t value = buffer[0];
    *buffer_ptr += 1;
    return value;
}

static uint32_t read_uint32(uint8_t **buffer_ptr)
{
    uint8_t *buffer = *buffer_ptr;
    uint32_t value = (uint32_t)buffer[0] << 24 | (uint32_t)buffer[1] << 16 | (uint32_t)buffer[2] << 8 | (uint32_t)buffer[3];
    *buffer_ptr += 4;
    return value;
}

static void buffer_align_8_32(uint8_t *dst, uint8_t *src, int dst_rowbytes, int src_rowbytes, int x, int y, int width, int height, uint8_t fill_value)
{
    int src_offset = y * src_rowbytes;
    int dst_offset = 0;
    int aligned_width = dst_rowbytes * 8;
    
    for(int dst_y = 0; dst_y < height; dst_y++)
    {
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
        }
        
        src_offset += src_rowbytes;
        dst_offset += dst_rowbytes;
    }
}

static void get_bounds(uint8_t *mask, int rowbytes, int width, int height, int *bx, int *by, int *bw, int *bh)
{
    int min_y = 0; int min_x = 0; int max_x = width; int max_y = height;

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
                    
                    min_y = he_min(min_y, y);
                    int y2 = y + 1;
                    max_y = he_max(max_y, y2);
                    
                    min_x = he_min(min_x, x);
                    int x2 = x + 1;
                    max_x = he_max(max_x, x2);
                }
            }
            
            src_offset += rowbytes;
        }
    }
    
    *bx = min_x; *by = min_y; *bw = max_x - min_x; *bh = max_y - min_y;
}

static void allocation_failed(void)
{
    playdate->system->logToConsole("HEBitmap: cannot allocate data");
}

static _HEBitmapAllocator HEBitmapAllocator_zero(void)
{
    return (_HEBitmapAllocator){
        .bitmaps = NULL,
        .bitmapsCount = 0,
        .data = NULL,
        .data_ptr = NULL
    };
}

static void HEBitmapAllocator_alloc_bitmaps(_HEBitmapAllocator *allocator, unsigned int length)
{
    if(length > 0)
    {
        allocator->bitmaps = playdate->system->realloc(NULL, length * sizeof(HEBitmap));
    }
}

static void HEBitmapAllocator_free(_HEBitmapAllocator *allocator)
{
    if(allocator->bitmaps)
    {
        playdate->system->realloc(allocator->bitmaps, 0);
    }
    
    if(allocator->data)
    {
        playdate->system->realloc(allocator->data, 0);
    }
}

void he_bitmap_init(PlaydateAPI *pd)
{
    playdate = pd;
}
